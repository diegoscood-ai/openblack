/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Plague as a quest run on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"), played after Worship Site's timeline in the same run, which builds the worship site the land's
// check waits for. The Indian village north of the temple's bay is handed to the player, as winning it would, and the
// quest's script is started as the land's control script starts it once the check's five-minute wait is over. The
// scroll at the trader's hut is pressed on with the hand and the introduction's texts are clicked on; then a handful of
// food is taken from the poisoned store with the hand, a heal miracle is cast over the village, the store is taken
// away, and the village is healed until the plague is beaten. A second timeline lets the plague win instead: once the
// introduction is over the village loses every one of its people, and the camera goes to the trader's spot for the
// failure. The points are map coordinates, taken from the places the quest's script and the land's script name; none
// has yet been seen in a running game.

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <vector>

#include "Enums.h"
#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
/// The plagued Indian village, where the land's map script makes it
constexpr glm::vec2 k_Village {2761.49f, 3102.94f};
constexpr ThingNear k_VillageThing {.type = script::ObjectType::Town, .point = k_Village, .radius = 30.0f};
/// The belief the village is given, more than anyone holds in it
constexpr float k_WinningBelief = 2.0f;
/// The trader's hut, where the scroll is put up, and the spot in front of it the trader walks to
constexpr glm::vec2 k_Hut {2810.98f, 3043.5103f};
constexpr glm::vec2 k_TraderSpot {2807.633f, 3048.362f};
/// The village store the poison comes from
constexpr glm::vec2 k_Store {2754.8f, 3106.68f};
constexpr ThingNear k_StoreThing {.type = script::ObjectType::Abode, .point = k_Store, .radius = 5.0f};
/// Where the heal miracles are cast: the village's middle, as the quest names it, then its houses and the store, where
/// its people are, from the land's script
constexpr glm::vec2 k_VillageMiddle {2785.0f, 3085.98f};
constexpr std::array<glm::vec2, 8> k_HealSpots {{
    {2761.49f, 3102.94f},
    {2785.0f, 3085.98f},
    {2807.633f, 3048.362f},
    {2785.18f, 3116.46f},
    {2835.29f, 3083.54f},
    {2764.42f, 3040.86f},
    {2819.31f, 3047.88f},
    {2754.8f, 3106.68f},
}};
/// How many times the heal miracles go round the village
constexpr size_t k_HealRounds = 5;
/// How long the scripts are given after each heal before the next
constexpr float k_HealSeconds = 4.0f;
/// Far from the village, out of the 100 of the trader's spot the failure waits for: the land's temple
constexpr glm::vec2 k_AwayFromVillage {3343.0f, 3220.0f};
/// How long the hand holds the Action button down on a thing to take it
constexpr float k_GrabHoldSeconds = 0.6f;
/// How long a beat whose end is not known is given before the timeline goes on
constexpr float k_RemarkSeconds = 12.0f;
/// How long the village is left after the first heal, for the one healed to eat from the store again
constexpr float k_RepoisonSeconds = 60.0f;

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "PlagueIntroTriggered LethysGone LethysInScript";
constexpr std::string_view k_Scripts = "PlagueMain ChallengeHighlightNotify";

/// Round each spot a heal is cast for, the villager nearest it is the one aimed at: a heal finds only those within its
/// reach of where it is cast, so it is cast where the people are, not where they were
constexpr float k_HealAimRadius = 30.0f;

/// A heal miracle given to the hand, as if from a bubble, and cast over the villager nearest a spot: the hand over him (or
/// the edge of the player's influence nearest him, where he is outside) and the Action button clicked there. A heal that
/// finds no one stays in the hand, and the next spot's is refused, so it is cast again there
void CastHeal(std::vector<Command>& commands, glm::vec2 point)
{
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Heal)});
	commands.push_back({.kind = Kind::HandOver,
	                    .delaySeconds = 1.0f,
	                    .point = point,
	                    .value = 2,
	                    .thing = ThingNear {.type = script::ObjectType::Villager, .point = point, .radius = k_HealAimRadius}});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
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

/// The village handed to the player and the quest started, its scroll pressed on and the introduction let play
std::vector<Command> Introduction()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    // [shortcut] the village takes more belief in the player than in anyone else, as winning it over would
	    // give it; its next fold hands it to the player (a direct change of hands is undone by that fold)
	    WinVillage(),
	    // the quest's script, started as the land's control script starts it once its check has waited five minutes
	    StartScript("PlagueMain"),
	    // [camera] the store, within 200 of it and on the screen while Lethys's hand hovers over it, which the evil
	    // advisor remarks on
	    CameraOn(k_Store, k_EyeAbove, k_EyeBack, 0.0f),
	    WaitTask("ChallengeHighlightNotify"),
	    {.kind = Kind::LogGlobals, .delaySeconds = 1.0f, .value = 60, .global = k_Globals, .script = k_Scripts},
	    // [camera] the scroll over the trader's hut
	    CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 6.0f),
	};
	// [real] the scroll pressed on
	PressScroll(commands, k_Hut);
	// the introduction: the trader, the advisors' lines and the camera given back
	commands.push_back(WaitText(6002));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RemarkSeconds));
	return commands;
}

