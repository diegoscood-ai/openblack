/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Idol as quest runs on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"), each played after a quest run's start (quest=SetupLand2/EnterLand2): once the arrival is
// over, the quest's script is started as the land's control script starts it once the player holds the idol's town
// and five more, which the run does not wait for. The timeline drives it as a player would: the scroll by the idol is
// pressed on with the hand and the introduction is let play; then the idol is set alight and kept burning, in place
// of the player's fire, until the script takes it as burnt, and the ending and the reward are let play. While the fire
// burns the camera is over the town, so the ending's scene waits for it to come back to the idol, and its first line is
// watched for as soon as it does. One run first lets the watch go on for three minutes, so the first worshippers set
// out; one kills the idol's maker before the fire, and one after it; one lets the worshippers reach the idol and kills
// one of them; one throws trees at the idol, lets a short fire go out and burns a tree by the idol with fireballs. The
// points are map coordinates, taken from the places the quest's script and the land's script name.

#include <cstddef>
#include <cstdint>

#include <array>
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
/// The idol's spot, where the scroll is put up and the idol made; the maker's spot, about 12 in front of it
constexpr glm::vec2 k_Idol {2695.73f, 3662.4f};
constexpr glm::vec2 k_Maker {2698.915f, 3650.481f};
/// The spot by the town's centre the script finds the worshippers' town at, about 250 from the idol: with the camera
/// over it, it is too far from the idol for the ending's scene to start
constexpr glm::vec2 k_TownCentre {2880.96f, 3494.28f};
/// The idol, a mobile static of the idol's info, and its maker, an Indian trader villager, as the script makes them
constexpr uint32_t k_IdolInfo = 10;
constexpr uint32_t k_IndianTrader = 34;
constexpr ThingNear k_IdolPick {
    .type = script::ObjectType::MobileStatic, .subtype = k_IdolInfo, .point = k_Idol, .radius = 5.0f};
constexpr ThingNear k_MakerPick {
    .type = script::ObjectType::Villager, .subtype = k_IndianTrader, .point = k_Maker, .radius = 5.0f};
/// The challenge's number, which its entry in the challenge log goes by
constexpr size_t k_Challenge = 10;
/// The camera while the watch runs: well back and up from the idol, so the idol's spot stays on the screen and the
/// camera within the 150 the ending waits for
constexpr float k_WatchEyeHeight = 40.0f;
constexpr float k_WatchEyeBack = 60.0f;
/// The introduction's last camera move, the snapshot after it and the end of its scene, after its last line
constexpr float k_IntroductionEndSeconds = 15.0f;
/// How long the watch is let run before the fire: past the 1801 passes of a tenth of a second after which the first
/// worshippers are sent
constexpr float k_WorshippersSeconds = 190.0f;
/// The fire kept going: the idol set alight again every 2 seconds, so each of the script's looks, 2.1 seconds apart,
/// finds it burning, for longer than the 16 looks in a row that burn the idol
constexpr int k_FireTimes = 24;
constexpr float k_FireEverySeconds = 2.0f;

/// The worshippers' run: the watch let run until the first two worshippers, sent after 1801 passes, have walked the
/// 320 or so from the town at the speed the script sets; then until the next one, sent 1801 passes later, has too
constexpr float k_FirstWorshippersArrivedSeconds = 260.0f;
constexpr float k_NextWorshipperArrivedSeconds = 180.0f;
/// Where the first worshipper sent walks to: a random spot within 8 either way of the maker's, as a quest run's draws
/// fall (the run of quest.the_idol drew this one), 7 from the maker
constexpr ThingNear k_FirstWorshipperPick {
    .type = script::ObjectType::Villager, .point = {2698.915f, 3657.481f}, .radius = 3.0f};

