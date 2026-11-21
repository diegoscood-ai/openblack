/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstddef>

#include "Creature/CreatureCatch.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"

/// How far through what it does a creature is, as the scripts ask it (PLAYED_PERCENTAGE): its time in whole
/// milliseconds over the length of the animation the game names what it does by, which is not always the one it plays
namespace openblack::creature_played_share
{
/// The share: whole milliseconds over the length
[[nodiscard]] inline float Share(float timeMs, float durationMs)
{
	return std::trunc(timeMs) / durationMs;
}

/// The animation an action on a thing is named by. Picking up, breaking, throwing, pointing and taking from the hand
/// leave the name standing gave the body, the stand pose. Keeping hold of something and letting go of it (putting it
/// down, tossing it away, lobbing it, eating it) are named by the animation they play, `played`
[[nodiscard]] constexpr size_t ObjectActionAnimation(creature_object_actions::Kind kind, size_t played)
{
	using creature_object_actions::Kind;
	switch (kind)
	{
	case Kind::Keep:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::Eat:
		return played;
	case Kind::PickUp:
	case Kind::Destroy:
	case Kind::Throw:
	case Kind::Point:
	case Kind::TakeFromHand:
		break;
	}
	return creature_layers::animations::k_Stand;
}

/// The animation a catch is named by: its sideways step from the moment it steps across to the end of the catch, else
/// the stand pose
[[nodiscard]] constexpr size_t CatchAnimation(bool stepped)
{
	return stepped ? creature_catch::k_CatchStep : creature_layers::animations::k_Stand;
}
} // namespace openblack::creature_played_share
