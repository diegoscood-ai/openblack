/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Sacrifice as quest runs on the game's own Land 2 (docs/wiki/project/scenarios.md,
// "Playing a timeline on a land"), each played after Worship Site's timeline in the same run, which builds the player's
// worship site the quest's watcher looks for. The Indian village north-west of the temple is handed to the player, as
// winning it would, and the watcher is started on it, as the land's control script starts it. The scroll at the house
// is pressed on with the hand and the introduction's texts are clicked on; then each run takes one of the quest's
// endings with the hand: a tree and then the boy given to the altar, the priest or the priestess given to it, one of the
// family thrown, the boy carried away from the altar and put down, or a villager of the family's village given to it.
// The points are map coordinates, taken from the places the quest's script names and the trees the land's map script
// makes; none has yet been seen in a running game but the boy's spot, where the first run read him standing.

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
using testbed_land_timeline::ScriptArgument;

/// The Indian village north-west of the temple whose family brings the offering, where the land's map script makes it
constexpr glm::vec2 k_Village {2196.11f, 3096.86f};
constexpr ThingNear k_VillageThing {.type = script::ObjectType::Town, .point = k_Village, .radius = 30.0f};
/// The belief the village is given, more than anyone holds in it
constexpr float k_WinningBelief = 2.0f;
/// The house north of that village where the scroll is put up and the family is made
constexpr glm::vec2 k_House {2195.354f, 3197.774f};
/// The spot south of the house the family walk off to in the introduction, more than 100 from the altar
constexpr glm::vec2 k_Away {2188.886f, 3135.653f};
/// The altar of the player's worship site, the spot the watcher looks for the site at
constexpr glm::vec2 k_Altar {3343.191f, 3220.2014f};
/// Where the scroll is moved once the introduction is over, beside the altar
constexpr glm::vec2 k_ScrollByAltar {3337.6362f, 3202.5454f};
/// The worship site the Indian village gets at the temple once it is the player's (its tribe's sub-type), the one
/// nearest the altar
constexpr ThingNear k_VillageSiteThing {
    .type = script::ObjectType::WorshipSite, .subtype = 4, .point = k_Altar, .radius = 50.0f};
/// A tree west of the temple, where the land's map script plants it
constexpr glm::vec2 k_Tree {3281.15f, 3269.76f};
constexpr ThingNear k_TreeThing {.type = script::ObjectType::Tree, .point = k_Tree, .radius = 5.0f};
/// Where the introduction leaves the three beside the altar
constexpr glm::vec2 k_BoySpot {3340.7705f, 3217.5757f};
constexpr glm::vec2 k_PriestSpot {3339.73f, 3219.159f};
constexpr glm::vec2 k_PriestessSpot {3342.677f, 3216.855f};
/// The priest and the priestess, the Indian villagers of those kinds the quest makes
constexpr uint32_t k_Priest = 68;
constexpr uint32_t k_Priestess = 69;
/// The three; the boy is an Indian farmer the script makes a child by his age, so a child to the picks
constexpr ThingNear k_BoyThing {.type = script::ObjectType::VillagerChild, .point = k_BoySpot, .radius = 3.0f};
constexpr ThingNear k_PriestThing {
    .type = script::ObjectType::Villager, .subtype = k_Priest, .point = k_PriestSpot, .radius = 3.0f};
constexpr ThingNear k_PriestessThing {
    .type = script::ObjectType::Villager, .subtype = k_Priestess, .point = k_PriestessSpot, .radius = 3.0f};
/// The boy once he is put down at the spot the family walked off to, wherever he has walked since
constexpr ThingNear k_BoyAwayThing {.type = script::ObjectType::VillagerChild, .point = k_Away, .radius = 60.0f};
/// A grown villager of the family's village, near its middle, worth more than 4000 at the altar
constexpr ThingNear k_VillagerThing {.type = script::ObjectType::Villager, .point = k_Village, .radius = 60.0f};
/// Where one of the family is thrown: about 120 from the altar, on the line past the tree west of the temple
constexpr glm::vec2 k_ThrownTo {3250.0f, 3295.0f};
/// The camera for a throw, high and far back enough to show the land the throw lands on
constexpr float k_ThrowEyeAbove = 50.0f;
constexpr float k_ThrowEyeBack = 70.0f;
/// Where the reward's chest falls, in front of the altar
constexpr glm::vec2 k_RewardSpot {3326.141f, 3200.9478f};
constexpr ThingNear k_RewardChest {.type = script::ObjectType::Reward, .point = k_RewardSpot, .radius = 10.0f};
/// How long the hand holds the Action button down on a thing to take it
constexpr float k_GrabHoldSeconds = 0.6f;
/// How long the priest's judgement of an offering is given: the 2 seconds the script waits, and his line
constexpr float k_JudgeSeconds = 15.0f;
/// How long the reward's scene is given: its camera's glides and the chest's fall
constexpr float k_RewardSceneSeconds = 20.0f;

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "SacrificeFinished SacrificeCompleted";
constexpr std::string_view k_Scripts =
    "SacrificeCheck Sacrifice SacrificeIntro ChallengeHighlightNotify StandardReminder rewardfromsky displayrewardhelp";