/// The tries' run: four trees within 35 of the idol, where the land's script plants them, each taken and thrown at the
/// idol in turn
constexpr std::array<glm::vec2, 4> k_ThrownTrees {{
    {2688.01f, 3630.77f},
    {2685.96f, 3631.23f},
    {2713.36f, 3632.10f},
    {2716.97f, 3630.20f},
}};
/// The tree the fireballs are thrown at, about 21 from the idol's spot and 22 from the maker's
constexpr glm::vec2 k_BurntTree {2677.17f, 3652.85f};
/// The player's influence round the idol, as the town the player holds by then would give it: the hand takes and
/// throws only within it
constexpr float k_IdolInfluenceRadius = 60.0f;
/// The camera while the trees are thrown: far enough back and up for both the trees and the idol to be on the screen
constexpr float k_ThrowEyeHeight = 60.0f;
constexpr float k_ThrowEyeBack = 110.0f;
/// How long the hand holds the Action button down on a thing to take it
constexpr float k_GrabHoldSeconds = 0.6f;
/// Between two throws: past two of the script's looks, so each hit is counted at its own look
constexpr float k_BetweenThrowsSeconds = 4.5f;
/// The short fire: the idol set alight three times 2 seconds apart, so at least two looks find it, then put out
constexpr int k_ShortFireTimes = 3;
/// The fireballs at the tree, 5 seconds apart, for the 16 looks in a row that burn the idol
constexpr int k_Fireballs = 7;
constexpr float k_BetweenFireballsSeconds = 5.0f;

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "IdolAlive";
constexpr std::string_view k_Scripts = "IdolPyreMain ChallengeHighlightNotify StandardReminder";

/// Logs the globals and scripts, some seconds after the command before it, and again every period if given
Command Log(float delay, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = k_Globals, .script = k_Scripts};
}

/// The arrival let play, the quest started as the land's control script starts it, its scroll pressed on with the hand
/// and its introduction let play to its last line
std::vector<Command> Introduction()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    StartScript("IdolPyreMain"),
	    // the scroll's notifier runs once the quest has made the idol and the scroll
	    WaitTask("ChallengeHighlightNotify"),
	    Log(0.0f, 30),
	    // [camera] the scroll by the idol
	    CameraOn(k_Idol, k_EyeAbove, k_EyeBack, 5.0f),
	};
	// [real] the scroll pressed on
	PressScroll(commands, k_Idol);
	commands.push_back(WaitText(1681));
	// [camera] the idol from well back once the introduction's scene is over
	commands.push_back(CameraOn(k_Idol, k_WatchEyeHeight, k_WatchEyeBack, 5.0f));
	commands.back().delaySeconds = k_IntroductionEndSeconds;
	commands.push_back(Log(1.0f));
	return commands;
}

/// [shortcut] The idol set alight by a script's SET_ON_FIRE, again and again, in place of the fire the player would
/// start and keep going near it, until the script takes it as burnt. [camera] Meanwhile the camera is over the town, so
/// the ending's scene, which waits for the idol's spot to be on the screen with the camera near it, waits for the
/// camera to come back
void KeepTheIdolBurning(std::vector<Command>& commands)
{
	commands.push_back(CameraOn(k_TownCentre, k_WatchEyeHeight, k_WatchEyeBack, 5.0f));
	for (int i = 0; i < k_FireTimes; ++i)
	{
		commands.push_back({.kind = Kind::ScriptSetOnFire,
		                    .delaySeconds = i == 0 ? 1.0f : k_FireEverySeconds,
		                    .value = 1,
		                    .amount = 1.0f,
		                    .thing = k_IdolPick});
	}
	commands.push_back(WaitGlobal("IdolAlive", 1.0f));
	commands.push_back(Log(0.5f));
}

/// [camera] The camera back on the idol from well back, so the ending's scene starts, and its first line, the good
/// advisor's, watched for at once
void BackToTheIdol(std::vector<Command>& commands)
{
	commands.push_back(CameraOn(k_Idol, k_WatchEyeHeight, k_WatchEyeBack, 5.0f));
	commands.push_back(WaitText(1686));
}

