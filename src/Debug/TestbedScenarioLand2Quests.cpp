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
/// The log entry's last reminder, from the homeless line on: "There are still homeless people in the Village."
constexpr uint32_t k_WorkshopLastReminder = 1668;
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

// The land's control script, started by itself as loading the land starts it

/// The villages whose winning starts a quest, as the control script names them: the Greek village (The Slavers), the
/// Indian village by the ring (The Riddles), the two Indian villages by the bay (The Sea) and the Greek village by the
/// healer's hut (The Spiritual Healer)
constexpr ThingNear k_SlaversTown {.type = script::ObjectType::Town, .point = {2248.68f, 2240.36f}, .radius = 30.0f};
constexpr ThingNear k_RiddlesTown {.type = script::ObjectType::Town, .point = {2771.34f, 2444.15f}, .radius = 30.0f};
constexpr ThingNear k_SeaTown {.type = script::ObjectType::Town, .point = {2761.49f, 3102.94f}, .radius = 30.0f};
constexpr ThingNear k_SeaTownWest {.type = script::ObjectType::Town, .point = {2196.11f, 3096.86f}, .radius = 30.0f};
constexpr ThingNear k_HealerTown {.type = script::ObjectType::Town, .point = {1978.14f, 2586.7f}, .radius = 30.0f};
/// The Indian village whose winning, with five others of the player's, lets the control script start The Idol, and
/// Lethys's Celtic village whose winning lets it start The Greedy Farmer
constexpr ThingNear k_IdolTown {.type = script::ObjectType::Town, .point = {2862.58f, 3448.27f}, .radius = 30.0f};
constexpr ThingNear k_FarmerTown {.type = script::ObjectType::Town, .point = {1800.44f, 3024.32f}, .radius = 30.0f};
/// The Idol's scroll by the idol, and its introduction's last line
constexpr glm::vec2 k_IdolScroll {2695.73f, 3662.4f};
constexpr uint32_t k_IdolIntroductionLine = 1681;
/// The farmer's house, where the Greedy Farmer's scroll is put up, and the introduction's last line
constexpr glm::vec2 k_FarmerHouse {1741.64f, 3011.21f};
constexpr uint32_t k_FarmerIntroductionLine = 1733;
/// A point far from the farm, where the camera waits while the quest makes the herd and the gang off the screen
constexpr glm::vec2 k_AwayFromFarm {2739.382f, 3052.035f};
/// The seconds given to a quest, once its script runs, to put up its scroll
constexpr float k_ScrollUpSeconds = 20.0f;
constexpr std::string_view k_LethysScripts =
    "LandControl2 TownOwnershipCheck Land2TownTakeOver GreedyFarmerCheck GreedyFarmer IdolPyreMain KillKhazarMain "
    "LethysVortex TheSlaversMain LostTreasureMain BaywatchMain ChallengeHighlightNotify";
/// More belief in the player than in anyone else, as winning a village gives it
constexpr float k_WinningBelief = 2.0f;
/// The camera over a village as it is won
constexpr float k_TownEyeAbove = 60.0f;
constexpr float k_TownEyeBack = 80.0f;
/// How often the control run's globals and scripts are logged
constexpr size_t k_ControlLogSeconds = 30;
/// How long the land runs after the last village is won, for what the control script does with six villages
constexpr float k_AfterLastTownSeconds = 60.0f;
constexpr std::string_view k_ControlGlobals = "KhazarGone LethysGone KhazarInScript LearnWorshipCompleted LeavingLandTwo";
constexpr std::string_view k_ControlScripts =
    "LandControl2 SetupLand2 EnterLand2 SingingStonesSongsMain HanoiFlood TreePuzzleOne LearnWorshipping PlagueCheck "
    "LostTreasureCheck SlaversCheck SpiritualHealerCheck SacrificeCheck BayWatchCheck GreedyFarmerCheck "
    "TownOwnershipCheck Land2TownTakeOver TheSlaversMain LostTreasureMain BaywatchMain SpiritualHealerMain "
    "KillKhazarMain";

