/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SamplePlay.h"

#include <cstdlib>

#include <algorithm>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "AudioManagerInterface.h"
#include "AudioSystem.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "Locator.h"
#include "QMixerLaws.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "SampleOutput.h"
#include "Sound.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::sample_play;

namespace
{
/// An LH_SampleInfo (0x90 bytes, LH_AudioSystem+0x50 .. +0x54)
struct ChannelState
{
	Channel handle {k_NoChannel}; ///< (openblack) the start the channel is on; 0 = never started
	entt::id_type sound {0};
	uint32_t bank {0};     ///< +0x04
	Owner owner {};        ///< +0x18
	int sample {0};        ///< +0x1C
	int group {0};         ///< +0x28
	int pitch {100};       ///< +0x34
	int volume {127};      ///< +0x38
	bool atmos {false};    ///< +0x00 AtmosInfo
	bool is3D {false};     ///< +0x48
	bool track {false};    ///< +0x4C
	int loops {0};         ///< +0x40: the loops it started with, 0 after LHSampleReleaseLoop (inferred: the DLL's pass count is not read)
	glm::vec3 position {}; ///< +0x50: the point the owner gave (without the offset)
	glm::vec3 offset {};   ///< +0x5C
	float maxDistance {0}; ///< +0x6C
	int priority {0};      ///< +0x70
	bool ownerGone {false};
};

struct State
{
	std::array<ChannelState, k_Channels> channels;
	uint32_t serial {0};
	/// LH_AudioSystem+0x3C, 127 by default (ctor 0x10015290)
	int master {qmixer::k_MaxVolume};
	/// LH_AudioSystem+0x14 (Create 0x100153F0 sets 1)
	bool active {true};
	Backend backend;
};
State g_State;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}

SampleOutput* Output()
{
	if (g_State.backend.output != nullptr)
	{
		return g_State.backend.output;
	}
	return Locator::audio::has_value() ? &Locator::audio::value().GetSampleOutput() : nullptr;
}

Sound* Lookup(entt::id_type id)
{
	if (g_State.backend.sound)
	{
		return g_State.backend.sound(id);
	}
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id))
	{
		return nullptr;
	}
	auto handle = sounds.Handle(id);
	return handle ? &*handle : nullptr;
}

int Rand()
{
	if (g_State.backend.rand)
	{
		return g_State.backend.rand();
	}
	return Locator::rng::has_value() ? Locator::rng::value().NextValue<int32_t>(0, 32767) : 0;
}

std::optional<glm::vec3> CameraPoint()
{
	if (g_State.backend.camera)
	{
		return g_State.backend.camera();
	}
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	return Locator::camera::value().GetOrigin();
}

size_t IndexOf(const ChannelState& channel)
{
	return static_cast<size_t>(&channel - g_State.channels.data());
}

/// +0x8C: the channel is in use (a sample is on it and has not finished)
bool InUse(const ChannelState& channel)
{
	const auto* output = Output();
	return channel.handle != k_NoChannel && output != nullptr && output->Playing(IndexOf(channel));
}

ChannelState* Find(Channel handle)
{
	if (handle == k_NoChannel)
	{
		return nullptr;
	}
	auto& channel = g_State.channels[(handle - 1) % k_Channels];
	return channel.handle == handle ? &channel : nullptr;
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
	case Owner::Kind::Key:
		return "key";
	case Owner::Kind::Object:
		return "object";
	default:
		return "thing";
	}
}

void Halt(ChannelState& channel, const char* why)
{
	if (InUse(channel))
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: channel {} stopped ({})", IndexOf(channel), why);
		}
		Output()->Stop(IndexOf(channel));
	}
}

/// The first channel of (bank, owner, sample), in use or not (LHSampleGetInfo 0x10013F60 and the loops of LHSampleStop,
/// LHSampleReleaseLoop, LHSampleSetPitch)
ChannelState* First(uint32_t bank, const Owner& owner, int sample)
{
	for (auto& channel : g_State.channels)
	{
		if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == owner && channel.sample == sample)
		{
			return &channel;
		}
	}
	return nullptr;
}