/// The ending let play to the reward's last line and the script's end
void RewardAndEnd(std::vector<Command>& commands)
{
	commands.push_back(WaitText(6109));
	commands.push_back(WaitTask("IdolPyreMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
}

std::vector<Command> IdolBurnt()
{
	auto commands = Introduction();
	// the watch goes on until the first worshippers set out from the town
	commands.push_back(Log(k_WorshippersSeconds));
	KeepTheIdolBurning(commands);
	// the ending: the idol's last scene, the maker's plea, the evil advisor's remark and the reward's two lines
	BackToTheIdol(commands);
	commands.push_back(WaitText(1685));
	RewardAndEnd(commands);
	// the challenge's entry tapped in the challenge log: its reminder, the good advisor's line
	commands.push_back({.kind = Kind::ReplayChallenge, .delaySeconds = 2.0f, .value = k_Challenge});
	commands.push_back(WaitText(2483));
	commands.push_back(Log(2.0f));
	return commands;
}

std::vector<Command> MakerKilled()
{
	auto commands = Introduction();
	// [shortcut] the maker killed by a script's SET_PROPERTY of his health, in place of the player's blow
	commands.push_back({.kind = Kind::ScriptSetHealth, .delaySeconds = 2.0f, .amount = 0.0f, .thing = k_MakerPick});
	// the advisors' two lines on his death
	commands.push_back(WaitText(1683));
	commands.push_back(Log(1.0f));
	KeepTheIdolBurning(commands);
	// the ending with the maker dead: the idol's last scene, the good advisor's line again and the reward's two lines
	BackToTheIdol(commands);
	RewardAndEnd(commands);
	return commands;
}

std::vector<Command> MakerKilledLate()
{
	auto commands = Introduction();
	KeepTheIdolBurning(commands);
	// [shortcut] the maker killed by a script's SET_PROPERTY of his health once the idol is done for, while the script
	// waits for the idol to be seen, in place of the idol's own fire or the player's blow
	commands.push_back({.kind = Kind::ScriptSetHealth, .delaySeconds = 1.0f, .amount = 0.0f, .thing = k_MakerPick});
	commands.push_back(Log(1.0f));
	// the ending with the maker dead: the idol's last scene, the good advisor's line on him and the reward's two lines
	BackToTheIdol(commands);
	RewardAndEnd(commands);
	return commands;
}

std::vector<Command> WorshippersCome()
{
	auto commands = Introduction();
	// the watch goes on until the first two worshippers have reached the maker
	commands.push_back(Log(k_FirstWorshippersArrivedSeconds));
	// [shortcut] the first of them killed by a script's SET_PROPERTY of his health, in place of the player's blow: the
	// good advisor's line on it
	commands.push_back({.kind = Kind::ScriptSetHealth, .delaySeconds = 1.0f, .amount = 0.0f, .thing = k_FirstWorshipperPick});
	commands.push_back(Log(5.0f));
	// the watch goes on until the next worshipper, sent alone, has walked there too
	commands.push_back(Log(k_NextWorshipperArrivedSeconds));
	KeepTheIdolBurning(commands);
	// the ending with worshippers in the flock: the idol's last scene, the worshippers' prayer with the maker's plea,
	// the evil advisor's remark and the reward's two lines
	BackToTheIdol(commands);
	commands.push_back(WaitText(1688));
	commands.push_back(WaitText(1685));
	RewardAndEnd(commands);
	return commands;
}

/// [real] A tree taken and thrown at the idol: the pointer on the tree, the Action button pressed once the hand is over
/// it and held past the grab's wait, which uproots it, and let go, the tree kept in the hand; then the hand's own throw
/// at the idol: the button pressed again, the hand moved towards the idol and the button let go as it moves. The
/// run-up's speed is worked out for a flight under gravity without the air's drag, so the tree lands near the idol;
/// that it hits the idol is unverified
void ThrowTreeAtTheIdol(std::vector<Command>& commands, glm::vec2 tree)
{
	const ThingNear pick {.type = script::ObjectType::Tree, .point = tree, .radius = 1.0f};
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 0.5f, .thing = pick});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .waitUntilFree = true, .value = k_ActionButton, .thing = pick});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 0.5f, .point = k_Idol});
}