/// A thing taken into the hand: the pointer on it, the Action button pressed once the hand is over it and let go a
/// moment later, which leaves the thing in the hand
void Take(std::vector<Command>& commands, const ThingNear& thing)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = thing});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = thing});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
}

/// What the hand holds given to a point, with the camera on it: the hand held over it and the Action button clicked
/// there; on the altar the altar takes it, on the land it is put down
void GiveAt(std::vector<Command>& commands, glm::vec2 point)
{
	commands.push_back(CameraOn(point, k_EyeAbove, k_EyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = point});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
}

/// One of the family taken up with the hand beside the altar, with the camera on the altar
void TakeBesideTheAltar(std::vector<Command>& commands, const ThingNear& thing)
{
	commands.push_back(CameraOn(k_Altar, k_EyeAbove, k_EyeBack, 0.0f));
	Take(commands, thing);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
}

/// What the hand holds thrown hard at a point of the land, as the player throws, with the camera high over the place
/// it is thrown from: the landing is what kills
void ThrowAt(std::vector<Command>& commands, glm::vec2 from, glm::vec2 to)
{
	commands.push_back(CameraOn(from, k_ThrowEyeAbove, k_ThrowEyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 1.0f, .point = to});
}

/// The village takes more belief in the player than in anyone else, as a gift gives it; its next fold hands it over
Command WinVillage()
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = 1.0f,
	        .amount = k_WinningBelief,
	        .thing = k_VillageThing};
}

/// The quest started and its introduction played, up to the family left beside the altar
std::vector<Command> Introduction()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // [shortcut] the Indian village takes more belief in the player than in anyone else, as winning it over would
	    // give it; its next fold hands it to the player (a direct change of hands is undone by that fold)
	    WinVillage(),
	    // [shortcut] the worship site the won village gets at the temple, built at once by a script's built percentage
	    // where its villagers would build it: the watcher waits for the site nearest the altar to be fully built
	    {.kind = Kind::AbodeBuilt, .delaySeconds = 5.0f, .built = 1.0f, .thing = k_VillageSiteThing},
	    // the quest's watcher, started on the village as the land's control script starts it; it finds that site
	    {.kind = Kind::StartScript,
	     .script = "SacrificeCheck",
	     .scriptArguments = {ScriptArgument {.kind = ScriptArgument::Kind::Thing,
	                                         .thingType = static_cast<uint8_t>(script::ObjectType::Town),
	                                         .thingPoint = k_Village,
	                                         .thingRadius = 30.0f}}},
	    WaitTask("ChallengeHighlightNotify"),
	    LogGlobals(k_Globals, k_Scripts, 1.0f),
	    // [camera] the scroll at the house
	    CameraOn(k_House, k_EyeAbove, k_EyeBack, 0.0f),
	};
	// [real] the scroll pressed on
	PressScroll(commands, k_House);
	// the introduction: the priest's lines at the house, the fade to the altar, his lines there and the advisors'. Its
	// scene ends in the turn its last line is read, before the next wait is first looked at, so that wait is for it not
	// running rather than for its end being seen
	commands.push_back(WaitText(1514));
	commands.push_back(WaitTask("SacrificeIntro", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	return commands;
}

/// The quest's ending waited for, and the globals logged once its scene is over
void Ending(std::vector<Command>& commands)
{
	commands.push_back(WaitGlobal("SacrificeFinished", 1.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 8.0f));
}

std::vector<Command> BoySacrificed()
{
	auto commands = Introduction();
	// [camera] the scroll, moved beside the altar; [real] the active scroll pressed on again: its reminder
	commands.push_back(CameraOn(k_ScrollByAltar, k_EyeAbove, k_EyeBack, 0.0f));
	PressScroll(commands, k_ScrollByAltar);
	commands.push_back(WaitText(1518));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	// [camera] the tree west of the temple; [real] the tree pulled up with the hand and given to the altar
	commands.push_back(CameraOn(k_Tree, k_EyeAbove, k_EyeBack, 0.0f));
	Take(commands, k_TreeThing);
	GiveAt(commands, k_Altar);
	// the priest judges the offering, after the evil advisor's grumble at a small one
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_JudgeSeconds));
	// [shortcut] whatever the hand still holds is let go of where it is, should the altar not have taken the tree
	commands.push_back({.kind = Kind::HandDrop, .delaySeconds = 0.5f});
	// [real] the boy taken up with the hand and given to the altar
	TakeBesideTheAltar(commands, k_BoyThing);
	GiveAt(commands, k_Altar);
	// the boy's ending: the priestess's line, the log closed at 1 with alignment -1
	Ending(commands);
	return commands;
}

