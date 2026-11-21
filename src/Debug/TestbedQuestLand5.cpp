/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 5's gold scrolls as quest runs, each played after a land entry's quest= has started it: Nemesis's Shielded
// Village and So This Is a Fight to the Death. The timelines do what the player would: they click the texts that wait
// for a click, lead the creature where the quests wait for it, kill the shield's worshippers, draw the fight's lightning,
// heal the creature and press the space bar at the credits' end. The points are map coordinates, taken from the places
// the challenge scripts name.

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL_scancode.h>

#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
/// Within 300 of the shield's centre, where the creature's coming starts the shield's scene
constexpr glm::vec2 k_NearTheShield {1700.0f, 2100.0f};
/// The shield's last line in its scene
constexpr uint32_t k_ShieldSceneLastLine = 5502;
/// The three worshippers who keep the stones' rays on the shield, each made by its stone, and their villager types
struct Worshipper
{
	uint32_t subtype;
	glm::vec2 at;
};
constexpr std::array<Worshipper, 3> k_Worshippers {{
    {.subtype = 36, .at = {2089.572f, 2068.963f}},
    {.subtype = 41, .at = {1568.99f, 2103.337f}},
    {.subtype = 35, .at = {1393.386f, 2998.66f}},
}};
constexpr float k_WorshipperPickRadius = 10.0f;
/// The shield's three stones, where the shield's script makes them (mobile statics of the stones' kind), and how far
/// each is moved off its spot
constexpr std::array<glm::vec2, 3> k_ShieldStones {{
    {2092.572f, 2070.963f},
    {1571.99f, 2105.337f},
    {1396.386f, 3000.66f},
}};
constexpr uint32_t k_StoneSubtype = 5;
constexpr float k_StonePickRadius = 3.0f;
constexpr glm::vec2 k_StoneMovedBy {10.0f, 0.0f};
/// The land's boulders and flat rock on the hill east of the third worshipper, where the map script makes them, each
/// picked within a little of its spot
constexpr std::array<glm::vec2, 4> k_HillBoulders {{
    {1512.01f, 2959.97f},
    {1519.98f, 2963.66f},
    {1527.59f, 2963.79f},
    {1524.53f, 2970.28f},
}};
constexpr float k_BoulderPickRadius = 2.0f;
/// A throw past the third worshipper: from the low hill east of him (about 14 above the sea) at the flat land about 25
/// from him (about 3 above it), lower than where it is thrown from, so the throw can reach it
constexpr glm::vec2 k_NearMissThrowFrom {1445.0f, 2972.0f};
constexpr glm::vec2 k_NearMissThrowTo {1405.0f, 3020.0f};
/// The camera for a throw, high and far back enough to show the land the throw crosses
constexpr float k_ThrowEyeAbove = 50.0f;
constexpr float k_ThrowEyeBack = 70.0f;
/// The camera for a recording's take, above a boulder and a little to the south, looking down at it, so that a press
/// at the screen's middle meets it
constexpr float k_TakeEyeAbove = 25.0f;
constexpr float k_TakeEyeBack = 4.0f;
/// Seconds the worshipper's checks are watched after each throw: a check comes every 3 seconds
constexpr float k_NearMissWatchSeconds = 6.0f;
/// Within 200 of Nemesis's last town, where his meteors and his shield answer the creature
constexpr glm::vec2 k_NearNemesisTown {3088.8f, 2500.0f};
/// The arena's middle: within 300 of it the creature's coming starts the last fight, and within 50 of it, once the fight
/// is won, the walk up the volcano follows
constexpr glm::vec2 k_TheArena {3464.969f, 2439.695f};
/// The last line before the fight, read before the cinema's last shots and the fight's start
constexpr uint32_t k_BeforeTheFightLine = 5545;
/// Seconds from that line being read to the duel being under way, the fight camera watching it
constexpr float k_FightUnderWaySeconds = 60.0f;
/// The fight's lightning drawn this many times, this far apart: each one the creature casts at the mirror
constexpr size_t k_LightningCasts = 30;
constexpr float k_SecondsBetweenCasts = 5.0f;
// [shortcut] the mirror's fight health set to none five times in the duel, every third cast (15 seconds apart, more
// than the 10 seconds Nemesis waits after each of his four heals), so that the fifth stays and the next cast knocks
// it out: openblack's fight lightning takes almost nothing off it (docs/wiki/creature/fights.md)
constexpr size_t k_FightHealthDrops = 5;
constexpr size_t k_CastsBetweenDrops = 3;
constexpr size_t k_FightHealthProperty = 33;
/// The last line of the cinema after the won fight
constexpr uint32_t k_AfterTheFightLastLine = 6036;
/// Where the creature lies, burnt, after the citadel's fall, and the advisors' last line there
constexpr glm::vec2 k_WhereTheCreatureLies {2263.772f, 2768.494f};
constexpr uint32_t k_CreatureBackLastLine = 6051;
constexpr float k_CreaturePickRadius = 30.0f;
/// The credits' last shot, where the camera ends its last move before the scene holds for the space bar
constexpr glm::vec2 k_CreditsLastShot {2959.795f, 3429.878f};
constexpr float k_CreditsShotRadius = 6.0f;
/// The space bar pressed this many times, this far apart, from 10 seconds after the camera's arrival
constexpr size_t k_SpacePresses = 8;
constexpr float k_SecondsBetweenPresses = 10.0f;
/// Seconds between the logs of a quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