std::vector<Command> Plague()
{
	auto commands = Introduction();
	// [camera] the store; [real] a handful of its poisoned food taken with the hand: the pointer on the store, the
	// Action button pressed a second later, once the hand is over it, and held past the grab's wait (its food pile is no
	// thing the hand can be over; the press does not wait for the hand, so the run goes on if it is not there)
	commands.push_back(CameraOn(k_Store, k_EyeAbove, k_EyeBack, 0.0f));
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_StoreThing});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RemarkSeconds));
	// [shortcut] what the hand holds let go of where it is, for the heal miracle
	commands.push_back({.kind = Kind::HandDrop, .delaySeconds = 0.5f});
	// [camera] the village; [real] a heal miracle cast over its middle, then the village left to eat
	commands.push_back(CameraOn(k_VillageMiddle, 50.0f, 60.0f, 0.0f));
	CastHeal(commands, k_VillageMiddle);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RepoisonSeconds));
	// [shortcut] the store taken away by a script's delete, where the player would empty it or destroy it
	commands.push_back({.kind = Kind::ScriptDelete, .delaySeconds = 1.0f, .thing = k_StoreThing});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RemarkSeconds));
	// [real] heal miracles cast round the village, five times over (each round cures about four), until no more than a quarter
	// are still sick
	for (size_t round = 0; round < k_HealRounds; ++round)
	{
		for (const auto spot : k_HealSpots)
		{
			CastHeal(commands, spot);
			commands.push_back(LogGlobals(k_Globals, k_Scripts, k_HealSeconds));
		}
	}
	// [camera] the spot in front of the trader's hut, which the ending waits for
	commands.push_back(CameraOn(k_TraderSpot, k_EyeAbove, k_EyeBack, 0.0f));
	// the win: the trader's praise, the log at 1, the lightning miracle and the evil advisor's line
	commands.push_back(WaitText(2612));
	commands.push_back(WaitTask("PlagueMain", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	return commands;
}

std::vector<Command> PlagueLost()
{
	auto commands = Introduction();
	// [camera] away from the village, so the failure waits for the camera
	commands.push_back(CameraOn(k_AwayFromVillage, k_EyeAbove, k_EyeBack, 0.0f));
	// [shortcut] every one of the village's people deleted by a script's delete, its buildings left standing, where the
	// plague or the player would kill them
	commands.push_back({.kind = Kind::KnockDownTown, .delaySeconds = 2.0f, .value = 1, .thing = k_VillageThing});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RemarkSeconds));
	// [camera] the spot in front of the trader's hut, which the failure waits for
	commands.push_back(CameraOn(k_TraderSpot, k_EyeAbove, k_EyeBack, 0.0f));
	// the failure: the trader's reproach and the log at 1, the good advisor's lines on an empty village, the evil
	// advisor's on a small one, and the plague lifted
	commands.push_back(WaitTask("PlagueMain", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	return commands;
}
} // namespace

void testbed_scenarios::land2_quests::AddPlagueScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_plague",
	    .name = "Land 2 quest run: The Plague, the store taken away and the village healed",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.the_plague). The texts that wait for a click are clicked on. "
	                   "It hands the Indian village with town id 10 to the player and starts the quest's script, as the "
	                   "land's control script does after its check; presses on the scroll at the trader's hut with the "
	                   "hand and lets the introduction play; takes a handful of food from the store with the hand and "
	                   "lets it go; casts a heal miracle over the village's middle and waits a minute; takes the store "
	                   "away by a script's delete; casts eight heal miracles round the village five times over, and puts the "
	                   "camera on the spot in front of the hut.",
	    .expected = "Lethys's hand hovers over the store (the evil advisor's 6000 if the camera sees it); the scroll's "
	                "press fills the store and poisons the village, and the introduction plays (1541, 1542, 6001 and "
	                "6002) with the log at 0. The handful of food draws 1554; the first heal draws 1549, and 1547 with "
	                "the log at 0 if the one healed falls sick again. The store gone draws 1551 (log at 0.5); the next "
	                "cure, 1550, 1552 and 1553 (log at 0.75). With a quarter or fewer still sick the trader's praise "
	                "(1548, log at 1), the lightning miracle in the village and 2612. Unverified: that Lethys's hand "
	                "is free in a quest run, that the hand takes food from the store's food pile, and that sixteen heal "
	                "miracles cure three quarters of the village.",
	    .commands = Plague(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_plague_lost",
	    .name = "Land 2 quest run: The Plague lost, the village's people all gone",
	    .facet = Facet::Story,
	    .description = "Played after quest.worship_site in the same run (quest=SetupLand2/EnterLand2 on Land2.txt, "
	                   "timeline=quest.worship_site/quest.the_plague_lost). The texts that wait for a click are clicked "
	                   "on. It hands the Indian village with town id 10 to the player, starts the quest's script, presses "
	                   "on the scroll and lets the introduction play, as quest.the_plague does; then, with the camera far "
	                   "off, deletes every one of the village's people by a script's delete, its buildings left "
	                   "standing, and puts the camera on the spot in front of the hut.",
	    .expected = "The introduction as in quest.the_plague. With the village under half its size the failure waits "
	                "for the camera; a trader comes out of the hut, the camera closes in, the log goes to 1 at -0.4 "
	                "and he says 1546; the camera goes back. The village being empty, the good advisor points at the "
	                "spot and says 1555 and 1556; with fewer than ten people the evil advisor says 1557; the store and "
	                "the village are no longer poisoned and the quest ends, with no lightning miracle. Unverified: "
	                "whether the trader made at the hut counts as one of the village's people, which would keep the "
	                "good advisor's lines from being said.",
	    .commands = PlagueLost(),
	    .onLand = true,
	});
}
