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
/// A walk is given this long and this much longer per unit of distance before it is given up on
constexpr float k_TimeLimitBaseMs = 5000.0f;
constexpr float k_TimeLimitPerUnitMs = 2.0f;
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

float creature_locomotion::LeashFraction(float walkFraction, float runFraction, float pull)
{
	return walkFraction + (pull * ((2.0f * runFraction) - walkFraction));
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

glm::vec2 creature_locomotion::DirectionOf(float heading)
{
	return {-std::sin(heading), -std::cos(heading)};
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

Side creature_locomotion::SideOf(float angle)
{
	return angle > 0.0f ? Side::Right : Side::Left;
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
	// The blend's length, worked out in floats and rounded to the nearest
	const auto blended = (static_cast<float>(toMs - fromMs) * weight) + static_cast<float>(fromMs);
	return SpinBlend {.from = from, .to = from + 1, .weight = weight, .durationMs = NearestInt(blended)};
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
	if (std::abs(angle) < k_WalkStraightAngle)
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

float creature_locomotion::TimeLimitMs(float distance)
{
	return (k_TimeLimitPerUnitMs * distance) + k_TimeLimitBaseMs;
}

glm::vec2 creature_locomotion::RunAwayPoint(glm::vec2 creature, glm::vec2 threat, float distance)
{
	auto away = creature - threat;
	// Fleeing something right on top of it, it runs away along x
	away = glm::length(away) > 0.0f ? glm::normalize(away) : glm::vec2(1.0f, 0.0f);
	return creature + (away * distance);
}
