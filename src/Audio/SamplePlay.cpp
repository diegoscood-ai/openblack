/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SamplePlay.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Sound.h"
#include "SoundGroup.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::sample_play;
using namespace openblack::ecs::components;

namespace
{
/// LH_AudioSystem+0xCC: the number of sample channels (ctor 0x1001535E)
constexpr size_t k_Channels = 16;
/// LHSampleRegister3DObjectFunction(fn 0x427200, 800.0) at 0x426E6B: LH_AudioSystem+0x44
constexpr float k_GlobalMaxDistance = 800.0f;
/// LHSampleSetMasterVolume (BWSetup AudioSampleMasterVolume = 0x7F)
constexpr int k_MasterVolume = 127;

/// An LH_SampleInfo (0x90 bytes, LH_AudioSystem+0x50 .. +0x54)
struct Channel
{
	entt::entity emitter {entt::null};
	uint32_t bank {0};     ///< +0x04
	Owner owner {};        ///< +0x18
	int sample {0};        ///< +0x1C
	int group {0};         ///< +0x28
	int pitch {100};       ///< +0x34
	int volume {127};      ///< +0x38
	bool atmos {false};    ///< +0x00 AtmosInfo
	bool is3D {false};     ///< +0x48
	bool track {false};    ///< +0x4C
	glm::vec3 offset {};   ///< +0x5C
	float maxDistance {0}; ///< +0x6C
	int priority {0};      ///< +0x70
	entt::id_type sound {0};
};

struct State
{
	std::array<Channel, k_Channels> channels;
	std::unordered_map<entt::id_type, uint32_t> banks;
	size_t bankGroups {0};
	bool gameSoundOff {false};
	bool scriptWideScreen {false};
};
State g_State;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}

bool Available()
{
	return Locator::audio::has_value() && Locator::entitiesRegistry::has_value() && Locator::resources::has_value();
}

/// The bank a sound belongs to: openblack's sound group ("InGame.sad"), as a hash
uint32_t BankOf(entt::id_type sound)
{
	auto& audio = Locator::audio::value();
	const auto& groups = audio.GetSoundGroups();
	if (g_State.bankGroups != groups.size())
	{
		g_State.banks.clear();
		for (const auto& [name, group] : groups)
		{
			const auto hash = static_cast<uint32_t>(std::hash<std::string> {}(name));
			for (const auto id : group.sounds)
			{
				g_State.banks[id] = hash;
			}
		}
		g_State.bankGroups = groups.size();
	}
	const auto found = g_State.banks.find(sound);
	return found != g_State.banks.end() ? found->second : 0;
}

bool IsBank(entt::id_type sound, const char* name)
{
	return BankOf(sound) == static_cast<uint32_t>(std::hash<std::string> {}(name));
}

/// +0x8C: the channel is in use (a sample is on it and has not finished)
bool InUse(const Channel& channel)
{
	return channel.emitter != entt::null && Locator::audio::value().EmitterExists(channel.emitter);
}

void Free(Channel& channel, const char* why)
{
	if (InUse(channel))
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: channel {} stopped ({})", &channel - g_State.channels.data(),
			                   why);
		}
		auto& audio = Locator::audio::value();
		audio.StopEmitter(channel.emitter);
		audio.DestroyEmitter(channel.emitter);
	}
	channel.emitter = entt::null;
}

const char* OwnerName(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::None:
		return "none";
	case Owner::Kind::Atmos:
		return "atmos";
	case Owner::Kind::SoundTag:
		return "sound tag";
	default:
		return "thing";
	}
}