constexpr std::string_view k_ShieldGlobals = "ShieldFinished NumberOfActiveStones";
constexpr std::string_view k_ShieldScripts = "ThrowThroughShield LookAfterShieldStone NemesisDefendHisLastTown";
constexpr std::string_view k_FightGlobals = "LandNumber";
constexpr std::string_view k_FightScripts = "TheBigFightMain EndOfGameCreatureSequenceGoToVolcano NemesisCitadelDestroyed "
                                            "VillageWavingSequence";

/// Logs the globals and scripts named every period while the timeline goes on
[[nodiscard]] Command LogEvery(std::string_view globals, std::string_view scripts)
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = globals, .script = scripts};
}

/// The wait for a text to have been said to its end, as a script's wait until read takes it, so that what the script does
/// next (a cinema's end) has been done
[[nodiscard]] Command WaitSaid(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true, .voice = true};
}

/// [shortcut] The player's creature put at a point of the land, as the leash would lead it there
[[nodiscard]] Command PutCreature(glm::vec2 point, float delay)
{
	return {.kind = Kind::MoveThing, .delaySeconds = delay, .point = point};
}

/// A key of the keyboard going down, or let go, as the window gives a real key's press and release
[[nodiscard]] Command KeyDown(SDL_Scancode key, float delay)
{
	return {.kind = Kind::KeyDown, .delaySeconds = delay, .value = static_cast<size_t>(key)};
}

[[nodiscard]] Command KeyUp(SDL_Scancode key, float delay)
{
	return {.kind = Kind::KeyUp, .delaySeconds = delay, .value = static_cast<size_t>(key)};
}

