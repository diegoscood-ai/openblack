/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandFeel.h"

#include <glm/geometric.hpp>

#include "ECS/HandMeshRay.h"

using namespace openblack;

namespace
{
/// How much of the way from the face towards the camera the hand leans, and how much it stands upwards
constexpr float k_TowardsCamera = 0.25f;
constexpr float k_Upwards = 0.5f;
/// Holding something, the hand is kept out from the face by this share of the held thing's radius
constexpr float k_HeldShare = 0.5f;
} // namespace

hand_feel::Feel hand_feel::FeelOf(const Picked& picked)
{
	if (picked.ignored)
	{
		return Feel::Nothing;
	}
	if (picked.living)
	{
		return picked.creature ? Feel::OnModel : Feel::AtPosition;
	}
	return picked.posedModel && !picked.animatedStatic ? Feel::AtPosition : Feel::OnModel;
}

glm::vec3 hand_feel::FeelDirection(glm::vec3 camera, glm::vec3 cursorDirection, std::optional<glm::vec3> cursorGround,
                                   float raise)
{
	if (!cursorGround.has_value())
	{
		return cursorDirection;
	}
	return glm::normalize(*cursorGround - camera + glm::vec3(0.0f, raise, 0.0f));
}

hand_feel::Rest hand_feel::RestOnModel(glm::vec3 camera, glm::vec3 cursorDirection, glm::vec3 feltDirection, glm::vec3 hitPoint,
                                       glm::vec3 hitNormal, bool feelsModel, std::optional<Holding> holding,
                                       std::optional<float> measuredAlong)
{
	// How far along the felt direction the face is, summed in the original's order
	const float along =
	    measuredAlong.has_value() ? *measuredAlong : ecs::hand_mesh_ray::AlongRay(camera, feltDirection, hitPoint);
	// Holding something, the point is put on the line of sight at the same distance
	auto point = camera + (holding.has_value() ? cursorDirection : feltDirection) * along;
	if (!measuredAlong.has_value() && !holding.has_value())
	{
		point = hitPoint;
	}
	if (along < 1.0f)
	{
		// Kept as openblack has it: a unit along the felt direction, where the original takes the line of sight
		point = camera + feltDirection;
	}
	if (holding.has_value() && holding->held)
	{
		float keptOut = holding->radius * k_HeldShare;
		if (holding->creature.has_value())
		{
			const float across = glm::length(glm::vec2(point.x, point.z) - holding->creature->centre);
			if (across < holding->creature->radius)
			{
				keptOut += holding->creature->radius - across;
			}
		}
		point += glm::normalize(camera - point) * keptOut;
	}
	if (!feelsModel)
	{
		return {.point = point, .up = std::nullopt};
	}
	// Kept as openblack has it: the way to the camera is taken from where the hand ends up, and the face's normal as the
	// mesh test gives it
	const auto towardsCamera = camera - point;
	const auto toCamera = glm::length(towardsCamera) > 0.0f ? glm::normalize(towardsCamera) : -feltDirection;
	return {.point = point, .up = toCamera * k_TowardsCamera - hitNormal + glm::vec3(0.0f, k_Upwards, 0.0f)};
}

glm::vec3 hand_feel::RestAtPosition(glm::vec3 camera, glm::vec3 cursorDirection, [[maybe_unused]] glm::vec3 feltDirection,
                                    glm::vec3 position, float radius)
{
	const float distance = glm::distance(position, camera) - radius;
	const auto first = camera + cursorDirection * distance;
	// Kept as openblack has it: the line of sight's rise, where the original takes the felt direction's
	return camera + cursorDirection * ((first.y - position.y) * cursorDirection.y + distance);
}

bool hand_feel::TurnsToLand(bool restsOnFace, bool overThing, float aboveLand, float handHeight)
{
	return !restsOnFace || (overThing && aboveLand < handHeight * 0.5f);
}
