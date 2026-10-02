/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AudioSystem.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio.h"
#include "AnimEffects.h"
#include "AtmosBanks.h"
#include "Banks.h"
#include "Device.h"
#include "Camera/Camera.h"
#include "EngineConfig.h"
#include "GameMusic.h"
#include "GameQueries.h"
#include "LanternSounds.h"
#include "Locator.h"
#include "MusicEngine.h"
#include "MusicStream.h"
#include "QMixerLaws.h"
#include "ScriptAudioState.h"
#include "Sound.h"
#include "SoundMap.h"
#include "SoundTags.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
struct State
{
	GameQueries queries;
	bool initialised {false};
	/// HelpSystem +0x45E8 && +0x45EC as the script set them (Game's wide screen hook, CHLApi without HelpSystem)
	bool scriptWideScreen {false};
	std::unordered_map<uint32_t, ObjectPositionFn> objects;
};
State g_State;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}

std::optional<glm::vec3> CameraPoint()
{
	if (g_State.queries.camera)
	{
		if (const auto camera = g_State.queries.camera())
		{
			return camera->position;
		}
		return std::nullopt;
	}
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	return Locator::camera::value().GetOrigin();
}

/// fn_00427200 / Get3DSoundPos of an owner (vtable +0x10), nullopt when it has none or is gone
std::optional<glm::vec3> OwnerPosition(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::Thing:
		if (g_State.queries.thingPosition)
		{
			return g_State.queries.thingPosition(static_cast<ThingId>(owner.thing));
		}
		return std::nullopt;
	case Owner::Kind::Object:
	{
		const auto found = g_State.objects.find(owner.id);
		if (found == g_State.objects.end() || !found->second)
		{
			return std::nullopt;
		}
		return found->second();
	}
	case Owner::Kind::SoundTag:
		// SoundTag::Get3DSoundPos 0x71EC90: its thing's (nullopt: no thing, the channel keeps its point)
		return tags::Get3DSoundPos(owner.id);
	default:
		return std::nullopt;
	}
}

/// GAudio's filters shared by PlaySoundEffect (0x429F25..0x429FD9) and SamplePlayAnimEffect (0x42A51B..0x42A5B8):
/// `ownerTest` is whether the unavailable owner is tested (is3D && track for the first, track for the second). The
/// reason the sample does not play (for OPENBLACK_SFX_TRACE), nullptr when it passes.
const char* FilterReason(const Sound& sound, uint32_t bank, const Owner& owner, bool ownerTest)
{
	// LHSampleGetUserParam: the 16 bits compared at 0x429F65 (cmp di, 1)
	const auto kind = static_cast<uint16_t>(sound.userParam);
	if (g_State.scriptWideScreen && kind == 1)
	{
		return "user parameter 1 with the script's wide screen";
	}
	if (IsInsideCitadel() && kind != 2)
	{
		return "inside the citadel, user parameter not 2";
	}
	// GScript+0x90 (SET_GAME_SOUND false): only GAudio+0x3C4 / +0x3C0 (banks 7 and 6 of 0x9CB3F8)
	if (GetScriptAudioState().gameSoundOff != 0 && bank != Bank(SfxBank::Villagers) && bank != Bank(SfxBank::HelpSprites))
	{
		return "SET_GAME_SOUND false";
	}
	const int state = g_State.queries.interfaceState ? g_State.queries.interfaceState() : 0;
	if ((state == 0x10 || state == 0x16 || state == 0x17) && kind == 4)
	{
		return "user parameter 4 in an interface state 0x10 / 0x16 / 0x17";
	}
	if (ownerTest && OwnerUnavailable(owner))
	{
		return "owner unavailable";
	}
	return nullptr;
}

bool Filtered(const Sound& sound, uint32_t bank, const Owner& owner, bool ownerTest)
{
	return FilterReason(sound, bank, owner, ownerTest) != nullptr;
}

std::string OwnerText(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::None:
		return "none";
	case Owner::Kind::Atmos:
		return "atmos";
	case Owner::Kind::Thing:
		return fmt::format("thing {}", static_cast<uint32_t>(owner.thing));
	case Owner::Kind::SoundTag:
		return fmt::format("tag {}", owner.id);
	case Owner::Kind::Key:
		return fmt::format("key {:#x}", owner.id);
	case Owner::Kind::Object:
		return fmt::format("object {}", owner.id);
	}
	return "?";
}

