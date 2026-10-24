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

#include <functional>
#include <optional>
#include <span>

#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"

// The rules of the wall-hugging walk that villagers (and other walkers) use to get round the circles of the things in
// their way.
//
// Speeds: a walker's speed is a whole number of map units (a 65536th of ten metres) walked each turn, as the game
// holds it (villager_speed's WholeSpeed gives it from WallHug::speed).
//
// The walk itself is in whole map units at whole game angles, as the game's: a walker faces one of 2048 directions, and
// its step is worked out from the game's sine table, so it is a little shorter than its speed.
//
// Going round a circle: each time the walker's next step crosses into another map cell, and when it has walked as many
// turns as the last look ahead found it could go, it looks along the arc of the circle it hugs, in the way it goes
// round, for the first point where another circle in that cell (or a stretch of water or the land's edge nearby) cuts
// across its path. When the walker's goal lies inside the circle it hugs, the point of the arc facing the goal ends the
// walk round as well: once the walker gets there it stops hugging and walks straight in to the goal. Whichever comes
// first along the arc decides: a point less than a step away hands the walker over to the circle that blocks it (looked
// at again in the same way, at most three times in a turn), or sends it straight to its goal; otherwise the walker
// keeps hugging for as many turns as the arc's length is worth, and turns to face round the circle, a little more
// outwards the further it is from the circle's middle.
//
// The game works all of this out with its floating point unit set to single precision, so every product, quotient,
// sum and square root here is a float operation, in the game's order.
//
// Pure, tested on made-up circles.

