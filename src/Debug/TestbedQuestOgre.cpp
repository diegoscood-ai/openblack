/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// Land 1's silver scroll The Ogre played past its introduction, as land timelines on the game's own Land 1 with the
// SkipBox answered 3 (docs/wiki/project/scenarios.md, "Playing a timeline on a land"). The skip path never runs the
// guide's fight lesson, so each raises the flag that lesson raises before the ogre is made. After the scroll's tap and
// the introduction, the player either feeds the ogre twice or has the creature fight him. The script looks every three
// seconds for a pile of miracle food within 50 of him while the creature is not within 50 of it, and starts a fight
// whenever the creature comes within 30 of him. So either the creature is kept at home and the hand pours food on him,
// twice (the good ending); or the creature walks up to him, up to three times. Either way, once he is asleep or beaten,
// the hand taps the reward's chest behind him. The points are map coordinates, taken from the places the quest's script
// names and from where the ogre sleeps once the introduction is over.

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "Enums.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedLandTimeline.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::ThingNear;

/// The wait for a task of a script: running
constexpr size_t k_Running = 0;
/// The species of creature the ogre is made as, as the scripts number them
constexpr uint32_t k_OgreSpecies = 13;

/// The scroll at the pass
constexpr glm::vec2 k_OgreScroll {2553.5718f, 3184.6072f};
/// Where the creature walks to from the temple to fight him: about 21 from where he lies down once the introduction is
/// over (about 2530.5, 3094.5, south of his lair), on the side of the pass it comes from
constexpr glm::vec2 k_BeforeTheOgre {2510.0f, 3092.0f};
/// The hollow at the far end of the pass where the reward's chest stands
constexpr glm::vec2 k_Rewarded {2514.21f, 3368.67f};

/// The reward's chest, a beach ball's, in its hollow
constexpr ThingNear k_RewardChest {.type = script::ObjectType::Reward, .point = k_Rewarded, .radius = 10.0f};
/// The ogre, wherever he sleeps between his lair and the pass's mouth: about 33 from either place
constexpr ThingNear k_Ogre {
    .type = script::ObjectType::Creature, .subtype = k_OgreSpecies, .point = {2534.6f, 3127.5f}, .radius = 60.0f};

/// How many times the creature is sent up to the ogre to fight him, each fight lowering his strength
constexpr size_t k_FightAttempts = 3;
/// The blows clicked in each attempt, one every few seconds, high, in the middle and low in turn; left alone in a fight
/// the creature fights by itself
constexpr size_t k_BlowsPerAttempt = 40;
constexpr float k_SecondsBetweenBlows = 3.0f;
constexpr float k_BlowChargeMs = 400.0f;

Command WaitTask(std::string_view script)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = k_Running, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

Command WaitGlobal(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::Equal};
}

/// Logs the globals and scripts named, some seconds after the command before it, and again every period when given one
Command LogGlobals(std::string_view globals, std::string_view scripts, float delay = 0.0f, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = globals, .script = scripts};
}

/// The player's creature walks to a point of the land once it is free: done picking up or putting down, or arrived
Command WalkTo(glm::vec2 point, float delay)
{
	return {.kind = Kind::WalkTo, .delaySeconds = delay, .waitUntilFree = true, .point = point};
}

/// The quest's globals and its scripts, logged at the end and every 20 seconds meanwhile, so a stalled run shows where
/// each script waits
constexpr std::string_view k_Globals = "CreatureDevelopment_Fight CreatureGuardianInitiated CreatureGuardianFinished "
                                       "EndSendMyCreatureAwayFromGuardianFlag FightOver GlobalHealSpell";
constexpr std::string_view k_Scripts = "CreatureGuardian CreatureGuardianFedOnce CreatureGuardianFedTwice "
                                       "CreatureGuardianFight CreatureGuardianYouWin CreatureGuardianYouRetire "
                                       "HealFightVictorOverTime GiveSpellDispenserReward";

/// The camera put 70 south of a point and 45 up, looking at it
Command CameraSouthOf(glm::vec2 point, float delay)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = delay,
	        .eye = {point.x, 45.0f, point.y - 70.0f},
	        .look = {point.x, 5.0f, point.y}};
}

/// The flag the guide's fight lesson raises, the scroll's tap (the hand on it, then the Action button pressed once the
/// hand is over it, as the scroll's click test asks), the introduction, and its end, when the script stops sending the
/// creature away from the ogre
std::vector<Command> UpToTheIntroductionsEnd()
{
	return {
	    {.kind = Kind::ClickTexts, .value = 1},
	    WaitTask("CreatureGuardian"),
	    {.kind = Kind::SetGlobal, .amount = 1.0f, .global = "CreatureDevelopment_Fight"},
	    CameraSouthOf(k_OgreScroll, 1.0f),
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_OgreScroll, .value = 1},
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = 3,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_OgreScroll, .radius = 15.0f}},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	    WaitText(1469, true),
	    LogGlobals(k_Globals, k_Scripts, 1.0f, 20),
	    WaitGlobal("EndSendMyCreatureAwayFromGuardianFlag", 1.0f),
	};
}

