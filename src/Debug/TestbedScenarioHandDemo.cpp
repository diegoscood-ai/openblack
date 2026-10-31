/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the tutorial's hand demos: the recorded mouse on the picture between the cinema bars, and a
// recorded click landing on the thing of its kind nearest where it was recorded

#include <vector>

#include <glm/vec2.hpp>

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the wood lesson's recording takes wood from the storage pit's wood pile, in metres on the map, and the kind of
/// pile it was: the scripts' third wood pile
constexpr glm::vec2 k_RecordedWoodPile {1830.12f, 2642.61f};
/// The pile is put a little way off the recorded place, within the reach a replayed click looks over
constexpr glm::vec2 k_WoodPileShift {2.5f, 0.0f};
} // namespace

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

	const glm::vec2 pile = k_RecordedWoodPile + k_WoodPileShift - flat_land::k_MapMiddle;
	all.push_back({
	    .id = "hand.demo_click_retarget",
	    .name = "A hand demo's click finding its wood pile",
	    .facet = Facet::Hand,
	    .description = "A wood pile is put two and a half metres east of where the wood lesson's recording takes wood from, "
	                   "and the wood lesson's hand demo plays from the start. The camera follows the recording. The Hand "
	                   "menu's switch \"Hand demo: no click retarget (old)\" leaves the click where the cursor is, for "
	                   "comparison.",
	    .expected = "When the recorded hand presses its button the click lands on the wood pile nearby, even when the cursor "
	                "is over the ground beside it: the hand takes wood from the pile. The clicks recorded on nothing land on "
	                "nothing. With the switch on, a press over the ground beside the pile takes no wood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .objects = {{.type = PotInfo::WoodPile_3, .offset = pile, .amount = 400}},
	    .fixtures = {.handDemo = fixtures::HandDemo {.name = "givewood"}},
	});
}
