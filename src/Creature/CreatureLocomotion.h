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
#include <functional>
#include <numbers>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How a creature walks and runs, once a game turn. Its speed follows a requested fraction of its top speed, speeding up
/// and slowing down at a steady rate, slower uphill. The distance it covers drives its walk and run animations, blended
/// by how fast it goes, so its feet keep to the ground. Before setting off, and at corners too sharp to walk round, it
/// stops and either steps off towards its way, walks straight on when nearly facing it, or turns on the spot.
///
/// Headings are angles about the vertical axis: a creature with heading h faces (-sin h, -cos h) on the x, z plane, as
/// a body rotated by h about y does, its meshes looking along their -z axis.
namespace openblack::creature_locomotion
{
/// The places of the moving animations in the creature spec file
namespace animations
{
constexpr size_t k_Stand = 0;
constexpr size_t k_Walk = 1;
constexpr size_t k_Run = 2;
/// Turning on the spot to the right by about 0, 90 and 180 degrees, then the same to the left: the right ones turn
/// the heading up (the original's yaw down), the left ones the other way
constexpr size_t k_RightSpin = 3;
constexpr size_t k_LeftSpin = 6;
/// Stepping off to the right by 0, 90 and 180 degrees, then the same to the left
constexpr size_t k_RightStep = 10;
constexpr size_t k_LeftStep = 13;
} // namespace animations

/// Speeding up and slowing down, in units a second each second
constexpr float k_Acceleration = 12.0f;
/// The slowest a moving creature is allowed to go
constexpr float k_MinSpeed = 0.1f;
/// Running flat out, a creature goes this much faster than its running speed
constexpr float k_TopSpeedMargin = 1.1f;
/// A route turning this far from the way the creature faces makes it stop and start again
constexpr float k_CornerAngle = std::numbers::pi_v<float> / 6.0f;
/// Starting off, a creature this nearly facing its way walks straight on
constexpr float k_WalkStraightAngle = std::numbers::pi_v<float> / 12.0f;
/// The slope ahead slows the creature down by this much per unit of rise over its radius, within these limits
constexpr float k_SlopeSlowing = 0.6f;
constexpr float k_MinSlopeFactor = 0.3f;
constexpr float k_MaxSlopeFactor = 1.1f;
/// A creature this exhausted only goes slowly
constexpr float k_ExhaustedAt = 0.8f;
/// The smallest and largest sizes a creature's body is set to
constexpr float k_MinSize = 0.05f;
constexpr float k_MaxSize = 4.0f;

/// How fast a creature of a size walks and runs, in units a second
struct Speeds
{
	float walk;
	float run;
};
/// Walking is 8 and running 20 units a second for each unit of 0.875 size + 0.25, the size as it is given: the speeds
/// are not kept within k_MinSize and k_MaxSize
[[nodiscard]] Speeds SpeedsFor(float size);

/// The speed a creature heads for when asked for a fraction of its top speed
[[nodiscard]] float TargetSpeed(float fraction, float runSpeed);
/// The fraction of its top speed it goes at: what is wanted, or only its slow fraction once exhausted
[[nodiscard]] float RequiredFraction(float wanted, float slowFraction, float exhaustion);

/// The fastest it may go so as to stop in the distance left
[[nodiscard]] float StopCap(float remaining);
/// The fastest it may go so as to reach a corner at the speed allowed there, given as a squared speed in units of the
/// deceleration
[[nodiscard]] float CornerCap(float remaining, float cornerAllowance);
/// How the slope ahead changes the speed: slower uphill, a little faster downhill
[[nodiscard]] float SlopeFactor(float altitudeAhead, float altitude, float radius);
/// The speed after some seconds of speeding up towards the target. It never slows down towards a lower target: only
/// the stop and corner caps slow it.
[[nodiscard]] float Accelerate(float speed, float target, float seconds);

/// An angle brought within -pi to pi
[[nodiscard]] float WrapAngle(float angle);
/// The heading a direction on the x, z plane faces
[[nodiscard]] float HeadingOf(glm::vec2 direction);
/// The direction on the x, z plane a heading faces
[[nodiscard]] glm::vec2 DirectionOf(float heading);
/// The heading a creature taken hold of by the hand turns to: towards the player's eye from where its body is drawn
/// (each difference a float subtraction), the body's yaw of that way kept as a float, or 0 when the eye is no more
/// than 0.02 away from it along both x and z
[[nodiscard]] float HeldTurnHeading(const glm::vec3& body, const glm::vec3& eye);

/// An animation cycle's length and the distance its root moves over it, in mesh units
struct Cycle
{
	float durationMs;
	float stride;
};

/// An animation played in the blend, its time and its weight
struct Slot
{
	size_t animation;
	float timeMs;
	float weight;
};

/// The walk and run blend for one turn
struct Gait
{
	/// Below walking speed the stand and the walk, above it the run and the walk
	std::array<Slot, 2> slots;
	/// The walk's time after this turn, and how far it moved on in milliseconds
	float walkTimeMs;
	float walkAdvanceMs;
};

/// Blends standing into walking below walking speed, and walking into running above it, by how fast the creature goes.
/// The walk moves on by the distance covered over the blend's stride, so the feet keep to the ground; the run keeps in
/// step with the walk. Weights stay within 0 and 1 even when the creature goes faster than its running speed.
[[nodiscard]] Gait BlendGait(float speed, const Speeds& speeds, float distance, float scale, const Cycle& stand,
                             const Cycle& walk, const Cycle& run, float walkTimeMs, float breathPhase);
/// A leg animation's time `share` of the way through the turn, from `fromMs` on by `advanceMs`: round its length when
/// it loops, else held within it
[[nodiscard]] float TrackTime(float fromMs, float advanceMs, float durationMs, bool looping, float share);

enum class Side : uint8_t
{
	Right,
	Left,
};

/// Which side a target is on: an angle turning towards the creature's right is positive
[[nodiscard]] Side SideOf(float angle);

/// Two of the three 0, 90 and 180 degree animations of a side, and how far to blend from the first to the second
struct Pair
{
	size_t from;
	size_t to;
	float weight;
};
/// The pair of steps showing an angle, from the side's first step: up to 90 degrees between 0 and 90, beyond it
/// between 90 and 180
[[nodiscard]] Pair PairFor(size_t first, float angle);

/// A turn on the spot, as the original works it out. These take the original's yaws, a yaw y facing (sin y, -cos y):
/// a locomotion heading h is the yaw -h, and the creature room's copy turns by yaws already.
///
/// The two spins a turn plays blended, and the blend's length
struct SpinBlend
{
	size_t from;
	size_t to;
	/// How far from the first to the second; it is not held within 0 and 1
	float weight;
	/// The blend's length in milliseconds, between the two spins' lengths, to the nearest
	int32_t durationMs;
};
/// The spins of a turn from one yaw to another. The angle is the difference within half a turn: above 0 the left
/// spins, else (none, or not a number) the right ones by the angle negated. Below a quarter turn (or not a number) the
/// 0 and 90 degree spins blended by the angle over a quarter turn, else the 90 and 180 degree ones by what is beyond
/// it, each a multiplication by the float nearest 2 / pi. Their lengths in milliseconds come from `lengthMs`, none for
/// an animation the species lacks: without all three of the side's, the turn has no spins.
[[nodiscard]] std::optional<SpinBlend> SpinsFor(float yaw, float targetYaw,
                                                const std::function<std::optional<int32_t>(size_t)>& lengthMs);
/// Turning to face something, the turn is over once the yaw is within a ten-thousandth of the target (a difference
/// that is not a number counts as within). It is checked before each step of the turn.
[[nodiscard]] bool TurnIsOver(float yaw, float targetYaw);
/// A turn without spins: the yaw comes round by the share of the stand's length gone so far (before this step) of
/// the angle left, within half a turn
[[nodiscard]] float SpinlessYaw(float yaw, float targetYaw, float timeMs, float standMs);

/// How a creature sets off towards a point, or turns to face one
struct Start
{
	enum class Kind : uint8_t
	{
		/// Stepping off sideways or backwards into a walk
		Step,
		/// Walking straight on
		Walk,
		/// Turning on the spot first: its spins are SpinsFor's
		Turn,
	};
	Kind kind;
	Side side;
	/// The steps' pair, for a step
	std::optional<Pair> animations;
};

/// What the creature has to start with
struct StartOptions
{
	/// The strides of the side's 0, 90 and 180 degree steps in mesh units, when the species has all three
	std::optional<std::array<float, 3>> stepStrides;
	/// Whether it may step or walk off at all, rather than only turn to face the point
	bool moving {true};
};

/// Steps when it has the steps and the point is further than every one of them takes it; else walks straight on when
/// nearly facing the point; else turns on the spot
[[nodiscard]] Start ChooseStart(float angle, float distanceInMesh, const StartOptions& options);

/// The distances around a destination that count as arriving: anywhere from min to max
struct Ring
{
	float min;
	float max;
};
/// The arrival ring for a walk of a distance: min never beyond the destination, max at least a hair beyond min
[[nodiscard]] Ring MoveRing(float distance, float minDistance, float maxDistance);
/// The arrival ring for walking up to an object: its radius and the creature's, and some way further
[[nodiscard]] Ring ObjectRing(float creatureRadius, float objectRadius, float extra);
/// Whether a creature this far from its destination has arrived
[[nodiscard]] bool Arrived(float distance, const Ring& ring);
/// How long a walk of a distance is given before it is given up on, in milliseconds
[[nodiscard]] float TimeLimitMs(float distance);
/// Running away from a threat: a distance further along the way from the threat to the creature
[[nodiscard]] glm::vec2 RunAwayPoint(glm::vec2 creature, glm::vec2 threat, float distance);
} // namespace openblack::creature_locomotion
