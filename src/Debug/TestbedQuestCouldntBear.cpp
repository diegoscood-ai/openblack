/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 3's main gold quest, "So You Couldn't Bear to Be Without Your Creature?", as a quest run on the game's own Land 3
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"): the land entry's quest= starts the land's setup,
// then the creature's prison in the background and the arrival, in the order the land's control script starts them.
// The timeline lets the arrival play as a player would watch it (its click-through lines clicked on), then puts the
// camera over the prison on the hill and logs the prison's state while the pillars hold the creature. Then the three
// villages that feed the pillars are wiped out one at a time, their buildings and their people, which the quest counts
// as a village won (a pillar reads a town with no building standing as the player's). A village whose people survive
// rebuilds and reads as standing again, its pillar rising. Handing a town over without the belief that wins it does not
// hold: the town's next belief count gives it back to the strongest believer. Each pillar's fall and its cut scene are
// waited on, the camera over the pillar and then back over its village, before the next village goes. Five minutes
// after the creature is freed, Lethys's last town is given belief in the player, enough to put it in trouble for the
// creed scene; after the scene it is given the belief that wins it, for Lethys's end. A second run wins the villages
// by belief instead: a village destroyed reads to the scripts as gone, so only won villages start the fanatics' fire
// attack (one pillar down) and the wolves' attack (two down), which that run lets play, the wolves taken out before
// they reach their village. A third run starts only the land's control script, as loading the land does, and plays
// the won villages, the creed scene and the exit through it. The points are map coordinates taken from the places the
// quest names.

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::TaskWait;

/// The prison spot on the hill, where the quest holds the player's creature between the three pillars
constexpr glm::vec2 k_Prison {2062.2075f, 2259.8464f};
/// Seconds between the logs of the prison's state while the pillars hold the creature: a few of the pillars' 3-second
/// checks each
constexpr float k_PrisonLogSeconds = 10.0f;
/// The three villages whose pillars hold the creature, Japanese, Indian and Egyptian, where the land's setup finds
/// their towns; each is picked within this of its spot
constexpr float k_VillagePickRadius = 20.0f;
constexpr std::array<glm::vec2, 3> k_Villages {{{2829.13f, 2822.8f}, {2831.99f, 2547.2f}, {2250.22f, 2188.63f}}};
/// Where each pillar's cut scene puts the camera to watch the pillar sink, and where it fades back to over the village
/// it was won from (the third pillar's scene goes on to free the creature instead); each camera is waited on within
/// the radius, which takes in its height above the land
constexpr std::array<glm::vec2, 3> k_PillarCameras {{{2073.003f, 2324.33f}, {2121.658f, 2261.63f}, {2020.365f, 2221.719f}}};
constexpr std::array<glm::vec2, 2> k_VillageCameras {{{2867.839f, 2796.466f}, {2827.757f, 2596.52f}}};
constexpr float k_PillarCameraRadius = 30.0f;
constexpr float k_VillageCameraRadius = 60.0f;
/// The good advisor's last line as the creature is freed, said once the third pillar is down
constexpr uint32_t k_FreedLastLine = 6944;
/// The belief in the player a village takes to be won: more than any other player's there (Lethys's 1.9 in his last
/// town included), so its next fold makes it the player's and keeps it so; the village is picked within this of its
/// spot, as the land's scripts pick a town
constexpr float k_WinningBelief = 2.0f;
constexpr float k_WinPickRadius = 100.0f;
/// The wolves' pack: its way from where it gathers to the Indian village's edge, and how far from its middle a wolf
/// is picked (the whole way, about 205 each side, but not the land's own wolves, the nearest about 296 from it); the
/// wolf's script sub-type and the pack's size, which no monk thins on this run
constexpr glm::vec2 k_PackWayMiddle {2620.0f, 2320.0f};
constexpr float k_PackPickRadius = 250.0f;
constexpr uint32_t k_WolfSubtype = 2;
constexpr size_t k_PackSize = 20;
/// The evil advisor's last line in the wolves' cut scene when the monk has not come, and the seconds from that line
/// read to the first wolf taken: the scene waits 12 seconds more, then closes
constexpr uint32_t k_WolvesLastLine = 5083;
constexpr float k_FirstKillSeconds = 14.0f;
/// The scripts of the attacks the won villages start, logged while they play
constexpr std::string_view k_AttackScripts = "FanaticAttack FanaticOnFire WolfAttack WaitUntilTownTakenOver";
/// Lethys's last town, high in the west, which the quest watches five minutes after the creature is freed; picked within
/// the radius the quest finds it in
constexpr glm::vec2 k_LethysTown {1846.96f, 2322.65f};
constexpr float k_LethysTownPickRadius = 50.0f;
/// Seconds from the creature's release to the belief given in Lethys's town: just before the quest's five-minute wait
/// ends
constexpr float k_TroubleSeconds = 290.0f;
/// The belief in the player that puts Lethys's town in trouble: the quest asks for Lethys's lead to fall below half of
/// his belief there when the creature was freed (1.9, so 0.95), and this leaves him ahead (1.9 against 1.2), so the town
/// stays his
constexpr float k_TroubleBelief = 1.2f;
/// Lethys's last line of the creed scene, and the advisors' last line after his temple is destroyed
constexpr uint32_t k_CreedLastLine = 5040;
constexpr uint32_t k_LethysGoneLastLine = 6947;

