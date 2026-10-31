/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLocomotion.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>

#include "3D/ObjectMatrix.h"

using namespace openblack;
using namespace openblack::creature_locomotion;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr float k_HalfPi = k_Pi / 2.0f;
/// Walking speed is this many units a second for each unit of the size factor, running this many
constexpr float k_WalkPerSize = 8.0f;
constexpr float k_RunPerSize = 20.0f;
/// The size factor is size times this plus the offset
constexpr float k_SizeSlope = 0.875f;
constexpr float k_SizeOffset = 0.25f;
/// A creature going slower than this isn't moving its feet
constexpr float k_Still = 1e-4f;
/// The arrival ring never reaches past the destination by less than this, and is never thinner than this
constexpr float k_RingInside = 0.05f;
constexpr float k_RingThickness = 0.001f;
/// A walk's route may be this long, and twice the way there longer
constexpr float k_RouteLengthBase = 5000.0f;
constexpr float k_RouteLengthPerUnit = 2.0f;
/// The route follower's plan ends with no destination left to plan: the destination inside an obstacle, or a failed
/// plan
constexpr int32_t k_PlanEndInside = 0;
constexpr int32_t k_PlanEndFailed = 3;
/// A turn on the spot: a quarter turn and the float nearest 2 / pi, and how near the target a turn to face something
/// is over
constexpr float k_QuarterTurn = 1.57079637f;
constexpr float k_PerQuarterTurn = 0.636619747f;
constexpr float k_TurnOverWithin = 1e-4f;

float Loop(float time, float duration)
{
	if (duration <= 0.0f)
	{
		return 0.0f;
	}
	const auto looped = std::fmod(time, duration);
	return looped < 0.0f ? looped + duration : looped;
}

/// A float rounded to the nearest whole number, halves to even, as the processor rounds by default; one that is not a
/// number or out of range gives the processor's "indefinite" integer, the smallest
/// A step's move for one turn, as a speed: ten times its length
constexpr float k_StepSpeedPerMove = 10.0f;

int32_t NearestInt(float value)
{
	if (!(value >= -2147483648.0f && value < 2147483648.0f))
	{
		return std::numeric_limits<int32_t>::min();
	}
	return static_cast<int32_t>(std::nearbyint(value));
}
} // namespace

Speeds creature_locomotion::SpeedsFor(float size)
{
	const auto factor = (k_SizeSlope * size) + k_SizeOffset;
	return {.walk = k_WalkPerSize * factor, .run = k_RunPerSize * factor};
}

float creature_locomotion::TargetSpeed(float fraction, float runSpeed)
{
	return std::clamp(fraction, 0.0f, 1.0f) * runSpeed * k_TopSpeedMargin;
}

float creature_locomotion::RequiredFraction(float wanted, float slowFraction, float exhaustion)
{
	return exhaustion >= k_ExhaustedAt ? slowFraction : wanted;
}

float creature_locomotion::StopCap(float remaining)
{
	return std::max(std::sqrt(2.0f * k_Acceleration * std::max(remaining, 0.0f)), k_MinSpeed);
}

float creature_locomotion::CornerCap(float remaining, float cornerAllowance)
{
	return std::max(
	    std::sqrt((k_Acceleration * std::max(cornerAllowance, 0.0f)) + (2.0f * k_Acceleration * std::max(remaining, 0.0f))),
	    k_MinSpeed);
}

float creature_locomotion::SlopeFactor(float altitudeAhead, float altitude, float radius)
{
	if (radius <= 0.0f)
	{
		return 1.0f;
	}
	return std::clamp(1.0f - (k_SlopeSlowing * (altitudeAhead - altitude) / radius), k_MinSlopeFactor, k_MaxSlopeFactor);
}

float creature_locomotion::Accelerate(float speed, float target, float seconds)
{
	if (speed < target)
	{
		return std::min(target, speed + (k_Acceleration * seconds));
	}
	return speed;
}

std::optional<float> creature_locomotion::StepHeadingFollowingRoute(float heading, float lastRouteHeading, float routeHeading)
{
	const auto change = affine::WrapAngle(routeHeading - lastRouteHeading);
	if (std::abs(change) > k_CornerAngle)
	{
		return std::nullopt;
	}
	return affine::WrapAngle(heading + change);
}