/// The channel allocation of LHSamplePlay (0x10011020). `restart`: the channel was in use and is restarted (the flag
/// 0x10011350 passes to the start); `leave`: mode 2 found its sample playing, nothing is started
Channel* Allocate(const Options& options, const Sound& sound, uint32_t bank, int mode, bool& restart, bool& leave)
{
	restart = false;
	leave = false;
	auto& channels = g_State.channels;
	const int group = sound.cloneGroup;
	const auto firstFree = [&channels]() -> Channel* {
		for (auto& channel : channels)
		{
			if (!InUse(channel))
			{
				return &channel;
			}
		}
		return nullptr;
	};
	Channel* found = nullptr;
	switch (mode)
	{
	case 1:
		found = firstFree();
		break;
	case 2:
		for (auto& channel : channels)
		{
			if (channel.bank == bank && channel.owner == options.owner &&
			    (channel.sample == sound.id || (channel.group == group && group > 0)) && InUse(channel))
			{
				leave = true;
				return &channel;
			}
		}
		found = firstFree();
		break;
	case 3:
		// the same sample of the same bank and owner, playing or not
		for (auto& channel : channels)
		{
			if (channel.bank == bank && channel.owner == options.owner && channel.sample == sound.id)
			{
				found = &channel;
				restart = true;
				break;
			}
		}
		// the options' name is "NONE" (the default): the clone group
		if (found == nullptr && group > 0)
		{
			for (auto& channel : channels)
			{
				if (channel.bank == bank && channel.owner == options.owner && channel.group == group)
				{
					found = &channel;
					restart = true;
					break;
				}
			}
		}
		if (found == nullptr)
		{
			found = firstFree();
			restart = false;
		}
		break;
	default:
		break;
	}
	if (found != nullptr)
	{
		restart = restart && InUse(*found);
		return found;
	}
	// 0x100112CA: no free channel: the lowest priority (a channel at 0 at once), if lower than the sample's
	Channel* lowest = nullptr;
	uint32_t lowestPriority = 0xFFFFFFFFu;
	for (auto& channel : channels)
	{
		const auto priority = InUse(channel) ? static_cast<uint32_t>(channel.priority) : 0u;
		if (priority == 0)
		{
			lowest = &channel;
			lowestPriority = 0;
			break;
		}
		if (priority < lowestPriority)
		{
			lowest = &channel;
			lowestPriority = priority;
		}
	}
	if (lowest != nullptr && lowestPriority < static_cast<uint32_t>(sound.priority))
	{
		restart = InUse(*lowest);
		return lowest;
	}
	return nullptr;
}

/// The camera distance the anim effect and PlaySoundEffect test (GCamera::GetDistanceSq; the render camera inside the
/// citadel)
float CameraDistance(glm::vec3 point)
{
	return Locator::camera::has_value() ? glm::distance(point, Locator::camera::value().GetOrigin()) : 0.0f;
}

/// GAudio's filters shared by PlaySoundEffect (0x429F36..0x429FD9) and SamplePlayAnimEffect (0x42A51B..0x42A5B8)
bool Filtered(const Options& options, const Sound& sound)
{
	const int kind = sound.userParam;
	if (g_State.scriptWideScreen && kind == 1)
	{
		return true;
	}
	if (IsInsideCitadel() && kind != 2)
	{
		return true;
	}
	// GScript+0x90 (SET_GAME_SOUND false): only GAudio+0x3C0 / +0x3C4 (dialogue banks 6 and 7, 0x9CB3F8)
	if (g_State.gameSoundOff && !IsBank(options.sound, "HelpSprites.sad") && !IsBank(options.sound, "Villagers.sad"))
	{
		return true;
	}
	// interface states 0x10 / 0x16 / 0x17 with kind 4: no such interface state in openblack; IsInsideCitadel(owner):
	// never inside
	return false;
}
} // namespace

float sample_play::QMixerGain(int volume)
{
	const int v = std::clamp(volume, 0, 127);
	// floor(master * v / 127) * 258 (0x100133C1..0x100133E3), / 32767 (0x18007AE8)
	return static_cast<float>(k_MasterVolume * v / 127 * 258) / 32767.0f;
}

float sample_play::DistanceGain(float minDistance, float maxDistance, float scale, float distance)
{
	if (distance > maxDistance)
	{
		return 0.0f;
	}
	if (distance <= minDistance || scale == 0.0f)
	{
		return 1.0f;
	}
	if (scale == 1.0f)
	{
		return minDistance / distance;
	}
	return minDistance / ((distance - minDistance) * scale + minDistance);
}