/// One of the family taken up with the hand and given to the altar
std::vector<Command> Sacrificed(const ThingNear& thing)
{
	auto commands = Introduction();
	TakeBesideTheAltar(commands, thing);
	GiveAt(commands, k_Altar);
	Ending(commands);
	return commands;
}

/// One of the family taken up with the hand and thrown at the land about 120 from the altar
std::vector<Command> Killed(const ThingNear& thing)
{
	auto commands = Introduction();
	TakeBesideTheAltar(commands, thing);
	ThrowAt(commands, k_Altar, k_ThrownTo);
	Ending(commands);
	return commands;
}

std::vector<Command> BoyReleased()
{
	auto commands = Introduction();
	// [real] the boy taken up beside the altar, carried to the spot the family walked off to and put down there
	TakeBesideTheAltar(commands, k_BoyThing);
	GiveAt(commands, k_Away);
	// the priest's impatient line at the altar, the log at 0 with alignment 0.5 and the boy let go from the script
	commands.push_back(WaitText(1517));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
	// [camera] the spot; [real] the boy taken up again where he walks and thrown at the house: his death still ends the
	// quest
	commands.push_back(CameraOn(k_Away, k_EyeAbove, k_EyeBack, 0.0f));
	Take(commands, k_BoyAwayThing);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	ThrowAt(commands, k_Away, k_House);
	Ending(commands);
	return commands;
}

std::vector<Command> Offered4000()
{
	auto commands = Introduction();
	// [camera] the family's village; [real] a grown villager taken up there with the hand and given to the altar
	commands.push_back(CameraOn(k_Village, k_EyeAbove, k_EyeBack, 0.0f));
	Take(commands, k_VillagerThing);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	GiveAt(commands, k_Altar);
	// the priest's judgement, the ending's scene and the reward's: the chest falls in front of the altar
	Ending(commands);
	commands.push_back(WaitTask("displayrewardhelp"));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RewardSceneSeconds));
	// [camera] the chest; [real] the chest tapped with the Action button: its help line
	commands.push_back(CameraOn(k_RewardSpot, k_EyeAbove, k_EyeBack, 0.0f));
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_RewardChest});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = k_RewardChest});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 10.0f));
	return commands;
}

/// One of the quest's runs, all played after Worship Site's timeline on the game's own Land 2
Scenario Run(std::string_view id, std::string_view name, std::string_view description, std::string_view expected,
             std::vector<Command> commands)
{
	return {
	    .id = id,
	    .name = name,
	    .facet = Facet::Story,
	    .description = description,
	    .expected = expected,
	    .commands = std::move(commands),
	    .onLand = true,
	};
}
} // namespace

