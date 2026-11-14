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

#include <optional>

#include <entt/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

enum class MoveStateClockwise
{
	Undefined,
	CounterClockwise,
	Clockwise,
};

enum class MoveState
{
	Linear,
	Orbit,
	ExitCircle,
	StepThrough,
	FinalStep,
	Arrived,
};

template <MoveState S>
struct MoveStateTagComponent
{
	static constexpr MoveState k_Value = S;
	MoveStateClockwise clockwise;
	glm::vec2 stepGoal;
};

using MoveStateLinearTag = MoveStateTagComponent<MoveState::Linear>;
using MoveStateOrbitTag = MoveStateTagComponent<MoveState::Orbit>;
using MoveStateExitCircleTag = MoveStateTagComponent<MoveState::ExitCircle>;
using MoveStateStepThroughTag = MoveStateTagComponent<MoveState::StepThrough>;
using MoveStateFinalStepTag = MoveStateTagComponent<MoveState::FinalStep>;
using MoveStateArrivedTag = MoveStateTagComponent<MoveState::Arrived>;

/// The move state the original keeps on a walker after its walk's tags are dropped (at its arrival, or as the hand or
/// a script stops it): the original changes it only as a walk is set up or steps, so it stays the walk's last. A
/// wall_hug::WalkState number. Only saving reads it
struct WalkStateAfterWalk
{
	uint8_t state {0};
};

/// The circle a walker heads for or goes round: one of the circles a thing covers on the ground (its collide data,
/// ecs::thing_circles), or the circle of a map cell of water or off the land, kept as it was when the walker came
/// across it
struct WallHugObjectReference
{
	/// The thing whose circle it is; none for water or the land's edge
	entt::entity entity;
	/// Which of the thing's circles, from 0
	uint16_t circle {0};
	glm::vec2 centre {0.0f};
	float radius {0.0f};
	/// Water or the land's edge, and its map cell (each index in 16 bits)
	bool landscape {false};
	glm::ivec2 cell {0};
	/// Water, the land's edge or a fence
	bool landscapeOrFence {false};
};

struct WallHug
{
	glm::vec2 goal;
	/// The step the walk takes now, in metres: the whole-unit step below made metres
	glm::vec2 stepMetres;
	float yAngle; // FIXME(bwrsandman): member is a little redundant with transform or atan on step could keep or not
	float speed;
	/// The step last aimed at the goal, in whole map units a turn (6553.6 to the metre)
	glm::ivec2 step {0};
	/// The game angle last set, in 2048ths of a turn
	uint16_t gameAngle {0};
	/// The turns left before a straight walk to the goal (STEP_THROUGH) aims its step again
	int8_t turnsUntilStepRebuild {0};
	/// The turns before it reaches the circle ahead, or looks along the circle it goes round again; 0xFF: none in reach.
	/// Kept with no circle held (a circle that went leaves it as it was)
	uint8_t turnsToObj {0xFF};
	/// How near its goal a walker going round a circle must come before it may leave it, from when it started round
	/// (in 128ths of a metre less one; 0xFFFF or more is the game's long entry distance; 0x7FFFFFFF: at its next chance,
	/// once a thing of many cells next to its circle left the map)
	uint32_t entryDistance {0};
	/// The map position (whole map units) the walk last placed it at: the walk goes on from there while the walker
	/// still stands where it was placed, and from its position made map units once something else moved it
	std::optional<glm::ivec2> placedAt;
	/// This turn's walk handed it over to water or the land's edge, or (going round) to a fence: the walk is one to try
	/// both ways round (the look ahead, not yet ported, takes it from here)
	bool needsLookahead {false};
};

} // namespace openblack::ecs::components