/// [shortcut] A fireball given to the hand, as if from a bubble, then [real] thrown at the tree by the idol from within
/// the player's influence, through the mouse as the player throws
void ThrowFireballAtTheTree(std::vector<Command>& commands, float delay)
{
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = delay, .value = static_cast<size_t>(SpellSeedType::Fire)});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_BurntTree, .value = 2});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 1.0f, .point = k_BurntTree});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .value = 1});
}

std::vector<Command> IdolTried()
{
	auto commands = Introduction();
	// [shortcut] the player's influence round the idol, as the town the player holds by then would give it
	commands.push_back({.kind = Kind::ScriptInfluence, .delaySeconds = 1.0f, .point = k_Idol, .radius = k_IdolInfluenceRadius});
	// [camera] the trees and the idol from well back
	commands.push_back(CameraOn(k_Idol, k_ThrowEyeHeight, k_ThrowEyeBack, 0.0f));
	// four trees thrown at the idol, each past two of the script's looks after the last: the third hit counted brings
	// the good advisor's hint
	for (const auto& tree : k_ThrownTrees)
	{
		ThrowTreeAtTheIdol(commands, tree);
		commands.push_back(Log(k_BetweenThrowsSeconds));
	}
	// [shortcut] a short fire: the idol set alight by a script's SET_ON_FIRE for two or three looks, then put out, in
	// place of a fire the player would start and let die: the evil advisor's line on it
	for (int i = 0; i < k_ShortFireTimes; ++i)
	{
		commands.push_back({.kind = Kind::ScriptSetOnFire,
		                    .delaySeconds = i == 0 ? 1.0f : k_FireEverySeconds,
		                    .value = 1,
		                    .amount = 1.0f,
		                    .thing = k_IdolPick});
	}
	commands.push_back({.kind = Kind::ScriptSetOnFire, .delaySeconds = 1.0f, .value = 0, .thing = k_IdolPick});
	commands.push_back(Log(8.0f));
	// [camera] the tree by the idol; fireballs thrown at it until it has burnt for the 16 looks in a row
	commands.push_back(CameraOn(k_BurntTree, k_WatchEyeHeight, k_WatchEyeBack, 0.0f));
	for (int i = 0; i < k_Fireballs; ++i)
	{
		ThrowFireballAtTheTree(commands, i == 0 ? 1.0f : k_BetweenFireballsSeconds);
	}
	// [camera] over the town until the idol is done for, then back to it for the ending
	commands.push_back(CameraOn(k_TownCentre, k_WatchEyeHeight, k_WatchEyeBack, 5.0f));
	commands.push_back(WaitGlobal("IdolAlive", 1.0f));
	commands.push_back(Log(0.5f));
	BackToTheIdol(commands);
	RewardAndEnd(commands);
	return commands;
}
} // namespace