/// The globals and scripts the chapter is read by: how many pillars are down, whether the arrival vortex is still
/// open, the land's number, whether Lethys has been finished off, and whether the monk's quest is done (which the
/// wolves' attack reads)
constexpr std::string_view k_Globals =
    "PrisonStatuesDeactivated VortexOpen LandNumber LethysKilled MonkHasReachedEnd GlobalDebug";
constexpr std::string_view k_Scripts = "GetThroughVortexL2 VortexEntry FreeTheCreature FreezeRayStatue "
                                       "LethysCreatureActing SetupLethysCreature DisableLocalWeather";

/// The wait for a task of a script: running, by default, or none running after one was
Command WaitTask(std::string_view script, TaskWait wait = TaskWait::Running)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

/// The wait for a text to have been shown and read
Command WaitText(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true};
}

/// Logs the globals and scripts named, some seconds after the command before it, and again every period if given
Command LogGlobals(std::string_view globals, std::string_view scripts, float delay = 0.0f, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = globals, .script = scripts};
}

Command ClickTexts()
{
	return {.kind = Kind::ClickTexts, .value = 1};
}

/// The camera put back from a point (to its south) and above it, looking at it; both heights are above the land there,
/// as the timeline's camera takes them
Command CameraOn(glm::vec2 point, float eyeHeight, float back, float lookHeight)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {point.x, eyeHeight, point.y - back},
	        .look = {point.x, lookHeight, point.y}};
}

/// The arrival as a player watches it, then the camera over the prison and two logs of its state
std::vector<Command> ArrivalAndPrison()
{
	return {
	    // the lines said with a click to go on (the advisors' look at the creature, the statues and the villages) are
	    // clicked on
	    ClickTexts(),
	    // the arrival runs once the land is set up; the prison was started just before it
	    WaitTask("GetThroughVortexL2"),
	    LogGlobals(k_Globals, k_Scripts, 0.0f, 30),
	    // the arrival's film: the vortex opens over the home town and the advisors step out (5012, 5013); the creature's
	    // moan and the camera swung to the prison (5063, clicked on); closer, the evil advisor points at it, the
	    // torture, and the good advisor's line on the statues (5018, clicked on), after which the temple and the home
	    // town's buildings are set down and the log entry is recorded; the villages (5020, clicked on), the camera back
	    // over the home town and the film's end
	    WaitText(5012),
	    WaitText(5063),
	    WaitText(5018),
	    WaitText(5020),
	    // half a minute after the film the advisors speak again (5703, 5704, 5705); the arrival then waits for the
	    // vortex to have closed and ends
	    WaitText(5705),
	    WaitTask("GetThroughVortexL2", TaskWait::Finished),
	    LogGlobals(k_Globals, k_Scripts),
	    // [camera] the prison on the hill: the creature held between the pillars, their freeze beams every 3 seconds and
	    // Lethys's creature tormenting it
	    CameraOn(k_Prison, 40.0f, 60.0f, 10.0f),
	    LogGlobals(k_Globals, k_Scripts, k_PrisonLogSeconds),
	    LogGlobals(k_Globals, k_Scripts, k_PrisonLogSeconds),
	};
}

/// The waits on a pillar's fall (index of its village): the count of pillars down, then the cut scene's camera over the
/// sinking pillar and, for the first two, back over the village
void WaitPillarDown(std::vector<Command>& commands, size_t i)
{
	commands.push_back({.kind = Kind::WaitGlobal,
	                    .waitUntilFree = true,
	                    .amount = static_cast<float>(i + 1),
	                    .global = "PrisonStatuesDeactivated",
	                    .compare = testbed_land_timeline::Compare::AtLeast});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	commands.push_back(
	    {.kind = Kind::WaitCamera, .waitUntilFree = true, .point = k_PillarCameras.at(i), .radius = k_PillarCameraRadius});
	if (i < k_VillageCameras.size())
	{
		commands.push_back({.kind = Kind::WaitCamera,
		                    .waitUntilFree = true,
		                    .point = k_VillageCameras.at(i),
		                    .radius = k_VillageCameraRadius});
	}
}

