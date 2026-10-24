/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the hand letting go of a miracle's seed as it casts it or a dispenser takes it: the hand
// opens, as after any throw

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddSeedThrowScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "hand.seed_cast_opens",
	    .name = "The hand opens as it casts a forest's seed",
	    .facet = Facet::Hand,
	    .description = "A forest's seed from a bubble is put in a hand 8 units over the grass north of the middle, held "
	                   "for two seconds and cast there; again every twelve. The camera looks down at the hand from "
	                   "close by.",
	    .expected = "While the seed is held the hand is closed round it. As the forest is cast the seed leaves the hand "
	                "and falls to the land with its trail, and at once the hand opens: it goes back to its empty "
	                "pose, the fingers spread and wiggling, blending into it over about an eighth of a second, as it "
	                "does after throwing anything. It stays open while the forest grows, though the seed lives on "
	                "with the forest.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 20.0f}, {10.0f, 40.0f}}, .distance = 0.45f},
	    .miracles = {{.type = MagicType::Forest,
	                  .point = {0.0f, 30.0f},
	                  .handOffset = {0.0f, 30.0f},
	                  .handHeight = 8.0f,
	                  .delaySeconds = 1.0f,
	                  .repeatSeconds = 12.0f,
	                  .byHand = true,
	                  .holdBeforePress = 2.0f}},
	});
	all.push_back({
	    .id = "hand.seed_to_dispenser_opens",
	    .name = "The hand opens as a dispenser takes its seed",
	    .facet = Facet::Hand,
	    .description = "A heal dispenser stands on the grass north of the middle. A forest's seed from a bubble is put in a "
	                   "hand 8 units over it, held for two seconds and let go over the dispenser; again every twelve. The "
	                   "camera looks down at the hand from close by.",
	    .expected = "While the seed is held the hand is closed round it. As it is let go over the dispenser, the dispenser "
	                "takes it: the seed is gone from the hand, a forest bubble floats at the dispenser with its flash, and "
	                "at once the hand opens: it goes back to its empty pose, the fingers spread and wiggling, blending "
	                "into it over about an eighth of a second, as it does after throwing anything.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 20.0f}, {10.0f, 40.0f}}, .distance = 0.45f},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 30.0f}}},
	    .miracles = {{.type = MagicType::Forest,
	                  .point = {0.0f, 30.0f},
	                  .handOffset = {0.0f, 30.0f},
	                  .handHeight = 8.0f,
	                  .delaySeconds = 1.0f,
	                  .repeatSeconds = 12.0f,
	                  .byHand = true,
	                  .holdBeforePress = 2.0f}},
	});
}
