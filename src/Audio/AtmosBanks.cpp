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
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/Clouds.h"
#include "AudioManagerInterface.h"
#include "Common/RandomNumberManager.h"
#include "Locator.h"
#include "SamplePlay.h"
#include "Sound.h"
#include "SoundGroup.h"
#include "SoundMap.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{

/// An LH_AudioBank of the atmos mixer
struct Bank
{
	bool registered {false};
	uint32_t group {0}; ///< +0x134: LHAtmosSetGroup
	int32_t volume {0}; ///< LHAtmosSetBankVolume 0..127
};

/// fn_100011B0: a bank's loop sample (+0x27C = 0)
struct Loop
{
	size_t bank;
	entt::id_type sample;
	uint32_t group;           ///< the sample's atmos group (+0x11A)
	int32_t volume;           ///< +0x10: the .sad volume (flag 0x20) or 127
	int32_t fade {0};         ///< +0x14
	bool playing {false};     ///< +0x18
	Channel channel {k_NoChannel};
};

/// A loose sample (+0x27C = f > 0) in the time-ordered queue
struct Loose
{
	size_t bank;
	entt::id_type sample;
	uint32_t group;
	int32_t volume; ///< +0xC
	int32_t frequency;
	uint32_t next; ///< +0x8: the counter value at which it is due
};

/// A playing loose sample (the mixer's channel list)
struct LooseChannel
{
	Channel emitter;
	size_t bank;
	uint32_t group;
	int32_t volume;
};

struct State
{
	bool initialised {false};
	std::array<Bank, k_AtmosTypeCount> banks;
	std::array<float, k_AtmosTypeCount> target {};  ///< GAudio+0x1CC
	std::array<float, k_AtmosTypeCount> current {}; ///< GAudio+0x204
	std::vector<Loop> loops;
	std::vector<Loose> queue;
	std::vector<LooseChannel> channels;
	uint32_t counter {0}; ///< the mixer's turn counter (+0x18)
	int32_t cornerA {1};  ///< [0x10037050]
	int32_t cornerB {1};  ///< [0x10037054]
};
State g_State;

/// MSVC rand(): 0..32767
int32_t Rand()
{
	return Locator::rng::value().NextValue<int32_t>(0, 32767);
}

/// The Dump's bank lines: the turns of OPENBLACK_ATMOS_TRACE's period
bool Trace()
{
	return sound_map::TraceThisTurn();
}

/// (openblack) the loop starts and loose samples: every one while OPENBLACK_ATMOS_TRACE is set
bool TraceEvents()
{
	static const bool k_Trace = std::getenv("OPENBLACK_ATMOS_TRACE") != nullptr;
	return k_Trace;
}

bool Available()
{
	return Locator::audio::has_value() && Locator::resources::has_value();
}

/// LHSampleSetVolume 0x10013400 (QMixer's linear law, sample_play::QMixerGain)
void SetGain(Channel emitter, int32_t volume)
{
	sample_play::SetVolume(emitter, volume);
}

/// LHSampleIsPlaying(LH_SampleInfo*) 0x10014070
bool IsPlaying(Channel emitter)
{
	return sample_play::IsPlaying(emitter);
}

/// LHSampleStop(LH_SampleInfo*) 0x10012DF0 (an atmos channel stops even while the audio is switched off)
void StopChannel(Channel& emitter)
{
	if (IsPlaying(emitter))
	{
		if (TraceEvents())
		{
			const auto infos = sample_play::Channels();
			for (const auto& info : infos)
			{
				if (info.handle == emitter)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos channel stopped: {}",
					                   Locator::audio::value().GetSound(info.sound).name);
				}
			}
		}
		sample_play::Stop(emitter);
	}
	emitter = k_NoChannel;
}

/// fn_100011B0 (0x10001304..0x100013A9): the list is walked from the head (on a reinsertion from the head's next, the
/// head itself being popped afterwards by fn_10001170) and the entry goes before the first one due at the same time
/// or later
void Enqueue(const Loose& loose)
{
	const auto at = std::lower_bound(g_State.queue.begin(), g_State.queue.end(), loose.next,
	                                 [](const Loose& other, uint32_t next) { return other.next < next; });
	g_State.queue.insert(at, loose);
}

uint32_t NextTime(int32_t frequency)
{
	return g_State.counter + static_cast<uint32_t>(4 * frequency + Rand() * 12 * frequency / 32767);
}

