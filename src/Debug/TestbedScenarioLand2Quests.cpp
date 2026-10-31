/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's gold scrolls as quest runs: each timeline is played after a land entry's quest= has started the quest's
// scripts on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a timeline on a land"). It drives the
// quest as a player would: a scroll is pressed on with the hand, as the scripts' click test asks of it, and the things
// the quest waits on are built, given or put down. The points are map coordinates, taken from the places the challenge
// scripts name; none of them has yet been seen in a running game.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
/// A thing of the land built this far, as building work leaves it
Command Built(ThingNear thing, float built, float delay = 1.0f)
{
	return {.kind = Kind::AbodeBuilt, .delaySeconds = delay, .built = built, .thing = thing};
}

/// A wood miracle given to the hand and poured over a point for some seconds, with the Action button held
void PourWood(std::vector<Command>& commands, glm::vec2 point, float seconds)
{
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Wood)});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = point});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = seconds, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
}

// The Workshop, planned at the end of the worship lesson

/// The spot the watcher finds the workshop at, which Khazar's hand plans it on
constexpr glm::vec2 k_Workshop {3252.0894f, 3341.0461f};
/// The planned workshop: the abode the watcher finds there
constexpr ThingNear k_WorkshopAbode {.type = script::ObjectType::Abode, .point = k_Workshop, .radius = 10.0f};
/// The first gold scroll, in front of the workshop
constexpr glm::vec2 k_WorkshopScroll {3246.0803f, 3335.6006f};
/// The scaffold the workshop makes from the wood, within the radius the quest finds it in
constexpr ThingNear k_WorkshopScaffold {.type = script::ObjectType::Scaffold, .point = k_Workshop, .radius = 10.0f};
/// Where the hand puts the scaffold down: open ground between the workshop and the village centre (not yet seen in a
/// running game)
constexpr glm::vec2 k_ScaffoldDrop {3240.0f, 3365.0f};
/// The camera for the hand's work at the workshop: nearly straight above the point, so that the line from the camera
/// to it stays within a few paces of it and the player's creature, which can't stand inside the workshop, isn't in
/// the way; still within the 50 the quest needs the camera to be near the workshop
constexpr float k_EyeOverhead = 40.0f;
constexpr float k_EyeOverheadBack = 5.0f;
/// How long each wood miracle is poured
constexpr float k_PourSeconds = 4.0f;
/// How many wood miracles are poured on the workshop: enough wood for its scaffold, which takes 800 or more
constexpr int k_WoodMiracles = 3;
/// The globals and scripts the workshop's end is read by
constexpr std::string_view k_WorkshopGlobals = "KhazarGone";
constexpr std::string_view k_WorkshopScripts = "TheWorkshop TheWorkshopComplete ChallengeHighlightNotify";

// Worship Site, after the arrival

/// The player's temple, where the arrival plans it at three tenths built
constexpr glm::vec2 k_Citadel {3361.26f, 3241.85f};
constexpr ThingNear k_CitadelThing {.type = script::ObjectType::Citadel, .point = k_Citadel, .radius = 50.0f};
/// The village centre, where Khazar's hand plans it at the arrival
constexpr glm::vec2 k_VillageCentre {3233.325f, 3398.706f};
constexpr ThingNear k_VillageCentreThing {.type = script::ObjectType::Abode, .point = k_VillageCentre, .radius = 10.0f};
/// The worship site of the player's Norse village, at the temple
constexpr ThingNear k_WorshipSiteThing {
    .type = script::ObjectType::WorshipSite, .subtype = 7, .point = k_Citadel, .radius = 50.0f};
/// Khazar's first scroll, by the temple (raised 5 above the land), and his second, before the village centre's lesson
constexpr glm::vec2 k_FirstScroll {3338.7944f, 3242.494f};
constexpr glm::vec2 k_SecondScroll {3308.4753f, 3242.7588f};
/// The home town's totem, at the village centre
constexpr ThingNear k_HomeTotem {.type = script::ObjectType::TotemStatue, .point = k_VillageCentre, .radius = 20.0f};
/// How far down the screen the pointer pulls the held totem
constexpr float k_TotemPullDown = 0.4f;
/// The help scripts that say the creature's help: one holding the dialogue as a quest's film begins is stopped with
/// every other help script, the info signs a quest has just started among them
constexpr std::string_view k_CreatureHelpJustText = "HelpForCreatureJustText";
constexpr std::string_view k_CreatureHelpPointing = "HelpForCreaturePointAtCreature";
/// The globals and scripts the lesson's end is read by
constexpr std::string_view k_WorshipGlobals = "LearnWorshipCompleted KhazarGone KhazarInScript";
constexpr std::string_view k_WorshipScripts = "LearnWorshipping WaitAndCreateWorshipSite CheckDyingWorshippers "
                                              "MiracleChallenges";
} // namespace

