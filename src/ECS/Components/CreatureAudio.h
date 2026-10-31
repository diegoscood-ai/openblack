/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <deque>
#include <string>
#include <vector>

#include "Audio/AnimEffectKeys.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureSoundQueue.h"

namespace openblack::ecs::components
{

/// The sounds a creature's animations passed in its last turn, waiting for the body's drawing to reach them, and the
/// sounds it made last. Each turn queues the moments its animations pass as they move on; the frames after it play each
/// once the drawing, a turn behind, reaches it, and the next turn plays whatever is left before its animations move on
struct CreatureAudio
{
	/// The sounds queued at the last turn, in order of how far through it they lie
	creature_audio::TurnQueue queued;
	/// The animations it played together by weight at the end of the last turn, from which the next turn's passes
	std::vector<creature_audio::Played> turnSlots;

	/// A sound the creature made, or was to make
	struct Heard
	{
		/// The game time it was asked for, in milliseconds since the creature was first heard
		float atMs;
		creature_audio::EventKind kind;
		audio::AnimEffectKeys keys;
		/// The bank it was looked up in, none when the species has no voice bank
		audio::BankId bank {audio::k_NoBank};
		/// The channel it started on, none when the bank's filters left it out or nothing started
		audio::Channel channel {audio::k_NoChannel};
		bool played {false};
		/// Why it was silent, if it was
		std::string note;
	};
	/// The last sounds, the newest last
	std::deque<Heard> recent;
	static constexpr size_t k_RecentCount = 24;
	float clockMs {0.0f};
};

} // namespace openblack::ecs::components