/// The channel allocation of LHSamplePlay (0x10011020). `restart`: the channel was in use and is restarted (the flag
/// 0x10011350 passes to the start); `leave`: mode 2 found its sample playing, nothing is started
ChannelState* Allocate(const Options& options, const Sound& sound, uint32_t bank, int mode, bool& restart, bool& leave)
{
	restart = false;
	leave = false;
	auto& channels = g_State.channels;
	const int group = sound.cloneGroup;
	const auto firstFree = [&channels]() -> ChannelState* {
		for (auto& channel : channels)
		{
			if (!InUse(channel))
			{
				return &channel;
			}
		}
		return nullptr;
	};
	ChannelState* found = nullptr;
	switch (mode)
	{
	case 1:
		found = firstFree();
		break;
	case 2:
		for (auto& channel : channels)
		{
			if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == options.owner &&
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
			if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == options.owner &&
			    channel.sample == sound.id)
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
				if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == options.owner &&
				    channel.group == group)
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
	ChannelState* lowest = nullptr;
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

Channel NextHandle(size_t index)
{
	// serial * 16 + index + 1, never 0 and never entt::null as an entity
	++g_State.serial;
	if (g_State.serial >= 0x0FFFFFFFu)
	{
		g_State.serial = 1;
	}
	return g_State.serial * static_cast<Channel>(k_Channels) + static_cast<Channel>(index) + 1;
}
} // namespace

void sample_play::SetBackend(Backend backend)
{
	g_State.backend = std::move(backend);
}

Sound* sample_play::GetSound(entt::id_type sound)
{
	return Lookup(sound);
}

std::array<ChannelInfo, k_Channels> sample_play::Channels()
{
	std::array<ChannelInfo, k_Channels> infos {};
	for (size_t i = 0; i < k_Channels; ++i)
	{
		const auto& channel = g_State.channels[i];
		auto& info = infos[i];
		info.handle = channel.handle;
		info.sound = channel.sound;
		info.bank = channel.bank;
		info.owner = channel.owner;
		info.sample = channel.sample;
		info.group = channel.group;
		info.priority = channel.priority;
		info.volume = channel.volume;
		info.pitch = channel.pitch;
		info.is3D = channel.is3D;
		info.track = channel.track;
		info.atmos = channel.atmos;
		info.playing = InUse(channel);
	}
	return infos;
}

Channel sample_play::Start(const Options& options)
{
	auto* output = Output();
	auto* sound = Lookup(options.sound);
	if (output == nullptr || sound == nullptr)
	{
		return k_NoChannel;
	}
	const uint32_t bank = sound->bank;
	const uint32_t flags = sound->overrides;
	// the .sad's fields where its flag is set and the caller did not set them (0x100119D4..0x10011B84)
	const auto fromSad = [flags, &options](uint32_t bit) { return (flags & bit) != 0 && (options.callerMask & bit) == 0; };
	const int mode = fromSad(0x400) ? sound->playMode : options.mode;
	bool restart = false;
	bool leave = false;
	ChannelState* channel = Allocate(options, *sound, bank, mode, restart, leave);
	if (channel == nullptr)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} (mode {}, owner {}): no channel", sound->name, mode,
			                   OwnerName(options.owner));
		}
		return k_NoChannel;
	}
	if (leave)
	{
		// mode 2: the options' pointer is cleared, LHSamplePlay returns the playing channel untouched
		return channel->handle;
	}
	const size_t index = IndexOf(*channel);
	if (restart)
	{
		Halt(*channel, "restarted");
	}

	const int volume = std::clamp(fromSad(0x20) ? sound->volume127 : options.volume, 0, qmixer::k_MaxVolume);
	const int loops = fromSad(0x40) ? sound->loops : options.loops;
	// the pitch (0x1001278B..0x1001283B): the .sad's with flag 0x1, else the options' +0x48, with the deviation
	const int pitch = qmixer::StartPitch(fromSad(0x1) ? sound->pitch : options.pitch, sound->pitchDeviation, Rand());
	// QSWaveMixSetDistanceMapping {min, max, scale} (0x10012159): the .sad's with flags 0x80 / 0x100 / 0x200
	SampleOutput::Start start;
	start.minDistance = fromSad(0x80) ? sound->minDistance : options.minDistance;
	start.maxDistance = fromSad(0x100) ? sound->mappingMaxDistance : options.maxDistance;
	start.scale = fromSad(0x200) ? sound->scale : options.scale;
	start.gain = qmixer::Gain(volume, g_State.master);
	start.pitch = qmixer::FrequencyRatio(sound->sampleRate, pitch);
	start.is3D = options.is3D;
	start.loops = loops;
	// the source position is pos + offset (0x10012159); a relative one is heard in the listener's frame: AL's (right,
	// up, -ahead), which the output receives as (z, y, x)
	glm::vec3 position = options.position + options.offset;
	if (options.is3D && options.relative)
	{
		const auto p = qmixer::PolarRelative(position);
		position = glm::vec3(-p.z, p.y, p.x);
	}
	start.relative = options.relative;
	start.position = options.is3D ? position : glm::vec3(0.0f);
	if (!output->Play(index, *sound, start))
	{
		channel->handle = k_NoChannel;
		return k_NoChannel;
	}

	channel->handle = NextHandle(index);
	channel->sound = options.sound;
	channel->bank = bank;
	channel->owner = options.owner;
	channel->sample = sound->id;
	channel->volume = volume;
	channel->pitch = pitch;
	channel->atmos = options.atmos;
	channel->is3D = options.is3D;
	channel->track = options.track;
	channel->loops = loops;
	channel->position = options.position;
	channel->offset = options.offset;
	channel->maxDistance = start.maxDistance;
	channel->ownerGone = false;
	// 0x10012AAA: the priority and clone group of the sample (the options' name is "NONE")
	channel->priority = sound->priority;
	channel->group = sound->cloneGroup;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Sample play: {} mode {} owner {} channel {}{}, volume {} -> gain {:.4f} (master {}), pitch {}, "
		                   "loops {}, {} at ({:.2f}, {:.2f}, {:.2f}){}",
		                   sound->name, mode, OwnerName(options.owner), index, restart ? " (restarted)" : "", volume,
		                   start.gain, g_State.master, pitch, loops, options.is3D ? "3D" : "2D", options.position.x,
		                   options.position.y, options.position.z, options.relative ? " relative" : "");
	}
	return channel->handle;
}