/// The pitch a started channel got (with the .sad's deviation), 0 when it did not start
int ChannelPitch(Channel channel)
{
	if (channel == k_NoChannel)
	{
		return 0;
	}
	for (const auto& info : sample_play::Channels())
	{
		if (info.handle == channel)
		{
			return info.pitch;
		}
	}
	return 0;
}

/// One OPENBLACK_SFX_TRACE line of GAudio::PlaySoundEffect 0x429E30
void TraceSoundEffect(const sample_play::Options& options, const Sound& sound, Channel channel, const std::string& result)
{
	// the mode LHSamplePlay starts with: the .sad's with its flag 0x400 unless the caller set it (0x10011B5C)
	const bool sadMode = (sound.overrides & 0x400) != 0 && (options.callerMask & 0x400) == 0;
	const auto at = options.position + options.offset;
	SPDLOG_LOGGER_INFO(spdlog::get("audio"),
	                   "SFX: {}/{} ({}) {} track {} at ({:.1f}, {:.1f}, {:.1f}) mode {} loops {} pitch {} owner {} -> {}",
	                   BankGroup(static_cast<BankId>(sound.bank)), sound.id, sound.name, options.is3D ? "3D" : "2D",
	                   options.track ? 1 : 0, at.x, at.y, at.z, sadMode ? sound.playMode : options.mode, options.loops,
	                   ChannelPitch(channel), OwnerText(options.owner),
	                   channel != k_NoChannel ? fmt::format("channel {}", channel) : result);
}

/// The atmos of ProcessAudioGameTurn 0x427080 and its gate
void ProcessAudioGameTurn(uint32_t turn)
{
	// 0x427086: LHWaveIsActive. ProcessMusic 0x427092 runs only then; fn_00429700 (the ThingMusicInfo purge, 0x4270C8)
	// always, here inside game_music::ProcessTurn right after the music (it touches nothing the atmos reads)
	const bool active = sample_play::IsActive();
	game_music::ProcessTurn(turn, active);
	if (!active)
	{
		return;
	}
	// fn_00429100 and ProcessAtmosBanks 0x428FE0
	atmos_banks::UpdateBanks();
	// fn_004270D0: LHSampleUpdate3DChannels + LHListenerUpdate
	sample_play::UpdateChannels();
	// 0x4270AC..0x4270C0: LHAtmosProcess(1) unless a video plays
	if (!IsVideoPlaying())
	{
		atmos_banks::Mix();
	}
}
} // namespace

// ---- banks (Banks.cpp) ------------------------------------------------------------------------------------------

BankId audio::CreatureBank(std::string_view species)
{
	return FindBank(fmt::format("audio/sfx/creature/{}.sad", species));
}

float audio::MaxDistance(Sample sample)
{
	// 0x42A430: 0 for no bank, else LHSampleGetMaxDistance
	if (sample.bank == k_NoBank)
	{
		return 0.0f;
	}
	const auto* sound = sample_play::GetSound(SampleId(sample.bank, sample.number));
	return sound != nullptr ? sound->maxDistance : 0.0f;
}

// ---- owners -------------------------------------------------------------------------------------------------------

const GameQueries& audio::Queries()
{
	return g_State.queries;
}

std::optional<glm::vec3> audio::OwnerSoundPosition(const Owner& owner)
{
	return OwnerPosition(owner);
}

std::optional<glm::vec3> audio::Get3DSoundPos(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::None:
		// 0x4272F9: LH3DTech::g_camera
		return CameraPoint();
	case Owner::Kind::Atmos:
		// 0x42726D: -1 gives 0
		return std::nullopt;
	case Owner::Kind::SoundTag:
		// 0x4272D5: a Base's Get3DSoundPos; a tag without a thing answers 1 with the point it was given (the info's
		// +0x50, 0x427209). For a new start (LHSamplePlayAnimEffect 0x10014B91 asks it on the channel just allocated,
		// before LHSamplePlay writes the options' point) that +0x50 is the channel's previous point, a stale value:
		// openblack gives the tag's own point (approximated; no caller starts an anim effect owned by a tag)
		if (const auto at = tags::Get3DSoundPos(owner.id))
		{
			return at;
		}
		return tags::Point(owner.id);
	case Owner::Kind::Key:
		// a plain number is never given to the 3D function (the voices' keys play 2D or untracked) (inferred)
		return std::nullopt;
	default:
		// 0x4272A6: a GameThing not IsAvailable gives 0; else Get3DSoundPos
		return OwnerPosition(owner);
	}
}

std::optional<glm::vec3> audio::ListenerPoint()
{
	return CameraPoint();
}

