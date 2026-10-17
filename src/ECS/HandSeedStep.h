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

#include "ECS/Systems/MagicSystemInterface.h"

/// What the action's press or release did with a spell seed in the hand, and what the miracles' service reports for
/// it. The hand's press and release record the branch they took; the report is read from it afterwards, so working it
/// out changes nothing in the game.
namespace openblack::ecs::hand_seed
{

/// The branch a press or release with a seed in the hand took
enum class Step : uint8_t
{
	/// Nothing: out of the influence, no seed, a target that does not take it, or no action in progress
	Nothing,
	/// Armed until the release, a gesture sound looping while the button is down
	Armed,
	/// The fail spot visual and sound: the miracle can't be cast there
	Failed,
	/// An apply packet still waits for its handler, so none is sent
	Waiting,
	/// An apply packet was sent
	Sent,
	/// A miracle cast while held was let go: its unlock packet was sent
	Unlocked,
};

/// What the miracles' service reports for a step. seedReady: the seed has been charged, so a failed step is the place's
/// fault. locked: after the step the miracle is being cast while the button is held
[[nodiscard]] constexpr systems::MagicSystemInterface::HandResult ResultOf(Step step, bool seedReady, bool locked) noexcept
{
	using HandResult = systems::MagicSystemInterface::HandResult;
	switch (step)
	{
	case Step::Armed:
		return HandResult::Readied;
	case Step::Failed:
		return seedReady ? HandResult::CantCastThere : HandResult::NotReady;
	case Step::Sent:
		return locked ? HandResult::CastHeld : HandResult::Cast;
	case Step::Unlocked:
		return HandResult::Released;
	case Step::Nothing:
	case Step::Waiting:
		break;
	}
	return HandResult::None;
}

/// An apply's result that takes the seed out of the hand: removed (0x16) or placed (0x17). Then the hand itself lets go
/// of it too, since nothing else is in it, and goes back to its empty, open pose. Kept (1), used up (3), nothing (5) and
/// out of the status's hand only (0x18) leave the hand holding as it was
[[nodiscard]] constexpr bool HandLetsGoAfterApply(int result) noexcept
{
	constexpr int k_Removed = 0x16;
	constexpr int k_Placed = 0x17;
	return result == k_Removed || result == k_Placed;
}

} // namespace openblack::ecs::hand_seed