void sample_play::Stop(entt::id_type sound, Owner owner)
{
	const auto* info = Lookup(sound);
	if (info == nullptr)
	{
		return;
	}
	auto* channel = First(info->bank, owner, info->id);
	// 0x10012D5B: nothing while switched off, unless an atmos channel
	if (channel == nullptr || (!g_State.active && !channel->atmos))
	{
		return;
	}
	Halt(*channel, "LHSampleStop");
}

void sample_play::Stop(Channel handle)
{
	const auto* started = Find(handle);
	if (started == nullptr || (!g_State.active && !started->atmos))
	{
		return;
	}
	if (auto* channel = First(started->bank, started->owner, started->sample); channel != nullptr)
	{
		Halt(*channel, "LHSampleStop(info)");
	}
}

void sample_play::StopOwner(uint32_t bank, Owner owner)
{
	for (auto& channel : g_State.channels)
	{
		if (channel.handle == k_NoChannel || channel.bank != bank || !(channel.owner == owner))
		{
			continue;
		}
		// 0x10012CA8..0x10012CB2: the first one met while switched off ends the whole stop
		if (!g_State.active && !channel.atmos)
		{
			return;
		}
		Halt(channel, "LHSampleStop, any sample");
	}
}

void sample_play::StopAll()
{
	for (auto& channel : g_State.channels)
	{
		if (!channel.atmos)
		{
			Halt(channel, "LHSampleStopAll");
		}
	}
}

bool sample_play::IsPlaying(entt::id_type sound, Owner owner)
{
	const auto* info = Lookup(sound);
	if (info == nullptr || !g_State.active)
	{
		return false;
	}
	const auto* channel = First(info->bank, owner, info->id);
	return channel != nullptr && InUse(*channel);
}

bool sample_play::IsOwnerPlaying(uint32_t bank, Owner owner)
{
	if (!g_State.active)
	{
		return false;
	}
	for (const auto& channel : g_State.channels)
	{
		if (channel.handle != k_NoChannel && channel.bank == bank && channel.owner == owner)
		{
			return InUse(channel);
		}
	}
	return false;
}

bool sample_play::IsPlaying(Channel handle)
{
	// LHSampleIsPlaying(LH_SampleInfo*) 0x10014070 answers 0 while switched off (0x1001407A) and looks at the first
	// channel of the info's (bank, owner, sample) (0x100140E6..0x10014100). (approximated) Here: the handle's own start,
	// also while switched off: openblack's game runs on while minimised (the original stops), and a caller asking every
	// turn (PSysSound 0x6D120A) would start its sample again on another channel each turn.
	const auto* channel = Find(handle);
	return channel != nullptr && InUse(*channel);
}

