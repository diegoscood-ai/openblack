/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of what the player's hand keeps of their influence once it reaches past the border

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddVirtualInfluenceScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	// The player's temple stands in the middle, built whole, so its border shows. The camera stands high south of it
	// looking north past it, the pointer starting low on the screen, near the temple. No land script runs, so what the
	// hand keeps past the border is on, at full strength, as a game starts it
	all.push_back({
	    .id = "hand.virtual_influence",
	    .name = "The hand reaching past the player's border",
	    .facet = Facet::Hand,
	    .description = "The player's temple, built, in the middle of the land, with the camera high to its south looking "
	                   "north past it. The pointer rests near the temple for three seconds, inside the player's "
	                   "influence, then sweeps up the screen over twelve seconds, out past the border's curtain towards "
	                   "the horizon, and rests there.",
	    .expected = "Near the temple the hand is in the player's influence. Past the border it still counts as in it for a "
	                "while: the land within ten metres of the hand, shrinking with the strength, is the player's, and the "
	                "hand keeps its in-influence look. Each turn out the strength drops, the more the further the hand is "
	                "from where it last was inside and the longer it has been out; once under a tenth it is gone at once "
	                "and the hand is out of the influence. The game log, at debug level, gives the strength each time it "
	                "passes a tenth, 'The hand is out of its player's influence at (x, z): strength ...', lower each time "
	                "and last 0.00.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.68f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 120.0f, -160.0f}, .look = {0.0f, 0.0f, 120.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.68f}},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 3.0f, .point = {0.0f, -0.33f}, .amount = 12.0f}},
	    .fixtures = {.temples = {{.at = glm::vec2 {0.0f, 0.0f}}}},
	});
}
