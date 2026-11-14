/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A skirmish's end as land timelines (docs/wiki/project/scenarios.md, "Playing a timeline on a land"), played after a
// quest run's start on the game's own Land 1: the timeline starts a playground land as a skirmish, as the skirmish
// box's Start Game does, and a god's temple then falls by a script's deletion of its heart with the temple's explosion,
// a shortcut for however a player would destroy it. The skirmish's own rule then decides who is out and how the game
// ends: an out god's message, the end box with its won or lost line, and Watch Game while two or more gods are left.

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// Seconds after the skirmish's start before a temple falls: its first turns played
constexpr float k_SettleSeconds = 10.0f;
/// Seconds a temple takes to be destroyed, and the end box a little after it: the run waits this long after a fall
constexpr float k_FallSeconds = 30.0f;

/// The skirmish started on a playground land (a script name with static storage, which the command keeps a view of)
Command Start(std::string_view land)
{
	return {.kind = Kind::StartSkirmish, .delaySeconds = 2.0f, .landScript = land};
}

/// [shortcut] A god's temple deleted by a script with the temple's explosion, where a player would destroy it
Command Fall(PlayerNames god, float delay)
{
	return {.kind = Kind::ScriptDeleteTemple, .player = god, .delaySeconds = delay};
}

/// The run kept going a while after the last fall, so that the destruction, the out message and the end box are in
/// its log
Command After(float delay)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .global = "GameEnded"};
}
} // namespace

void testbed_scenarios::AddQuestSkirmishEndScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.skirmish_won",
	    .name = "Skirmish: Two Gods, the other god's temple falls",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start on Land1.txt. Starts Playgrounds/TwoGods.txt as a skirmish, as "
	                   "the skirmish box's Start Game does; ten seconds in, the other god's temple is deleted by a "
	                   "script with the temple's explosion (a shortcut), and the run goes on 30 seconds.",
	    .expected = "The other god has no temple once its heart is destroyed, so it is out: its mind stops and its "
	                "creature goes; with no other god left the game ends and the end box opens with "
	                "\"Congratulations! You've won the game!\".",
	    .commands = {Start("Playgrounds/TwoGods.txt"), Fall(PlayerNames::PLAYER_TWO, k_SettleSeconds), After(k_FallSeconds)},
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.skirmish_out_then_lost",
	    .name = "Skirmish: Three Gods, a rival out, then the player's temple falls",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start on Land1.txt. Starts Playgrounds/ThreeGods.txt as a skirmish; "
	                   "ten seconds in, player two's temple is deleted by a script with the temple's explosion, and 30 "
	                   "seconds later the player's own (shortcuts); the run goes on 30 seconds.",
	    .expected = "Player two is out: \"<name> is out of the game.\" is shown and the game goes on. Then the "
	                "player is out with one god left: the game ends and the end box says \"You have lost the game.\".",
	    .commands = {Start("Playgrounds/ThreeGods.txt"), Fall(PlayerNames::PLAYER_TWO, k_SettleSeconds),
	                 Fall(PlayerNames::PLAYER_ONE, k_FallSeconds), After(k_FallSeconds)},
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.skirmish_lost_watch",
	    .name = "Skirmish: Three Gods, the player's temple falls first",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start on Land1.txt. Starts Playgrounds/ThreeGods.txt as a skirmish; "
	                   "ten seconds in, the player's own temple is deleted by a script with the temple's explosion (a "
	                   "shortcut); the run goes on 30 seconds.",
	    .expected = "The player is out with two gods left: the end box says \"You have lost the game.\" with Watch "
	                "Game and Leave Game.",
	    .commands = {Start("Playgrounds/ThreeGods.txt"), Fall(PlayerNames::PLAYER_ONE, k_SettleSeconds), After(k_FallSeconds)},
	    .onLand = true,
	});
}