glm::vec3 sample_play::PolarRelative(glm::vec3 position)
{
	// 0x100122BC..0x10012522 in doubles, the angles in degrees with LHaudio's 180 * 0.318471 (not 180 / pi)
	constexpr double k_Degrees = 180.0 * 0.318471;
	const double x = position.x;
	const double y = position.y;
	const double z = position.z;
	const double range = std::sqrt(x * x + y * y + z * z);
	double elevation = 0.0;
	if (z != 0.0)
	{
		if (y == 0.0 && x == 0.0)
		{
			elevation = z < 0.0 ? -90.0 : 90.0;
		}
		else
		{
			elevation = std::atan(z / std::sqrt(x * x + y * y)) * k_Degrees;
		}
	}
	const auto ftolAbs = [](double v) { return static_cast<double>(std::abs(static_cast<int32_t>(v))); };
	double azimuth = 0.0;
	if (x == 0.0)
	{
		azimuth = y < 0.0 ? 180.0 : 0.0;
	}
	else if (y == 0.0)
	{
		azimuth = x > 0.0 ? 90.0 : 270.0;
	}
	else if (x > 0.0)
	{
		azimuth = y > 0.0 ? 90.0 - std::atan(y / x) * k_Degrees : 90.0 + std::atan(ftolAbs(y) / x) * k_Degrees;
	}
	else
	{
		azimuth = y < 0.0 ? 270.0 - std::atan(ftolAbs(y) / ftolAbs(x)) * k_Degrees
		                  : 270.0 + std::atan(y / ftolAbs(x)) * k_Degrees;
	}
	// QMixer 0x1800AA85: the angles * pi / 180 (the exact pi)
	constexpr double k_Radians = 3.14159265358979323846 / 180.0;
	const double az = azimuth * k_Radians;
	const double el = elevation * k_Radians;
	const double flat = std::cos(el) * range;
	return {static_cast<float>(std::sin(az) * flat), static_cast<float>(std::sin(el) * range),
	        static_cast<float>(std::cos(az) * flat)};
}

entt::entity sample_play::Play(const Options& options)
{
	if (!Available())
	{
		return entt::null;
	}
	auto& audio = Locator::audio::value();
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(options.sound))
	{
		return entt::null;
	}
	const auto& sound = audio.GetSound(options.sound);
	const uint32_t bank = BankOf(options.sound);
	const uint32_t flags = sound.overrides;
	// the .sad's fields where its flag is set and the caller did not set them (0x100119D4..0x10011B84)
	const auto fromSad = [flags, &options](uint32_t bit) { return (flags & bit) != 0 && (options.callerMask & bit) == 0; };
	const int mode = fromSad(0x400) ? sound.playMode : options.mode;
	bool restart = false;
	bool leave = false;
	Channel* channel = Allocate(options, sound, bank, mode, restart, leave);
	if (channel == nullptr)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} (mode {}, owner {}): no channel", sound.name, mode,
			                   OwnerName(options.owner));
		}
		return entt::null;
	}
	if (leave)
	{
		// mode 2: the options' pointer is cleared, LHSamplePlay returns the playing channel untouched
		return channel->emitter;
	}
	if (restart)
	{
		Free(*channel, "restarted");
	}
	channel->emitter = entt::null;

	const int volume = std::clamp(fromSad(0x20) ? static_cast<int>(sound.volume127) : options.volume, 0, 127);
	const int loops = fromSad(0x40) ? sound.loops : options.loops;
	const bool is3D = options.is3D;
	// a relative position is heard in the listener's frame: AL's (right, up, -ahead), which AudioPlayer receives as
	// (z, y, x) of the emitter's position
	glm::vec3 position = options.position;
	if (is3D && options.relative)
	{
		const auto p = PolarRelative(options.position);
		position = glm::vec3(-p.z, p.y, p.x);
	}
	const bool relative = !is3D || options.relative;
	if (!is3D)
	{
		position = glm::vec3(0.0f);
	}
	const auto emitter =
	    audio.CreateEmitter(options.sound, loops != 0 ? PlayType::Repeat : PlayType::Once, position, glm::vec3(0.0f),
	                        glm::vec2(0.0f), QMixerGain(volume), AudioStatus::Playing, relative);
	if (emitter == entt::null)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<Transform>(emitter).position = position;
	if (!is3D)
	{
		// a 2D channel has no distance mapping and never goes quiet with the distance
		registry.Get<AudioEmitter>(emitter).cutDistance = 0.0f;
	}
	// the pitch: the .sad's (flag 0x1) or 100, with the start's deviation (AudioManager::CreateEmitter, 0x1001278B)
	audio.PlayEmitter(emitter);

	channel->emitter = emitter;
	channel->bank = bank;
	channel->owner = options.owner;
	channel->sample = sound.id;
	channel->sound = options.sound;
	channel->volume = volume;
	channel->pitch = sound.pitch;
	channel->atmos = options.atmos;
	channel->is3D = is3D;
	channel->track = options.track;
	channel->offset = glm::vec3(0.0f);
	channel->maxDistance = sound.mappingMaxDistance;
	// 0x10012AAA: the priority and clone group of the sample (the options' name is "NONE")
	channel->priority = sound.priority;
	channel->group = sound.cloneGroup;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Sample play: {} mode {} owner {} channel {}{}, volume {} -> gain {:.4f}, {} at ({:.2f}, {:.2f}, "
		                   "{:.2f}){}",
		                   sound.name, mode, OwnerName(options.owner), channel - g_State.channels.data(),
		                   restart ? " (restarted)" : "", volume, QMixerGain(volume), is3D ? "3D" : "2D", options.position.x,
		                   options.position.y, options.position.z, options.relative ? " relative" : "");
	}
	return emitter;
}