/// [shortcut] A village takes more belief in the player than in anyone else, as winning it over would give it; its next
/// fold hands it to the player. The camera is put over it first
void WinTown(std::vector<Command>& commands, const ThingNear& town)
{
	commands.push_back(CameraOn(town.point, k_TownEyeAbove, k_TownEyeBack, 0.0f));
	commands.push_back({.kind = Kind::TownGainsBelief,
	                    .player = PlayerNames::PLAYER_ONE,
	                    .delaySeconds = 1.0f,
	                    .amount = k_WinningBelief,
	                    .thing = town});
}

/// Khazar's home town, where the land makes it
constexpr ThingNear k_KhazarHomeTown {.type = script::ObjectType::Town, .point = {2761.43f, 1980.72f}, .radius = 30.0f};
/// The gifts of belief that change a town's hands, each kept below the town's cap on belief (10): more than Lethys's in
/// his village, more than Khazar's in his home town, and more than the player's in a village won a while before
constexpr float k_FromLethysBelief = 4.0f;
constexpr float k_FromKhazarBelief = 9.0f;
constexpr float k_ToLethysBelief = 9.0f;
/// Seconds after each change of hands for the gods' visits to the camera and their lines to end
constexpr float k_GodsLinesSeconds = 40.0f;
constexpr std::string_view k_GodsScripts = "LandControl2 TownOwnershipCheck Land2TownTakeOver KhazarConfrontPlayer "
                                           "LethysConfrontPlayer KhazarAI LethysTauntAI";