void testbed_scenarios::land2_quests::AddSacrificeScenarios(std::vector<Scenario>& all)
{
	all.push_back(
	    Run("quest.the_sacrifice", "Land 2 quest run: The Sacrifice, the boy given to the altar",
	        "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	        "timeline=quest.worship_site/quest.the_sacrifice). The texts that wait for a click are clicked on. It hands the "
	        "Indian village north-west of the temple to the player and starts the quest's watcher on it, as the land's "
	        "control script does; presses on the scroll at the house with the hand and lets the introduction play; presses on "
	        "the active scroll beside the altar again; pulls up a tree west of the temple with the hand and clicks with it on "
	        "the altar; then takes up the boy beside the altar and clicks with him on the altar.",
	        "The watcher finds the built worship site and the player's village and starts the quest; the scroll's press makes "
	        "the family and plays the introduction (texts 1529, 1530, 1510, 1511, 1512, 1513 and 1514) with the log at 0, and "
	        "the scroll moves beside the altar, where the second press says the reminder (1518). The tree's sacrifice raises "
	        "the site's total: two seconds later the evil advisor grumbles (6488) and the priest judges it (1516, 1520 or "
	        "1515), and the log's share rises. The boy taken in the hand and sacrificed ends the quest with the priestess's "
	        "line (1528), the log at 1 and alignment -1; the end check logs SacrificeFinished=1 and SacrificeCompleted=1. "
	        "Unverified: that the hand pulls the tree up and the altar takes it.",
	        BoySacrificed()));
	all.push_back(
	    Run("quest.the_sacrifice_priest", "Land 2 quest run: The Sacrifice, the priest given to the altar",
	        "Played after quest.worship_site in the same run (timeline=quest.worship_site/quest.the_sacrifice_priest). Starts "
	        "the quest and plays its introduction as quest.the_sacrifice does, then takes up the priest beside the altar with "
	        "the hand and clicks with him on the altar.",
	        "With the priest dead when the total rises only the evil advisor's grumble (6488) is said; the ending's line "
	        "1526, the log at 1 with alignment 1, and no reward.",
	        Sacrificed(k_PriestThing)));
	all.push_back(
	    Run("quest.the_sacrifice_priestess", "Land 2 quest run: The Sacrifice, the priestess given to the altar",
	        "Played after quest.worship_site in the same run (timeline=quest.worship_site/quest.the_sacrifice_priestess). "
	        "Starts the quest and plays its introduction as quest.the_sacrifice does, then takes up the priestess beside the "
	        "altar with the hand and clicks with her on the altar.",
	        "The priest judges the big offering (1515); the ending's line 1524, the log at 1 with alignment 0, and no reward.",
	        Sacrificed(k_PriestessThing)));
	all.push_back(
	    Run("quest.the_sacrifice_priest_killed", "Land 2 quest run: The Sacrifice, the priest thrown",
	        "Played after quest.worship_site in the same run (timeline=quest.worship_site/quest.the_sacrifice_priest_killed). "
	        "Starts the quest and plays its introduction as quest.the_sacrifice does, then takes up the priest beside the "
	        "altar with the hand and throws him at the land about 120 away.",
	        "The priest dies of the landing and leaves a body: the ending's line 1525, the log at 1 with alignment 0.5, and "
	        "no reward. Unverified: that the landing kills him.",
	        Killed(k_PriestThing)));
	all.push_back(
	    Run("quest.the_sacrifice_priestess_killed", "Land 2 quest run: The Sacrifice, the priestess thrown",
	        "Played after quest.worship_site in the same run "
	        "(timeline=quest.worship_site/quest.the_sacrifice_priestess_killed). Starts the quest and plays its introduction "
	        "as quest.the_sacrifice does, then takes up the priestess beside the altar with the hand and throws her at the "
	        "land about 120 away.",
	        "The priestess dies of the landing and leaves a body: the ending's line 1523, the log at 1 with alignment -0.2, "
	        "and no reward. Unverified: that the landing kills her.",
	        Killed(k_PriestessThing)));
	all.push_back(
	    Run("quest.the_sacrifice_boy_killed", "Land 2 quest run: The Sacrifice, the boy thrown",
	        "Played after quest.worship_site in the same run (timeline=quest.worship_site/quest.the_sacrifice_boy_killed). "
	        "Starts the quest and plays its introduction as quest.the_sacrifice does, then takes up the boy beside the altar "
	        "with the hand and throws him at the land about 120 away.",
	        "The boy dies of the landing and leaves a body: the ending's line 1527, the log at 1 with alignment -1, and no "
	        "reward. Unverified: that the landing kills him.",
	        Killed(k_BoyThing)));
	all.push_back(
	    Run("quest.the_sacrifice_released", "Land 2 quest run: The Sacrifice, the boy carried off",
	        "Played after quest.worship_site in the same run (timeline=quest.worship_site/quest.the_sacrifice_released). "
	        "Starts the quest and plays its introduction as quest.the_sacrifice does, then takes up the boy beside the altar "
	        "with the hand, carries him to the spot south of the house the family walked off to and puts him down there; then "
	        "takes him up again and throws him at the house.",
	        "Put down more than 100 from the altar the boy is let go: the priest's line 1517, the log at 0 with alignment 0.5 "
	        "and the boy released, the camera left on the altar. His later death still ends the quest: the line 1527, the log "
	        "at 1 with alignment -1. Unverified: that the click over the land puts him down and that the landing of the throw "
	        "kills him.",
	        BoyReleased()));
	all.push_back(
	    Run("quest.the_sacrifice_4000", "Land 2 quest run: The Sacrifice, 4000 offered",
	        "Played after quest.worship_site in the same run (timeline=quest.worship_site/quest.the_sacrifice_4000). Starts "
	        "the quest and plays its introduction as quest.the_sacrifice does, then takes up a grown villager in the family's "
	        "village with the hand and clicks with him on the altar; then taps the reward's chest.",
	        "The villager's sacrifice adds more than 4000: the priest judges it (1515) and the log reaches 1; the ending's "
	        "lines 1519 and 1521 with the log at 1 and alignment 1; then the reward's scene drops the chest in front of the "
	        "altar and its tap says its help line. Unverified: that a grown villager stands near the village's middle for the "
	        "hand to take.",
	        Offered4000()));
}
