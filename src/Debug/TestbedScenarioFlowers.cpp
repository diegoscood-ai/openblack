/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of the land script's flower patches: both kinds side by side on the lake's bank by day, made
// through the script's flowers command

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The middle of the lake's near bank, which falls northwards to the water at the same slope all the way down: 10
/// units south of here the plane's edge, 10 north the bank's foot
constexpr float k_Bank = 130.0f;
/// The patches stand this far west and east of the lake's middle line
constexpr float k_Apart = 6.0f;
/// The camera stands over the shallows to the north east of the patches and looks south, down onto the bank's face
constexpr glm::vec3 k_Eye {10.0f, 22.0f, 160.0f};
constexpr glm::vec3 k_Look {0.0f, 0.5f, k_Bank};
} // namespace

void testbed_scenarios::AddFlowersScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "sky.flowers_on_bank",
	    .name = "Flower patches on a bank by day",
	    .facet = Facet::Sky,
	    .description = "A patch of white flowers (west) and a patch of pink flowers (east) side by side on the lake's near "
	                   "bank, where it falls to the water, made as a land script's flowers command makes them: unturned "
	                   "and at full size. It is midday and the clock stands still; the camera looks south from over the "
	                   "shallows at the bank's face.",
	    .expected = "Each patch lies moulded to the bank, tilted with it, with none of it floating above or sunk into the "
	                "ground. The patches are not shaded by the sun: they are lit as the land under them is, the same on "
	                "every side whichever way their faces turn. Neither casts a shadow on the land, and no shadow falls on "
	                "them.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = k_Eye, .look = k_Look},
	    .fixtures = {.flowers = {{.type = FlowersInfo::WhiteOne, .at = glm::vec2 {-k_Apart, k_Bank}},
	                             {.type = FlowersInfo::Pink, .at = glm::vec2 {k_Apart, k_Bank}}}},
	});
}