/// [shortcut] A village won by belief (index of its village): the belief that wins it for the player, which its next
/// fold makes the player's and keeps so; picked as the land's scripts pick a town
Command WinVillage(size_t i)
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = 2.0f,
	        .amount = k_WinningBelief,
	        .thing = testbed_land_timeline::ThingNear {
	            .type = script::ObjectType::Town, .point = k_Villages.at(i), .radius = k_WinPickRadius}};
}

void AddWipedOut(std::vector<Scenario>& all)
{
	auto commands = ArrivalAndPrison();
	// [shortcut] each village wiped out, its buildings and its people, which the quest takes as won (a shortcut of the
	// miracles that destroy it); the pillar it feeds falls at its next 3-second check and its cut scene plays, which is
	// waited on by the count of pillars down, then by the camera over the sinking pillar and back over the village. With
	// all three down the creature is freed
	for (size_t i = 0; i < k_Villages.size(); ++i)
	{
		commands.push_back({.kind = Kind::KnockDownTown,
		                    .delaySeconds = 2.0f,
		                    .thing = testbed_land_timeline::ThingNear {
		                        .type = script::ObjectType::Town, .point = k_Villages.at(i), .radius = k_VillagePickRadius}});
		WaitPillarDown(commands, i);
	}
	// the third pillar's scene: Lethys's call to his creature, the camera over the prison and the good advisor's lines,
	// the freeze lifted and the creature let go
	commands.push_back(WaitText(k_FreedLastLine));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_PrisonLogSeconds));
	const testbed_land_timeline::ThingNear lethysTown {
	    .type = script::ObjectType::Town, .point = k_LethysTown, .radius = k_LethysTownPickRadius};
	// [shortcut] Lethys's last town given belief in the player, as the player's miracles and acts there would give it,
	// enough to cut his lead below the half the quest waits for but not to win it: the creed scene follows, Lethys
	// begging to be spared, the creed's glow on the creature and the exit vortex opened
	commands.push_back({.kind = Kind::TownGainsBelief,
	                    .player = PlayerNames::PLAYER_ONE,
	                    .delaySeconds = k_TroubleSeconds,
	                    .amount = k_TroubleBelief,
	                    .thing = lethysTown});
	commands.push_back(WaitText(k_CreedLastLine));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	// [shortcut] the town then won by belief, which the quest waits for after the creed scene: Lethys's temple is
	// destroyed and he is gone
	commands.push_back({.kind = Kind::TownGainsBelief,
	                    .player = PlayerNames::PLAYER_ONE,
	                    .delaySeconds = 2.0f,
	                    .amount = k_WinningBelief,
	                    .thing = lethysTown});
	commands.push_back(WaitText(k_LethysGoneLastLine));
	commands.push_back(WaitTask("FreeTheCreature", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));

	all.push_back({
	    .id = "quest.so_you_couldnt_bear",
	    .name = "Land 3 quest run: So You Couldn't Bear to Be Without Your Creature?",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/FreeTheCreature&/GetThroughVortexL2 on "
	                   "Land3.txt). The texts that wait for a click are clicked on. It lets the arrival's film play, "
	                   "then the advisors' remarks half a minute later, waits for the arrival to end, puts the camera "
	                   "over the prison on the hill and logs the prison's state twice, 10 seconds apart; then wipes out "
	                   "the Japanese, Indian and Egyptian villages (buildings and villagers) one at a time, each "
	                   "waited on until its pillar has fallen and its cut scene has come back over the village, and "
	                   "waits for the creature's release. Five minutes on it gives Lethys's last town belief in the "
	                   "player, enough for the creed scene but not to win it, waits for the scene's last line, then "
	                   "gives it the belief that wins it and waits for Lethys's end.",
	    .expected = "The land's setup loads the player's creature at the prison from the profile's mind file, bars it "
	                "from the temple, finds the four towns, stills the weather round the home town for 10 minutes, "
	                "makes Lethys's wolf at 1.5 times its size and sets Lethys's personality. The prison makes its "
	                "three pillars indestructible and puts the creature on its spot, leash off, indestructible, at a "
	                "tenth of its health. The arrival's film opens the vortex with Nemesis's music, the advisors' "
	                "lines in order, the temple, worship site, storage pit and workshop scaffold, the log entry at 0 "
	                "with its reminder, and ends with the music stopped; the vortex fades and the log reads it closed. At "
	                "the prison every pillar fires its beam and freeze at the creature every 3 seconds and keeps it "
	                "from tiring, and Lethys's creature torments it in turn; the log reads no pillar down. Each village "
	                "destroyed brings its pillar down at its next check, with its cut scene, and the count of "
	                "pillars down rises to 3; the third frees the creature. With the villages destroyed rather than "
	                "won, neither the fanatics' fire attack nor the wolves' attack starts (both need their village "
	                "standing and the player's), and no pillar rises again. With Lethys's lead in his last town cut "
	                "below half, the pillars are forced down, both creatures are moved to the lowland and the creed "
	                "scene plays: Lethys begs, the creed's glow shows on the creature, the exit vortex opens with its "
	                "scroll. With the town then won, Lethys's hand flies away, his temple explodes and the advisors "
	                "say he is gone. "
	                "Unverified: the camera's heights over the hill, and that each cut scene's camera comes within "
	                "the radius waited on.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The three villages won by belief in turn, the two attacks they start let play (the wolves taken out), up to the
/// creature's release
void AppendVillagesWon(std::vector<Command>& commands)
{
	commands.push_back(LogGlobals(k_Globals, k_AttackScripts, 0.0f, 30));
	// the Japanese village won: its pillar falls, and with one pillar down and that village the player's the prison
	// starts the fanatics' fire attack, which plays by itself (its fishermen are not put out)
	commands.push_back(WinVillage(0));
	WaitPillarDown(commands, 0);
	commands.push_back(WaitTask("FanaticAttack"));
	// the Indian village won: with two pillars down and that village the player's, the wolves' attack starts; its pack
	// comes five minutes later, while the third pillar still stands
	commands.push_back(WinVillage(1));
	WaitPillarDown(commands, 1);
	commands.push_back(WaitTask("WolfAttack"));
	commands.push_back(WaitText(k_WolvesLastLine));
	// [shortcut] each wolf of the pack taken out of the game by a script's delete once the cut scene is over, before any
	// reaches the village, as the player's blows would leave the pack, as in the wolves' own quest run. A wolf already
	// dying is passed over, so each pick takes a living one
	for (size_t i = 0; i < k_PackSize; ++i)
	{
		commands.push_back({.kind = Kind::ScriptDelete,
		                    .delaySeconds = i == 0 ? k_FirstKillSeconds : 0.5f,
		                    .thing = testbed_land_timeline::ThingNear {.type = script::ObjectType::Animal,
		                                                               .subtype = k_WolfSubtype,
		                                                               .point = k_PackWayMiddle,
		                                                               .radius = k_PackPickRadius,
		                                                               .aliveOnly = true}});
	}
	commands.push_back(WaitTask("WolfAttack", TaskWait::Finished));
	// the fire attack, seen running above, has most often ended by now
	commands.push_back(WaitTask("FanaticAttack", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_Globals, k_AttackScripts, 1.0f));
	// the Egyptian village won: the third pillar falls and the creature is freed
	commands.push_back(WinVillage(2));
	WaitPillarDown(commands, 2);
	commands.push_back(WaitText(k_FreedLastLine));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_PrisonLogSeconds));
}