void AddShieldedVillage(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("ThrowThroughShield"),
	    LogEvery(k_ShieldGlobals, k_ShieldScripts),
	    PutCreature(k_NearTheShield, 2.0f),
	    WaitSaid(k_ShieldSceneLastLine),
	};
	// [shortcut] each worshipper killed, as the player's blow would kill him, which takes his stone's ray off the shield
	for (const auto& worshipper : k_Worshippers)
	{
		commands.push_back({.kind = Kind::ScriptSetHealth,
		                    .delaySeconds = 2.0f,
		                    .amount = 0.0f,
		                    .thing = ThingNear {.type = script::ObjectType::Villager,
		                                        .subtype = worshipper.subtype,
		                                        .point = worshipper.at,
		                                        .radius = k_WorshipperPickRadius}});
	}
	commands.push_back(WaitTask("ThrowThroughShield", TaskWait::Finished));
	commands.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, 1.0f));
	// then by Nemesis's last town, for his meteors and his shield
	commands.push_back(PutCreature(k_NearNemesisTown, 2.0f));
	commands.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, 60.0f));
	all.push_back({
	    .id = "quest.nemesis_shielded_village",
	    .name = "Land 5 quest run: Nemesis's shielded village",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/ThrowThroughShield&/"
	                   "NemesisDefendHisLastTown on Land5.txt). The texts that wait for a click are clicked on. It puts "
	                   "the creature within 300 of the shield, waits for the shield's scene to end, kills the three "
	                   "worshippers by their stones, waits for the shield's quest to end, then puts the creature within 200 "
	                   "of Nemesis's last town and logs a minute later.",
	    .expected = "The creature's coming makes the stones indestructible and plays the scene: the worshippers made "
	                "by their stones, each stone's ray on the shield, the shield cast, the advisors' five lines. Each "
	                "worshipper killed takes his ray off and the shield shrinks; with none left it goes, and the evil "
	                "advisor's line follows. By Nemesis's town his meteors and his shield answer the creature.",
	    .commands = std::move(commands),
	    .onLand = true,
	});

	// the three stones moved off their spots before the creature comes, as the player's hand would carry them
	std::vector<Command> moved {
	    ClickTexts(),
	    WaitTask("ThrowThroughShield"),
	    LogEvery(k_ShieldGlobals, k_ShieldScripts),
	};
	for (const auto& stone : k_ShieldStones)
	{
		moved.push_back({.kind = Kind::MoveThing,
		                 .delaySeconds = 1.0f,
		                 .point = stone + k_StoneMovedBy,
		                 .thing = ThingNear {.type = script::ObjectType::MobileStatic,
		                                     .subtype = k_StoneSubtype,
		                                     .point = stone,
		                                     .radius = k_StonePickRadius}});
	}
	moved.push_back(PutCreature(k_NearTheShield, 2.0f));
	moved.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, 60.0f));
	all.push_back({
	    .id = "quest.nemesis_shielded_village_stones_moved",
	    .name = "Land 5 quest run: Nemesis's shielded village, its stones moved off their spots first",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start, as quest.nemesis_shielded_village. The texts that wait for a "
	                   "click are clicked on. It moves the three stones off their spots (a shortcut of the player's "
	                   "hand carrying them), then puts the creature within 300 of the shield and logs a minute later.",
	    .expected = "The creature's coming makes the stones indestructible, but with none on its spot the shield's scene "
	                "never plays and the shield never goes up.",
	    .commands = std::move(moved),
	    .onLand = true,
	});

	// once the shield is up, the land's own boulders on the hill east of the third worshipper thrown past him, one
	// after another, so that a flying rock is near him when one of his checks looks
	std::vector<Command> nearMiss {
	    ClickTexts(),
	    WaitTask("ThrowThroughShield"),
	    LogEvery(k_ShieldGlobals, k_ShieldScripts),
	    PutCreature(k_NearTheShield, 2.0f),
	    WaitSaid(k_ShieldSceneLastLine),
	};
	for (const auto& boulder : k_HillBoulders)
	{
		nearMiss.push_back(CameraOn(boulder, k_ThrowEyeAbove, k_ThrowEyeBack, 0.0f));
		// [shortcut] the hand takes the boulder at once, as the player's grab would, wherever its influence reaches
		nearMiss.push_back(
		    {.kind = Kind::HandTake,
		     .delaySeconds = 1.0f,
		     .thing = ThingNear {.type = script::ObjectType::Rock, .point = boulder, .radius = k_BoulderPickRadius}});
		// [shortcut] thrown through the testbed's pointer from the hill, as the player's throw would go
		nearMiss.push_back(CameraOn(k_NearMissThrowFrom, k_ThrowEyeAbove, k_ThrowEyeBack, 0.0f));
		nearMiss.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 1.0f, .point = k_NearMissThrowTo});
		nearMiss.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, k_NearMissWatchSeconds));
	}
	nearMiss.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, 60.0f));
	all.push_back({
	    .id = "quest.nemesis_shielded_village_near_miss",
	    .name = "Land 5 quest run: Nemesis's shielded village, boulders thrown past a worshipper",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start, as quest.nemesis_shielded_village. The texts that wait for a "
	                   "click are clicked on. It puts the creature within 300 of the shield and waits for the shield's "
	                   "scene to end; then, four times, the hand takes one of the land's boulders on the hill east of "
	                   "the third worshipper (a shortcut of the player's grab) and throws it from the hill through the "
	                   "testbed's pointer at the flat land beside him, and it logs a minute later.",
	    .expected = "A boulder flying within 40 of the worshipper when one of his checks looks startles him: he ducks, "
	                "runs to his hiding spot, looks round there and later walks back to dance again.",
	    .commands = std::move(nearMiss),
	    .onLand = true,
	});

	// the same throws for a recording (tools/checks/realinput/nemesis_near_miss.txt): no pointer commands, so the
	// replayed mouse alone takes and throws the boulders; the timeline only clicks the texts, starts the shield and
	// places the camera, in rounds of 15 seconds: 25 above a boulder and 4 to the south, looking down at it, for its
	// take (4 seconds), then over the hill for its throw (8 seconds)
	std::vector<Command> nearMissRecorded {
	    ClickTexts(),
	    WaitTask("ThrowThroughShield"),
	    LogEvery(k_ShieldGlobals, k_ShieldScripts),
	    PutCreature(k_NearTheShield, 2.0f),
	    WaitSaid(k_ShieldSceneLastLine),
	};
	// the fourth, the flat rock, can't be picked up
	for (const auto& boulder : std::span(k_HillBoulders).first(3))
	{
		nearMissRecorded.push_back({.kind = Kind::CameraTo,
		                            .delaySeconds = 2.0f,
		                            .eye = {boulder.x, k_TakeEyeAbove, boulder.y - k_TakeEyeBack},
		                            .look = {boulder.x, 0.5f, boulder.y}});
		auto throwCamera = CameraOn(k_NearMissThrowFrom, k_ThrowEyeAbove, k_ThrowEyeBack, 0.0f);
		throwCamera.delaySeconds = 4.0f;
		nearMissRecorded.push_back(throwCamera);
		nearMissRecorded.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, 8.0f));
		nearMissRecorded.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, 1.0f));
	}
	nearMissRecorded.push_back(LogGlobals(k_ShieldGlobals, k_ShieldScripts, 60.0f));
	all.push_back({
	    .id = "realinput.nemesis_shielded_village_near_miss",
	    .name = "Land 5 quest run for a recording: Nemesis's shielded village, boulders thrown past a worshipper",
	    .facet = Facet::Story,
	    .description = "For tools/checks/real_input_tap.sh, with no pointer commands: played after a quest run's start, "
	                   "as quest.nemesis_shielded_village_near_miss. The texts that wait for a click are clicked on. It "
	                   "puts the creature within 300 of the shield and waits for the shield's scene to end; then, three "
	                   "times, it puts the camera above one of the land's boulders on the hill east of the third "
	                   "worshipper for the recording to take it with the hand, and over the hill for the recording to "
	                   "throw it at the flat land beside him; it logs a minute later.",
	    .expected = "A boulder flying within 40 of the worshipper when one of his checks looks startles him: he ducks, "
	                "runs to his hiding spot, looks round there and later walks back to dance again.",
	    .commands = std::move(nearMissRecorded),
	    .onLand = true,
	});
}