float audio::IslandAltitude(float x, float z)
{
	return g_State.queries.landAltitude ? g_State.queries.landAltitude(x, z) : 0.0f;
}

bool audio::OwnerUnavailable(const Owner& owner)
{
	// 0x429D20: only a GameThing can be unavailable; 0 and -1 are not tested
	if (owner.kind != Owner::Kind::Thing || !g_State.queries.thingPosition)
	{
		return false;
	}
	return !g_State.queries.thingPosition(static_cast<ThingId>(owner.thing)).has_value();
}

void audio::RegisterObject(uint32_t id, ObjectPositionFn position)
{
	g_State.objects[id] = std::move(position);
}

void audio::UnregisterObject(uint32_t id)
{
	g_State.objects.erase(id);
}

uint32_t audio::NewObjectId()
{
	// (openblack) 0 is no owner's: the numbers start at 1 and are not reused
	static uint32_t s_Next = 0;
	return ++s_Next;
}

// ---- GAudio::PlaySoundEffect ----------------------------------------------------------------------------------------

Channel audio::PlaySoundEffectOptions(const sample_play::Options& options)
{
	if (!g_State.initialised)
	{
		return k_NoChannel;
	}
	const auto* sound = sample_play::GetSound(options.sound);
	if (sound == nullptr)
	{
		if (SfxTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SFX: sound {:#x} not loaded", options.sound);
		}
		return k_NoChannel;
	}
	// 0x429E59..0x429F1F: a 3D sample with a sample number
	if (options.is3D && sound->id != 0)
	{
		const auto camera = CameraPoint();
		// GetGSFXSampleMaxDistance 0x42A430 (.sad +0x26C raw), the options' +0x58 when 0 (0x429EFC..0x429F0B)
		const float maxDistance = sound->maxDistance != 0.0f ? sound->maxDistance : options.maxDistance;
		if (camera)
		{
			// GCamera::GetDistanceSq of pos + offset; inside the citadel from LH3DTech::g_camera (the same point here)
			const auto d = options.position + options.offset - *camera;
			const float distanceSq = glm::dot(d, d);
			if (distanceSq > maxDistance * maxDistance)
			{
				if (Trace())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} culled, camera {:.1f} > max {:.1f}",
					                   sound->name, std::sqrt(distanceSq), maxDistance);
				}
				if (SfxTrace())
				{
					TraceSoundEffect(options, *sound, k_NoChannel,
					                 fmt::format("culled (camera {:.1f} > max {:.1f})", std::sqrt(distanceSq), maxDistance));
				}
				return k_NoChannel;
			}
		}
	}
	if (const char* reason = FilterReason(*sound, sound->bank, options.owner, options.is3D && options.track))
	{
		if (SfxTrace())
		{
			TraceSoundEffect(options, *sound, k_NoChannel, fmt::format("filtered ({})", reason));
		}
		return k_NoChannel;
	}
	const auto channel = sample_play::Start(options);
	if (SfxTrace())
	{
		TraceSoundEffect(options, *sound, channel, "no channel");
	}
	return channel;
}

Channel audio::SamplePlayAnimEffect(Owner owner, float distance, const AnimKey& key, AnimAction action, BankId bank,
                                    bool track, float minDistance, float maxDistance)
{
	// 0x42A4BC: an action goes to the key variant 0x100146F0 with no filter and no min / max (0x42A4C6 / 0x42A4C8)
	if (action != AnimAction::Play)
	{
		return anim_effects::PlayKey(owner, distance, key, action, track, bank, 0.0f, 0.0f);
	}
	// 0x42A4F9: LHSampleGetAnimEffectNumber; 0 = nothing (0x42A515)
	const int sample = anim_effects::Number(key, bank);
	if (sample == 0 || !g_State.initialised)
	{
		return k_NoChannel;
	}
	const auto* sound = sample_play::GetSound(SampleId(bank, sample));
	if (sound == nullptr)
	{
		return k_NoChannel;
	}
	// 0x42A51B..0x42A5B8: the user parameter's filters (LHSampleGetUserParam 0x42A520), the banks after SET_GAME_SOUND
	// (0x42A56E), the interface states, and an unavailable owner when tracking (0x42A5A1, no is3D test)
	const char* reason = FilterReason(*sound, bank, owner, track);
	// 0x42A5D4: LHSamplePlayAnimEffect(owner, dist, n, track, bank, min, max)
	const auto channel =
	    reason != nullptr ? k_NoChannel : anim_effects::Play(owner, distance, sample, track, bank, minDistance, maxDistance);
	if (SfxTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "SFX: anim effect {{{}, {}, {}, {}, {}}} -> {}/{} ({}) 3D track {} distance {:.1f} pitch {} owner {} "
		                   "-> {}",
		                   key[0], key[1], key[2], key[3], key[4], BankGroup(bank), sample, sound->name, track ? 1 : 0,
		                   distance, ChannelPitch(channel), OwnerText(owner),
		                   reason != nullptr                 ? fmt::format("filtered ({})", reason)
		                   : channel != k_NoChannel ? fmt::format("channel {}", channel)
		                                            : std::string("not started (800 / max distance, or no channel)"));
	}
	return channel;
}