/// A meal for the ogre: a food seed given to the hand as a bubble's would be (the player's worship on this path offers
/// none), then poured on him with the Action button held for three seconds. The script's walk to the food ends within
/// his height of it and then waits for him to be within 10 of it, so the food is poured on him, wherever he lies: the
/// camera is brought behind him and let go, the pointer put on him and the button pressed once the hand is over him
std::vector<Command> PourAMeal(float delay)
{
	return {
	    {.kind = Kind::HoldSeed, .delaySeconds = delay, .value = static_cast<size_t>(SpellSeedType::Food)},
	    {.kind = Kind::CameraFollow, .delaySeconds = 1.0f, .amount = 1.5f, .thing = k_Ogre},
	    {.kind = Kind::CameraFollow, .delaySeconds = 2.0f, .value = 1},
	    {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .thing = k_Ogre},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_Ogre},
	    {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3},
	};
}

/// The hand taps the reward's chest open with the Action button, the camera looking at it from the south, away from the
/// ogre's lair
std::vector<Command> TapTheRewardChest(float delay)
{
	return {
	    CameraSouthOf(k_Rewarded, delay),
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_RewardChest},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_RewardChest},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	};
}

void Append(std::vector<Command>& commands, const std::vector<Command>& more)
{
	commands.insert(commands.end(), more.begin(), more.end());
}
} // namespace

void testbed_scenarios::AddQuestOgreScenarios(std::vector<Scenario>& all)
{
	{
		auto commands = UpToTheIntroductionsEnd();
		// Given back to the player, the creature is kept at home by its pen, far from the food
		commands.push_back({.kind = Kind::ConfineToHome, .delaySeconds = 15.0f, .radius = 20.0f});
		Append(commands, PourAMeal(5.0f));
		commands.push_back(WaitText(1470, true));
		// once the first meal's film is over and he has gone back to his lair, food is poured for him again
		Append(commands, PourAMeal(10.0f));
		commands.push_back(WaitText(1471, true));
		commands.push_back(WaitGlobal("CreatureGuardianFinished", 2.0f));
		Append(commands, TapTheRewardChest(3.0f));
		commands.push_back(WaitGlobal("CreatureGuardianInitiated", 2.0f));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
		all.push_back({
		    .id = "story.q_ogre",
		    .name = "Land 1: The Ogre (fed twice)",
		    .facet = Facet::Story,
		    .description = "Raises the flag the guide's fight lesson raises, which the skip path never runs, taps the "
		                   "ogre's scroll and waits for the introduction's end. The creature is kept at home; twice, a "
		                   "food seed is given to the hand and poured on the ogre; then the hand taps the reward's chest "
		                   "behind him.",
		    .expected = "Each time the food is there and the creature far from it, the ogre walks up to it and eats it in "
		                "a film (texts 1470, then 1471); after the second he falls asleep, the camera flies through the "
		                "pass to the reward (CreatureGuardianFinished=2) and he is removed once out of sight. The tapped "
		                "chest gives a beach ball; a healing miracle dispenser is built near the temple (texts 5691, "
		                "5692) and CreatureGuardianInitiated=2.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = UpToTheIntroductionsEnd();
		constexpr std::array<size_t, 3> k_Bands {0, 1, 2};
		for (size_t attempt = 0; attempt < k_FightAttempts; ++attempt)
		{
			// The creature walks up to the ogre; within 30 of him the fight starts, once 30 seconds have passed since
			// the last. (A leash tied to him starts no fight: a creature under a script's control refuses one.)
			commands.push_back(WalkTo(k_BeforeTheOgre, attempt == 0 ? 15.0f : 35.0f));
			for (size_t blow = 0; blow < k_BlowsPerAttempt; ++blow)
			{
				commands.push_back({.kind = Kind::FightBlow,
				                    .delaySeconds = k_SecondsBetweenBlows,
				                    .value = k_Bands.at(blow % k_Bands.size()),
				                    .chargeMs = k_BlowChargeMs});
			}
			// Both endings of a fight set FightOver once their film is over; once he is beaten, the next attempts find
			// it set
			commands.push_back(WaitGlobal("FightOver", 1.0f));
			commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
		}
		// The chest can be reached only once he is beaten, when an influence circle replaces the one keeping the hand
		// away from it
		commands.push_back(WaitGlobal("CreatureGuardianFinished", 1.0f));
		Append(commands, TapTheRewardChest(3.0f));
		commands.push_back(WaitGlobal("CreatureGuardianInitiated", 2.0f));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
		all.push_back({
		    .id = "story.q_ogre_fight",
		    .name = "Land 1: The Ogre (fought)",
		    .facet = Facet::Story,
		    .description = "As story.q_ogre up to the introduction's end; then the creature walks from the temple to the "
		                   "ogre and fights him, the player clicking blows high, in the middle and low, up to three fights "
		                   "half a minute apart at least; once he is beaten the hand taps the reward's chest.",
		    .expected = "Within 30 of him the creature is held, its leash switched off and taken off, the evil advisor "
		                "warns (text 1472) and the ogre fights it, his strength lowered by 0.2 each fight. Won (less "
		                "fight health left in him): he is killed for good (text 1473), the camera flies through the pass "
		                "(CreatureGuardianFinished=1), the chest opens to a beach ball and a healing miracle dispenser is "
		                "built near the temple (texts 5691, 5692, CreatureGuardianInitiated=2). Lost: both stand idle "
		                "(text 1474), he goes back to his lair and heals, and the next attempt fights him again.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
}