/// InitAtmos 0x428EF0 -> fn_00428F30: LHBankRegister of the 14 banks; the DLL's fn_10001610 per bank
void Register()
{
	auto& audio = Locator::audio::value();
	const auto& groups = audio.GetSoundGroups();
	for (size_t i = 1; i < k_AtmosTypeCount; ++i)
	{
		const auto* name = k_AtmosTypes[i].bank;
		const auto found = groups.find(name);
		if (found == groups.end())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Atmos: no sound bank {}", name);
			continue;
		}
		g_State.banks[i].registered = true;
		bool anyLoose = false;
		for (const auto id : found->second.sounds)
		{
			const auto& sound = audio.GetSound(id);
			// +0x27C >= 0 and a sample id > 0
			if (sound.atmosFrequency < 0 || sound.id <= 0)
			{
				continue;
			}
			// vol = the .sad's with flag 0x20, else 127
			const auto volume = static_cast<int32_t>(sound.volume127);
			const auto group = static_cast<uint32_t>(sound.atmosGroup);
			if (sound.atmosFrequency == 0)
			{
				g_State.loops.push_back({i, id, group, volume});
			}
			else
			{
				Enqueue({i, id, group, volume, sound.atmosFrequency, NextTime(sound.atmosFrequency)});
				anyLoose = true;
			}
		}
		// the first loose sample plays within 20 turns
		if (anyLoose && !g_State.queue.empty())
		{
			g_State.counter = g_State.queue.front().next - 20;
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Atmos: {} loops and {} loose samples registered", g_State.loops.size(),
	                   g_State.queue.size());
	g_State.initialised = true;
}

/// fn_00429100
void SetTargets(bool insideCitadel)
{
	if (insideCitadel)
	{
		// rep stosd of 15 dwords from +0x1CC: the 14 targets and current[0]
		g_State.target.fill(0.0f);
		g_State.current[0] = 0.0f;
		return;
	}
	g_State.target = sound_map::GetVolumes();
	// the 15th float copied is GSoundMap+0xEC (the receiver's x): it lands in current[0], NONE's (no bank)
	g_State.current[0] = sound_map::GetReceiverX();
}

/// ProcessAtmosBanks 0x428FE0
void ProcessBanks()
{
	const bool trace = Trace();
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		// 0x428FFA: GAudio+0x190 > -0.6 (the double at 0x8C4A08)
		g_State.banks[i].group = atmos_banks::Alignment() > -0.6f ? 1 : 2;
		float& current = g_State.current[i];
		const float target = g_State.target[i];
		// slow near the ends
		const float step = (current > 0.1f && current <= 0.8f) ? 0.04f : 0.02f;
		if (target > current)
		{
			current = std::min(current + step, target);
		}
		else
		{
			current = std::max(current - step, target);
		}
		const auto sent = static_cast<int32_t>(current * 127.0f);
		g_State.banks[i].volume = std::clamp(sent, 0, 127);
		if (trace)
		{
			std::array<char, 128> line {};
			std::snprintf(line.data(), line.size(), "%s Vol=%3.3f Sent=%d Step=%d", k_AtmosTypes[i].name,
			              static_cast<double>(current), sent, static_cast<int>(step * 100.0f));
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
		}
	}
}

/// The loop's LHSamplePlay (0x10001A18): AtmosInfo 1, is3D 0, owner -1, loops -1, mode 2, caller mask 0x20, volume 0
Channel PlayLoop(entt::id_type sample)
{
	sample_play::Options options;
	options.sound = sample;
	options.atmos = true;
	options.is3D = false;
	options.owner = sample_play::Owner::AtmosMixer();
	options.loops = -1;
	options.mode = 2;
	options.callerMask = 0x20;
	options.volume = 0;
	return sample_play::Start(options);
}

/// A loose sample's LHSamplePlay (0x10001BD7): AtmosInfo = its entry, is3D 1, +0x0C 1, relative (+0x14 = 1) at
/// (x, y, 0), owner -1, caller mask 0x20; the mode (3), pitch, deviation and distance mapping are the .sad's or the
/// defaults
Channel PlayLoose(entt::id_type sample, glm::vec3 position, int32_t volume)
{
	sample_play::Options options;
	options.sound = sample;
	options.atmos = true;
	options.is3D = true;
	options.relative = true;
	options.position = position;
	options.owner = sample_play::Owner::AtmosMixer();
	options.callerMask = 0x20;
	options.volume = volume;
	return sample_play::Start(options);
}