void sample_play::ReleaseLoop(entt::id_type sound, Owner owner)
{
	const auto* info = Lookup(sound);
	if (info == nullptr || !g_State.active)
	{
		return;
	}
	auto* channel = First(info->bank, owner, info->id);
	if (channel == nullptr || !InUse(*channel))
	{
		return;
	}
	Output()->ReleaseLoop(IndexOf(*channel));
	channel->loops = 0;
}

int sample_play::Loops(entt::id_type sound, Owner owner)
{
	// LHSampleGetInfo 0x10013F60 (the first channel of the three, in use or not) +0x40
	const auto* info = Lookup(sound);
	if (info == nullptr)
	{
		return 0;
	}
	const auto* channel = First(info->bank, owner, info->id);
	return channel != nullptr ? channel->loops : 0;
}

entt::id_type sample_play::SoundOf(Channel handle)
{
	const auto* channel = Find(handle);
	return channel != nullptr ? channel->sound : 0;
}

int sample_play::Random(int count)
{
	// LH_AudioSystem::Rand(n) 0x10015710: Rand() 0x10015740 (0..32767) * n / 32767 (the magic 0x20005, 0x10015717..
	// 0x1001572B). A Rand() of 32767 gives n itself, one past a list (the DLL then reads the next word): (approximated)
	// kept inside the list here.
	if (count <= 0)
	{
		return 0;
	}
	const int value = static_cast<int>((static_cast<int64_t>(Rand()) * count) / 32767);
	return std::min(value, count - 1);
}

void sample_play::SetPitch(entt::id_type sound, Owner owner, int percent)
{
	const auto* info = Lookup(sound);
	if (info == nullptr || percent <= 0)
	{
		return;
	}
	auto* channel = First(info->bank, owner, info->id);
	// 0x10013572..0x1001357C: nothing while switched off, unless an atmos channel; 0x1001357E: not in use; 0x10013588:
	// the same pitch
	if (channel == nullptr || (!g_State.active && !channel->atmos) || !InUse(*channel) || channel->pitch == percent)
	{
		return;
	}
	Output()->SetPitch(IndexOf(*channel), qmixer::FrequencyRatio(info->sampleRate, percent));
	channel->pitch = percent;
}

void sample_play::SetVolume(Channel handle, int volume)
{
	const auto* started = Find(handle);
	// 0x10013412..0x10013420: nothing while switched off, unless the channel is an atmos one (+0x00)
	if (started == nullptr || (!g_State.active && !started->atmos))
	{
		return;
	}
	// 0x10013489..0x100134A9: the first channel of its (bank, owner, sample), then 0x100134BE: in use
	auto* channel = First(started->bank, started->owner, started->sample);
	if (channel == nullptr || !InUse(*channel))
	{
		return;
	}
	// 0x10013473..0x10013487: clamped to 0..127
	const int v = std::clamp(volume, 0, qmixer::k_MaxVolume);
	// +0x38: nothing when it already has that volume
	if (channel->volume == v)
	{
		return;
	}
	channel->volume = v;
	Output()->SetGain(IndexOf(*channel), qmixer::Gain(v, g_State.master));
}

void sample_play::SetMasterVolume(int master)
{
	// 0x100150F2..0x10015100: the same value or more than 127 is ignored
	if (master == g_State.master || master < 0 || master > qmixer::k_MaxVolume)
	{
		return;
	}
	g_State.master = master;
	for (auto& channel : g_State.channels)
	{
		if (InUse(channel))
		{
			Output()->SetGain(IndexOf(channel), qmixer::Gain(channel.volume, master));
		}
	}
}

int sample_play::MasterVolume()
{
	return g_State.master;
}