bool creature_locomotion::MeetsCorner(float heading, float routeHeading)
{
	return std::abs(affine::WrapAngle(routeHeading - heading)) >= k_CornerAngle;
}

float creature_locomotion::WrapAngle(float angle)
{
	angle = std::fmod(angle + k_Pi, 2.0f * k_Pi);
	if (angle < 0.0f)
	{
		angle += 2.0f * k_Pi;
	}
	return angle - k_Pi;
}

float creature_locomotion::HeadingOf(glm::vec2 direction)
{
	return std::atan2(-direction.x, -direction.y);
}

float creature_locomotion::HeadingFromDirection(glm::vec2 direction)
{
	const auto yaw = static_cast<float>(affine::GetYAngle(glm::vec3(direction.x, 0.0f, direction.y)));
	return -yaw;
}

glm::vec2 creature_locomotion::DirectionOf(float heading)
{
	return {-std::sin(heading), -std::cos(heading)};
}

glm::vec2 creature_locomotion::RouteStartTarget(glm::vec2 position, float routeHeading, float nodeLength)
{
	// shorter than the shortest leg, or not a number, is the shortest leg
	const float length = !(nodeLength >= k_ShortestStartLeg) ? k_ShortestStartLeg : nodeLength;
	// (approximate) the original's sine and cosine are extended: in double they may differ in the last bit before
	// the product is rounded to a float
	const auto heading = static_cast<double>(routeHeading);
	const auto x = static_cast<float>(std::sin(heading) * static_cast<double>(length));
	const auto z = static_cast<float>(-(std::cos(heading) * static_cast<double>(length)));
	return {x + position.x, z + position.y};
}

float creature_locomotion::HeldTurnHeading(const glm::vec3& body, const glm::vec3& eye)
{
	// the dead zone is compared as a double
	constexpr double k_DeadZone = 0.02;
	const glm::vec3 way = eye - body;
	// the body's yaw: 0 (a positive zero) in the dead zone, else the way's
	const bool dead =
	    !(std::abs(static_cast<double>(way.x)) > k_DeadZone) && !(std::abs(static_cast<double>(way.z)) > k_DeadZone);
	const float yaw = dead ? 0.0f : static_cast<float>(affine::GetYAngle(way));
	// the heading is minus the yaw
	return -yaw;
}

Gait creature_locomotion::BlendGait(float speed, const Speeds& speeds, float distance, float scale, const Cycle& stand,
                                    const Cycle& walk, const Cycle& run, float walkTimeMs, float breathPhase)
{
	const auto standTime = stand.durationMs * breathPhase;
	if (speed <= k_Still || speeds.walk <= 0.0f)
	{
		return {.slots = {{{.animation = animations::k_Stand, .timeMs = standTime, .weight = 1.0f},
		                   {.animation = animations::k_Walk, .timeMs = walkTimeMs, .weight = 0.0f}}},
		        .walkTimeMs = walkTimeMs,
		        .walkAdvanceMs = 0.0f};
	}

	if (speed < speeds.walk)
	{
		const auto walkWeight = speed / speeds.walk;
		// The blend's stride shrinks with the walk's weight, so the feet don't slide
		const auto stride = walkWeight * walk.stride * scale;
		const auto advance = stride > 0.0f ? walk.durationMs * distance / stride : 0.0f;
		const auto time = Loop(walkTimeMs + advance, walk.durationMs);
		return {.slots = {{{.animation = animations::k_Stand, .timeMs = standTime, .weight = 1.0f - walkWeight},
		                   {.animation = animations::k_Walk, .timeMs = time, .weight = walkWeight}}},
		        .walkTimeMs = time,
		        .walkAdvanceMs = advance};
	}

	const auto range = speeds.run - speeds.walk;
	const auto runWeight = range > 0.0f ? std::clamp((speed - speeds.walk) / range, 0.0f, 1.0f) : 1.0f;
	const auto walkWeight = 1.0f - runWeight;
	const auto stride = ((walkWeight * walk.stride) + (runWeight * run.stride)) * scale;
	const auto advance = stride > 0.0f ? walk.durationMs * distance / stride : 0.0f;
	const auto time = Loop(walkTimeMs + advance, walk.durationMs);
	// The run keeps in step with the walk
	const auto runTime = walk.durationMs > 0.0f ? run.durationMs * time / walk.durationMs : 0.0f;
	return {.slots = {{{.animation = animations::k_Run, .timeMs = runTime, .weight = runWeight},
	                   {.animation = animations::k_Walk, .timeMs = time, .weight = walkWeight}}},
	        .walkTimeMs = time,
	        .walkAdvanceMs = advance};
}

