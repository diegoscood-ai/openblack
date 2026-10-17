/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingRules.h"

#include <cmath>

#include <glm/gtc/constants.hpp>

#include "3D/ObjectMatrix.h"
#include "Physics/Body.h"

namespace openblack::physics::living
{

glm::mat3 OriginalRows(const glm::mat3& body)
{
	// drawn = M x K (the drawn pi / 2, see the header): M's right = -drawn[2], up = drawn[1], fwd = drawn[0] (exact). Only
	// for a drawn matrix now (the bodies have the original's rows)
	return glm::mat3(-body[2], body[1], body[0]);
}

glm::mat3 BodyRows(const glm::mat3& body, [[maybe_unused]] bool fromHand)
{
	// both paths build the body with the original's rows: as they are
	return body;
}

VillagerLandingPose VillagerLandingPoseOf(const glm::mat3& turnStartRows)
{
	const float a = turnStartRows[0].y; // the turn-start right.y
	// float(pi) added to the double, then one float rounding of the sum
	const auto plusPi = [](double angle) { return static_cast<float>(angle + static_cast<double>(glm::pi<float>())); };
	if (!(a >= -0.5f)) // less or NaN
	{
		// GetYAngle(up) as it is: no pi, no wrap
		return {1, static_cast<float>(affine::GetYAngle(turnStartRows[1])), k_DesireAnger, 0.5f};
	}
	if (a > 0.5f) // strictly greater
	{
		return {2, affine::WrapAngle(plusPi(affine::GetYAngle(turnStartRows[1]))), k_DesireAnger, 0.5f};
	}
	return {0, affine::WrapAngle(plusPi(affine::GetYAngle(turnStartRows[2]))), k_DesireCompassion, 0.1f};
}

uint16_t AnimalLandType(float rightY)
{
	if (rightY > 0.5f) // strictly greater
	{
		return 1;
	}
	if (!(rightY >= -0.5f)) // less or NaN
	{
		return 2;
	}
	return 0;
}

float AnimalLandingYaw(const glm::vec3& currentFwdRow)
{
	// one float rounding of GetYAngle + pi, then the wrap
	return affine::WrapAngle(static_cast<float>(affine::GetYAngle(currentFwdRow) + static_cast<double>(glm::pi<float>())));
}

float YawFromRotation(const glm::mat3& rows)
{
	// m6 = fwd.x, m8 = fwd.z: atan2(-fwd.x, fwd.z), stored as a float
	return static_cast<float>(affine::ArcTanOctant(rows[2].z, -rows[2].x));
}

glm::mat3 DrawnRotation(float yAngle)
{
	return affine::AngleY(yAngle + glm::half_pi<float>());
}

int InjuredChange(float before, float after)
{
	// both tests are strict and against the float mark: a life of exactly the mark is neither above nor below it
	if (before > k_InjuredLife && after < k_InjuredLife)
	{
		return 1;
	}
	if (before < k_InjuredLife && after > k_InjuredLife)
	{
		return -1;
	}
	return 0;
}

std::optional<float> LivingCrush(float knock)
{
	// strict, and a knock that compares with nothing hurts nothing; the share is worked out in floats
	if (!(knock > k_HurtingKnock))
	{
		return std::nullopt;
	}
	return (knock - k_HurtingKnock) * k_CrushPerKnock;
}

FlyingObjectResponse RespondToFlyingObject(float distance, float speed)
{
	return distance < k_RunSeconds * speed ? FlyingObjectResponse::Run : FlyingObjectResponse::Point;
}

float FlyingSpeed(glm::vec3 velocity)
{
	return std::sqrt(((velocity.z * velocity.z) + (velocity.y * velocity.y)) + (velocity.x * velocity.x));
}

bool ShelterSeesFlyer(float distance, float radius)
{
	return radius > distance;
}

bool IsActuallyInTheAir(float speed, float centreHeight, float landHeight, float radius)
{
	return speed > k_AirborneSpeed || (centreHeight - landHeight) + radius > k_AirborneHeight;
}

float CreatureMass(float scale, float thinFat, float weakStrong)
{
	constexpr float k_SizeToLength = 8.333334f;
	constexpr float k_BuildWeight = 0.15f;
	constexpr float k_MassScale = 100.0f;
	const float length = scale * k_SizeToLength;
	// in the game's order: the build, then the scale, then the length three times
	return ((weakStrong + thinFat) * k_BuildWeight + 1.0f) * k_MassScale * length * length * length;
}

std::optional<float> CreatureCrush(float impact, float creatureMass)
{
	const float ratio = impact / (creatureMass * k_Gravity);
	// at most the cap, and the cap for a knock that compares with nothing
	const float knock = ratio < k_CreatureKnockCap ? ratio : k_CreatureKnockCap;
	const float crush = knock * k_CreatureCrushPerKnock;
	if (!(crush > k_CreatureLeastCrush))
	{
		return std::nullopt;
	}
	return crush;
}

float CarriedShare(float carried, float creatureWeight)
{
	// in the game's order: the creature's weight against nothing, then the share against nothing and against 1
	if (!(creatureWeight < 0.0f || creatureWeight > 0.0f))
	{
		return 0.0f;
	}
	const float share = carried / creatureWeight;
	if (!(share >= 0.0f))
	{
		return 0.0f;
	}
	return share > 1.0f ? 1.0f : share;
}

} // namespace openblack::physics::living
