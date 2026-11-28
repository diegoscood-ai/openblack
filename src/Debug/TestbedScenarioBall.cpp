/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of the football as a town's plaything: footballs made as the script's CREATE makes them, each
// given to the town nearest it

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The two villages, 120 units apart east to west
constexpr glm::vec2 k_West {-60.0f, 40.0f};
constexpr glm::vec2 k_East {60.0f, 40.0f};
} // namespace

void testbed_scenarios::AddBallScenarios(std::vector<Scenario>& all)
{
	// The balls are made in order: the first and the second nearest the west village, the third nearest the east one
	all.push_back({
	    .id = "objects.ball_nearest_town",
	    .name = "Footballs given to the nearest town",
	    .facet = Facet::Objects,
	    .description = "Two villages of the player's stand 120 units apart, west and east. Three footballs are made one "
	                   "after the other as a script's CREATE makes them: the first 30 units south of the west village, "
	                   "the second 35 units north of it, the third 15 units east of the middle and 30 south, 81 units "
	                   "from the west village and 54 from the east one.",
	    .expected = "The log has a line for each ball. The west village's town takes the first ball. The second is nearest "
	                "the west village too, which keeps the first and does not take it; no other town takes it either. "
	                "The east village's town takes the third, the nearest town however far the other is. The three balls "
	                "lie on the land where they were made.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-90.0f, 0.0f}, {90.0f, 80.0f}}},
	    .fixtures = {.villages = {{.at = k_West, .huts = 2, .villagers = 2}, {.at = k_East, .huts = 2, .villagers = 2}},
	                 .balls = {{.at = k_West + glm::vec2(0.0f, -30.0f)},
	                           {.at = k_West + glm::vec2(0.0f, 35.0f)},
	                           {.at = glm::vec2(15.0f, 10.0f)}}},
	});
}
