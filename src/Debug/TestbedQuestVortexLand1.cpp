/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 1's last gold scroll, Leave Through the Vortex, as a land timeline: played on the game's own Land 1 through a
// land entry's timeline= (docs/wiki/project/scenarios.md, "Playing a timeline on a land"), after the quest run has
// started the quest's script with the globals the storm would have set. It brings the camera to the vortex's place,
// lets both cut-scenes play, drops something into the vortex, presses on the scroll and waits for the land to be told
// to end. The points are map coordinates, taken from the places the challenge script names; none is yet seen in a
// running game.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::ThingNear;

Command WaitTask(std::string_view script)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

/// Where the vortex opens and its scroll stands (the quest's own marker, at its height of 11.4 above the sea)
constexpr glm::vec2 k_Vortex {1700.2275f, 2520.1829f};
/// The camera put south of the vortex looking at it: well within the 200 the quest's first wait asks for, and the 150
/// and the view its second wait asks for
constexpr glm::vec3 k_VortexEye {k_Vortex.x, 40.0f, k_Vortex.y - 60.0f};
constexpr glm::vec3 k_VortexLook {k_Vortex.x, 11.0f, k_Vortex.y};
/// The scroll, made over the vortex once the second cut-scene's lines are said
constexpr ThingNear k_VortexScroll {.type = script::ObjectType::Highlight, .point = k_Vortex, .radius = 15.0f};
/// What the player picks up and lets go of, which brings the shepherd back: a cow of the herd the land makes just south
/// of the vortex, at its flock's middle. A testbed choice: the quest takes anything that is in the hand, and its letting
/// go anywhere
constexpr glm::vec2 k_Herd {1687.09f, 2489.71f};
constexpr ThingNear k_TestCow {
    .type = script::ObjectType::Animal, .subtype = 8, .point = k_Herd, .radius = 30.0f, .aliveOnly = true};
/// The camera put south of the herd looking at it, so that the pointer can be put on a cow
constexpr glm::vec3 k_HerdEye {k_Herd.x, 40.0f, k_Herd.y - 60.0f};
constexpr glm::vec3 k_HerdLook {k_Herd.x, 2.0f, k_Herd.y};
/// How long the Action button is held on the cow to pick it up, past the hand's grab wait
constexpr float k_GrabHoldSeconds = 0.6f;
/// The globals and scripts the quest's end is read by
constexpr std::string_view k_VortexGlobals = "IsSkippingCreatureGuide OpenVortexL1 VortexOpen PlayerCreatureType LeaveLandNow";
constexpr std::string_view k_VortexScripts = "LeaveThroughVortexL1 QuestHighlightNotify";
} // namespace