/// LHAtmosProcess(1) 0x100018B0
void Process()
{
	auto& audio = Locator::audio::value();
	auto& banks = g_State.banks;

	// 1. the loose channels of a group other than their bank's fade by 5 a turn
	std::erase_if(g_State.channels, [](const LooseChannel& channel) { return !IsPlaying(channel.emitter); });
	for (auto& channel : g_State.channels)
	{
		if (channel.group != 0 && channel.group != banks[channel.bank].group && channel.volume - 5 >= 0)
		{
			channel.volume -= 5;
			SetGain(channel.emitter, channel.volume);
		}
	}

	// 2. the loops
	for (auto& loop : g_State.loops)
	{
		const auto& bank = banks[loop.bank];
		if (loop.playing && !IsPlaying(loop.channel))
		{
			loop.playing = false;
			loop.channel = k_NoChannel;
		}
		if (loop.playing && bank.volume == 0)
		{
			// a dry stop: the bank's volume has already faded
			StopChannel(loop.channel);
			loop.playing = false;
		}
		if (!loop.playing && bank.volume != 0 && (loop.group == 0 || loop.group == bank.group))
		{
			loop.channel = PlayLoop(loop.sample);
			loop.playing = loop.channel != k_NoChannel;
			loop.fade = 0;
			if (TraceEvents() && loop.playing)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos loop start: {} of {}", audio.GetSound(loop.sample).name,
				                   k_AtmosTypes[loop.bank].name);
			}
		}
	}
	for (auto& loop : g_State.loops)
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
				StopChannel(loop.channel);
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
			SetGain(loop.channel, bank.volume * loop.fade / 127);
		}
	}

	// 3. at most one loose sample a turn: the head of the queue
	++g_State.counter;
	if (g_State.queue.empty() || g_State.queue.front().next > g_State.counter)
	{
		return;
	}
	Loose loose = g_State.queue.front();
	g_State.queue.erase(g_State.queue.begin());
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
				x = static_cast<float>(5 * g_State.cornerA);
				y = static_cast<float>(5 * g_State.cornerB);
				const int32_t a = g_State.cornerA;
				g_State.cornerA = -a;
				g_State.cornerB = -a * g_State.cornerB;
			}
		}
		const int32_t volume = bank.volume * loose.volume / 127;
		// relative to the listener at LHaudio (x, y, 0): x to the right, y ahead, z up (sample_play::PolarRelative), so
		// the loose samples lie flat around the listener, 2 to 7.1 units away
		const auto emitter = PlayLoose(loose.sample, glm::vec3(x, y, 0.0f), volume);
		if (emitter != k_NoChannel)
		{
			g_State.channels.push_back({emitter, loose.bank, loose.group, volume});
		}
		if (TraceEvents())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Atmos loose sample: {} of {} at ({:.0f}, {:.0f}) volume {}",
			                   audio.GetSound(loose.sample).name, k_AtmosTypes[loose.bank].name, x, y, volume);
		}
	}
	// rescheduled even when it did not sound
	loose.next = NextTime(loose.frequency);
	Enqueue(loose);
}
} // namespace

float atmos_banks::Alignment()
{
	// GPlayer::ProcessPlayers 0x64A697 -> fn_0064AC30 (every turn): the GPlayer::GetAlignmentValue (player+0x60 -> +8)
	// of MapCoords::CalculateMostInfluentialPlayer at the camera (the interface status' CameraPos, +0xB0), through
	// fn_005E2240: a = clamp((alignment + 1) / 2, 0, 1), GAudio+0x190 = 2a - 1 (DoCitadelMultiplayer passes 0.5 = 0).
	// The same value as the sky's alignment target (fn_0064AC30 -> fn_005E2240), so it has a single source:
	// Clouds::InfluentialPlayerAlignment, which is ecs::effects::alignment::GetInterfaceAlignment() x 2 - 1 (fn_0064AC30's
	// value, once a turn; the test hook OPENBLACK_TEST_SKY_ALIGNMENT and the debug slider override both alike).
	const float a = std::clamp((Clouds::InfluentialPlayerAlignment() + 1.0f) * 0.5f, 0.0f, 1.0f);
	return 2.0f - 2.0f * (1.0f - a) - 1.0f;
}

void atmos_banks::UpdateBanks()
{
	if (!Available())
	{
		return;
	}
	if (!g_State.initialised)
	{
		Register();
	}
	// GAudio::ProcessAudioGameTurn 0x427080: fn_00429100 0x427099 and ProcessAtmosBanks 0x4270A0
	SetTargets(sample_play::IsInsideCitadel());
	ProcessBanks();
}

void atmos_banks::Mix()
{
	if (!Available() || !g_State.initialised)
	{
		return;
	}
	// 0x4270C0: LHAtmosProcess(1)
	Process();
}

void atmos_banks::Silence()
{
	if (!g_State.initialised || !Available())
	{
		return;
	}
	for (auto& loop : g_State.loops)
	{
		StopChannel(loop.channel);
		loop.playing = false;
	}
	for (auto& channel : g_State.channels)
	{
		StopChannel(channel.emitter);
	}
	g_State.channels.clear();
}

void atmos_banks::Clear()
{
	// GGame::ClearMap 0x552D98 -> GAudio::Reset 0x426CA0: LHAtmosProcess(0), LHSampleStopAll and the alignment back to
	// 0; the banks, their targets and current volumes are kept (InitAtmos runs once)
	Silence();
}