/// [shortcut] A town takes more belief in a player than in anyone else, as winning it over would give it; its next fold
/// hands it to that player. The camera is put over it first
void GiveTown(std::vector<Command>& commands, const ThingNear& town, PlayerNames player, float belief)
{
	commands.push_back(CameraOn(town.point, k_TownEyeAbove, k_TownEyeBack, 0.0f));
	commands.push_back(
	    {.kind = Kind::TownGainsBelief, .player = player, .delaySeconds = 1.0f, .amount = belief, .thing = town});
}
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
	// the first scroll, which stays as the quest's reminder, pressed on again: the log entry, complete, speaks its
	// last reminder about the homeless
	workshop.push_back(CameraOn(k_WorkshopScroll, k_EyeAbove, k_EyeBack, 0.0f));
	ReplayReminder(workshop, k_WorkshopScroll, k_WorkshopLastReminder);
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
	                   "drop point, then presses on the second scroll on the workshop and, the quest over, on the first "
	                   "scroll again for its reminder.",
	    .expected = "The workshop built and the player's starts the quest: its scroll, the engineer and his film. The "
	                "wood poured in makes a scaffold (the store falls by 800 or more), the second film plays and the advisors "
	                "show the scaffold; once it is put down the homeless are counted, then the second scroll gives the "
	                "Forest miracle; the first scroll pressed again says the reminder 1668 through the good advisor, "
	                "and the end check logs KhazarGone=0. Unverified: that the poured wood reaches the "
	                "workshop's store, that the scaffold's drop point is open ground, and that the homeless share falls "
	                "below 40% without more houses.",
	    .commands = std::move(workshop),
	    .onLand = true,
	});

	// The Workshop's watcher with Khazar dead before the workshop is built, and with the workshop never built
	std::vector<Command> workshopKhazarDead {
	    ClickTexts(),
	    WaitGlobal("LearnWorshipCompleted", 1.0f),
	    {.kind = Kind::SetGlobal, .delaySeconds = 1.0f, .amount = 1.0f, .global = "KhazarGone"},
	    StartScript("TheWorkshop"),
	    LogGlobals(k_WorkshopGlobals, k_WorkshopScripts, 10.0f),
	};
	all.push_back({
	    .id = "quest.the_workshop_khazar_dead",
	    .name = "Land 2 quest run: The Workshop, Khazar dead before the workshop is built",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (timeline=quest.worship_site/"
	                   "quest.the_workshop_khazar_dead). Once the worship lesson is done it sets Khazar's death flag "
	                   "(KhazarGone=1), standing in for his death, and starts the watcher as the land's control script "
	                   "does; the workshop is left as planned.",
	    .expected = "The watcher sees Khazar gone and ends: no scroll, no engineer; the end check logs KhazarGone=1 "
	                "with TheWorkshop and TheWorkshopComplete not running.",
	    .commands = std::move(workshopKhazarDead),
	    .onLand = true,
	});
	std::vector<Command> workshopNeverBuilt {
	    ClickTexts(),
	    WaitGlobal("LearnWorshipCompleted", 1.0f),
	    StartScript("TheWorkshop"),
	    LogGlobals(k_WorkshopGlobals, k_WorkshopScripts, 30.0f),
	};
	all.push_back({
	    .id = "quest.the_workshop_never_built",
	    .name = "Land 2 quest run: The Workshop, the workshop never built",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (timeline=quest.worship_site/"
	                   "quest.the_workshop_never_built). Once the worship lesson is done it starts the watcher as the "
	                   "land's control script does and leaves the workshop as Khazar's hand planned it, unbuilt, for "
	                   "30 seconds.",
	    .expected = "The watcher looks at the workshop every 5 seconds and finds it unbuilt; the quest does not start: "
	                "the end check logs TheWorkshop running and TheWorkshopComplete not running.",
	    .commands = std::move(workshopNeverBuilt),
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
	// the run of Khazar's death while the second scroll waits branches here
	auto deadAtSecondScroll = worship;
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

	// Worship Site with Khazar dead: his death's flag (KhazarGone=1) set by a script, standing in for his death by
	// Nemesis, once the lesson waits for the buildings, or once the second scroll waits
	std::vector<Command> deadEarly {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    StartScript("LearnWorshipping"),
	    WaitTask("WaitAndCreateWorshipSite"),
	    LogGlobals(k_WorshipGlobals, k_WorshipScripts, 1.0f),
	    {.kind = Kind::SetGlobal, .delaySeconds = 1.0f, .amount = 1.0f, .global = "KhazarGone"},
	    WaitGlobal("LearnWorshipCompleted", 1.0f),
	    LogGlobals(k_WorshipGlobals, k_WorshipScripts, 2.0f),
	};
	all.push_back({
	    .id = "quest.worship_site_khazar_dead",
	    .name = "Land 2 quest run: Worship Site, Khazar dead before the buildings are done",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). Once the arrival is "
	                   "over it starts the lesson as the land's control script does, and once the lesson waits for the "
	                   "temple and the village centre it sets Khazar's death flag (KhazarGone=1), standing in for his "
	                   "death.",
	    .expected = "The wait for the buildings ends; no scroll, builder or film: the worship site is allowed at once, "
	                "the village centre can be picked up again, the second scroll, the lesson, the workshop scaffold "
	                "and the challenges' scroll are skipped, and the end check logs LearnWorshipCompleted=1.",
	    .commands = std::move(deadEarly),
	    .onLand = true,
	});
	deadAtSecondScroll.push_back({.kind = Kind::SetGlobal, .delaySeconds = 2.0f, .amount = 1.0f, .global = "KhazarGone"});
	deadAtSecondScroll.push_back(WaitGlobal("LearnWorshipCompleted", 1.0f));
	deadAtSecondScroll.push_back(LogGlobals(k_WorshipGlobals, k_WorshipScripts, 2.0f));
	all.push_back({
	    .id = "quest.worship_site_khazar_dead_second_scroll",
	    .name = "Land 2 quest run: Worship Site, Khazar dead while the second scroll waits",
	    .facet = Facet::Story,
	    .description = "Played as quest.worship_site to the second scroll (quest=SetupLand2/EnterLand2 on Land2.txt): "
	                   "the buildings built, the first scroll pressed on with the hand, the worship site built; then, "
	                   "with the second scroll waiting, Khazar's death flag set (KhazarGone=1), standing in for his "
	                   "death.",
	    .expected = "The second scroll's notifier stops and makes the scroll active, the scroll stays where it is, the "
	                "lesson's film and what follows it are skipped, and the end check logs LearnWorshipCompleted=1.",
	    .commands = std::move(deadAtSecondScroll),
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

	// The land's control script started by itself (quest=LandControl2): it plays the arrival and starts every
	// background quest and watcher; four villages are then won, so that the control script's watchers start their quests
	// themselves, and a fifth once those have begun
	std::vector<Command> control {
	    ClickTexts(),
	    {.kind = Kind::LogGlobals, .value = k_ControlLogSeconds, .global = k_ControlGlobals, .script = k_ControlScripts},
	    // the last watcher the land's control script starts after the arrival
	    WaitTask("TownOwnershipCheck"),
	    LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f),
	};
	WinTown(control, k_SlaversTown);
	control.push_back(WaitTask("TheSlaversMain"));
	WinTown(control, k_SeaTown);
	WinTown(control, k_SeaTownWest);
	WinTown(control, k_RiddlesTown);
	// the bay's watcher looks every 4 minutes, the ring village's every 6
	control.push_back(WaitTask("BaywatchMain"));
	control.push_back(WaitTask("LostTreasureMain"));
	control.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f));
	// a sixth village of the player's: the control script also starts Khazar's death
	WinTown(control, k_HealerTown);
	control.push_back(WaitTask("SpiritualHealerMain"));
	control.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, k_AfterLastTownSeconds));
	all.push_back({
	    .id = "quest.land_control_2",
	    .name = "Land 2 quest run: the land's control script",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControl2 on Land2.txt). The texts that wait for a "
	                   "click are clicked on, the arrival's too. Once the control script has started its watchers it wins "
	                   "the Greek village (belief, a shortcut for winning it over), waits for The Slavers to start, wins "
	                   "the two Indian villages by the bay and the one by the ring, waits for The Sea and The Riddles to "
	                   "start, then wins the Greek village by the healer's hut, a sixth village of the player's, waits "
	                   "for The Spiritual Healer and lets the land run a minute.",
	    .expected = "The arrival plays, then the singing stones, the beach temple's flood and the tree puzzle start with "
	                "the watchers; each won village starts its quest from the control script's own watcher, The Sea's "
	                "within 4 minutes and The Riddles' within 6. With six villages the control script starts Khazar's "
	                "death. Unverified: that every won village stays the player's while the watchers look.",
	    .commands = std::move(control),
	    .onLand = true,
	});

	// The land's control script started by itself, with the villages that start The Idol and The Greedy Farmer: the
	// village by the idol and four others won together make six of the player's, with which the control script starts
	// The Idol and, in the same pass, Khazar's death; once that is over, Lethys's village by the farm is won
	std::vector<Command> lethys {
	    ClickTexts(),
	    {.kind = Kind::LogGlobals, .value = k_ControlLogSeconds, .global = k_ControlGlobals, .script = k_LethysScripts},
	    WaitTask("TownOwnershipCheck"),
	};
	WinTown(lethys, k_SlaversTown);
	WinTown(lethys, k_SeaTown);
	WinTown(lethys, k_SeaTownWest);
	WinTown(lethys, k_RiddlesTown);
	WinTown(lethys, k_IdolTown);
	lethys.push_back(WaitTask("IdolPyreMain"));
	lethys.push_back(WaitTask("KillKhazarMain"));
	lethys.push_back(WaitTask("KillKhazarMain", TaskWait::NotRunning));
	lethys.push_back(LogGlobals(k_ControlGlobals, k_LethysScripts, 1.0f));
	// [camera] the scroll by the idol
	lethys.push_back(CameraOn(k_IdolScroll, k_EyeAbove, k_EyeBack, 5.0f));
	// [real] the scroll pressed on, and the introduction let play to its last line
	PressScroll(lethys, k_IdolScroll);
	lethys.push_back(WaitText(k_IdolIntroductionLine));
	lethys.push_back(LogGlobals(k_ControlGlobals, k_LethysScripts, 5.0f));
	WinTown(lethys, k_FarmerTown);
	// [camera] far from the farm, so that the quest makes its herd and gang off the screen
	lethys.push_back(CameraOn(k_AwayFromFarm, k_TownEyeAbove, k_TownEyeBack, 0.0f));
	// the farm's watcher looks every 8 minutes
	lethys.push_back(WaitTask("GreedyFarmer"));
	lethys.push_back(LogGlobals(k_ControlGlobals, k_LethysScripts, k_ScrollUpSeconds));
	// [camera] the farmer's house; [real] its scroll pressed on, and the introduction let play to its last line
	lethys.push_back(CameraOn(k_FarmerHouse, k_TownEyeAbove, k_TownEyeBack, 0.0f));
	PressScroll(lethys, k_FarmerHouse);
	lethys.push_back(WaitText(k_FarmerIntroductionLine));
	lethys.push_back(LogGlobals(k_ControlGlobals, k_LethysScripts, k_AfterLastTownSeconds));
	all.push_back({
	    .id = "quest.land_control_2_lethys",
	    .name = "Land 2 quest run: the land's control script, The Idol and The Greedy Farmer",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControl2 on Land2.txt). The texts that wait for a "
	                   "click are clicked on. Once the control script has started its watchers it wins five villages by "
	                   "belief (a shortcut for winning them over), the village by the idol last, for six of the "
	                   "player's; it waits for The Idol and Khazar's death to start and the death to end, presses on the "
	                   "idol's scroll and lets its introduction play, then wins Lethys's village by the farm with the "
	                   "camera kept away, waits for The Greedy Farmer, presses on its scroll at the farmer's house and "
	                   "lets its introduction play.",
	    .expected = "With six villages the control script starts The Idol and then, in the same pass, Khazar's death; "
	                "with Lethys's village won the farm's watcher starts The Greedy Farmer within 8 minutes. Both "
	                "introductions play from their scrolls. Unverified: that Lethys's village is won by the belief "
	                "given and stays the player's.",
	    .commands = std::move(lethys),
	    .onLand = true,
	});

	// The land's control script started by itself, with towns changing hands between the player and the gods, each
	// followed by the gods' lines: Khazar's village by the slavers' camp won, then lost to Lethys, Khazar's home town
	// won, and last Lethys's village by the farm won, which is also a reason for Khazar's death on the control script's
	// next check (a run of 2026-11-06 that won it second saw Khazar gone before the later changes)
	std::vector<Command> gods {
	    ClickTexts(),
	    {.kind = Kind::LogGlobals, .value = k_ControlLogSeconds, .global = k_ControlGlobals, .script = k_GodsScripts},
	    WaitTask("TownOwnershipCheck"),
	};
	WinTown(gods, k_SlaversTown);
	gods.push_back(LogGlobals(k_ControlGlobals, k_GodsScripts, k_GodsLinesSeconds));
	GiveTown(gods, k_SlaversTown, PlayerNames::PLAYER_THREE, k_ToLethysBelief);
	gods.push_back(LogGlobals(k_ControlGlobals, k_GodsScripts, k_GodsLinesSeconds));
	GiveTown(gods, k_KhazarHomeTown, PlayerNames::PLAYER_ONE, k_FromKhazarBelief);
	gods.push_back(LogGlobals(k_ControlGlobals, k_GodsScripts, k_GodsLinesSeconds));
	GiveTown(gods, k_FarmerTown, PlayerNames::PLAYER_ONE, k_FromLethysBelief);
	gods.push_back(LogGlobals(k_ControlGlobals, k_GodsScripts, k_GodsLinesSeconds));
	all.push_back({
	    .id = "quest.land_control_2_gods",
	    .name = "Land 2 quest run: the land's control script, towns changing hands between the player and the gods",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControl2 on Land2.txt). The texts that wait for a "
	                   "click are clicked on. Once the control script has started its watchers, 40 seconds apart, each "
	                   "with the camera over the town and by a gift of belief (a shortcut for winning or losing it): "
	                   "Khazar's village by the slavers' camp to the player, then to Lethys, Khazar's home town to the "
	                   "player, and last Lethys's village by the farm to the player.",
	    .expected = "For Khazar's villages Khazar complains; for the village Lethys takes from the player he gloats; "
	                "for Lethys's village Khazar praises the player and six seconds later Lethys rages. Unverified: that "
	                "each gift is enough to change the town's hands, and that the dialogue is free for each line.",
	    .commands = std::move(gods),
	    .onLand = true,
	});

	AddDestroyItScenarios(all);
	AddLeaveVortexL2Scenarios(all);
	AddSlaversScenarios(all);
	AddSacrificeScenarios(all);
	AddPlagueScenarios(all);
	AddBeachTempleScenarios(all);
	AddRiddlesScenarios(all);
}
