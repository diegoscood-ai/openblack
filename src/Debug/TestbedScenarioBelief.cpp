/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a town's belief in the player: the believers it gained float up from the land under the
// cursor

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddBeliefScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	constexpr glm::vec2 k_Centre {0.0f, 40.0f};
	// The scenario's town is a Celtic town of the player's, believing a little in the player as a land's script makes
	// its player's town believe, well under the cap of 10 so that what it gains shows. The camera looks north at the
	// town centre from 25 m south of it; the pointer stands on the land in front of the centre, then to its left
	all.push_back({
	    .id = "objects.believers_gained",
	    .name = "The believers a town gained float up from the land",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre of the player's town, which has one villager and believes 1 in the player. The "
	                   "pointer goes to the land in front of the centre and the town gains 0.025 of belief in the player, "
	                   "as a gift to its store gives it; six seconds later the pointer goes to the land left of the centre "
	                   "and the town gains 0.25.",
	    .expected = "Within a second of each gain the tooltip says how many believers were gained (25, then 250), and the "
	                "same count, in yellow over a black shadow one pixel left and down, floats up from the land under the "
	                "pointer: its text ends at the point, it rises 10 m a second, fades in its last second and is gone "
	                "after 5 seconds. The second number rises from the second point, not following the pointer.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 15.0f}, .look = {0.0f, 7.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.8f}},
	                 {.kind = Kind::TownGainsBelief, .delaySeconds = 1.0f, .amount = 0.025f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.25f, 0.75f}},
	                 {.kind = Kind::TownGainsBelief, .delaySeconds = 0.5f, .amount = 0.25f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.75f, 0.85f}}},
	    .townBelief = 1.0f,
	});
}