float creature_locomotion::TrackTime(float fromMs, float advanceMs, float durationMs, bool looping, float share)
{
	const auto time = fromMs + (advanceMs * share);
	if (looping)
	{
		return durationMs > 0.0f ? std::fmod(time, durationMs) : 0.0f;
	}
	return std::clamp(time, 0.0f, std::max(durationMs - 1.0f, 0.0f));
}

Side creature_locomotion::SideOf(float angle)
{
	return angle > 0.0f ? Side::Right : Side::Left;
}

float creature_locomotion::WalkTimeAfterStep(float stepTimeMs, float stepDurationMs, float walkDurationMs, Side side)
{
	if (!(stepDurationMs > 0.0f))
	{
		return 0.0f;
	}
	const float through = stepTimeMs / stepDurationMs;
	const float time = side == Side::Right ? walkDurationMs * through : (through - 0.5f) * walkDurationMs;
	return std::trunc(time);
}

Pair creature_locomotion::PairFor(size_t first, float angle)
{
	const auto turn = std::min(std::abs(angle), k_Pi);
	if (turn < k_HalfPi)
	{
		return {.from = first, .to = first + 1, .weight = turn / k_HalfPi};
	}
	return {.from = first + 1, .to = first + 2, .weight = std::clamp((turn - k_HalfPi) / k_HalfPi, 0.0f, 1.0f)};
}

glm::vec3 creature_locomotion::BlendedMove(const StepMove& from, const StepMove& to, float weight)
{
	// Each part blended on its own, every step a float
	const auto blend = [weight](float a, float b) {
		const float difference = b - a;
		const float share = difference * weight;
		return share + a;
	};
	const glm::vec3 move {blend(from.move.x, to.move.x), blend(from.move.y, to.move.y), blend(from.move.z, to.move.z)};
	const float stride = blend(from.stride, to.stride);
	// A part that is not a number counts as none
	const auto some = [](float part) { return part < 0.0f || part > 0.0f; };
	if (!some(move.x) && !some(move.y) && !some(move.z))
	{
		return move;
	}
	// The length summed from the last part to the first
	const float zz = move.z * move.z;
	const float yy = move.y * move.y;
	const float xx = move.x * move.x;
	const float sum = zz + yy;
	const float squared = sum + xx;
	const float factor = stride / std::sqrt(squared);
	return {factor * move.x, factor * move.y, factor * move.z};
}

int32_t creature_locomotion::BlendedLength(int32_t fromMs, int32_t toMs, float weight)
{
	const float blended = (static_cast<float>(toMs - fromMs) * weight) + static_cast<float>(fromMs);
	return NearestInt(blended);
}

float creature_locomotion::StepFollowerSpeed(glm::vec3 stepMove, float scale, int32_t turnMs, int32_t durationMs, float yaw)
{
	// This turn's share of the move, its rise or fall left out
	const float scaled = static_cast<float>(turnMs) * scale;
	const float share = scaled / static_cast<float>(durationMs);
	const float x = share * stepMove.x;
	const float z = share * stepMove.z;
	// Turned by the yaw: the cosine stored as a float, the sine at full precision until each product is rounded
	const auto cosine = static_cast<float>(std::cos(static_cast<double>(yaw)));
	const double sine = std::sin(static_cast<double>(yaw));
	const float cosineX = cosine * x;
	const auto minusSineZ = static_cast<float>(-sine * static_cast<double>(z));
	const float turnedX = cosineX + minusSineZ;
	const auto sineX = static_cast<float>(sine * static_cast<double>(x));
	const float cosineZ = cosine * z;
	const float turnedZ = sineX + cosineZ;
	const float xx = turnedX * turnedX;
	const float zz = turnedZ * turnedZ;
	const float squared = xx + zz;
	return std::sqrt(squared) * k_StepSpeedPerMove;
}

float creature_locomotion::SteppingStopDistance(glm::vec3 stepMove, float scale, float alongNode)
{
	// The length summed from the last part to the first, as the step's blend measures it
	const float zz = stepMove.z * stepMove.z;
	const float yy = stepMove.y * stepMove.y;
	const float xx = stepMove.x * stepMove.x;
	const float sum = zz + yy;
	const float squared = sum + xx;
	const float scaled = std::sqrt(squared) * scale;
	return scaled - alongNode;
}

