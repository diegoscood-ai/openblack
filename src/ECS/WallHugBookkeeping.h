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

#include <array>
#include <functional>
#include <limits>
#include <optional>

#include <glm/vec2.hpp>

// The bookkeeping that every wall-hugging walker shares, villagers and animals alike: what a walker does when the
// circle it hugs goes away, where the circles of water and of the land's edge stand, and the look ahead that tries a
// walk round a circle both ways before the walker commits to one.
//
// Positions and steps are in whole map units (a 65536th of ten metres), angles in 2048ths of a turn, as the game holds
// them.
//
// Pure, tested on made-up walks.

namespace openblack::ecs::wall_hug
{

/// A walker's move state, numbered as the game numbers it
enum class WalkState : uint8_t
{
	Arrived = 0x1,
	FinalStep = 0x4,
	Wander = 0x5,
	StepThrough = 0xB,
	Linear = 0xC,
	LinearClockwise = 0xD,
	LinearAnticlockwise = 0xE,
	OrbitClockwise = 0xF,
	OrbitAnticlockwise = 0x10,
	ExitAnticlockwise = 0x11,
	ExitClockwise = 0x12,
};

// ---- A hugged circle that goes ----------------------------------------------------------------------------------

/// The entry distance of a walker going round a thing that overlaps a thing of many cells that leaves the map: so far
/// that it leaves its orbit at its next chance
constexpr uint32_t k_LeaveAtOnce = 0x7FFFFFFF;
/// An entry distance this large or more is cleared as the walker leaves its orbit
constexpr uint32_t k_LongEntryDistance = 0xFFFF;

/// What a walker does the moment the circle it hugs goes away (its thing leaves the map, or the thing's circles are
/// made again), before anything else moves
struct CircleGone
{
	/// Its move state afterwards
	WalkState state;
	/// It first faces its goal again, as far as it can turn, and takes the step towards it
	bool reAim;
	/// It then looks ahead along its step again, from where it stands
	bool lookAgain;
	/// Last, it lets go of the circle it holds (the one that went, or the one it has just found)
	bool letGo;
};

/// A walker heading straight on looks again; one going round turns back to heading straight on, the same way round,
/// and looks again; one leaving a circle does the same and then lets go of what it found; any other lets go
[[nodiscard]] CircleGone OnCircleGone(WalkState state);

// ---- Water and the land's edge -----------------------------------------------------------------------------------

/// After the things of a map cell, a walker looking for what blocks it comes across the water and the land's edge of
/// that cell and of the eight round it, in this order (x, z cell offsets)
constexpr std::array<glm::ivec2, 9> k_LandscapeCellOrder {{
    {0, 0},
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
    {-1, -1},
    {-1, 1},
    {1, -1},
    {1, 1},
}};

/// The middle of a map cell in metres, where the circle of its water or of the land's edge stands. A cell is counted
/// in 16 bits, so one off the map below zero has its middle below zero too
[[nodiscard]] glm::vec2 LandscapeCircleCentre(glm::ivec2 cell);

// ---- The look ahead ------------------------------------------------------------------------------------------------

/// A trial walk gets this many turns, and one more, to reach its goal
constexpr int32_t k_TrialTurns = 1500;
/// A trial walker goes this many times faster than the walker it stands in for
constexpr uint16_t k_TrialSpeedFactor = 4;

/// How a trial walk did: the turns it had left on reaching its goal (-1 when it never did), and the square of the
/// distance in metres between where it ended and its goal
struct TrialResult
{
	int32_t turnsLeft;
	float distanceSq;
};

/// The trial of going round the other way when turning round leaves the walker walking straight to its goal: no turns
/// left, and as far away as can be
constexpr TrialResult k_TrialGivenUp {0, std::numeric_limits<float>::max()};

/// Walks a trial: moveTo makes one turn of it and tells whether the walker reached its goal. It runs until it does,
/// or until the turns run out. The turns left
[[nodiscard]] int32_t RunTrial(const std::function<bool()>& moveTo);

/// Whether trial a did better than trial b: more turns left, or as many and nearer the goal. Two distances that cannot
/// be compared count as nearer
[[nodiscard]] bool DidBetter(const TrialResult& a, const TrialResult& b);

/// What turning round reads and writes of a walker
struct WalkPose
{
	WalkState state;
	uint16_t angle;
	glm::ivec2 position;
	/// The step across (x, z) and its height part, as the game holds a step
	glm::ivec2 step;
	float stepAltitude;
};

/// A walker that turns round to go round the other way
struct TurnedRound
{
	WalkPose pose;
	/// Whether it turned at all (only the hugging states do)
	bool turned;
	/// Whether it looks along its circle again, the new way round (it does when it is going round one it still holds)
	bool lookAlongCircle;
};

/// Turning round: the walker jumps three steps ahead, and its state, its angle (half a turn) and its step go the other
/// way. The game mixes up the step's parts as it turns it: the new step across x is minus the old one, the new step
/// along z is minus the old height part truncated to a whole number, and the new height part is minus the old step
/// along z. So a walker on the ground turns round to walk along x only, until it next faces its goal
[[nodiscard]] TurnedRound TurnRound(const WalkPose& pose);

/// A walker going round a circle that the look ahead leaves as it is faces along the circle again: a quarter turn from
/// the way to the circle's middle, the way it goes round. Any other walker keeps its angle
[[nodiscard]] std::optional<uint16_t> HeadingWhenKept(WalkState state, uint16_t angleToCentre);

} // namespace openblack::ecs::wall_hug