void AddWon(std::vector<Scenario>& all)
{
	auto commands = ArrivalAndPrison();
	AppendVillagesWon(commands);
	all.push_back({
	    .id = "quest.so_you_couldnt_bear_won",
	    .name = "Land 3 quest run: So You Couldn't Bear to Be Without Your Creature?, the villages won",
	    .facet = Facet::Story,
	    .description = "As the quest run with the villages wiped out, with the same start, but each village is won by "
	                   "belief: the Japanese village, then, once its pillar is down and the fanatics' attack has "
	                   "started, the Indian village; once the wolves' attack has started and its cut scene has ended, it "
	                   "takes the pack's twenty wolves out one by one with a script's delete, waits for both attacks to "
	                   "end, then wins the Egyptian village and waits for the creature's release.",
	    .expected = "Each village won brings its pillar down at its next check, with its cut scene. With one pillar down "
	                "and the Japanese village the player's, the fanatics' fire attack starts: a minute later its cut "
	                "scene, the fishermen set alight, who burn on the path. With two down and the Indian village the "
	                "player's, the wolves' attack starts and five minutes later its pack of twenty forms and sets off, "
	                "with its cut scene; the monk's quest not being done, no monk thins the pack and the monk's wonder "
	                "is not given. With every wolf gone before any reaches the village, the wolves' entry is updated "
	                "to complete. The third village won brings the last pillar down and frees the creature. "
	                "Unverified: the camera's heights over the hill, and that each cut scene's camera comes within "
	                "the radius waited on.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
/// The land's control script's waits and the scripts it starts, in the background or in turn, with the exit's
constexpr std::string_view k_ControlGlobals = "FinishedLand3 PrisonStatuesDeactivated VortexOpen LethysKilled";
constexpr std::string_view k_ControlScripts = "LandControl3 SetupLand3 ThrowBlokeMain FreeTheCreature GetThroughVortexL2 "
                                              "SwapToApe ShaolinMonk TreePuzzleTwo LeaveThroughVortexL3";
/// The exit vortex the creed scene opens, and its scroll 20 above it; the camera is put back from the scroll and above
/// it, and the hand's press looks for the scroll within the radius
constexpr glm::vec2 k_ExitVortex {2274.481f, 2043.0614f};
constexpr float k_ExitScrollRaised = 20.0f;
constexpr float k_ExitEyeAbove = 25.0f;
constexpr float k_ExitEyeBack = 30.0f;
constexpr float k_ExitScrollPickRadius = 15.0f;
/// The Action (right) button
constexpr size_t k_ActionButton = 3;

void AddLandControl(std::vector<Scenario>& all)
{
	auto commands = ArrivalAndPrison();
	// once the arrival is over, the control script starts its last background quests
	commands.push_back(WaitTask("TreePuzzleTwo"));
	commands.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 0.0f, 30));
	// [shortcut] the three villages won by belief
	AppendVillagesWon(commands);
	// [shortcut] Lethys's last town given belief in the player, enough for the creed scene but not to win it
	commands.push_back({.kind = Kind::TownGainsBelief,
	                    .player = PlayerNames::PLAYER_ONE,
	                    .delaySeconds = k_TroubleSeconds,
	                    .amount = k_TroubleBelief,
	                    .thing = testbed_land_timeline::ThingNear {
	                        .type = script::ObjectType::Town, .point = k_LethysTown, .radius = k_LethysTownPickRadius}});
	commands.push_back(WaitText(k_CreedLastLine));
	commands.push_back(WaitTask("LeaveThroughVortexL3"));
	// the scene's camera is the script's until the scene is over: the quest then lets the creature go and waits on
	// Lethys's town
	commands.push_back(WaitTask("WaitUntilTownTakenOver"));
	commands.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f));
	// [camera] the exit's scroll over the vortex, pressed on with the hand at once, sparing Lethys
	commands.push_back(CameraOn(k_ExitVortex, k_ExitEyeAbove + k_ExitScrollRaised, k_ExitEyeBack, k_ExitScrollRaised));
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_ExitVortex, .value = 1});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = testbed_land_timeline::ThingNear {
	                        .type = script::ObjectType::Highlight, .point = k_ExitVortex, .radius = k_ExitScrollPickRadius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	// the exit marks the land finished and the control script returns
	commands.push_back(WaitTask("LandControl3", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_ControlGlobals, k_ControlScripts, 1.0f));
	all.push_back({
	    .id = "quest.land_control_3",
	    .name = "Land 3 quest run: the land's control script",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that starts only the land's control script (quest=LandControl3 "
	                   "on Land3.txt), as loading the land does. The texts that wait for a click are clicked on. It plays "
	                   "the arrival and the prison as quest.so_you_couldnt_bear does, waits for the control script's last "
	                   "background quests, wins the three villages as quest.so_you_couldnt_bear_won does, then puts "
	                   "Lethys's last town in trouble as quest.so_you_couldnt_bear does; once the creed scene is over it "
	                   "presses on the exit vortex's scroll, sparing Lethys, and waits for the control script to end.",
	    .expected = "The control script runs the land's setup, starts the man who wants to be thrown and the prison in the "
	                "background, plays the arrival and then starts the Rejuvenator, the Shaolin and the tree puzzle in the "
	                "background. The villages won bring the pillars down, the fire and wolves' attacks start and the "
	                "creature is freed; the creed scene opens the exit vortex with its scroll, whose press dives the "
	                "camera into it, marks the land finished, and the control script returns.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestCouldntBearScenarios(std::vector<Scenario>& all)
{
	AddWipedOut(all);
	AddWon(all);
	AddLandControl(all);
}