void sample_play::UpdateChannels()
{
	auto* output = Output();
	const auto camera = CameraPoint();
	if (output == nullptr || !camera)
	{
		return;
	}
	// LHSampleUpdate3DChannels 0x10014310 with the game's function 0x427200
	for (auto& channel : g_State.channels)
	{
		if (channel.atmos || !channel.is3D || !channel.track || !InUse(channel))
		{
			continue;
		}
		glm::vec3 at = *camera;
		switch (channel.owner.kind)
		{
		case Owner::Kind::None:
			break;
		case Owner::Kind::Atmos:
			// 0x42726D: the atmos owner (-1) gives no position: LHSampleStop
			Halt(channel, "tracked, atmos owner");
			channel.owner = {};
			continue;
		case Owner::Kind::SoundTag:
		{
			// a SoundTag is a Base, not a GameThing (fn_00427200 0x4272D5): SoundTag::Get3DSoundPos 0x71EC90 is its
			// thing's, and with no thing (a point tag, or one whose thing has died) it answers 1 without writing, so the
			// channel keeps its point (+0x50, read first at 0x427209); it never stops the channel
			const auto position = g_State.backend.ownerPosition ? g_State.backend.ownerPosition(channel.owner)
			                                                    : std::optional<glm::vec3> {};
			at = position ? *position : channel.position;
			break;
		}
		case Owner::Kind::Key:
			// a key is never tracked (the voices pass track 0, 0x70F931): not moved (inferred)
			continue;
		case Owner::Kind::Thing:
		case Owner::Kind::Object:
		{
			if (!g_State.backend.ownerPosition)
			{
				continue;
			}
			const auto position = channel.ownerGone ? std::nullopt : g_State.backend.ownerPosition(channel.owner);
			if (!position)
			{
				// 0x4272AD: GameThing::IsAvailable() == 0 -> 0 -> LHSampleStop (0x1001439D) and the owner +0x18 = 0
				// (0x100143A2)
				Halt(channel, "tracked, owner gone");
				channel.owner = {};
				continue;
			}
			at = *position;
			break;
		}
		}
		channel.position = at;
		at += channel.offset;
		// 0x100143BC: the camera at or beyond the channel's max distance stops it
		if (glm::distance(at, *camera) >= channel.maxDistance)
		{
			Halt(channel, "tracked, beyond its max distance");
			continue;
		}
		output->SetPosition(IndexOf(channel), at);
	}
	// LHListenerUpdate (fn_004270D0 0x4271EF): QMixer's listener
	if (Locator::audio::has_value())
	{
		Locator::audio::value().UpdateListener();
	}
	output->SetListener(*camera);
}

void sample_play::Switch(bool on)
{
	if (!on)
	{
		StopAll();
		g_State.active = false;
		return;
	}
	g_State.active = true;
}

bool sample_play::IsActive()
{
	return g_State.active;
}

void sample_play::ClearInfoList()
{
	if (!g_State.active)
	{
		return;
	}
	for (auto& channel : g_State.channels)
	{
		// 0x100142EB..0x100142F0: +0x1C, +0x18 and +0x04
		channel.sample = 0;
		channel.owner = {};
		channel.bank = 0;
	}
}

void sample_play::ReleaseSources()
{
	if (auto* output = Output(); output != nullptr)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} channel sources released", output->Sources());
		}
		output->DeleteAll();
	}
}

void sample_play::OnThingDeleted(entt::entity thing)
{
	for (auto& channel : g_State.channels)
	{
		if (channel.owner.kind == Owner::Kind::Thing && channel.owner.thing == thing)
		{
			channel.ownerGone = true;
		}
	}
}

void sample_play::UpdateFrame()
{
	if (auto* output = Output(); output != nullptr)
	{
		output->Update();
	}
}

// ---- agua's names ------------------------------------------------------------------------------------------------

entt::entity sample_play::Play(const Options& options)
{
	return AsEntity(Start(options));
}

void sample_play::SetVolume(entt::entity emitter, int volume)
{
	SetVolume(AsChannel(emitter), volume);
}

void sample_play::ProcessTurn()
{
	UpdateChannels();
}

void sample_play::SetGameSound(bool enabled)
{
	audio::SetGameSound(enabled);
}

void sample_play::SetScriptWideScreen(bool on)
{
	audio::SetScriptWideScreen(on);
}

bool sample_play::IsInsideCitadel()
{
	return audio::IsInsideCitadel();
}

bool sample_play::IsVideoPlaying()
{
	return audio::IsVideoPlaying();
}

float sample_play::QMixerGain(int volume)
{
	return qmixer::Gain(volume, qmixer::k_MaxVolume);
}

float sample_play::DistanceGain(float minDistance, float maxDistance, float scale, float distance)
{
	return qmixer::DistanceGain(minDistance, maxDistance, scale, distance);
}

glm::vec3 sample_play::PolarRelative(glm::vec3 position)
{
	return qmixer::PolarRelative(position);
}

void sample_play::Clear()
{
	ReleaseSources();
}
