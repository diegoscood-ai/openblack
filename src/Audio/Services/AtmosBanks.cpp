/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AtmosBanks.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Audio/Device/Sound.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/AudioSystem.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/AtmosVoices.h"
#include "Audio/Services/AtmosVoicesInterface.h"
#include "Audio/SoundMap.h"
#include "Common/MachineClock.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{

/// A bank of the atmos mixer
struct Bank
{
	bool registered {false};
	uint32_t group {0}; ///< the bank's atmos group
	int32_t volume {0}; ///< the bank volume 0..127
};

/// A bank's loop sample (atmos frequency 0)
struct Loop
{
	size_t bank;
	entt::id_type sample;
	uint32_t group; ///< the sample's atmos group
	int32_t volume; ///< the .sad volume (flag 0x20) or 127
	int32_t fade {0};
	bool playing {false};
	Channel channel {k_NoChannel};
};

/// A loose sample (atmos frequency f > 0) in the time-ordered queue
struct Loose
{
	size_t bank;
	entt::id_type sample;
	uint32_t group;
	int32_t volume;
	int32_t frequency;
	uint32_t next; ///< the counter value at which it is due
};

/// A playing loose sample (the mixer's channel list)
struct LooseChannel
{
	Channel emitter;
	size_t bank;
	uint32_t group;
	int32_t volume;       ///< the volume last sent to it (the debug window's)
	int32_t sampleVolume; ///< its record's own volume
};

struct AtmosBanksState
{
	bool initialised {false};
	std::array<Bank, k_AtmosTypeCount> banks;
	std::array<float, k_AtmosTypeCount> target {};
	std::array<float, k_AtmosTypeCount> current {};
	std::vector<Loop> loops;
	std::vector<Loose> queue;
	std::vector<LooseChannel> channels;
	uint32_t counter {0}; ///< the mixer's turn counter
	int32_t cornerA {1};  ///< x sign of the corner a loose sample at the listener is pushed to
	int32_t cornerB {1};  ///< its y sign
};

/// The AtmosBanks state (Locator::audioState)
AtmosBanksState& AtmosBanksData()
{
	return openblack::Locator::audioState::value().Get<AtmosBanksState>();
}

/// The C runtime's rand(), 0..32767, which the atmos mixer calls straight: the one generator of src/Audio
/// (sample_play::Rand)
int32_t Rand()
{
	return sample_play::Rand();
}

/// The Dump's bank lines: the turns of OPENBLACK_ATMOS_TRACE's period
bool Trace()
{
	return sound_map::TraceThisTurn();
}

/// (openblack) the loop starts and loose samples: every one while OPENBLACK_ATMOS_TRACE is set
bool TraceEvents()
{
	static const bool k_Trace = debug_env::AtmosTrace();
	return debug_env::TraceLogs(k_Trace, "audio");
}

bool Available()
{
	return Locator::resources::has_value();
}

/// A sample of a registered bank (the resources the bank registration filled, Banks.cpp)
const Sound& SoundOf(entt::id_type id)
{
	return Locator::resources::value().GetSounds().Handle(id);
}

/// A sample's name for the trace lines; "?" without the resources (records registered by a test)
std::string SampleName(entt::id_type id)
{
	return Available() ? SoundOf(id).name : std::string("?");
}

/// Stop a sample (an atmos channel stops even while the audio is switched off). Silence stops every kept channel
/// without asking whether it plays
void StopChannel(AtmosVoicesInterface& voices, Channel& emitter)
{
	if (emitter != k_NoChannel)
	{
		if (TraceEvents() && voices.IsPlaying(emitter))
		{
			const auto infos = sample_play::Channels();
			for (const auto& info : infos)
			{
				if (info.handle == emitter)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos channel stopped: {}", SampleName(info.sound));
				}
			}
		}
		voices.Stop(emitter);
	}
	emitter = k_NoChannel;
}

/// The list is walked from the head (on a reinsertion from the head's next, the head itself being popped afterwards)
/// and the entry goes before the first one due at the same time or later
void Enqueue(const Loose& loose)
{
	auto& state = AtmosBanksData();
	const auto at = std::lower_bound(state.queue.begin(), state.queue.end(), loose.next,
	                                 [](const Loose& other, uint32_t next) { return other.next < next; });
	state.queue.insert(at, loose);
}

uint32_t NextTime(int32_t frequency)
{
	return AtmosBanksData().counter + static_cast<uint32_t>(4 * frequency + Rand() * 12 * frequency / 32767);
}

