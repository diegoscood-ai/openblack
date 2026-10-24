/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureBringHome.h"

#include <algorithm>

#include <glm/vec3.hpp>

#include "Creature/CreatureEating.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_bring_home;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;

namespace
{
/// Picking the thing up, looking at it on the way
Step PickUp(uint32_t thing)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing},
	        .gaze = Gaze {.object = thing}};
}

/// Walking home to within a distance, with the look of a walk there
Step WalkHome(glm::vec2 home, float within)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = home, .maxDistance = within},
	        .gaze = Gaze {.point = glm::vec3 {home.x, 0.0f, home.y}, .goingTowards = true}};
}

/// Letting go of what it holds with the put-down clip
Step PutDown()
{
	return {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}};
}
} // namespace

bool creature_bring_home::CanBeBroughtHome(float metresFromHome, bool living, bool canPickUp)
{
	return metresFromHome > k_AwayFromHome && !living && canPickUp;
}

bool creature_bring_home::HasFoodInIt(float growth, float food)
{
	return growth > 0.0f && food > 0.0f;
}

float creature_bring_home::BringHomeWithin(float height)
{
	return std::min(k_NearestHome, height);
}

std::vector<Step> creature_bring_home::BringHome(std::optional<uint32_t> pickUp, glm::vec2 home, float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back(WalkHome(home, BringHomeWithin(height)));
	agenda.push_back(PutDown());
	return agenda;
}

std::vector<Step> creature_bring_home::BringFoodHome(std::optional<uint32_t> pickUp, glm::vec2 home, float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back(WalkHome(home, height));
	agenda.push_back(PutDown());
	return agenda;
}

std::vector<Step> creature_bring_home::TakeFoodFromFieldHome(const std::optional<Field>& take, glm::vec2 home, float height)
{
	std::vector<Step> agenda;
	if (take.has_value())
	{
		// The creature does not look at a special thing as it walks to the field
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::ToPoint, .point = take->point, .maxDistance = height}});
		agenda.push_back({.kind = Step::Kind::Move,
		                  .seconds = creature_eating::k_FaceSourceSeconds,
		                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = take->field}});
		agenda.push_back({.kind = Step::Kind::ClearObjectToActOn});
		agenda.push_back(
		    {.kind = Step::Kind::Object,
		     .order = {.kind = ObjectOrder::Kind::PickUpPortion, .object = take->field, .portion = PotInfo::WheatInHand}});
	}
	agenda.push_back(WalkHome(home, height));
	agenda.push_back(PutDown());
	return agenda;
}