namespace openblack::ecs::wall_hug
{

/// The look ahead's answer when nothing blocks the walker in reach
constexpr uint8_t k_NoObstacleInReach = 0xff;
/// How far round a walker that is handed over too often in one turn keeps going before it looks again
constexpr uint8_t k_TurnsAfterTooManyHandovers = 10;
/// The radius of the circle a stretch of water or the land's edge takes up in a map cell, in metres
constexpr float k_LandscapeBlockerRadius = 7.2f;

/// A map position (whole map units, a 65536th of ten metres) in metres, as the walk's geometry takes it
[[nodiscard]] glm::vec2 ToPoint(glm::ivec2 whole);
/// A point in metres as a map position, truncated
[[nodiscard]] glm::ivec2 ToWhole(glm::vec2 metres);
/// The square of the distance between two map positions, in metres
[[nodiscard]] float MetresDistanceSq(glm::ivec2 a, glm::ivec2 b);

/// The step a walker makes in a turn along a game angle (2048 to the circle, 0 along x, 512 along z), in whole map
/// units: a sixteenth of its speed (whole units a turn) times the sine table's 65536ths, over 4096
[[nodiscard]] glm::ivec2 StepAlong(uint16_t angle, int32_t wholeSpeed);
/// Whether a walker is within its step (its speed in whole units) of a point
[[nodiscard]] bool WithinStep(glm::ivec2 position, glm::ivec2 point, int32_t wholeSpeed);
/// How many game angles a walker turns each turn going round a circle of this radius (metres): its speed over the
/// radius times one constant (game angles per map unit of arc and metre of radius), truncated, and one more
[[nodiscard]] uint16_t OrbitTurn(int32_t wholeSpeed, float radius);
/// Which way a walker heading for a circle goes round it on reaching it: clockwise (x to the right, z up) when the
/// circle's middle lies to the right of its step
[[nodiscard]] bool GoesRoundClockwise(glm::ivec2 position, glm::ivec2 centre, glm::ivec2 step);

/// When a walker starts to go round a circle it notes how far from its goal it is, in 128ths of a metre less one; it
/// may only leave the circle once nearer the goal than that.
[[nodiscard]] uint32_t EntryDistance(glm::ivec2 position, glm::ivec2 goal);
/// Whether a walker going round a circle leaves it now: it is nearer its goal than when it started round, and the goal
/// lies ahead of it, off to the side away from the way it turns.
[[nodiscard]] bool LeavesCircle(glm::ivec2 position, glm::ivec2 goal, glm::ivec2 step, uint32_t entryDistance, bool clockwise);

/// A circle that can block a walker
struct BlockingCircle
{
	glm::vec2 centre;
	float radius;
	/// Water or the land's edge rather than a thing
	bool landscape;
	/// Water, the land's edge or a fence
	bool landscapeOrFence;
};

/// Whether a model is a fence's: the American fence and the Celtic short and tall fences
[[nodiscard]] bool IsFenceModel(MeshId model);

/// What a walker going round a circle does after looking along it
struct CircleSweep
{
	enum class Outcome
	{
		/// It keeps going round the circle it now hugs
		Hug,
		/// It stops hugging and steps straight towards its goal
		StepThrough,
	};
	Outcome outcome;
	/// The blocking circle it was handed over to, if it was
	std::optional<size_t> hugged;
	/// The turns it goes round before it looks again; k_NoObstacleInReach for no end in reach
	uint8_t turnsToObstacle;
	/// The game angle it now walks at, when it was turned
	std::optional<uint16_t> heading;
	/// Whether the last circle it was handed over to was water, the land's edge or a fence
	bool needsLookahead;
};

/// What a walker going round a circle looks at
struct CircleSweepInput
{
	/// The walker's map position and its goal's, in whole map units
	glm::ivec2 position;
	glm::ivec2 goal;
	/// The circle the walker hugs, in metres
	glm::vec2 centre;
	float radius;
	bool clockwise;
	/// The walker's speed in whole map units a turn
	int32_t wholeSpeed;
	/// The circles that may block it, in the order the game comes across them
	std::span<const BlockingCircle> blockers;
};

/// A walker going round a circle looks along its arc, as above
[[nodiscard]] CircleSweep SweepCircle(const CircleSweepInput& input);

/// The angle of the arc from the end of the stretch of the circle a walker can go round to the walker's own direction
/// from the circle's middle, both unit vectors: the arccosine of their dot product as the game's runtime gives it,
/// stored as a float, or the rest of the full turn when the arc is the long way round
[[nodiscard]] float ArcAngle(glm::vec2 end, glm::vec2 dir, bool clockwise);

/// What a walker heading straight on finds ahead of it
struct LineScan
{
	/// The turns until it reaches the circle ahead; k_NoObstacleInReach when nothing is in reach
	uint8_t turnsToObstacle;
	/// The circle it will reach, of those it was given
	std::optional<size_t> circle;
};

/// A walker heading straight on from its map position along its step looks for the nearest circle its line meets, of
/// those given in the order the game comes across them: the first circle whose near edge comes before every other's
/// along the line, where a circle the walker has gone more than a fifth of a metre into doesn't count. It reaches it
/// after the distance to that edge over its step, in whole turns; more than 255 turns away is out of reach.
[[nodiscard]] LineScan ScanLine(glm::ivec2 position, glm::ivec2 step, std::span<const BlockingCircle> circles);
/// The same from a point and a step in metres (a step of whole map units made metres gives ScanLine's answer)
[[nodiscard]] LineScan ScanLineFrom(glm::vec2 origin, glm::vec2 step, std::span<const BlockingCircle> circles);

// Trying both ways round: a walker that comes up against water, the land's edge or a fence while going round may be
// going the long way. Such a walker waits for a trial, and once a turn, after the turn's game code, the game takes one
// waiting walker and tries its walk out with a stand-in: a grown-up villager of the first kind made where it stands,
// with its walk, going four times its speed at the stand-in's own pace. The stand-in walks on for a turn and then up
// to 1,500 more, once as the walker goes and once turned round. The way that gets there with more turns to spare wins,
// or, with as many, the one that ends nearer the goal; when turned round wins, the walker is turned round. A walker is
// tried once until it next leaves a circle, stops hugging or sets off on a new walk.

/// How many turns a trial walk gets after its first
constexpr int32_t k_TrialWalkTurns = 1500;
/// How much faster than the walker the stand-in goes
constexpr int32_t k_TrialSpeedUp = 4;
/// A stand-in's age, in years
constexpr uint32_t k_TrialWalkerAge = 21;

/// How a trial walk went: the turns it had to spare when it got there (-1 if it never did), and how far from the goal
/// it ended, squared, in metres
struct TrialWalk
{
	int32_t turnsToSpare;
	float distanceSq;
};

/// The turns to spare of a trial walk: it walks a turn, then up to k_TrialWalkTurns more until one says it got there
[[nodiscard]] int32_t TurnsToSpare(const std::function<bool()>& walkOneTurn);

/// Whether the walk turned round beats the walk as it goes
[[nodiscard]] bool TurnedRoundIsBetter(const TrialWalk& turnedRound, const TrialWalk& asItGoes);

/// A walker turned round: it jumps three steps on, then faces back the way it came, half a turn round. The game turns
/// its step round with the across and height parts swapped: across and along the map's x it goes back; its new along
/// part is its old height part backwards (truncated), and its new height part its old along part backwards. A step
/// that was just aimed has no height part, so it comes out with no along part, until it is next aimed; turned round
/// twice before that, its along part comes back.
struct TurnedRoundWalker
{
	glm::ivec2 position;
	/// The step's x and z, in whole map units
	glm::ivec2 step;
	/// The step's height part, which the game keeps with it
	float stepAltitude;
	uint16_t gameAngle;
};
[[nodiscard]] TurnedRoundWalker TurnRound(glm::ivec2 position, glm::ivec2 step, float stepAltitude, uint16_t gameAngle);

/// The heading of a walker going round a circle that stays the way it goes after its trial: a quarter turn round from
/// facing the circle's middle, the way it goes round
[[nodiscard]] uint16_t HeadingRound(glm::ivec2 position, glm::ivec2 centre, bool clockwise);

} // namespace openblack::ecs::wall_hug