/// Register the 14 banks with the atmos mixer
void RegisterAll()
{
	auto& state = AtmosBanksData();
	std::vector<atmos_banks::Record> records;
	for (size_t i = 1; i < k_AtmosTypeCount; ++i)
	{
		const auto* name = k_AtmosTypes[i].bank;
		// the bank of Audio\SFX\Atmos registered by its file name (banks::LoadAll)
		const auto bank = FindBank(std::string("/") + name);
		if (bank == k_NoBank)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Atmos: no sound bank {}", name);
			continue;
		}
		records.clear();
		for (const auto id : banks::Samples(bank))
		{
			records.push_back(atmos_banks::RecordOf(id, SoundOf(id)));
		}
		// the library reads time(0) for its srand at each registration
		atmos_banks::RegisterBank(i, records, static_cast<uint32_t>(machine_clock::UnixTime()));
	}
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Atmos: {} loops and {} loose samples registered", state.loops.size(),
	                   state.queue.size());
	state.initialised = true;
}

/// The targets are the sound map's volumes, or 0 inside the citadel
void SetTargets(bool insideCitadel)
{
	auto& state = AtmosBanksData();
	if (insideCitadel)
	{
		// 15 floats cleared: the 14 targets and current[0]
		state.target.fill(0.0f);
		state.current[0] = 0.0f;
		return;
	}
	state.target = sound_map::GetVolumes();
	// the 15th float copied is the sound map's receiver x: it lands in current[0], NONE's (no bank)
	state.current[0] = sound_map::GetReceiverX();
}

void ProcessBanks(AtmosVoicesInterface& voices)
{
	auto& state = AtmosBanksData();
	const bool trace = Trace();
	const float alignment = atmos_banks::Alignment();
	const uint32_t group = atmos_banks::GroupFor(alignment);
	if (TraceEvents() && group != state.banks[0].group)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos group {} (alignment {:.3f})", group, alignment);
	}
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		// the group, the fade, then the volume, bank by bank
		atmos_banks::SetGroup(i, group);
		float& current = state.current[i];
		const float step = atmos_banks::FadeStep(current);
		const auto [volume, sent] = atmos_banks::StepBankVolume(current, state.target[i]);
		current = volume;
		atmos_banks::SetBankVolume(voices, i, sent);
		if (trace)
		{
			std::array<char, 128> line {};
			std::snprintf(line.data(), line.size(), "%s Vol=%3.3f Sent=%d Step=%d", k_AtmosTypes[i].name,
			              static_cast<double>(current), sent, static_cast<int>(step * 100.0f));
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
		}
	}
}

/// Play a loop: atmos, 2D, owner -1, loops -1, mode 2, caller mask 0x20, volume 0
Channel PlayLoop(AtmosVoicesInterface& voices, entt::id_type sample)
{
	const auto optionsOwner = sample_play::Owner::AtmosMixer();
	sample_play::Options options {
	    .sound = sample,
	    .atmos = true,
	    .is3D = false,
	    .owner = optionsOwner,
	    .volume = 0,
	    .loops = -1,
	    .mode = 2,
	    .callerMask = 0x20,
	};
	return voices.Play(options);
}

/// Play a loose sample: atmos, 3D, relative to the listener at (x, y, 0), owner -1, caller mask 0x20; the mode (3),
/// pitch, deviation and distance mapping are the .sad's or the defaults
Channel PlayLoose(AtmosVoicesInterface& voices, entt::id_type sample, glm::vec3 position, int32_t volume)
{
	const auto optionsOwner = sample_play::Owner::AtmosMixer();
	sample_play::Options options {
	    .sound = sample,
	    .atmos = true,
	    .is3D = true,
	    .track = false,
	    .relative = true,
	    .owner = optionsOwner,
	    .volume = volume,
	    .position = position,
	    .callerMask = 0x20,
	};
	return voices.Play(options);
}

/// The inactive pass: the loops stop and every atmos channel (the loops' and the loose samples') is stopped
void StopAll(AtmosVoicesInterface& voices)
{
	auto& state = AtmosBanksData();
	for (auto& loop : state.loops)
	{
		StopChannel(voices, loop.channel);
		loop.playing = false;
	}
	for (auto& channel : state.channels)
	{
		StopChannel(voices, channel.emitter);
	}
	state.channels.clear();
}

