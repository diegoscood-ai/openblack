/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>

#include "Audio/Services/SoundMap.h"

namespace openblack::audio::atmos_banks
{

/// The ambient banks: the audio's atmos part and the audio library's mixer of the banks. Each of the 14 ATMOS_TYPE
/// banks of Audio\SFX\Atmos (the sound groups "ocean.sad"...) has a volume 0..127 that follows the sound map's volume
/// of its type; its loops (atmos frequency 0) play 2D while that is not 0, fading in by 5 a turn, and one loose sample
/// (frequency f > 0, next time = counter + 4f + U[0, 12f] turns) at most starts a turn, from the head of one queue for
/// all banks, at a random point beside the listener.

/// The atmos of the audio's game turn (audio::ProcessTurn calls them in its order, once per game turn after the sound
/// map's update, when the game is not paused, past turn 5 and with the audio active): UpdateBanks sets the targets (the
/// sound map's volumes, all 0 inside the citadel) and runs ProcessAtmosBanks (step 0.02 / 0.04 towards them, group by
/// the alignment, bank volume = current * 127); then the channels and the listener; then Mix, the mixer's pass, unless
/// a video plays. The first UpdateBanks registers the banks.
void UpdateBanks();
void Mix();

/// The camera alignment (-1 evil .. 1 good), which ProcessAtmosBanks compares with -0.6 to put every bank in group 1 or
/// 2: GameQueries::cameraAlignment (0 when unset)
[[nodiscard]] float Alignment();

/// The group of every bank for that alignment, 1 when it is above the double -0.6, else 2 (a NaN too)
[[nodiscard]] uint32_t GroupFor(float alignment);

/// The step a bank's volume moves by in a turn: 0.04 above 0.1 and up to 0.8, else 0.02 (slow near the ends)
[[nodiscard]] float FadeStep(float current);

/// One turn of a bank's fade (UpdateBanks): the current volume moves by FadeStep towards the target without
/// passing it. Gives the new volume and what is sent to the bank, the volume x 127 truncated (the mixer clamps it to
/// 0..127)
[[nodiscard]] std::pair<float, int32_t> StepBankVolume(float current, float target);

/// The loops stop and every atmos channel (the loops' and the loose samples') is stopped; the other samples play on.
/// The end of a turn calls it instead of the audio's game turn while the game is paused or in the first 5 turns.
void Silence();

/// A new map (the audio's reset): Silence, the atmos channels stopped; the banks keep their volumes, as in the
/// original
void Clear();

/// A copy of the ambient banks' state, for the debug window. Reading it changes nothing: no channel is asked and no
/// random number is drawn
struct View
{
	/// One of the 14 banks, by ATMOS_TYPE
	struct Bank
	{
		bool registered {false};
		uint32_t group {0};
		int32_t volume {0}; ///< 0..127
		float target {0.0f};
		float current {0.0f};
	};
	/// A loop sample
	struct Loop
	{
		size_t bank {0};
		entt::id_type sample {0};
		uint32_t group {0};
		int32_t volume {0}; ///< its own volume, 0..127
		int32_t fade {0};
		bool playing {false};
	};
	/// A loose sample waiting in the queue, in the order they are due
	struct Loose
	{
		size_t bank {0};
		entt::id_type sample {0};
		uint32_t group {0};
		int32_t frequency {0};
		uint32_t next {0}; ///< the turn counter's value at which it is due
	};
	/// A loose sample that started and is still kept as playing
	struct Playing
	{
		size_t bank {0};
		uint32_t group {0};
		int32_t volume {0};
	};

	bool initialised {false}; ///< the banks are registered
	uint32_t counter {0};     ///< the mixer's turn counter
	std::array<Bank, k_AtmosTypeCount> banks {};
	std::vector<Loop> loops;
	std::vector<Loose> queue;
	std::vector<Playing> playing;
};

/// The banks as they are now; an empty view when the audio's state is not there
[[nodiscard]] View GetView();

/// GetView's copy of a state shaped as the banks' own: the flag and the counter; each bank's registered flag, group
/// and volume, with its target and current value; the loops; the queue; the loose channels kept as playing. A template,
/// so that the copy is checked on a fake state
template <typename State>
[[nodiscard]] View MakeView(const State& state)
{
	View view;
	view.initialised = state.initialised;
	view.counter = state.counter;
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		const auto& bank = state.banks.at(i);
		view.banks.at(i) = {
		    .registered = bank.registered,
		    .group = bank.group,
		    .volume = bank.volume,
		    .target = state.target.at(i),
		    .current = state.current.at(i),
		};
	}
	view.loops.reserve(state.loops.size());
	for (const auto& loop : state.loops)
	{
		view.loops.push_back({
		    .bank = loop.bank,
		    .sample = loop.sample,
		    .group = loop.group,
		    .volume = loop.volume,
		    .fade = loop.fade,
		    .playing = loop.playing,
		});
	}
	view.queue.reserve(state.queue.size());
	for (const auto& loose : state.queue)
	{
		view.queue.push_back({
		    .bank = loose.bank,
		    .sample = loose.sample,
		    .group = loose.group,
		    .frequency = loose.frequency,
		    .next = loose.next,
		});
	}
	view.playing.reserve(state.channels.size());
	for (const auto& channel : state.channels)
	{
		view.playing.push_back({.bank = channel.bank, .group = channel.group, .volume = channel.volume});
	}
	return view;
}

} // namespace openblack::audio::atmos_banks