void openblack::testbed_scenarios::AddQuestVortexLand1Scenarios(std::vector<Scenario>& all)
{
	// The quest started after the glade's choice, as the land's control script starts it in the background after the
	// guide's lessons (a land entry's quest=SetupLand1/ProtectGateKeysFlag=1/ProtectGateKeys/ChooseYourCreature with
	// timeline=quest.choose_your_creature/quest.the_lost_brother/quest.the_sculptor_and_glade/
	// quest.leave_through_the_vortex_land_1): the guide kept, the storm's go-ahead given, the advisor's line, the camera
	// brought near, the opening cut-scene, a villager dropped into the vortex, the shepherd's return, the scroll pressed on
	all.push_back({
	    .id = "quest.leave_through_the_vortex_land_1",
	    .name = "Land 1 quest run: Leave Through the Vortex, from the golden light to the dive",
	    .facet = Facet::Story,
	    .description = "Played after the glade's choice in the same quest run (quest.the_sculptor_and_glade), so that the "
	                   "player has a creature. It starts LeaveThroughVortexL1 with the guide not skipped and the storm's "
	                   "go-ahead set, lets the advisor's line about the golden light be read, puts the camera near the "
	                   "spot, lets the vortex's opening cut-scene play, picks up a cow of the herd by the vortex with the "
	                   "hand and lets go of it over the vortex, lets the shepherd's return play, then presses on the gold "
	                   "scroll over the vortex.",
	    .expected = "The camera near the spot starts the cut-scene (the vortex made, the shepherd and the advisors' lines "
	                "1531 to 1536); the drop ends the test and the shepherd is flung back (6102); the scroll appears 20 "
	                "above the vortex (1538), the reminder 4460 is said, and the press starts the dive: the fade, the guide "
	                "story's scripts stopped and LeaveLandNow becomes 1. A quest run has no land control script, so the "
	                "land does not change. Unverified: that the camera may be put there under the glade's camera zones.",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            // the quest started once the creature is chosen, with the guide not skipped, as the land's control script
	            // starts it after the guide's lessons; the vortex opens at once, as the guide's storm opens it
	            {.kind = Kind::SetGlobal, .amount = 0.0f, .global = "IsSkippingCreatureGuide"},
	            {.kind = Kind::StartScript, .script = "LeaveThroughVortexL1"},
	            {.kind = Kind::SetGlobal, .amount = 1.0f, .global = "OpenVortexL1"},
	            WaitTask("LeaveThroughVortexL1"),
	            // the good advisor's line about the golden light, then the evil one's point; the dialogue ends with it
	            WaitText(6100, true),
	            // [camera] the player brings the camera to the spot: the quest waits for it within 200
	            {.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_VortexEye, .look = k_VortexLook},
	            // the opening cut-scene, to its last line ("I bet you anything ...")
	            WaitText(1536, true),
	            // [camera] once the cinema has given the camera back (the advisors go home about 3 seconds after the
	            // line is read), the player looks at the herd by the vortex; [real] the Action button held on a cow
	            // there, past the grab's wait, picks it up, and let go of, the cow is kept in the hand
	            {.kind = Kind::CameraTo, .delaySeconds = 5.0f, .eye = k_HerdEye, .look = k_HerdLook},
	            {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_TestCow},
	            {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_TestCow},
	            {.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = 3},
	            // [camera] back at the vortex; [real] the cow let go of over it, which ends the test
	            {.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_VortexEye, .look = k_VortexLook},
	            {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Vortex},
	            {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
	            // the shepherd's return and the advisors' talk, to the evil advisor's last line before the scroll
	            WaitText(1540, true),
	            // [real] the Action button pressed on the scroll over the vortex
	            {.kind = Kind::CameraTo, .delaySeconds = 4.0f, .eye = k_VortexEye, .look = {k_Vortex.x, 25.0f, k_Vortex.y}},
	            {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_Vortex, .value = 1},
	            {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_VortexScroll},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            // the dive into the vortex and the fade; the land is then told to end
	            {.kind = Kind::WaitGlobal,
	             .waitUntilFree = true,
	             .amount = 1.0f,
	             .global = "LeaveLandNow",
	             .compare = Compare::Equal},
	            {.kind = Kind::LogGlobals, .delaySeconds = 2.0f, .global = k_VortexGlobals, .script = k_VortexScripts},
	        },
	    .logHandGrab = true,
	    .logHandLock = true,
	    .onLand = true,
	});

	// The same quest through the real input path (tools/checks/realinput/vortex_land_1.txt, replayed through
	// tools/checks/real_input_tap.sh): the timeline only clicks the texts and logs where the quest's task stands, so
	// the replayed keys, wheel and mouse alone bring the camera to the spot and back to the vortex, and take something
	// into the hand
	all.push_back({
	    .id = "realinput.vortex_land_1",
	    .name = "Land 1 real input: Leave Through the Vortex, the camera brought by the player",
	    .facet = Facet::Story,
	    .description = "Played in a quest run that starts LeaveThroughVortexL1 with the guide not skipped and the "
	                   "storm's go-ahead set (quest=IsSkippingCreatureGuide=0/OpenVortexL1=1/LeaveThroughVortexL1). The "
	                   "timeline only clicks the texts and logs the quest's globals and task every 5 seconds: the "
	                   "camera's moves and the hand's take are the replayed input's.",
	    .expected = "The quest waits until the player's camera comes within 200 of the spot, then plays the opening "
	                "cut-scene (1531 to 1536); after the 60-second timer or the hand's take and let-go it waits until "
	                "the player's camera has the vortex in view within 150, then the shepherd comes back (6102).",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            {.kind = Kind::LogGlobals,
	             .delaySeconds = 1.0f,
	             .value = 5,
	             .global = k_VortexGlobals,
	             .script = k_VortexScripts},
	        },
	    .logHandGrab = true,
	    .onLand = true,
	});

	// MISSING: none for the quest run. The land change itself (the load of Land 2) needs the land's
	// control script, which a quest run does not start: only the natural run (story_vortex.sh) reaches it.
}
