/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a tutorial hand demo's cursor: the recorded mouse on the picture between the cinema bars

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddHandDemoScenarios(std::vector<Scenario>& all)
{
	namespace fixtures = testbed_fixtures;
	// The demo starts through the game's own playback, which turns the cinema bars on and snaps them in; from then on it
	// sets the camera from its records, so the testbed's shot only stands until it starts
	all.push_back({
	    .id = "hand.demo_cursor_between_bars",
	    .name = "A tutorial hand demo's cursor between the cinema bars",
	    .facet = Facet::Hand,
	    .description = "The tutorial's recorded demo drag plays on the empty testbed from the start: it brings the cinema "
	                   "bars in, moves the camera as it was recorded, and moves the hand, grips the land and drags it. The "
	                   "Hand menu's switch \"Hand demo: whole-screen cursor (old)\" shows the old mapping for comparison.",
	    .expected = "The bars snap in at once, top and bottom. The hand follows the recorded path inside the letterboxed "
	                "picture, closer to its middle than the whole screen would put it, and grips and drags the land three "
	                "times where the recording did. With the switch on, the hand moves over the whole screen and so sits "
	                "away from those spots, by up to a bar's height near the top and the bottom.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .fixtures = {.handDemo = fixtures::HandDemo {.name = "drag"}},
	});
}
