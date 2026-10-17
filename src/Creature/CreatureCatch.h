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
#include <numbers>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// A creature catching a thing flying at it: whether it can reach where the thing will pass in time, and the four catching
/// animations blended by how high and how far to the side the thing comes, played together to the moment the hand closes
namespace openblack::creature_catch
{
/// The step towards a catch, whose length and travel give how far a creature can reach
constexpr size_t k_CatchStep = 225;
/// The four catching animations, blended by height and side: low and to one side, low and to the other, high and to
/// one side, high and to the other
constexpr std::array<size_t, 4> k_CatchAnimations {231, 230, 229, 228};

/// A creature only catches what weighs less than this share of its own weight
constexpr float k_MostWeightShare = 0.8f;
/// A creature tries to catch another player's throw only when a draw of a hundred is at most this
constexpr uint32_t k_OtherPlayersChance = 2;
constexpr uint32_t k_OtherPlayersDraw = 100;
/// The thing must fly across the land at least this fast, squared
constexpr float k_LeastSpeedSquared = 1.0f;
/// It must pass closest within this many seconds, and with at least this long to spare once the hand is to close
constexpr float k_MostSeconds = 5.0f;
constexpr float k_LeastSpare = 0.05f;
/// The creature turns to face the thing first when it is more than this far round, in radians
constexpr float k_TurnAngle = std::numbers::pi_v<float> / 12.0f;
/// How far the blend may lean past the four animations' own reaches before the catch misses
constexpr float k_LeastWeight = -0.2f;
constexpr float k_MostWeight = 1.2f;

/// What the catch's reach depends on
struct Approach
{
	/// Where the thing is and how it flies, and where the creature stands
	glm::vec3 thing;
	glm::vec3 velocity;
	glm::vec3 creature;
	/// The creature's size, and the scale its model is drawn at
	float size;
	float modelScale;
	/// The moment the catching hand closes, in milliseconds into the catch
	float catchMs;
	/// How long the step towards a catch lasts, in milliseconds, and how far it carries the creature across in its model's
	/// units
	float stepMs;
	float stepTravel;
};
/// Whether the creature can step to where the thing passes closest across the land before its hand must close: the
/// thing flying across fast enough, passing within five seconds with time to spare, and nearer the creature then than
/// the step reaches. Worked out step by step as the game does, each step rounded to a float, so that the answer is the
/// game's to the bit
[[nodiscard]] bool Reaches(const Approach& approach);

/// What decides whether a creature may set about catching anything at all, asked before anything about the thing
struct Eligibility
{
	/// The player's hand holds it
	bool heldByHand {false};
	/// Its plan is to fight, or it is fighting
	bool plansToFight {false};
	bool fighting {false};
	/// A script controls it
	bool scriptControlled {false};
	/// It is free to take up a reaction, and a script has given it a desire to follow alone
	bool availableForReaction {true};
	bool onlyDesire {false};
	/// Its plan is to catch something already
	bool plansToCatch {false};
};
/// Whether the creature may set about catching: not one the hand holds, one that plans to fight or is fighting, one a
/// script controls, one already planning a catch, nor one that is not free to react unless a script gave it a desire
/// to follow alone
[[nodiscard]] bool MayTry(const Eligibility& creature);
/// The most a creature can catch, by its own weight; the thing must weigh less
[[nodiscard]] float MostWeight(float creatureWeight);
/// Whether the creature goes for a throw of another player's, by a draw of a hundred
[[nodiscard]] bool GoesForOtherPlayersThrow(uint32_t draw);
/// The catch only starts while the thing flies fast enough, its speed weighed across and up or down, not along the
/// land's other axis, as the game does
[[nodiscard]] bool FastEnoughToStart(glm::vec3 velocity);

/// How the four animations are blended: how high and how far to the side the thing is between where the hand gets to in
/// each, each kept within the limits, and whether either had to be
struct Blend
{
	std::array<float, 4> weights;
	bool clamped;
	/// How high the thing was, as kept within the limits
	float height {0.5f};
};
/// The thing's place against the creature, in its own axes at the world's scale; the hand's place in each catching
/// animation at the moment it closes, in the model's units; the model's scale
[[nodiscard]] Blend Weigh(glm::vec3 thing, const std::array<glm::vec3, 4>& hands, float modelScale, bool mirrored);
/// The blend once the thing has left the air: the side goes back to the middle, and the height stays as it was last
[[nodiscard]] Blend WeighWithout(float lastHeight);

/// Ready to catch, the creature waits for the thing, steps across to it when it will pass out of reach, catches it when
/// it arrives at the moment the hand would close, or gives up when it is behind or too late
enum class Readiness : uint8_t
{
	Wait,
	Step,
	Catch,
	GiveUp,
};
struct Ready
{
	Readiness readiness;
	/// The step or the catch is made with the left hand, the right one's mirrored
	bool mirrored;
};
/// A step further than this share of the way from the high corner's hand to the other high corner's is out of reach
constexpr float k_ReachBeyondHand = 1.2f;
/// The thing arrives in time when it is due from a hundredth of a second before the hand closes to a tenth after
constexpr float k_EarlyArrival = 0.01f;
constexpr float k_LateArrival = 0.1f;
/// The thing's place and flight in the creature's own axes (ahead is along negative z), the hand's place in each
/// catching animation as it closes, the model's scale, and how long after the catch begins the hand closes, in seconds
[[nodiscard]] Ready ReadyToCatch(glm::vec3 thing, glm::vec3 velocity, const std::array<glm::vec3, 4>& hands, float modelScale,
                                 float leadSeconds);

/// How far a frame of the side step carries the creature: its share of the step clip's whole travel, sized and turned as
/// the creature is. Stepping with the left hand takes the whole of it back the other way, height and depth too, as the
/// game does (where the step ends is judged with only the sideways part turned round).
[[nodiscard]] glm::vec3 StepMove(glm::vec3 travel, const glm::mat3& rotation, float modelScale, float stepMs, float durationMs,
                                 bool mirrored);
/// After a frame of the step, a creature that stands too near ground it may not stand on is put at the nearest place it
/// may, searched for from half a unit out to below a thousand, with a little more room than the test needs
constexpr float k_StepClearance = 7.05f;
constexpr float k_StepSearchClearance = 7.1f;
constexpr float k_StepSearchDistance = 999.5f;

/// How long after the catch begins the hand closes, in seconds, for a creature of a size (its animations play faster the
/// smaller it is), worked out as the game does
[[nodiscard]] float LeadSeconds(float catchMs, float size);
/// A looping animation's clock a turn on, as the game moves it: on by the turn's step, back round once when it passes
/// the end, and never at or past the end
[[nodiscard]] int32_t AdvanceCyclic(int32_t timeMs, int32_t stepMs, int32_t durationMs);
/// Whether the step ends in this turn: it would reach its end with the turn's step
[[nodiscard]] bool StepEnds(int32_t timeMs, int32_t stepMs, int32_t durationMs);
/// Whether the catch ends in this turn: played on, as it is while reaching and once caught, when it would reach its end;
/// drawn back after a miss, when less than the turn's step is left of it
[[nodiscard]] bool CatchEnds(bool drawnBack, int32_t timeMs, int32_t stepMs, int32_t durationMs);
/// The catch's clock a turn on: played on while reaching and once caught, drawn back after a miss, down to its start
[[nodiscard]] int32_t CatchClock(bool drawnBack, int32_t timeMs, int32_t stepMs, int32_t durationMs);
} // namespace openblack::creature_catch