void AddFightToTheDeath(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("TheBigFightMain"),
	    LogEvery(k_FightGlobals, k_FightScripts),
	    // [shortcut] the creature knows the lightning bolt, as one taught it on the lands before would
	    {.kind = Kind::KnowMiracle, .delaySeconds = 1.0f, .value = static_cast<size_t>(MagicType::LightningBolt)},
	    WaitSaid(k_BeforeTheFightLine),
	};
	// In the duel, the player draws the fight's lightning with the mouse again and again, through the same recogniser the
	// cursor goes through; the creature casts it at the mirror, whose fight health it takes, and which never casts by
	// itself
	for (size_t cast = 0; cast < k_LightningCasts; ++cast)
	{
		commands.push_back({.kind = Kind::DrawGesture,
		                    .delaySeconds = cast == 0 ? k_FightUnderWaySeconds : k_SecondsBetweenCasts,
		                    .value = static_cast<size_t>(GestureType::ForkUp)});
		if (cast % k_CastsBetweenDrops == 0 && cast / k_CastsBetweenDrops < k_FightHealthDrops)
		{
			// [shortcut] the mirror's fight health to none, as a script's SET_PROPERTY of it does
			commands.push_back(
			    {.kind = Kind::ScriptOpponentProperty, .delaySeconds = 0.5f, .value = k_FightHealthProperty, .amount = 0.0f});
		}
	}
	commands.push_back(LogGlobals(k_FightGlobals, k_FightScripts, 1.0f));
	// [shortcut] once the fight is won and Nemesis has had his say, the creature put on the arena's middle, as the leash
	// would lead it there, for the walk up the volcano
	commands.push_back(WaitSaid(k_AfterTheFightLastLine));
	commands.push_back(PutCreature(k_TheArena, 3.0f));
	// The land's control script runs the end's scripts one after another, each once the one before has ended
	// The fight's script ends as soon as the creature stands there, often before this wait has seen it run: it is
	// waited on until no task of it runs
	commands.push_back(WaitTask("TheBigFightMain", TaskWait::NotRunning));
	commands.push_back(StartScript("EndOfGameCreatureSequenceGoToVolcano"));
	commands.push_back(WaitTask("EndOfGameCreatureSequenceGoToVolcano", TaskWait::Finished));
	commands.push_back(StartScript("NemesisCitadelDestroyed"));
	// [shortcut] the creature, back burnt by the Greek town, healed, as the player's heal would heal it
	commands.push_back(WaitSaid(k_CreatureBackLastLine));
	commands.push_back({.kind = Kind::ScriptSetHealth,
	                    .delaySeconds = 2.0f,
	                    .amount = 1.0f,
	                    .thing = ThingNear {.type = script::ObjectType::Creature,
	                                        .point = k_WhereTheCreatureLies,
	                                        .radius = k_CreaturePickRadius}});
	commands.push_back(WaitTask("NemesisCitadelDestroyed", TaskWait::Finished));
	commands.push_back(StartScript("VillageWavingSequence"));
	// at the credits' end, once the scene holds, the player presses the space bar
	commands.push_back(
	    {.kind = Kind::WaitCamera, .waitUntilFree = true, .point = k_CreditsLastShot, .radius = k_CreditsShotRadius});
	for (size_t press = 0; press < k_SpacePresses; ++press)
	{
		commands.push_back(KeyDown(SDL_SCANCODE_SPACE, press == 0 ? 10.0f : k_SecondsBetweenPresses));
		commands.push_back(KeyUp(SDL_SCANCODE_SPACE, 0.5f));
	}
	commands.push_back(WaitTask("VillageWavingSequence", TaskWait::Finished));
	commands.push_back(LogGlobals(k_FightGlobals, k_FightScripts, 1.0f));
	all.push_back({
	    .id = "quest.so_this_is_a_fight_to_the_death",
	    .name = "Land 5 quest run: So this is a fight to the death",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/TheBigFightMain on Land5.txt); "
	                   "it starts the volcano walk, the citadel's fall and the credits in turn, each once the one before "
	                   "has ended, as the land's control script does. The texts that wait for a click are clicked on. The "
	                   "creature is taught the lightning bolt; the cinema leaves it within 300 of the arena, which starts "
	                   "the fight. In the duel the fight's lightning is drawn with the mouse every 5 seconds, and every "
	                   "third time, five times, the mirror's fight health is set to none. Once the fight is won the "
	                   "creature is put on the arena's middle; once it is back by the Greek town it is healed; at the "
	                   "credits' end the space bar is pressed.",
	    .expected = "The creature casts the lightning at the mirror; Nemesis's heals bring its fight health back four "
	                "times, then the next cast knocks it out and the fight is won. His farewell, the walk up the "
	                "volcano and the film, the citadel's fall, the creature's return and the credits follow, and the "
	                "space bar ends them.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestLand5Scenarios(std::vector<Scenario>& all)
{
	AddShieldedVillage(all);
	AddFightToTheDeath(all);
}
