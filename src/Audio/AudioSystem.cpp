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
#include <unordered_map>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio.h"
#include "AtmosBanks.h"
#include "AudioManagerInterface.h"
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
/// LHSampleRegister3DObjectFunction(fn_00427200, 800.0) at 0x426E6B: LH_AudioSystem+0x44, the farthest an anim effect
/// starts (LHSamplePlayAnimEffect 0x1001475A / 0x10014A8A)
constexpr float k_AnimEffectMaxDistance = 800.0f;

struct BankEntry
{
	std::string path; ///< lower case, '/' separators
	std::string group;
};

struct State
{
	std::vector<BankEntry> banks; ///< BankId - 1
	/// GAudio+0x3A8 + 4 * type
	std::array<BankId, static_cast<size_t>(SfxBank::_COUNT)> types {};
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

std::string Normalised(std::string_view path)
{
	std::string text(path);
	std::replace(text.begin(), text.end(), '\\', '/');
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

bool EndsWith(const std::string& text, const std::string& tail)
{
	return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
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
	default:
		return std::nullopt;
	}
}

/// GAudio's filters shared by PlaySoundEffect (0x429F25..0x429FD9) and SamplePlayAnimEffect (0x42A51B..0x42A5B8):
/// `ownerTest` is whether the unavailable owner is tested (is3D && track for the first, track for the second)
bool Filtered(const Sound& sound, uint32_t bank, const Owner& owner, bool ownerTest)
{
	// LHSampleGetUserParam: the 16 bits compared at 0x429F65 (cmp di, 1)
	const auto kind = static_cast<uint16_t>(sound.userParam);
	if (g_State.scriptWideScreen && kind == 1)
	{
		return true;
	}
	if (IsInsideCitadel() && kind != 2)
	{
		return true;
	}
	// GScript+0x90 (SET_GAME_SOUND false): only GAudio+0x3C4 / +0x3C0 (banks 7 and 6 of 0x9CB3F8)
	if (GetScriptAudioState().gameSoundOff != 0 && bank != Bank(SfxBank::Villagers) && bank != Bank(SfxBank::HelpSprites))
	{
		return true;
	}
	const int state = g_State.queries.interfaceState ? g_State.queries.interfaceState() : 0;
	if ((state == 0x10 || state == 0x16 || state == 0x17) && kind == 4)
	{
		return true;
	}
	return ownerTest && OwnerUnavailable(owner);
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

// ---- banks --------------------------------------------------------------------------------------------------------

BankId audio::RegisterBank(const std::filesystem::path& path, std::string_view group)
{
	const auto normalised = Normalised(path.generic_string());
	for (size_t i = 0; i < g_State.banks.size(); ++i)
	{
		if (g_State.banks[i].path == normalised)
		{
			return static_cast<BankId>(i + 1);
		}
	}
	g_State.banks.push_back({normalised, std::string(group)});
	const auto id = static_cast<BankId>(g_State.banks.size());
	// fn_0042A390: the slot of a type is filled once, with the bank of its path
	for (size_t type = 1; type < k_SfxBankPaths.size(); ++type)
	{
		if (g_State.types[type] == k_NoBank && EndsWith(normalised, Normalised(k_SfxBankPaths[type])))
		{
			g_State.types[type] = id;
		}
	}
	return id;
}

BankId audio::Bank(SfxBank type)
{
	const auto index = static_cast<size_t>(type);
	return index < g_State.types.size() ? g_State.types[index] : k_NoBank;
}

BankId audio::FindBank(std::string_view path)
{
	const auto wanted = Normalised(path);
	for (size_t i = 0; i < g_State.banks.size(); ++i)
	{
		if (EndsWith(g_State.banks[i].path, wanted))
		{
			return static_cast<BankId>(i + 1);
		}
	}
	return k_NoBank;
}

std::string audio::BankGroup(BankId bank)
{
	return bank != k_NoBank && bank <= g_State.banks.size() ? g_State.banks[bank - 1].group : std::string {};
}

entt::id_type audio::SampleId(BankId bank, int number)
{
	const auto group = BankGroup(bank);
	if (group.empty())
	{
		return 0;
	}
	const auto key = fmt::format("{}/{}", group, number);
	return entt::hashed_string(key.c_str()).value();
}

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

std::optional<glm::vec3> audio::OwnerSoundPosition(const Owner& owner)
{
	return OwnerPosition(owner);
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
				return k_NoChannel;
			}
		}
	}
	if (Filtered(*sound, sound->bank, options.owner, options.is3D && options.track))
	{
		return k_NoChannel;
	}
	return sample_play::Start(options);
}

Channel audio::PlayAnimEffectSample(entt::id_type id, Owner owner, glm::vec3 position, bool track)
{
	if (!g_State.initialised)
	{
		return k_NoChannel;
	}
	const auto* sound = sample_play::GetSound(id);
	if (sound == nullptr)
	{
		return k_NoChannel;
	}
	if (Filtered(*sound, sound->bank, owner, track))
	{
		return k_NoChannel;
	}
	// 0x10014A83: the camera distance against the global max (800) and the .sad's raw +0x26C
	if (const auto camera = CameraPoint())
	{
		const float distance = glm::distance(position, *camera);
		if (distance > k_AnimEffectMaxDistance || distance > sound->maxDistance)
		{
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: anim effect {} culled, camera {:.1f} > max {:.1f}",
				                   sound->name, distance, std::min(k_AnimEffectMaxDistance, sound->maxDistance));
			}
			return k_NoChannel;
		}
	}
	sample_play::Options options;
	options.sound = id;
	options.is3D = true;
	options.track = track;
	options.owner = owner;
	options.position = position;
	return sample_play::Start(options);
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
	g_State.scriptWideScreen = on;
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
}

void audio::Shutdown()
{
	// GAudio::ToBeDeleted 0x426FE0 -> ~LH_AudioSystem (StopAll, ... 0x10015C70): the sources before the context
	sample_play::StopAll();
	sample_play::ReleaseSources();
	sample_play::SetBackend({});
	g_State.objects.clear();
	g_State.initialised = false;
}

void audio::ProcessTurn(float skyType, uint32_t turn)
{
	if (!g_State.initialised)
	{
		return;
	}
	// GGame::EndTurn 0x54E96F: GSoundMap::Update (+ Dump 0x54E981)
	sound_map::Update(skyType);
	// 0x54E989 SoundTag::ProcessSoundTags: the street lanterns' tags, then the others (milestone B3 joins them)
	lantern_sounds::ProcessTurn();
	sound_tags::ProcessTurn();
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
	sound_tags::Clear();
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
	// (openblack) the old players' emitters (AnimationSounds, the trees, the fire, the spells... until B2..B5 move them to
	// the channels) are channels of the original too, which LHSampleStopAll 0x426CE6 stops: without this the registry
	// reset of the new map dropped them with their sources still playing (a looping one for ever)
	if (Locator::audio::has_value())
	{
		Locator::audio::value().DestroyAllEmitters();
	}
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