void testbed_scenarios::AddLand2QuestScenarios(std::vector<Scenario>& all)
{
	AddQuestNemesisNoScenarios(all);
	AddLethysTakesCreatureQuest(all);
	AddImpressVillageQuest(all);
	// The Workshop, after Worship Site's timeline in the same run: once the worship lesson is done, the watcher is
	// started as the land's control script starts it; the lesson's end has planned the workshop through Khazar's hand.
	// The quest is not started on its own, since the hand's scaffold builds only in a town with a building within 50
	// of it, which a direct start of the quest does not have there
	std::vector<Command> workshop {
	    ClickTexts(),
	    WaitGlobal("LearnWorshipCompleted", 1.0f),
	    StartScript("TheWorkshop"),
	    WaitTask("TheWorkshop"),
	    CameraOn(k_Workshop, k_EyeAbove, k_EyeBack, 0.0f),
	    Built(k_WorkshopAbode, 1.0f, 2.0f),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_WorkshopScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(workshop, k_WorkshopScroll);
	// the engineer's film ends on his line asking for wood; he then waits at his spot for it
	workshop.push_back(WaitText(1658));
	// the camera looks down on the workshop: from further back, the player's creature can stand between the camera and
	// the workshop and take the hand's pour in its place
	workshop.push_back(CameraOn(k_Workshop, k_EyeOverhead, k_EyeOverheadBack, 0.0f));
	for (int i = 0; i < k_WoodMiracles; ++i)
	{
		PourWood(workshop, k_Workshop, k_PourSeconds);
	}
	// the wood is turned into a scaffold: the engineer's film, then the advisor shows the scaffold
	workshop.push_back(WaitText(1676));
	workshop.push_back({.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = k_WorkshopScaffold});
	workshop.push_back(WaitText(1663));
	workshop.push_back(CameraOn(k_ScaffoldDrop, k_EyeOverhead, k_EyeOverheadBack, 0.0f));
	workshop.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_ScaffoldDrop});
	workshop.push_back({.kind = Kind::HandDrop, .delaySeconds = 1.0f});
	workshop.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
	workshop.push_back(WaitText(1667));
	// the second scroll, on the workshop, once the homeless are housed
	workshop.push_back(WaitTask("ChallengeHighlightNotify"));
	workshop.push_back(CameraOn(k_Workshop, k_EyeAbove, k_EyeBack, 0.0f));
	PressScroll(workshop, k_Workshop);
	workshop.push_back(WaitText(1672));
	workshop.push_back(LogGlobals(k_WorkshopGlobals, k_WorkshopScripts, 2.0f));
	all.push_back({
	    .id = "quest.the_workshop",
	    .name = "Land 2 quest run: The Workshop",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.the_workshop). The texts that wait for a click are clicked on. "
	                   "Once the worship lesson is done it starts the watcher, as the land's control script does, builds "
	                   "the workshop "
	                   "Khazar's hand planned, presses on the first gold scroll with the hand once the scroll's notifier "
	                   "runs, pours three wood miracles on the workshop after the engineer's film with the camera above it, "
	                   "takes the scaffold the workshop makes into the hand and puts it down with the camera above the "
	                   "drop point, then presses on the second scroll on the workshop.",
	    .expected = "The workshop built and the player's starts the quest: its scroll, the engineer and his film. The "
	                "wood poured in makes a scaffold (the store falls by 800 or more), the second film plays and the advisors "
	                "show the scaffold; once it is put down the homeless are counted, then the second scroll gives the "
	                "Forest miracle and the end check logs KhazarGone=0. Unverified: that the poured wood reaches the "
	                "workshop's store, that the scaffold's drop point is open ground, and that the homeless share falls "
	                "below 40% without more houses.",
	    .commands = std::move(workshop),
	    .onLand = true,
	});

	// Worship Site after the arrival (quest=SetupLand2/EnterLand2): the arrival's film, whose texts wait for a click,
	// plans the temple and the village centre; once it is over the lesson is started, as the land's control script
	// starts it, and waits on them
	std::vector<Command> worship {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    StartScript("LearnWorshipping"),
	    WaitTask("WaitAndCreateWorshipSite"),
	    CameraOn(k_Citadel, k_EyeAbove, k_EyeBack, 0.0f),
	    Built(k_CitadelThing, 1.0f, 2.0f),
	    Built(k_VillageCentreThing, 1.0f),
	    WaitTask("CheckForKhazarChallengeHighlightNotify"),
	    CameraOn(k_FirstScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(worship, k_FirstScroll);
	// the builder disciple's film ends with the advisors' lines; the worship site is then built
	worship.push_back(WaitText(4512));
	worship.push_back(CameraOn(k_Citadel, k_EyeAbove, k_EyeBack, 0.0f));
	worship.push_back(Built(k_WorshipSiteThing, 0.5f, 3.0f));
	worship.push_back(Built(k_WorshipSiteThing, 1.0f, 3.0f));
	worship.push_back(WaitTask("CheckForKhazarChallengeHighlightNotify"));
	worship.push_back(CameraOn(k_SecondScroll, k_EyeAbove, k_EyeBack, 0.0f));
	// the second scroll is pressed once no creature help holds the dialogue: the lesson's film would stop it, and
	// with it the two info signs the lesson has just put up
	worship.push_back(WaitTask(k_CreatureHelpJustText, TaskWait::NotRunning));
	worship.push_back(WaitTask(k_CreatureHelpPointing, TaskWait::NotRunning));
	PressScroll(worship, k_SecondScroll);
	worship.push_back(WaitGlobal("LearnWorshipCompleted", 1.0f));
	worship.push_back(LogGlobals(k_WorshipGlobals, k_WorshipScripts, 2.0f));
	all.push_back({
	    .id = "quest.worship_site",
	    .name = "Land 2 quest run: Worship Site",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. Once the arrival is over it starts the lesson, as "
	                   "the land's control script does; once the lesson's wait for the buildings runs, it "
	                   "builds the temple and the village centre the arrival planned, presses on Khazar's first scroll by "
	                   "the temple with the hand, builds the worship site half and then wholly after the builder "
	                   "disciple's film, and presses on his second scroll; it ends once the lesson is done.",
	    .expected = "Khazar's two remarks on the finished buildings, his scroll, the builder disciple's film, the log at "
	                "0, 0.5 and 1 as the site is built, his second scroll, the totem and miracle hand demos, the "
	                "workshop planned and the fireball challenge's start; the end check logs LearnWorshipCompleted=1. "
	                "Unverified: the worship site is made with no building site until the temple is finished (fork "
	                "issue #175), so building it may not take; the arrival's film has not been run to its end.",
	    .commands = std::move(worship),
	    .onLand = true,
	});

	// After Worship Site's timeline in the same run: the home town's totem is pulled down with the hand, as a player
	// lowers it to call the worshippers back, below the four tenths the dying worshippers' watcher stops at
	const std::vector<Command> totem {
	    CameraOn(k_VillageCentre, k_EyeAbove, k_EyeBack, 0.0f),
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_HomeTotem},
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = k_HomeTotem},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.0f, k_TotemPullDown}, .amount = 2.0f},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = k_ActionButton},
	    LogGlobals(k_WorshipGlobals, k_WorshipScripts, 3.0f),
	};
	all.push_back({
	    .id = "quest.worship_site_totem",
	    .name = "Land 2 quest run: Worship Site, the totem lowered",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run. It puts the camera on the village centre, points "
	                   "the hand at the home town's totem, holds the Action button on it and moves the pointer down "
	                   "four tenths of the screen over two seconds, then lets go.",
	    .expected = "The hand takes hold of the totem and lowers it below 0.4 of its height: the dying worshippers' "
	                "watcher reads the height under 0.4 and ends, and the end check logs CheckDyingWorshippers no longer "
	                "running. Unverified: that the pull lowers the totem by more than a tenth.",
	    .commands = totem,
	    .onLand = true,
	});

	// After Worship Site's timeline in the same run: the home town's totem is pushed up with the hand, as a player
	// raises it to send more of the village to worship. A worshipper dies from worship only by starving at the site
	// or on the way back from it, so with most of the town there the dying worshippers' watcher has deaths to warn of
	const std::vector<Command> totemRaised {
	    CameraOn(k_VillageCentre, k_EyeAbove, k_EyeBack, 0.0f),
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_HomeTotem},
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = k_HomeTotem},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.0f, -k_TotemPullDown}, .amount = 2.0f},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = k_ActionButton},
	    LogGlobals(k_WorshipGlobals, k_WorshipScripts, 3.0f),
	};
	all.push_back({
	    .id = "quest.worship_site_totem_raised",
	    .name = "Land 2 quest run: Worship Site, the totem raised",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run. It puts the camera on the village centre, points "
	                   "the hand at the home town's totem, holds the Action button on it and moves the pointer up four "
	                   "tenths of the screen over two seconds, then lets go; the run then goes on by itself.",
	    .expected = "The hand takes hold of the totem and raises it above its half height, and the end check logs "
	                "CheckDyingWorshippers still running. Over the rest of the run hungry worshippers die at the site, "
	                "and the watcher's warning comes back every minute after the first. Unverified: how far the push "
	                "raises the totem, and that a worshipper dies from worship before the run ends.",
	    .commands = totemRaised,
	    .onLand = true,
	});

	AddDestroyItScenarios(all);
	AddLeaveVortexL2Scenarios(all);
}