std::optional<SpinBlend> creature_locomotion::SpinsFor(float yaw, float targetYaw,
                                                       const std::function<std::optional<int32_t>(size_t)>& lengthMs)
{
	auto angle = affine::WrapAngle(targetYaw - yaw);
	const auto first = angle > 0.0f ? animations::k_LeftSpin : animations::k_RightSpin;
	if (first == animations::k_RightSpin)
	{
		angle = -angle;
	}
	const std::array lengths {lengthMs(first), lengthMs(first + 1), lengthMs(first + 2)};
	if (std::ranges::any_of(lengths, [](const auto& length) { return !length.has_value(); }))
	{
		return std::nullopt;
	}
	// A turn that is not a number counts as below a quarter turn
	const bool beyond = angle >= k_QuarterTurn;
	const auto from = beyond ? first + 1 : first;
	const auto weight = (beyond ? angle - k_QuarterTurn : angle) * k_PerQuarterTurn;
	const auto fromMs = *lengths.at(from - first);
	const auto toMs = *lengths.at(from - first + 1);
	return SpinBlend {.from = from, .to = from + 1, .weight = weight, .durationMs = BlendedLength(fromMs, toMs, weight)};
}

bool creature_locomotion::TurnIsOver(float yaw, float targetYaw)
{
	return !(std::abs(yaw - targetYaw) >= k_TurnOverWithin);
}

float creature_locomotion::SpinlessYaw(float yaw, float targetYaw, float timeMs, float standMs)
{
	const auto left = affine::WrapAngle(targetYaw - yaw);
	return affine::WrapAngle(((timeMs * left) / standMs) + yaw);
}

Start creature_locomotion::ChooseStart(float angle, float distanceInMesh, const StartOptions& options)
{
	const auto side = SideOf(angle);
	if (options.moving && options.stepStrides.has_value() &&
	    std::ranges::all_of(*options.stepStrides, [distanceInMesh](float stride) { return distanceInMesh > stride; }))
	{
		const auto first = side == Side::Right ? animations::k_RightStep : animations::k_LeftStep;
		return {.kind = Start::Kind::Step, .side = side, .animations = PairFor(first, angle)};
	}
	// Only a creature with its side's steps walks straight on when nearly facing the way; without them, and when only
	// turning to face, it always turns on the spot
	if (options.moving && options.stepStrides.has_value() && std::abs(angle) < k_WalkStraightAngle)
	{
		return {.kind = Start::Kind::Walk, .side = side, .animations = std::nullopt};
	}
	return {.kind = Start::Kind::Turn, .side = side, .animations = std::nullopt};
}

Ring creature_locomotion::MoveRing(float distance, float minDistance, float maxDistance)
{
	const auto min = std::max(std::min(minDistance, distance - k_RingInside), 0.0f);
	return {.min = min, .max = std::max(maxDistance, min + k_RingThickness)};
}

Ring creature_locomotion::ObjectRing(float creatureRadius, float objectRadius, float extra)
{
	const auto min = creatureRadius + objectRadius;
	return {.min = min, .max = min + std::max(extra, k_RingThickness)};
}

bool creature_locomotion::Arrived(float distance, const Ring& ring)
{
	return distance <= ring.max;
}

float creature_locomotion::MaxRouteLength(float distance)
{
	return (k_RouteLengthPerUnit * distance) + k_RouteLengthBase;
}

creature_locomotion::Planning creature_locomotion::PlanningOutcome(bool hasRoute, bool givenUp, int32_t planEnd)
{
	if (hasRoute)
	{
		return Planning::Ready;
	}
	if (givenUp || planEnd == k_PlanEndInside || planEnd == k_PlanEndFailed)
	{
		return Planning::Failed;
	}
	return Planning::Searching;
}

glm::vec2 creature_locomotion::RunAwayPoint(glm::vec2 creature, glm::vec2 threat, float distance)
{
	auto away = creature - threat;
	// Fleeing something right on top of it, it runs away along x
	away = glm::length(away) > 0.0f ? glm::normalize(away) : glm::vec2(1.0f, 0.0f);
	return creature + (away * distance);
}
