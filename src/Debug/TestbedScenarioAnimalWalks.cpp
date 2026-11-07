/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the grazers' walks round what stands in their way: a hungry cow going round a pillar of
// rock on its way to food behind it, and what it does when a smaller pillar standing in the big one is taken off the
// map while it goes round; and a hungry cow with no food near that looks for grass beside a pillar

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The cow, made facing east as every animal is, with the food behind it to the west-south-west: it sets off east,
/// turning a little towards the food at each map cell it enters, and so comes on the pillar's west side, passing north
/// of its middle, and goes round it clockwise, over its north and east sides
constexpr glm::vec2 k_Cow {-10.0f, 45.0f};
/// The pillars' middle, east of the cow and a little south of its way
constexpr glm::vec2 k_Pillar {2.0f, 41.0f};
/// The food pile, within the reach that draws a hungry grazer
constexpr glm::vec2 k_Food {-35.0f, 33.0f};
/// Seconds into the scenario that the small pillar is deleted: after the cow has come on the big one and started round
/// it, and before it reaches the big one's south side
constexpr float k_DeleteAfterSeconds = 14.0f;

/// The hungry cow, the big pillar and, in its middle at a quarter of its size, the small one (object 2): the small one's
/// circle lies within the big one's, so the two things of many cells overlap and the cow, going round the big one, never
/// meets the small one's circle
std::vector<ObjectSetup> CowAndPillars()
{
	return {{.type = AnimalInfo::Cow, .offset = k_Cow, .hungry = true},
	        {.type = FeatureInfo::FatPilarChalk, .offset = k_Pillar, .scale = 0.5f},
	        {.type = FeatureInfo::FatPilarChalk, .offset = k_Pillar, .scale = 0.125f}};
}

/// The grazing cow, with the pillar's middle 10 m south of it: the pillar stands in the map cell beside the cow that its
/// search for grass would take first, square to its right as it faces east
constexpr glm::vec2 k_GrazingCow {-40.0f, 45.0f};
constexpr glm::vec2 k_PillarBeside {-40.0f, 35.0f};
} // namespace

void testbed_scenarios::AddAnimalWalkScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	all.push_back({
	    .id = "animals.orbit_leaves_at_once",
	    .name = "Animals: a cow going round a pillar leaves at once when an overlapping thing goes",
	    .facet = Facet::Animals,
	    .description = "A hungry cow, made facing east, with a pile of food 23 m behind it to the west-south-west, and a fat "
	                   "pillar of rock 17 m east of it and 4 m south, with a pillar a quarter of its size standing in its "
	                   "middle. The food draws the cow at once; 14 seconds in, a script deletes the small pillar.",
	    .expected = "The cow sets off east, turning a little towards the food at each map cell, meets the big pillar's west "
	                "side and goes round it clockwise, over its north and east sides. As the small pillar leaves the map, "
	                "every animal going round a thing it overlaps is told to leave at its next chance, whatever its "
	                "distance to its goal: the cow leaves the big pillar as soon as the food is ahead of it and on its outer "
	                "side, at about the pillar's south point, heading south, where in animals.orbit_goes_round it carries on "
	                "round to the south-west side. In the Path Finding window (left click on the cow) its entry distance "
	                "reads 'leave at once' from the deletion until it leaves, then 0.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Food}},
	    .objects = CowAndPillars(),
	    .commands = {{.kind = Kind::ScriptDelete, .delaySeconds = k_DeleteAfterSeconds, .object = 2}},
	    .fixtures = {.piles = {{.type = PotInfo::FoodPile, .at = k_Food}}},
	});

	all.push_back({
	    .id = "animals.orbit_goes_round",
	    .name = "Animals: a cow goes round a pillar to its far side",
	    .facet = Facet::Animals,
	    .description = "As animals.orbit_leaves_at_once, but nothing is deleted: a hungry cow, the food behind it and the "
	                   "two pillars in its way.",
	    .expected = "The cow goes round the big pillar clockwise until it is nearer the food than where it started round, "
	                "with the food ahead of it and on its outer side: it passes the pillar's south point and leaves on its "
	                "south-west side, heading south-west. In the Path Finding window its entry distance is its distance to "
	                "the food as it started round, in 128ths of a metre, and stays so after it leaves.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Food}},
	    .objects = CowAndPillars(),
	    .fixtures = {.piles = {{.type = PotInfo::FoodPile, .at = k_Food}}},
	});

	all.push_back({
	    .id = "animals.graze_beside_a_pillar",
	    .name = "Animals: a hungry cow looks for grass beside a pillar, never in its cells",
	    .facet = Facet::Animals,
	    .description = "A hungry cow, made facing east, with no food anywhere near, and a fat pillar of rock whose middle is "
	                   "10 m south of it, in the map cell to the cow's right that its search for grass would take first.",
	    .expected = "The cow looks for grass in a spiral of map cells round its own, ahead of it or square to its side. It "
	                "passes over every cell the pillar stands in and walks to the first cell clear of it, to its east or "
	                "south-east, where it lowers its head and eats. It never sets off into the pillar's cells to graze "
	                "against the rock.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .objects = {{.type = AnimalInfo::Cow, .offset = k_GrazingCow, .hungry = true},
	                {.type = FeatureInfo::FatPilarChalk, .offset = k_PillarBeside, .scale = 0.5f}},
	});
}