entt::entity sample_play::PlaySoundEffect(const Options& options)
{
	if (!Available() || !Locator::resources::value().GetSounds().Contains(options.sound))
	{
		return entt::null;
	}
	const auto& sound = Locator::audio::value().GetSound(options.sound);
	if (options.is3D)
	{
		// GetGSFXSampleMaxDistance 0x42A430 (.sad +0x26C raw), the options' +0x58 (9999) when 0
		const float maxDistance = sound.maxDistance != 0.0f ? sound.maxDistance : 9999.0f;
		const float distance = CameraDistance(options.position);
		if (distance * distance > maxDistance * maxDistance)
		{
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} culled, camera {:.1f} > max {:.1f}", sound.name,
				                   distance, maxDistance);
			}
			return entt::null;
		}
	}
	if (Filtered(options, sound))
	{
		return entt::null;
	}
	return Play(options);
}

entt::entity sample_play::PlayAnimEffect(entt::id_type sound, entt::entity thing, glm::vec3 position, bool track)
{
	if (!Available() || !Locator::resources::value().GetSounds().Contains(sound))
	{
		return entt::null;
	}
	Options options;
	options.sound = sound;
	options.is3D = true;
	options.track = track;
	options.owner = Owner::Of(thing);
	options.position = position;
	const auto& info = Locator::audio::value().GetSound(sound);
	if (Filtered(options, info))
	{
		return entt::null;
	}
	// 0x10014A83: the camera distance against the global max (800) and the .sad's raw +0x26C
	const float distance = CameraDistance(position);
	if (distance > k_GlobalMaxDistance || distance > info.maxDistance)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: anim effect {} culled, camera {:.1f} > max {:.1f}", info.name,
			                   distance, std::min(k_GlobalMaxDistance, info.maxDistance));
		}
		return entt::null;
	}
	return Play(options);
}

void sample_play::SetPitch(entt::id_type sound, Owner owner, int percent)
{
	if (!Available())
	{
		return;
	}
	const uint32_t bank = BankOf(sound);
	const auto& info = Locator::audio::value().GetSound(sound);
	for (auto& channel : g_State.channels)
	{
		if (channel.bank == bank && channel.owner == owner && channel.sample == info.id && InUse(channel))
		{
			if (channel.pitch != percent)
			{
				Locator::audio::value().SetEmitterPitch(channel.emitter, static_cast<float>(percent));
				channel.pitch = percent;
			}
			return;
		}
	}
}

