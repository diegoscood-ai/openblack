/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of the hand knocking on a house at night, with its people asleep inside (raffclar's, with the
// knock given as the player's click on the house)

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddKnockScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	constexpr glm::vec2 k_House {0.0f, 30.0f};
	// Without a storage pit the town has no food or wood, and wants them as much as sleep: the first of its desires is
	// then food, and nobody goes home for the night. A villager made stays in its first state for hundreds of turns
	// before it decides anything, so each takes a step a second in and then decides afresh
	// The camera looks north at the hut from 15 m south of it, at a point 1.5 m above the ground in the hut's middle, so
	// the pointer in the middle of the screen is on the hut. A short click of the Action (right) button there is a tap
	all.push_back({
	    .id = "villagers.knock_at_night",
	    .name = "Knocking on a house at night",
	    .facet = Facet::Hand,
	    .description = "A Celtic hut of the player's town at night with two grown villagers of the town beside it, and the "
	                   "town's storage pit with food and wood east of it. A second in, each villager takes a step and decides "
	                   "afresh. Once they have gone in to sleep, the pointer goes to "
	                   "the hut and the Action (right) button is clicked on it, twice, three seconds apart; then the "
	                   "pointer moves away.",
	    .expected = "At night the town, which has food and wood in store, wants sleep most, so they go home into the hut, "
	                "where they are not drawn. Each click is a tap: the knock reaches the hut with the next turn, plays a "
	                "knocking sound at the hand, and the hand plays its knock on the roof once where it is. A row of little "
	                "people grows in over the hut for a second, stays six and shrinks away over the last, a gold one for "
	                "each adult living there and a green one for each free place. Everyone inside comes out a few steps "
	                "from the door, yawns and decides what to do; they don't go back to bed for that decision. The second "
	                "knock plays the next of the nine knocking sounds and holds the row again.",
	    .environment = {.hour = 23.0f, .dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 15.0f}, .look = {0.0f, 1.5f, 30.0f}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_House, .yawDegrees = 180.0f},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_House + glm::vec2 {-6.0f, -8.0f},
	                 .walkTo = k_House + glm::vec2 {-6.0f, -9.0f},
	                 .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_House + glm::vec2 {6.0f, -8.0f},
	                 .walkTo = k_House + glm::vec2 {6.0f, -9.0f},
	                 .joinTown = true},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_House + glm::vec2 {35.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 17.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.9f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.15f, 0.85f}}},
	});
}