Channel audio::PlaySoundEffect(const PlayOptions& options)
{
	sample_play::Options lh = options;
	if (lh.sound == 0)
	{
		lh.sound = SampleId(options.sample.bank, options.sample.number);
	}
	return PlaySoundEffectOptions(lh);
}

// ---- the script's switches ----------------------------------------------------------------------------------------

void audio::SetGameSound(bool enabled)
{
	// GScript::SetGameSound 0x7100B0
	if (!enabled)
	{
		StopAllSoundEffects();
		GetScriptAudioState().gameSoundOff = 1;
		return;
	}
	GetScriptAudioState().gameSoundOff = 0;
}

void audio::SetScriptWideScreen(bool on)
{
	// (openblack test hook, audio session) OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1: the audio never sees the script's wide
	// screen (the Land 1 intro holds it until a click), to compare the anim effects with the wide screen filters off
	static const bool k_Ignore = std::getenv("OPENBLACK_AUDIO_TEST_NO_WIDESCREEN") != nullptr;
	if (k_Ignore)
	{
		on = false;
	}
	if (Trace() && on != g_State.scriptWideScreen)
	{
		// the samples of user parameter 1 and the villagers' footsteps are skipped while it is on
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: script wide screen {}", on ? "on" : "off");
	}
	g_State.scriptWideScreen = on;
}

bool audio::IsScriptWideScreen()
{
	return g_State.scriptWideScreen;
}

bool audio::IsInsideCitadel()
{
	return g_State.queries.insideCitadel && g_State.queries.insideCitadel();
}

bool audio::IsVideoPlaying()
{
	return g_State.queries.videoPlaying && g_State.queries.videoPlaying();
}

// ---- life cycle ---------------------------------------------------------------------------------------------------

void audio::Init(GameQueries queries)
{
	g_State.queries = std::move(queries);
	sample_play::Backend backend;
	backend.camera = []() { return CameraPoint(); };
	backend.ownerPosition = [](const Owner& owner) { return OwnerPosition(owner); };
	sample_play::SetBackend(std::move(backend));
	// fn_00428250 (0x426E70): BWSetup AudioSampleMasterVolume -> LHSampleSetMasterVolume
	if (Locator::config::has_value())
	{
		auto& config = Locator::config::value();
		if (const char* volume = std::getenv("OPENBLACK_TEST_SAMPLE_VOLUME"); volume != nullptr)
		{
			config.audioSampleMasterVolume = static_cast<uint32_t>(std::atoi(volume));
		}
		sample_play::SetMasterVolume(static_cast<int>(config.audioSampleMasterVolume));
	}
	g_State.initialised = true;
	if (auto logger = spdlog::get("audio"))
	{
		SPDLOG_LOGGER_INFO(logger, "GAudio: sample master volume {}", sample_play::MasterVolume());
	}
	// the GAudio ctor 0x426D40 -> fn_00429CB0 (0x426F58): the sample banks of 0x9CB3F8, and with them the atmos ones,
	// every .sad of Audio\ through LHBankRegister 0x10002240 (Banks.h). (approximated) the original registers the 14
	// atmos banks later, InitAtmos 0x428EF0 -> fn_00428F30 from GGame::FinishInitialisation; nothing plays in between
	banks::LoadAll();
}

void audio::Shutdown()
{
	// GAudio::ToBeDeleted 0x426FE0 -> ~LH_AudioSystem (StopAll, ... 0x10015C70): the sources before the context
	sample_play::StopAll();
	sample_play::ReleaseSources();
	sample_play::SetBackend({});
	g_State.objects.clear();
	anim_effects::Clear();
	g_State.initialised = false;
}

bool audio::SoundExists()
{
	// GAudio::IsInstalled 0x426D30 -> LHWaveIsInstalled: the wave device of LH_AudioSystem::Create. (approximated) here
	// the audio is initialised on the OpenAL device (device::Open; without one the channels' output is the
	// NullSampleOutput)
	return g_State.initialised && device::IsOpen();
}