/// The active pass of the atmos mixer
void ProcessActive(AtmosVoicesInterface& voices)
{
	auto& state = AtmosBanksData();
	auto& banks = state.banks;

	// 1. the loose channels of a group other than their bank's fade by 5 a turn, from the volume the channel has (its
	// start's, or what the bank's volume or this fade last gave it)
	std::erase_if(state.channels, [&voices](const LooseChannel& channel) { return !voices.IsPlaying(channel.emitter); });
	for (auto& channel : state.channels)
	{
		if (channel.group == 0 || channel.group == banks[channel.bank].group)
		{
			continue;
		}
		const int32_t volume = voices.Volume(channel.emitter) - 5;
		if (volume >= 0)
		{
			channel.volume = volume;
			voices.SetVolume(channel.emitter, volume);
		}
	}

	// 2. the loops
	for (auto& loop : state.loops)
	{
		const auto& bank = banks[loop.bank];
		if (loop.playing && !voices.IsPlaying(loop.channel))
		{
			loop.playing = false;
			loop.channel = k_NoChannel;
		}
		if (loop.playing && bank.volume == 0)
		{
			// a dry stop: the bank's volume has already faded
			StopChannel(voices, loop.channel);
			loop.playing = false;
		}
		if (!loop.playing && bank.volume != 0 && (loop.group == 0 || loop.group == bank.group))
		{
			loop.channel = PlayLoop(voices, loop.sample);
			loop.playing = loop.channel != k_NoChannel;
			loop.fade = 0;
			if (TraceEvents() && loop.playing)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos loop start: {} of {}", SampleName(loop.sample),
				                   k_AtmosTypes[loop.bank].name);
			}
		}
	}
	for (auto& loop : state.loops)
	{
		const auto& bank = banks[loop.bank];
		if (loop.group != 0 && loop.group != bank.group)
		{
			if (!loop.playing)
			{
				continue;
			}
			if (loop.fade <= 0)
			{
				StopChannel(voices, loop.channel);
				loop.playing = false;
				loop.fade = 0;
				continue;
			}
			loop.fade = std::max(0, loop.fade - 5);
		}
		else if (loop.playing)
		{
			// +5 a turn, unclamped until it reaches the loop's volume
			loop.fade = loop.fade < loop.volume ? loop.fade + 5 : loop.volume;
		}
		if (loop.playing)
		{
			voices.SetVolume(loop.channel, bank.volume * loop.fade / 127);
		}
	}

	// 3. at most one loose sample a turn: the head of the queue. The turn counter only runs while the queue has one
	if (state.queue.empty())
	{
		return;
	}
	++state.counter;
	if (state.queue.front().next > state.counter)
	{
		return;
	}
	Loose loose = state.queue.front();
	state.queue.erase(state.queue.begin());
	const auto& bank = banks[loose.bank];
	if (bank.volume != 0 && (loose.group == 0 || loose.group == bank.group))
	{
		// integers -2..2 (rand() * 4 / 32767), pushed off the listener
		float x = static_cast<float>(2 - Rand() * 4 / 32767);
		float y = static_cast<float>(2 - Rand() * 4 / 32767);
		if (std::abs(x) + std::abs(y) <= 1.0f)
		{
			x *= 4.0f;
			y *= 4.0f;
			if (x == 0.0f && y == 0.0f)
			{
				x = static_cast<float>(5 * state.cornerA);
				y = static_cast<float>(5 * state.cornerB);
				const int32_t a = state.cornerA;
				state.cornerA = -a;
				state.cornerB = -a * state.cornerB;
			}
		}
		const int32_t volume = bank.volume * loose.volume / 127;
		// relative to the listener at (x, y, 0): x to the right, y ahead, z up (sample_play::PolarRelative), so
		// the loose samples lie flat around the listener, 2 to 7.1 units away
		const auto emitter = PlayLoose(voices, loose.sample, glm::vec3(x, y, 0.0f), volume);
		// a start that finds the sample or its clone group playing (the .sad's play mode 2) gives that channel back,
		// which stays what it was (a loop's, or another loose sample's): only a new channel is a loose one
		const bool known =
		    std::ranges::any_of(state.channels, [emitter](const LooseChannel& other) { return other.emitter == emitter; }) ||
		    std::ranges::any_of(state.loops, [emitter](const Loop& loop) { return loop.channel == emitter; });
		if (emitter != k_NoChannel && !known)
		{
			state.channels.push_back({emitter, loose.bank, loose.group, std::clamp(volume, 0, 127), loose.volume});
		}
		if (TraceEvents())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos loose sample: {} of {} at ({:.0f}, {:.0f}) volume {}",
			                   SampleName(loose.sample), k_AtmosTypes[loose.bank].name, x, y, volume);
		}
	}
	// rescheduled even when it did not sound
	loose.next = NextTime(loose.frequency);
	Enqueue(loose);
}
} // namespace

float atmos_banks::FadeStep(float current)
{
	// slow near the ends
	return (current > 0.1f && current <= 0.8f) ? 0.04f : 0.02f;
}

std::pair<float, int32_t> atmos_banks::StepBankVolume(float current, float target)
{
	const float step = FadeStep(current);
	const float volume = target > current ? std::min(current + step, target) : std::max(current - step, target);
	return {volume, static_cast<int32_t>(volume * 127.0f)};
}

