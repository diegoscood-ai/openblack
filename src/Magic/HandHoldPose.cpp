/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandHoldPose.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/ObjectMatrix.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::hand_hold;

namespace
{
/// A vector of unit length, or of none when it has none
glm::vec3 UnitOrNone(glm::vec3 v)
{
	const float length = glm::length(v);
	return length > 0.0f ? v / length : glm::vec3(0.0f);
}

/// A vector made unit length, left as it is when it is zero
glm::vec3 UnitUnlessZero(glm::vec3 v)
{
	if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f)
	{
		v *= 1.0f / std::sqrt((v.x * v.x + v.y * v.y) + v.z * v.z);
	}
	return v;
}

/// A cursor lag held to the sway's reach
float ClampLag(float v)
{
	return v > -k_SwayLag ? (v < k_SwayLag ? v : k_SwayLag) : -k_SwayLag;
}
} // namespace

HoldType hand_hold::HoldTypeOf(bool ready, HoldType recorded)
{
	return ready ? recorded : HoldType::Magic;
}

float hand_hold::HoldTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize)
{
	switch (hold)
	{
	case HoldType::None:
		return 0.0f;
	case HoldType::Above:
	{
		const float grip = std::min(1.0f, reach / (k_StandardHandHeight * handSize * k_AboveReachShare));
		return static_cast<float>(durationMs) * 0.5f * (1.0f - grip);
	}
	case HoldType::Magic:
		return static_cast<float>(durationMs >> 1);
	case HoldType::Grain:
	case HoldType::Fingers:
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
		break;
	}
	const float grip = std::min(1.0f, reach / (k_StandardHandHeight * handSize));
	return static_cast<float>(durationMs >> 1) * grip;
}

float hand_hold::SeedHang([[maybe_unused]] HoldType hold, float lowering, float height)
{
	return lowering * height;
}

float hand_hold::HoldLift(HoldType hold, float hang, float handSize)
{
	return hold == HoldType::Above   ? k_AboveLift
	       : hold == HoldType::Magic ? k_StandardHandHeight * handSize
	                                 : std::max(hang, k_MinimumSideLift);
}

glm::vec2 hand_hold::CursorSway(glm::vec2 cursorLag)
{
	return {(k_MaxSway / k_SwayLag) * ClampLag(cursorLag.x), (k_MaxSway / k_SwayLag) * ClampLag(cursorLag.y)};
}

glm::vec3 hand_hold::HeldUp(glm::vec3 camera, glm::vec3 hand, float roll, float pitch)
{
	// Straight up turned about the line to the camera, then about the level line across it
	const auto toCamera = UnitUnlessZero(camera - hand);
	const glm::mat3 rolled = affine::AxisAngle(toCamera, roll);
	const auto across = UnitUnlessZero(glm::vec3(0.0f - toCamera.z, 0.0f, toCamera.x - 0.0f));
	return (affine::AxisAngle(across, pitch) * rolled) * glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::mat3 hand_hold::HeldBasis(glm::vec3 facing, glm::vec3 up)
{
	const auto side = UnitUnlessZero(glm::cross(facing, up));
	return {side, up, glm::cross(side, up)};
}

glm::mat3 hand_hold::SeedTurn(const glm::mat3& basis, float yRotate, bool rightHanded)
{
	auto x = basis[0];
	auto z = basis[2];
	if (rightHanded)
	{
		x = -x;
		z = -z;
	}
	const float c = std::cos(yRotate);
	const float s = std::sin(yRotate);
	const auto turnedX = x * c + z * s;
	const auto turnedZ = z * c - x * s;
	return {UnitOrNone(turnedX), UnitOrNone(basis[1]), UnitOrNone(turnedZ)};
}

glm::vec3 hand_hold::HangBelow(glm::vec3 hand, glm::vec3 up, float hang)
{
	return hand - up * hang;
}

void hand_hold::StepHoldingSpring(HoldingSpring& spring, glm::vec3 required, uint32_t clockMs)
{
	do
	{
		spring.stepMs += k_SpringStepMs;
		const auto d = required - spring.position;
		spring.velocity += (d * k_SpringStiffness - spring.velocity * k_SpringDamping) * k_SpringStepSeconds;
		const float speed = glm::length(spring.velocity);
		if (speed > k_MaxHandSpeed)
		{
			spring.velocity *= k_MaxHandSpeed / speed;
		}
		spring.position += spring.velocity * k_SpringStepSeconds;
	} while (spring.stepMs < clockMs);
}

HeldPlace hand_hold::ReleasePlace(const HeldPlace& turnPlace, const std::optional<HeldPlace>& drawn)
{
	return drawn.value_or(turnPlace);
}
