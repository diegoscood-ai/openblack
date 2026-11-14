/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a scaffold founding a town: a scaffold of five set building away from every town, as a
// script's SET_ACTIVE sets it

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The village the scaffold comes from, and where the scaffold stands, 120 units east of it: further from every town
/// than a scaffold looks for one to build for
constexpr glm::vec2 k_Village {-70.0f, 30.0f};
constexpr glm::vec2 k_Scaffold {50.0f, 30.0f};
/// A small wood 40 units north east of the scaffold, well within a town's reach for its forests
constexpr glm::vec2 k_Wood {80.0f, 55.0f};
} // namespace

void testbed_scenarios::AddScaffoldScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "building.scaffold_founds_town",
	    .name = "A scaffold of five founds a town",
	    .facet = Facet::Objects,
	    .description = "A village of the player's stands west of the middle, and a small wood of five beeches north east "
	                   "of the middle. A second after the start a scaffold standing for five scaffolds, of the village, is "
	                   "made 120 units east of it, far from every town, and a script's SET_ACTIVE sets it building at "
	                   "once where it stands.",
	    .expected = "A new town is founded where the scaffold stands: a village centre is laid out there, being built, "
	                "with the scaffold on its site, and a pile of wood appears beside it, the new town's temporary wood "
	                "store. The log's line for the scaffold says it built a TOWN_CENTRE for the new town, the new town's "
	                "number and its owner (the neutral player: a script's SET_ACTIVE names no player), that it took 1 "
	                "forest, the wood's, measured from its wood store, and where the wood store stands.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-100.0f, 0.0f}, {100.0f, 80.0f}}},
	    .commands = {{.kind = Command::Kind::ScaffoldSetActive, .delaySeconds = 1.0f, .point = k_Scaffold, .value = 5}},
	    .fixtures = {.villages = {{.at = k_Village, .huts = 2, .villagers = 2}},
	                 .trees = {{.type = TreeInfo::Beech, .at = k_Wood, .count = 5, .spread = 12.0f}}},
	});
}