void sample_play::SetVolume(entt::entity emitter, int volume)
{
	if (!Available())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(emitter) || !registry.AnyOf<AudioEmitter>(emitter))
	{
		return;
	}
	const int v = std::clamp(volume, 0, 127);
	for (auto& channel : g_State.channels)
	{
		if (channel.emitter == emitter)
		{
			// +0x38: nothing when it already has that volume
			if (channel.volume == v)
			{
				return;
			}
			channel.volume = v;
		}
	}
	registry.Get<AudioEmitter>(emitter).volume = QMixerGain(v);
}

void sample_play::Stop(entt::id_type sound, Owner owner)
{
	if (!Available())
	{
		return;
	}
	const uint32_t bank = BankOf(sound);
	const auto& info = Locator::audio::value().GetSound(sound);
	for (auto& channel : g_State.channels)
	{
		if (channel.bank == bank && channel.owner == owner && channel.sample == info.id)
		{
			Free(channel, "LHSampleStop");
		}
	}
}

bool sample_play::IsPlaying(entt::id_type sound, Owner owner)
{
	if (!Available())
	{
		return false;
	}
	const uint32_t bank = BankOf(sound);
	const auto& info = Locator::audio::value().GetSound(sound);
	for (const auto& channel : g_State.channels)
	{
		if (channel.bank == bank && channel.owner == owner && channel.sample == info.id && InUse(channel))
		{
			return true;
		}
	}
	return false;
}

void sample_play::ReleaseLoop(entt::id_type sound, Owner owner)
{
	if (!Available())
	{
		return;
	}
	const uint32_t bank = BankOf(sound);
	const auto& info = Locator::audio::value().GetSound(sound);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& channel : g_State.channels)
	{
		if (channel.bank == bank && channel.owner == owner && channel.sample == info.id && InUse(channel))
		{
			registry.Get<AudioEmitter>(channel.emitter).loop = PlayType::Once;
		}
	}
}

void sample_play::ProcessTurn()
{
	if (!Available() || !Locator::camera::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto camera = Locator::camera::value().GetOrigin();
	// LHSampleUpdate3DChannels 0x10014310 with the game's function 0x427200
	for (auto& channel : g_State.channels)
	{
		if (channel.atmos || !channel.is3D || !channel.track || !InUse(channel))
		{
			continue;
		}
		glm::vec3 at = camera;
		if (channel.owner.kind == Owner::Kind::SoundTag)
		{
			// 0x427200: a SoundTag is no GameThing, so its own Get3DSoundPos (vt +0x10) would be asked; the tags only
			// track with a thing (SoundTag::Set 0x71E55D) and the ported ones pass false, so this does not happen
			continue;
		}
		if (channel.owner.kind == Owner::Kind::Atmos)
		{
			Free(channel, "tracked, atmos owner");
			continue;
		}
		if (channel.owner.kind == Owner::Kind::Thing)
		{
			const auto* transform = registry.Valid(channel.owner.thing) ? registry.TryGet<const Transform>(channel.owner.thing)
			                                                             : nullptr;
			if (transform == nullptr)
			{
				// not available: LHSampleStop
				Free(channel, "tracked, owner gone");
				continue;
			}
			at = transform->position;
		}
		at += channel.offset;
		if (glm::distance(at, camera) > channel.maxDistance)
		{
			Free(channel, "tracked, beyond its max distance");
			continue;
		}
		registry.Get<Transform>(channel.emitter).position = at;
	}
	// LHListenerUpdate
	Locator::audio::value().UpdateListener();
}

void sample_play::SetGameSound(bool enabled)
{
	if (!enabled)
	{
		if (Available())
		{
			for (auto& channel : g_State.channels)
			{
				Free(channel, "SET_GAME_SOUND false");
			}
		}
		g_State.gameSoundOff = true;
		return;
	}
	g_State.gameSoundOff = false;
}

void sample_play::SetScriptWideScreen(bool on)
{
	g_State.scriptWideScreen = on;
}

bool sample_play::IsInsideCitadel()
{
	return false;
}

bool sample_play::IsVideoPlaying()
{
	return false;
}

void sample_play::Clear()
{
	if (Available())
	{
		for (auto& channel : g_State.channels)
		{
			Free(channel, "new map");
		}
	}
	g_State.channels = {};
}