uint32_t audio::TickCount()
{
	// GetTickCount: milliseconds, wrapping at 2^32
	using namespace std::chrono;
	return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

bool audio::SfxTrace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SFX_TRACE") != nullptr;
	return k_Trace;
}

void audio::ProcessTurn(float skyType, uint32_t turn)
{
	if (!g_State.initialised)
	{
		return;
	}
	// GGame::EndTurn 0x54E96F: GSoundMap::Update (+ Dump 0x54E981)
	sound_map::Update(skyType);
	// 0x54E989 SoundTag::ProcessSoundTags (the street lanterns' tags among the others; openblack first gives the
	// lanterns made since the last turn their tag, GStreetLantern::CallVirtualFunctionsForCreation 0x734810)
	lantern_sounds::ProcessTurn();
	tags::ProcessSoundTags();
	// 0x54E997: GAudio::ProcessAudioGameTurn past turn 5, else AtmosProcess(0) 0x54E9B4
	if (turn > 5)
	{
		ProcessAudioGameTurn(turn);
	}
	else
	{
		atmos_banks::Silence();
	}
}

void audio::Paused()
{
	// GGame::EndTurn 0x54E993 with g_game+0x14 & 4: AtmosProcess(0)
	atmos_banks::Silence();
}

void audio::UpdateFrame()
{
	// the 16 channels' finite loops (QMixer counts them as it mixes)
	sample_play::UpdateFrame();
	// the options dialog's slider 0x64 applies the sample master at once (fn_00428600, DialogBoxOptions 0x5145A3);
	// LHSampleSetMasterVolume ignores the same value (0x100150F9)
	if (g_State.initialised && Locator::config::has_value())
	{
		sample_play::SetMasterVolume(static_cast<int>(Locator::config::value().audioSampleMasterVolume));
	}
}

void audio::OnThingDeleted(entt::entity thing)
{
	sample_play::OnThingDeleted(thing);
}

void audio::ClearMap()
{
	// the map's SoundTags go with the objects of ClearMap (SoundTag::ToBeDeleted 0x71ECB0)
	lantern_sounds::Clear();
	tags::Clear();
	// GAudio::Reset 0x426CA0: the music part (+0x18C, fn_00428190, +0x28, +0x24, +0x180, +0x1C; LHMusicStop(0) 0x426CD3,
	// LHMusicSwitch(0)/(1) of the two LHGlobalSwitch, ReleaseAllThingMusicInfo 0x426D28)
	{
		const auto lock = game_music::Lock();
		if (auto* gameMusic = game_music::Get(); gameMusic != nullptr)
		{
			gameMusic->Reset();
		}
	}
	// 0x426CDD LHAtmosProcess(0)
	atmos_banks::Clear();
	// 0x426CE6 LHSampleStopAll, 0x426CF6 LHGlobalSwitch(0) (LHWaveSwitch(0): StopAll again, inactive), the wait for
	// LHSampleGetNumberPlaying() == 0 (0x426D01..0x426D13: nothing to wait for here), LHSampleClearInfoList 0x426D18 (a
	// no-op while inactive) and LHGlobalSwitch(1) 0x426D23
	sample_play::StopAll();
	sample_play::Switch(false);
	sample_play::ClearInfoList();
	sample_play::Switch(true);
	// (openblack) the channels' OpenAL sources: a new map starts with none
	sample_play::ReleaseSources();
}

void audio::OnFocus(bool active)
{
	// fn_00428720 -> LHGlobalSwitch: LHWaveSwitch 0x10015D40 and LHMusicSwitch 0x1000EB00 (midi and redbook unused)
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "GAudio: LHGlobalSwitch({})", active ? 1 : 0);
	}
	sample_play::Switch(active);
	if (auto* system = music::Get(); system != nullptr)
	{
		system->With([active](MusicEngine& engine) { engine.Switch(active ? 1 : 0); });
	}
}

void audio::SetSampleMasterVolume(int volume)
{
	// LHSampleSetMasterVolume ignores more than 127 (0x100150FD)
	if (volume < 0 || volume > qmixer::k_MaxVolume)
	{
		return;
	}
	if (Locator::config::has_value())
	{
		Locator::config::value().audioSampleMasterVolume = static_cast<uint32_t>(volume);
	}
	sample_play::SetMasterVolume(volume);
}

int audio::SampleMasterVolume()
{
	return sample_play::MasterVolume();
}