uint32_t atmos_banks::GroupFor(float alignment)
{
	// the float compared with the double -0.59999999999999998: below, equal or unordered -> group 2, else 1
	return static_cast<double>(alignment) > -0.6 ? 1 : 2;
}

float atmos_banks::Alignment()
{
	// read as it is (by ProcessAtmosBanks and the alignment music): the game writes it once a turn, and
	// GameQueries::cameraAlignment gives that value. Unset: 0 (the audio's reset).
	const auto& query = Queries().cameraAlignment;
	return query ? query() : 0.0f;
}

void atmos_banks::UpdateBanks()
{
	if (!Available())
	{
		return;
	}
	if (!AtmosBanksData().initialised)
	{
		RegisterAll();
	}
	// the targets, then ProcessAtmosBanks
	SetTargets(sample_play::IsInsideCitadel());
	AtmosVoices voices;
	ProcessBanks(voices);
}

void atmos_banks::Mix()
{
	if (!Available() || !AtmosBanksData().initialised)
	{
		return;
	}
	AtmosVoices voices;
	atmos_banks::Process(voices, true);
}

void atmos_banks::Silence()
{
	if (!AtmosBanksData().initialised || !Available())
	{
		return;
	}
	AtmosVoices voices;
	atmos_banks::Process(voices, false);
}

void atmos_banks::Clear()
{
	// the audio's reset on a new map: Silence, every sample stopped and the alignment back to 0; the banks, their
	// targets and current volumes are kept (the registration runs once)
	Silence();
}

void atmos_banks::RegisterBank(size_t bank, std::span<const Record> records, uint32_t clock)
{
	auto& state = AtmosBanksData();
	state.banks.at(bank).registered = true;
	// srand(time(0)) before the bank's draws. The audio library also seeds once at its init, before these:
	// overwritten, not emulated
	sample_play::SeedRand(clock);
	// the earliest time this bank's loose samples are due (unsigned, none = the largest)
	constexpr uint32_t k_None = std::numeric_limits<uint32_t>::max();
	uint32_t earliest = k_None;
	for (const auto& record : records)
	{
		// atmos frequency >= 0 and a sample number other than 0 (compared unsigned: a negative one registers)
		if (record.frequency < 0 || record.sample == 0)
		{
			continue;
		}
		if (record.frequency == 0)
		{
			state.loops.push_back({bank, record.sound, record.group, record.volume});
		}
		else
		{
			const uint32_t next = NextTime(record.frequency);
			Enqueue({bank, record.sound, record.group, record.volume, record.frequency, next});
			earliest = std::min(earliest, next);
		}
	}
	if (earliest == k_None)
	{
		return;
	}
	// this bank's first loose sample plays within 20 turns: the counter goes to 20 turns before it (wrapping below 0),
	// and every queued sample due before the new counter, of this bank or an earlier one, is drawn again from it, from
	// the head while the head is due before the counter
	state.counter = earliest - 20;
	for (size_t n = state.queue.size(); n > 0 && state.queue.front().next < state.counter; --n)
	{
		Loose loose = state.queue.front();
		state.queue.erase(state.queue.begin());
		loose.next = NextTime(loose.frequency);
		Enqueue(loose);
	}
}

atmos_banks::Record atmos_banks::RecordOf(entt::id_type id, const Sound& sound)
{
	return {
	    .sound = id,
	    .sample = sound.id,
	    .frequency = sound.atmosFrequency,
	    .group = static_cast<uint32_t>(sound.atmosGroup),
	    .volume = static_cast<int32_t>(sound.atmosVolume),
	};
}

void atmos_banks::SetGroup(size_t bank, uint32_t group)
{
	AtmosBanksData().banks.at(bank).group = group;
}

void atmos_banks::SetBankVolume(AtmosVoicesInterface& voices, size_t bank, int32_t volume)
{
	auto& state = AtmosBanksData();
	auto& entry = state.banks.at(bank);
	const int32_t clamped = std::clamp(volume, 0, 127);
	if (entry.volume == clamped)
	{
		return;
	}
	entry.volume = clamped;
	// the bank's loose samples playing in its group, or in none, take the new volume at once
	for (auto& channel : state.channels)
	{
		if (channel.bank == bank && (channel.group == 0 || channel.group == entry.group))
		{
			const int32_t sent = channel.sampleVolume * clamped / 127;
			channel.volume = std::clamp(sent, 0, 127);
			voices.SetVolume(channel.emitter, sent);
		}
	}
}

void atmos_banks::Process(AtmosVoicesInterface& voices, bool active)
{
	if (active)
	{
		ProcessActive(voices);
	}
	else
	{
		StopAll(voices);
	}
}

atmos_banks::View atmos_banks::GetView()
{
	if (!Locator::audioState::has_value())
	{
		return {};
	}
	return MakeView(AtmosBanksData());
}