void testbed_scenarios::AddQuestIdolScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_idol",
	    .name = "Land 2 quest run: The Idol, the idol burnt",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. Once the arrival is over it starts IdolPyreMain, "
	                   "as the land's control script does; presses on the scroll by the idol with the hand and lets the "
	                   "introduction play; lets the watch run for three minutes; then, with the camera over the town, "
	                   "sets the idol alight every 2 seconds by a script's SET_ON_FIRE, standing in for the player's "
	                   "fire, until the script takes it as burnt; brings the camera back to the idol and lets the ending "
	                   "and the reward play; then taps the challenge's entry in the challenge log.",
	    .expected = "The idol and the scroll are made; the press starts the introduction: the maker made and praying, "
	                "texts 1678 to 1681, the snapshot at 0 and the idol's chant. After three minutes the first two "
	                "worshippers set out from the town. With the idol burning, 16 looks in a row find fire "
	                "(IdolAlive=1): the idol is set on fire for good, and the script waits while the camera is away. "
	                "Once it is back: the ending's snapshot at 1 with alignment 0.2, text 1686, the bangs and flashes "
	                "and the idol deleted, the maker's plea (1687, or 1688 with worshippers), the scroll deleted, the "
	                "evil advisor's 1685, the fireball's two levels given to the town and texts 6108 and 6109; the "
	                "log's entry replayed says text 2483.",
	    .commands = IdolBurnt(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_idol_maker_dead",
	    .name = "Land 2 quest run: The Idol, the maker killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_idol does, then kills the idol's maker by a script's "
	                   "SET_PROPERTY of his health, standing in for the player's blow, and keeps the idol burning as "
	                   "quest.the_idol does until the script takes it as burnt.",
	    .expected = "The maker's death brings texts 1684 and 1683 and the log updated to 0.5 with alignment -0.6; the "
	                "idol then burns as in quest.the_idol, the ending skips the maker's plea, and the good advisor "
	                "repeats 1684 with the log updated to 1 with alignment -1.0 before the reward's texts 6108 and "
	                "6109.",
	    .commands = MakerKilled(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_idol_maker_dead_late",
	    .name = "Land 2 quest run: The Idol, the maker killed once the idol is done for",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_idol does and keeps the idol burning with the camera over "
	                   "the town until the script takes it as burnt; then, while the script waits for the idol to be "
	                   "seen, kills the idol's maker by a script's SET_PROPERTY of his health, standing in for the "
	                   "idol's fire or the player's blow, and brings the camera back.",
	    .expected = "No line on the maker's death; the ending's snapshot at 1 with alignment 0.2, text 1686, no plea, "
	                "the good advisor's 1684 and the log updated to 1 with alignment -0.4, then texts 6108 and 6109.",
	    .commands = MakerKilledLate(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_idol_worshippers",
	    .name = "Land 2 quest run: The Idol, worshippers come and one is killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_idol does; lets the watch run until the first two "
	                   "worshippers have walked to the maker; kills the first of them by a script's SET_PROPERTY of his "
	                   "health, standing in for the player's blow; lets the watch run until the next worshipper, sent "
	                   "alone, has walked there too; then keeps the idol burning as quest.the_idol does and lets the "
	                   "ending play.",
	    .expected = "Each worshipper standing on his spot is added to the flock, turns to the idol and prays; the death "
	                "brings text 1689; the next send is of one villager. The ending's worshippers turn to the camera "
	                "and pray with the maker's 1688, and are released before the evil advisor's 1685 and the reward's "
	                "texts. Unverified: that a walker ever stands exactly on his spot, and the spot the first one "
	                "draws.",
	    .commands = WorshippersCome(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_idol_tries",
	    .name = "Land 2 quest run: The Idol, battered, a short fire, then a tree burnt by fireballs",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_idol does; puts the player's influence round the idol, "
	                   "standing in for the town the player holds by then; takes four trees by the idol with the hand "
	                   "and throws each at the idol, past two of the script's looks apart; sets the idol alight by a "
	                   "script's SET_ON_FIRE for two or three looks and puts it out; then throws fireballs, given to "
	                   "the hand as if from a bubble, at a tree 21 from the idol until the script takes the idol as "
	                   "burnt, and lets the ending play.",
	    .expected = "Each hit on the idol is counted at the next look with no fire, and the third brings text 1692; "
	                "the fire seen at two looks or more and gone brings text 1691; the burning tree is found by the "
	                "fire question until IdolAlive=1, and the ending plays to texts 6108 and 6109. Unverified: that "
	                "the thrown trees hit the idol, and how long the tree burns.",
	    .commands = IdolTried(),
	    .onLand = true,
	});
}
