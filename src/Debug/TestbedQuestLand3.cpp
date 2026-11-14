/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 3's gold scrolls as quest runs, each played after a land entry's quest= has started it: The Wolves Are
// Possessed, Fire! Fire! I'm on Fire! and Leave Through the Vortex. The timelines do what the player would: they click
// the texts that wait for a click, win a village by belief where the quest waits for the player to have won it, kill
// the wolves, and press on the vortex's scroll. The points are map coordinates, taken from the places the challenge
// scripts name.

#include <cstddef>

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
/// The Indian village the wolves go for, where the quest finds its town; picked within this of its spot
constexpr glm::vec2 k_IndianVillage {2831.99f, 2547.2f};
constexpr float k_VillagePickRadius = 20.0f;
/// The belief in the player the village takes to be won: more than any other player's there, so its next fold makes
/// it the player's and keeps it so
constexpr float k_WinningBelief = 2.0f;
/// The pack's way from where it gathers to the village's edge, and how far from its middle a wolf is picked: the whole
/// way, about 205 each side, but not the land's own wolves, the nearest of which is about 296 from it
constexpr glm::vec2 k_PackWayMiddle {2620.0f, 2320.0f};
constexpr float k_PackPickRadius = 250.0f;
/// The wolf's script sub-type, and how many the pack has once the monk has thinned it
constexpr uint32_t k_WolfSubtype = 2;
constexpr size_t k_PackAfterMonk = 8;
/// The monk's last line before the wolves' cut scene ends
constexpr uint32_t k_MonkLastLine = 5061;
/// Seconds from that line read to the first wolf taken: the cut scene closes about three seconds after it
constexpr float k_FirstKillSeconds = 6.0f;
/// The vortex the creed scene opens, and the scroll over it, raised 20 above it
constexpr glm::vec2 k_Vortex {2274.481f, 2043.0614f};
constexpr float k_ScrollRaised = 20.0f;
/// Seconds between the logs of a quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

constexpr std::string_view k_WolfGlobals = "PrisonStatuesDeactivated MonkHasReachedEnd";
constexpr std::string_view k_WolfScripts = "WolfAttack WaitUntilTownTakenOver GetRidOfWolf";
constexpr std::string_view k_FireGlobals = "PrisonStatuesDeactivated MonkHasReachedEnd";
constexpr std::string_view k_FireScripts = "FanaticAttack FanaticOnFire";
/// The Shaolin: the Japanese village whose belief in the player lets the guru's scroll appear, the belief given there
/// (the quest waits for 0.02), the scroll by the village, and where the guru stands outside his temple
constexpr glm::vec2 k_JapaneseVillage {2829.13f, 2822.8f};
constexpr float k_FirstBelief = 0.05f;
constexpr glm::vec2 k_ShaolinScroll {2773.265f, 2933.875f};
constexpr glm::vec2 k_GuruStart {2431.581f, 3095.998f};
constexpr float k_GuruPickRadius = 40.0f;
/// The guru's script sub-type, the evil advisor's last line before the guru sets off, and the last line he says at his
/// hidden spot before the scene closes
constexpr uint32_t k_GuruSubtype = 64;
constexpr uint32_t k_SetOffLine = 5049;
constexpr uint32_t k_HiddenSpotLine = 5053;
/// Seconds from that line read to the camera taken away: his sign-off, the camera's six-second pull back and the
/// scene's end
constexpr float k_LeaveGuruSeconds = 15.0f;
/// The camera's distance behind the guru, as a factor of the follow shot's: well within the 40 that keeps him slow
constexpr float k_TrailFactor = 1.0f;
/// The camera's distance behind him while lagging, as a factor of the follow shot's: about 47 back and 55 away, past
/// the 40 that speeds him up and within the 70 that loses him; and far behind, about 76 back and 84 away, past 70; and
/// the seconds it lags first
constexpr float k_LagFactor = 2.0f;
constexpr float k_LostFactor = 3.2f;
constexpr float k_LagSeconds = 20.0f;
/// The good advisor's last line when he is lost, his line in his scene before he sets off again, and the evil
/// advisor's line when he has seen the camera
constexpr uint32_t k_LostLastLine = 5066;
constexpr uint32_t k_AgainLine = 5068;
constexpr uint32_t k_SeenLastLine = 5062;
/// Seconds from his line read to the camera trailing him again: his sign-off and the scene's four-second camera move
constexpr float k_AfterAgainSeconds = 8.0f;
/// The camera close in front of his standing place, well within the 25 he waits for
constexpr float k_TempleEyeAbove = 8.0f;
constexpr float k_TempleEyeBack = 8.0f;
constexpr float k_TempleLookAbove = 2.0f;
/// The count of points he has set off for once he stands at the eighteenth, where he turns to look south; the
/// checkpoint his walk restarts from once past the seventeenth; and the seconds from the restart to the camera
/// trailing him, after the screen has faded back in
constexpr float k_LookOutWaypoint = 19.0f;
/// The middle of his way from the temple to his hidden spot, and a radius that takes in all of it
constexpr glm::vec2 k_GuruWayMiddle {2450.0f, 3140.0f};
constexpr float k_GuruWayRadius = 200.0f;
constexpr float k_LastCheckpoint = 16.0f;
constexpr float k_AfterFadeSeconds = 2.0f;
constexpr std::string_view k_WonderScripts = "ShaolinGivesWonder DidYouKnow";
constexpr std::string_view k_ShaolinGlobals = "MonkWayPointReached SavedWaypoint MonkHasReachedEnd MonkFinishedWith";
constexpr std::string_view k_ShaolinScripts = "ShaolinMonk ShaolinMonkMove ChallengeHighlightNotify LookAfterMonkRising";
/// The Rejuvenator: the Egyptian village the quest waits for the player to own, the belief that wins it (more than the
/// most Lethys may hold there, 1.5), the old woman's hut and her soap box beside it, where the villagers are brought
constexpr glm::vec2 k_EgyptianVillage {2250.22f, 2188.63f};
constexpr float k_EgyptWinningBelief = 2.0f;
constexpr glm::vec2 k_Hut {2080.205f, 2597.1482f};
constexpr float k_HutRaised = 4.0f;
constexpr glm::vec2 k_BroughtTo {2083.0f, 2590.0f};
/// How far from the village's spot a villager to bring is looked for, and how many old ones she rejuvenates
constexpr float k_VillagerPickRadius = 200.0f;
constexpr size_t k_Rejuvenated = 3;
/// Her introduction's last line, her tired line, and the advisors' last line after the hut blows up
constexpr uint32_t k_HutIntroLastLine = 1929;
constexpr uint32_t k_TiredLine = 1938;
constexpr uint32_t k_ApeLastLine = 1933;
/// The swap: seconds after her tired line before a child is brought; the ape picked within this of the hut; the
/// player's creature, picked within this of where the land's setup puts it, and put 32 from the hut, within the 50
/// the swap needs; the evil advisor's line with the creature far, the good advisor's question, and the evil advisor's
/// line after the swap
constexpr float k_ChildLaterSeconds = 12.0f;
constexpr float k_ApePickRadius = 10.0f;
constexpr glm::vec2 k_MyCreatureHome {2031.836f, 2238.0178f};
constexpr float k_MyCreaturePickRadius = 40.0f;
/// Where the player's creature is put for the swap: 20 south of the new one, towards its home. Its mind keeps walking it
/// home after it is put there (as a script's placing of a creature leaves its plan alone), so it is put there only just
/// before the press and close enough to still be within the swap's 50 when the swap's script looks
constexpr glm::vec2 k_CreatureBeside {2083.0f, 2577.0f};
/// The camera for the swap: 40 east of the hut and 25 above the land, level with the middle between the hut and the
/// player's creature's spot; and about where the old body stands after the swap, 15 from the new one towards that spot
constexpr float k_SideEyeEast = 40.0f;
constexpr float k_SideEyeAbove = 25.0f;
constexpr float k_SideLookAbove = 3.0f;
constexpr float k_SwapMiddleZ = 2585.0f;
constexpr glm::vec2 k_OldBodyAbout {2082.0f, 2582.0f};
constexpr uint32_t k_BringCreatureLine = 5104;
constexpr uint32_t k_ConfirmLine = 2420;
constexpr uint32_t k_SwappedLine = 5105;
/// Her death: the hut picked within this of its spot and hurt this much (its health is 1), her single line then;
/// the woman by her script sub-type, picked within this of her soap box, thrown 40 to the hut's south, and picked within
/// this of there some seconds later, once she has landed
constexpr float k_HutPickRadius = 5.0f;
constexpr float k_HutHit = 0.2f;
constexpr uint32_t k_HutHurtLine = 1936;
constexpr uint32_t k_WomanSubtype = 49;
constexpr glm::vec2 k_SoapBox {2080.2183f, 2593.2275f};
constexpr float k_WomanPickRadius = 5.0f;
constexpr glm::vec2 k_ThrowTo {2080.0f, 2553.0f};
/// [shortcut] The player's influence over the hut, as winning the Egyptian village gives it: the land's story starts
/// the quest once the player owns that village, which this run skips, so the hut is outside the player's influence,
/// where the hand's press and release do nothing. It reaches past the throw's point and its run-up
constexpr float k_HutInfluenceRadius = 100.0f;
/// How long the Action button is held on the woman for the hand to take her
constexpr float k_GrabHoldSeconds = 0.6f;
constexpr float k_LandedSeconds = 8.0f;
constexpr float k_LandedPickRadius = 60.0f;
constexpr std::string_view k_ApeGlobals = "PrisonStatuesDeactivated";
constexpr std::string_view k_ApeScripts = "SwapToApe RevertVillager ChallengeHighlightNotify SwapCreatures";
constexpr std::string_view k_VortexGlobals = "VortexOpen LandNumber";
constexpr std::string_view k_VortexScripts = "LeaveThroughVortexL3 ChallengeHighlightNotify";

/// Logs the globals and scripts named every period while the timeline goes on
[[nodiscard]] Command LogEvery(std::string_view globals, std::string_view scripts)
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = globals, .script = scripts};
}

/// The wolves' quest up to the end of its cut scene: the village won, the pack formed and thinned by the monk
[[nodiscard]] std::vector<Command> WolvesUpToTheirWalk()
{
	return {
	    ClickTexts(),
	    WaitTask("WolfAttack"),
	    LogEvery(k_WolfGlobals, k_WolfScripts),
	    // the quest waits 5 minutes, then for the Indian village to be the player's
	    WaitTask("WaitUntilTownTakenOver"),
	    // [shortcut] the village won by belief: its next fold hands it to the player
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_IndianVillage, .radius = k_VillagePickRadius}},
	    // the pack gathers and sets off; the cut scene follows it, and the monk thins it to eight
	    WaitText(k_MonkLastLine),
	};
}

void AddWolves(std::vector<Scenario>& all)
{
	auto commands = WolvesUpToTheirWalk();
	// [shortcut] each wolf of the pack taken out of the game by a script's delete once the cut scene is over, before any
	// reaches the village, as the player's blows would leave the pack: a wolf killed leaves its pack once it lies dead.
	// A script's health of 0 is not used, as it leaves the wolf walking on in its pack. A wolf already dying is passed
	// over, so each pick takes a living one
	for (size_t i = 0; i < k_PackAfterMonk; ++i)
	{
		commands.push_back({.kind = Kind::ScriptDelete,
		                    .delaySeconds = i == 0 ? k_FirstKillSeconds : 0.5f,
		                    .thing = ThingNear {.type = script::ObjectType::Animal,
		                                        .subtype = k_WolfSubtype,
		                                        .point = k_PackWayMiddle,
		                                        .radius = k_PackPickRadius,
		                                        .aliveOnly = true}});
	}
	commands.push_back(WaitTask("WolfAttack", TaskWait::Finished));
	commands.push_back(LogGlobals(k_WolfGlobals, k_WolfScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_wolves_are_possessed",
	    .name = "Land 3 quest run: The Wolves Are Possessed",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/MonkHasReachedEnd=1/WolfAttack on Land3.txt). "
	                   "The texts that wait for a click are clicked on. Once the quest waits for the Indian village it "
	                   "gives the village the belief that wins it for the player; after the cut scene's last line it "
	                   "takes the pack's eight wolves out one by one with a script's delete, and waits for the quest to "
	                   "end, logging its state every 30 seconds.",
	    .expected = "Five minutes in, the quest waits for the village; once it is the player's, the pack of twenty forms "
	                "about 510 from it and sets off at half speed, the cut scene follows it with the monk thinning it to "
	                "eight, and the story-log entry starts at nothing done. With every wolf gone before any is within 25 "
	                "of the village's edge, the entry is updated to complete and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
	// The same quest with no wolf killed: the pack reaches the village, which loses its faith in the player
	auto lost = WolvesUpToTheirWalk();
	lost.push_back(WaitTask("WolfAttack", TaskWait::Finished));
	lost.push_back(LogGlobals(k_WolfGlobals, k_WolfScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_wolves_are_possessed_lost",
	    .name = "Land 3 quest run: The Wolves Are Possessed, the village lost",
	    .facet = Facet::Story,
	    .description = "As the wolves' quest run, with the same start, but no wolf is killed: it waits for the quest to "
	                   "end, logging its state every 30 seconds.",
	    .expected = "The pack of eight walks on; once a wolf is within 25 of the village's edge the pack is let loose, a "
	                "cut scene flies the camera to the village, the entry is marked complete, the good advisor says the "
	                "wolves have reached the village and that the villagers are switching their faith; the village "
	                "believes in Lethys its most belief plus one, and in the player 0.3 of what it believed in its owner, "
	                "so that its next fold hands it to Lethys; the quest ends.",
	    .commands = std::move(lost),
	    .onLand = true,
	});
}

void AddFire(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("FanaticAttack"),
	    LogEvery(k_FireGlobals, k_FireScripts),
	    WaitTask("FanaticAttack", TaskWait::Finished),
	    LogGlobals(k_FireGlobals, k_FireScripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.fire_fire_im_on_fire",
	    .name = "Land 3 quest run: Fire! Fire! I'm on Fire!",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/MonkHasReachedEnd=1/FanaticAttack(0) on "
	                   "Land3.txt). The texts that wait for a click are clicked on; nothing puts the fishermen out. It "
	                   "waits for the quest to end, logging its state every 30 seconds.",
	    .expected = "A minute in, the cut scene: Lethys's lines, his three fireballs setting the sixteen fishermen "
	                "alight, the advisors, and the monk's water miracles; then the fishermen run for the village. With "
	                "none put out, each burns to death on the path, and after the last the music stops, the weather "
	                "comes back and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The fishermen's way into the Japanese village: past the second point on their path they make for the town centre
/// and the storage pit across this spot, about 41 from the town centre, inside the influence the village gives once it
/// is the player's
constexpr glm::vec2 k_FishermenWay {2860.0f, 2795.0f};

void AddFirePickedUp(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("FanaticAttack"),
	    // [shortcut] the Japanese village won by belief, as the prison's start of the attack needs it the player's; the
	    // hand reaches only into the influence the village then gives
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_JapaneseVillage, .radius = k_VillagePickRadius}},
	    LogEvery(k_FireGlobals, k_FireScripts),
	    WaitTask("FanaticAttack", TaskWait::Finished),
	    // [camera] over the fishermen's way into the village, for the player's hand to reach them
	    CameraOn(k_FishermenWay, k_EyeAbove, k_EyeBack, 0.0f),
	    WaitTask("FanaticOnFire", TaskWait::NotRunning),
	    LogGlobals(k_FireGlobals, k_FireScripts, 1.0f),
	};
	all.push_back({
	    .id = "realinput.fire_fire_picked_up",
	    .name = "Land 3 real input: Fire! Fire! I'm on Fire!, a burning fisherman picked up",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/MonkHasReachedEnd=1/FanaticAttack(0) on "
	                   "Land3.txt), with tools/checks/real_input_tap.sh's recorded presses (tools/checks/realinput/"
	                   "fire_fire_picked_up.txt); it has no pointer commands. The texts that wait for a click are "
	                   "clicked on. It gives the Japanese village the belief that wins it, and once the attack's cut "
	                   "scene is over puts the camera over the fishermen's way into the village; it waits for the "
	                   "fishermen's runs to end.",
	    .expected = "The attack plays as in quest.fire_fire_im_on_fire. A fisherman the hand picks up waits to be "
	                "dropped; once put down he runs on to his target, or to the second point if he had not passed it.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// Where the monk's two water miracles are left on the path up to the village, and the first point of the fishermen's
/// path, 23 from them
constexpr glm::vec2 k_MonkWater {2871.7f, 2719.1f};

void AddFireWater(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("FanaticAttack"),
	    // [shortcut] the Japanese village won by belief, as the prison's start of the attack needs it the player's
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_JapaneseVillage, .radius = k_VillagePickRadius}},
	    LogEvery(k_FireGlobals, k_FireScripts),
	    WaitTask("FanaticAttack", TaskWait::Finished),
	    // [camera] over the monk's water miracles, the fishermen's first point beside them
	    CameraOn(k_MonkWater, k_EyeAbove, k_EyeBack, 0.0f),
	    WaitTask("FanaticOnFire", TaskWait::NotRunning),
	    LogGlobals(k_FireGlobals, k_FireScripts, 1.0f),
	};
	all.push_back({
	    .id = "realinput.fire_fire_water",
	    .name = "Land 3 real input: Fire! Fire! I'm on Fire!, the monk's water by hand",
	    .facet = Facet::Story,
	    .description = "As realinput.fire_fire_picked_up, but once the attack's cut scene is over the camera is put over "
	                   "the monk's two water miracles on the path, for tools/checks/real_input_tap.sh's presses "
	                   "(tools/checks/realinput/fire_fire_water.txt) to take one and cast it on the fishermen.",
	    .expected = "A fisherman the water puts out while alive ends his run and joins the Japanese village.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddVortex(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_Vortex, k_EyeAbove + k_ScrollRaised, k_EyeBack, k_ScrollRaised),
	};
	PressScroll(commands, k_Vortex);
	commands.push_back(WaitTask("LeaveThroughVortexL3", TaskWait::Finished));
	commands.push_back(LogGlobals(k_VortexGlobals, k_VortexScripts, 1.0f));
	all.push_back({
	    .id = "quest.leave_through_the_vortex_land_3",
	    .name = "Land 3 quest run: Leave through the vortex",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/LeaveThroughVortexL3(marker@2274.481;29.8098;"
	                   "2043.0614) on Land3.txt). The texts that wait for a click are clicked on. Once the scroll's "
	                   "notifier runs, it puts the camera on the scroll over the vortex, presses on it with the hand and "
	                   "waits for the quest to end.",
	    .expected = "The scroll appears 20 above the vortex; the press switches it on and the camera flies high above "
	                "the vortex, looking down into it, and dives in; the quest ends with the land's exit.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

[[nodiscard]] ThingNear Guru()
{
	return {.type = script::ObjectType::Villager, .subtype = k_GuruSubtype, .point = k_GuruStart, .radius = k_GuruPickRadius};
}

/// The Shaolin's timeline up to the guru setting off: the village's belief given, the scroll pressed on, his
/// introduction and the advisors' dialogue played
[[nodiscard]] std::vector<Command> ShaolinStart()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("ShaolinMonk"),
	    LogEvery(k_ShaolinGlobals, k_ShaolinScripts),
	    // [shortcut] the Japanese village given a little belief in the player, as the player's first acts there give it
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_FirstBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_JapaneseVillage, .radius = k_VillagePickRadius}},
	    // the scroll appears by the village once the quest has read the belief
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_ShaolinScroll, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(commands, k_ShaolinScroll);
	// the introduction's scene and the advisors' dialogue
	commands.push_back(WaitText(k_SetOffLine));
	return commands;
}

void AddShaolin(std::vector<Scenario>& all)
{
	const auto guru = Guru();
	auto commands = ShaolinStart();
	// the camera trails him from behind, close enough to keep him slow and on screen and never in the half circle in
	// front of him, as a player following him unseen does
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 0.5f, .value = 2, .amount = k_TrailFactor, .thing = guru});
	commands.push_back(WaitGlobal("MonkHasReachedEnd", 1.0f));
	commands.push_back({.kind = Kind::CameraFollow, .value = 1});
	// the success scene at the hidden spot; once it is over the camera is taken far from him, so that he goes home
	commands.push_back(WaitText(k_HiddenSpotLine));
	commands.push_back(CameraOn(k_JapaneseVillage, k_EyeAbove, k_EyeBack, 0.0f));
	commands.back().delaySeconds = k_LeaveGuruSeconds;
	commands.push_back(WaitGlobal("MonkFinishedWith", 1.0f));
	commands.push_back(LogGlobals(k_ShaolinGlobals, k_ShaolinScripts, 6.0f));
	all.push_back({
	    .id = "quest.the_shaolin",
	    .name = "Land 3 quest run: The Shaolin",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/ShaolinMonk on Land3.txt). The texts that wait "
	                   "for a click are clicked on. It gives the Japanese village a little belief in the player, presses "
	                   "on the scroll by the village, waits for the advisors' dialogue after the guru's introduction, "
	                   "then keeps the camera behind the guru as he walks down the mountain, until he reaches his hidden "
	                   "spot; after the success scene it takes the camera away and waits for him to go home.",
	    .expected = "The guru is made at his temple and waits; with the village's belief at 0.02 or more the silver scroll "
	                "appears by the village. The press starts the introduction: the music, the camera's sweep up the "
	                "mountain, the guru's four lines and his sign-off, the log entry at 0, and the advisors' three lines. "
	                "Trailed unseen and within 40, he walks his 24 points slowly; at the last, the butterflies, his "
	                "floating and his lines, the log entry at 1 and the scroll gone. With the camera away he is put back "
	                "at his temple.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The guru wherever he is on his way down the mountain: his kind is the only one of its sub-type on the land
[[nodiscard]] ThingNear GuruOnHisWay()
{
	return {
	    .type = script::ObjectType::Villager, .subtype = k_GuruSubtype, .point = k_GuruWayMiddle, .radius = k_GuruWayRadius};
}

/// The camera trailing the guru from behind, at a share of the follow shot's distance
[[nodiscard]] Command TrailGuru(float factor, float delay)
{
	return {.kind = Kind::CameraFollow, .delaySeconds = delay, .value = 2, .amount = factor, .thing = GuruOnHisWay()};
}

/// Waits for the guru's walk to have passed a point, by the quest's count of the points he has set off for
[[nodiscard]] Command WaitWaypoint(float waypoint, Compare compare)
{
	return {.kind = Kind::WaitGlobal,
	        .waitUntilFree = true,
	        .amount = waypoint,
	        .global = "MonkWayPointReached",
	        .compare = compare};
}

/// After a failure: the camera let go and put close in front of the guru's standing place by the temple, as a player
/// goes back to him there
void BackToTheTemple(std::vector<Command>& commands)
{
	commands.push_back({.kind = Kind::CameraFollow, .value = 1});
	commands.push_back(CameraOn(k_GuruStart, k_TempleEyeAbove, k_TempleEyeBack, k_TempleLookAbove));
}

void AddShaolinCaught(std::vector<Scenario>& all)
{
	auto commands = ShaolinStart();
	// lagging behind: the camera trails him farther than 40 but within 70, so he walks faster
	commands.push_back(TrailGuru(k_LagFactor, 0.5f));
	// then too far behind, past 70: he is lost and goes home
	commands.push_back(TrailGuru(k_LostFactor, k_LagSeconds));
	commands.push_back(WaitText(k_LostLastLine));
	// the camera taken to him at his temple; his scene, and he sets off again, trailed unseen as in the quest's run
	BackToTheTemple(commands);
	commands.push_back(WaitText(k_AgainLine));
	commands.push_back(TrailGuru(k_TrailFactor, k_AfterAgainSeconds));
	// past his seventeenth point (the checkpoint at the sixteenth kept), at the eighteenth he turns to look out over
	// the land to the south: the camera kept to his south is then in front of him, and he sees it
	commands.push_back(WaitWaypoint(k_LookOutWaypoint, Compare::AtLeast));
	commands.push_back(LogGlobals(k_ShaolinGlobals, k_ShaolinScripts));
	commands.push_back({.kind = Kind::CameraFollow, .value = 0, .amount = k_TrailFactor, .thing = GuruOnHisWay()});
	commands.push_back(WaitText(k_SeenLastLine));
	// back to the temple; his scene again, then the screen fades and he starts again from the checkpoint, trailed
	// unseen to his hidden spot
	BackToTheTemple(commands);
	commands.push_back(WaitWaypoint(k_LastCheckpoint, Compare::Equal));
	commands.push_back(TrailGuru(k_TrailFactor, k_AfterFadeSeconds));
	commands.push_back(WaitGlobal("MonkHasReachedEnd", 1.0f));
	commands.push_back({.kind = Kind::CameraFollow, .value = 1});
	commands.push_back(WaitText(k_HiddenSpotLine));
	commands.push_back(CameraOn(k_JapaneseVillage, k_EyeAbove, k_EyeBack, 0.0f));
	commands.back().delaySeconds = k_LeaveGuruSeconds;
	commands.push_back(WaitGlobal("MonkFinishedWith", 1.0f));
	commands.push_back(LogGlobals(k_ShaolinGlobals, k_ShaolinScripts, 6.0f));
	all.push_back({
	    .id = "quest.the_shaolin_caught",
	    .name = "Land 3 quest run: The Shaolin, the guru lost and then seen",
	    .facet = Facet::Story,
	    .description = "As quest.the_shaolin up to the advisors' dialogue. Then the camera trails the guru from behind "
	                   "farther than 40 for a while, then farther than 70; after the advisor's lines it goes close to "
	                   "him at his temple and, once his scene is over, trails him unseen. When he has set off for his "
	                   "nineteenth point it is kept to his south; after the advisor's line it goes back to the temple, "
	                   "and once he starts again from his checkpoint it trails him unseen to his hidden spot, then is "
	                   "taken away.",
	    .expected = "Lagging, he speeds up to 0.7; past 70 he is put back at his temple with the good advisor's two "
	                "lines, puffs of smoke, and waits for the camera within 25; then his scene, and he sets off again. "
	                "Past the seventeenth point the checkpoint is the sixteenth; turned south at the eighteenth he sees "
	                "the camera and says so, with the evil advisor's line; after his scene the screen fades and he "
	                "stands at the sixteenth point. From there, trailed unseen, he reaches the hidden spot as in the "
	                "quest's run.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddShaolinWonder(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("ShaolinGivesWonder"),
	    LogEvery(k_ShaolinGlobals, k_WonderScripts),
	    WaitTask("ShaolinGivesWonder", TaskWait::Finished),
	    LogGlobals(k_ShaolinGlobals, k_WonderScripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.the_shaolin_wonder",
	    .name = "Land 3 quest run: The Shaolin's Wonder",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/ShaolinGivesWonder on Land3.txt), the guru's "
	                   "reward scene started as the creature's rescue starts it once his quest is done. The texts that "
	                   "wait for a click are clicked on; it waits for the scene to end, logging its state every 30 "
	                   "seconds.",
	    .expected = "Twenty minutes in, the screen fades to black and a second guru is made on a ledge; the scene fades "
	                "in on him with his lines, the building planned below is begun as a building site, an info sign is "
	                "put beside it, the camera rises and sweeps down to it, and the scene ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// [shortcut] a villager of the Egyptian village (an adult, or with child a child) put beside the hut, as the player's
/// hand carries one there
[[nodiscard]] Command BringVillager(bool child, float delay)
{
	return {.kind = Kind::MoveThing,
	        .delaySeconds = delay,
	        .point = k_BroughtTo,
	        .thing = ThingNear {.type = child ? script::ObjectType::VillagerChild : script::ObjectType::Villager,
	                            .point = k_EgyptianVillage,
	                            .radius = k_VillagerPickRadius}};
}

/// The Rejuvenator's timeline up to the old woman's introduction: the village won, the scroll over the hut pressed on,
/// her scene and the advisors' lines played
[[nodiscard]] std::vector<Command> RejuvenatorStart()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("SwapToApe"),
	    LogEvery(k_ApeGlobals, k_ApeScripts),
	    // [shortcut] the Egyptian village won by belief: its next fold hands it to the player
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_EgyptWinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_EgyptianVillage, .radius = k_VillagePickRadius}},
	    // the scroll appears over the hut once the quest has read the village as the player's
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_Hut, k_EyeAbove + k_HutRaised, k_EyeBack, k_HutRaised),
	};
	PressScroll(commands, k_Hut);
	commands.push_back(WaitText(k_HutIntroLastLine));
	return commands;
}

/// Three old villagers brought to her, each waited on through her rejuvenation's scene; then her tired line
void RejuvenateThree(std::vector<Command>& commands)
{
	for (size_t i = 0; i < k_Rejuvenated; ++i)
	{
		commands.push_back(BringVillager(false, 2.0f));
		commands.push_back(WaitTask("RevertVillager"));
		commands.push_back(WaitTask("RevertVillager", TaskWait::Finished));
	}
	commands.push_back(WaitText(k_TiredLine));
}

void AddRejuvenator(std::vector<Scenario>& all)
{
	auto commands = RejuvenatorStart();
	// the scroll pressed again for the reminder its log entry took with the introduction, the good advisor's
	constexpr uint32_t k_ApeReminderLine = 2489;
	ReplayReminder(commands, k_Hut, k_ApeReminderLine);
	RejuvenateThree(commands);
	// tired, she asks for a child; one brought to her, and her magic goes wrong
	commands.push_back(BringVillager(true, 2.0f));
	commands.push_back(WaitText(k_ApeLastLine));
	// the camera taken away from her, so that she goes
	commands.push_back(CameraOn(k_EgyptianVillage, k_EyeAbove, k_EyeBack, 0.0f));
	commands.back().delaySeconds = 3.0f;
	commands.push_back(WaitTask("SwapToApe", TaskWait::Finished));
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_rejuvenator",
	    .name = "Land 3 quest run: The Rejuvenator",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/SwapToApe on Land3.txt). The texts that wait "
	                   "for a click are clicked on. It gives the Egyptian village the belief that wins it, presses on the "
	                   "scroll over the hut, waits for the old woman's introduction, presses the scroll again for its "
	                   "reminder, puts three villagers of the village "
	                   "beside the hut one at a time, each waited on through her scene, then a child once she is tired, "
	                   "and waits for the quest to end with the camera taken away.",
	    .expected = "Once the village is the player's the silver scroll appears over the hut; the press brings the old "
	                "woman out, her lines and the advisors', the log entry at 0. Each villager beside the hut is turned "
	                "back into a child in a scene; after three she is tired and the entry goes to a half. The child "
	                "brought then makes her magic go wrong: the hut blows up, the child becomes an ape creature, the "
	                "entry is done, the advisors speak and the creature swap is offered.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The ape (or chimp) she made, picked by its kind where the child stood by the hut's door
[[nodiscard]] ThingNear Ape()
{
	return {.type = script::ObjectType::Creature, .point = k_Hut, .radius = k_ApePickRadius};
}

/// The ape clicked with the hand, as the player clicks it: the pointer on it, the Action button pressed once the hand is
/// over it and let go
void ClickApe(std::vector<Command>& commands, float delay)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = delay, .thing = Ape()});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = Ape()});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

/// The swap offered up to the good advisor's question: as quest.the_rejuvenator up to her tired line, the child brought
/// a while later, then the scroll over the new creature pressed with the player's creature beside it
[[nodiscard]] std::vector<Command> SwapOfferToTheQuestion()
{
	auto commands = RejuvenatorStart();
	RejuvenateThree(commands);
	// tired, she asks for a child; one brought a while later, and her magic goes wrong
	commands.push_back(BringVillager(true, k_ChildLaterSeconds));
	commands.push_back(WaitText(k_ApeLastLine));
	// [shortcut] once the swap's script runs, the player's creature put within 50 of the new one, as the player would
	// walk it there on its leash; the camera to their east, so that neither stands in front of the other
	commands.push_back(WaitTask("SwapCreatures"));
	commands.push_back({.kind = Kind::CameraTo,
	                    .delaySeconds = 3.0f,
	                    .eye = {k_Hut.x + k_SideEyeEast, k_SideEyeAbove, k_SwapMiddleZ},
	                    .look = {k_Hut.x, k_SideLookAbove, k_SwapMiddleZ}});
	// the scroll over the new creature pressed: the two meet and the good advisor asks; clicking the new creature
	// confirms the swap. The pointer is on the scroll first, and the creature is put beside the new one only then, just
	// before the press
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_Hut, .value = 1});
	commands.push_back(
	    {.kind = Kind::MoveThing,
	     .delaySeconds = 0.2f,
	     .point = k_CreatureBeside,
	     .thing =
	         ThingNear {.type = script::ObjectType::Creature, .point = k_MyCreatureHome, .radius = k_MyCreaturePickRadius}});
	commands.push_back(
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.3f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_Hut, .radius = k_ScrollPickRadius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back(WaitText(k_ConfirmLine));
	return commands;
}

/// The scroll over the new creature pressed again, once the offer has started over: the player's creature is still
/// beside it, so the two meet again and the good advisor asks again
void PressTheScrollAgain(std::vector<Command>& commands, float delay)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = delay, .point = k_Hut, .value = 1});
	commands.push_back(
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_Hut, .radius = k_ScrollPickRadius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back(WaitText(k_ConfirmLine));
}

void AddRejuvenatorSwap(std::vector<Scenario>& all)
{
	auto commands = SwapOfferToTheQuestion();
	ClickApe(commands, 1.0f);
	commands.push_back(WaitText(k_SwappedLine));
	// [shortcut] the new creature, now the player's, put far away, as the player would walk it off; then the scroll over
	// the old body pressed
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 3.0f, .point = k_MyCreatureHome, .thing = Ape()});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = k_OldBodyAbout, .value = 1});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back(WaitText(k_BringCreatureLine));
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, 5.0f));
	all.push_back({
	    .id = "quest.the_rejuvenator_swap",
	    .name = "Land 3 quest run: The Rejuvenator, the creature swapped",
	    .facet = Facet::Story,
	    .description = "As quest.the_rejuvenator up to her tired line; the child is brought 12 seconds later. Once the "
	                   "swap's script runs it puts the camera east of the hut and the pointer on the scroll over the new "
	                   "creature, then puts the player's creature 20 from the new one and at once presses on the scroll "
	                   "and, after the good advisor's question, clicks the new creature. After the swap it puts the new "
	                   "creature, now the player's, far away and presses "
	                   "on the scroll over the old body.",
	    .expected = "With the creature near, the press walks it to within 15 of the new one, both look at each other, "
	                "the good advisor asks to confirm; the click swaps the minds in a short scene with the sound, the "
	                "evil advisor's line follows, and a scroll comes over the old body, which cannot be picked up. "
	                "Pressed with the player's creature far away, that scroll gives the evil advisor's \"You'll need to "
	                "bring our Creature, Boss.\" and comes back.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The player's own creature where the swap's script has walked it, within 15 of the new one on its home side
constexpr glm::vec2 k_MyCreatureAtTheApe {2082.0f, 2582.0f};
constexpr float k_MyCreatureAtTheApeRadius = 10.0f;
/// Longer than the 110 seconds the offer waits for an answer
constexpr float k_PastTheConfirmSeconds = 115.0f;

void AddRejuvenatorSwapCancelled(std::vector<Scenario>& all)
{
	const ThingNear myCreature {
	    .type = script::ObjectType::Creature, .point = k_MyCreatureAtTheApe, .radius = k_MyCreatureAtTheApeRadius};
	auto commands = SwapOfferToTheQuestion();
	// the player's own creature clicked at the question, as the player clicks it: the pointer on it, the Action button
	// pressed once the hand is over it and let go
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = myCreature});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = myCreature});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, 3.0f));
	PressTheScrollAgain(commands, 1.0f);
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, 5.0f));
	all.push_back({
	    .id = "quest.the_rejuvenator_swap_cancel",
	    .name = "Land 3 quest run: The Rejuvenator, the creature swap cancelled",
	    .facet = Facet::Story,
	    .description = "As quest.the_rejuvenator_swap up to the good advisor's question; then it clicks the player's own "
	                   "creature with the pointer and the Action button, and presses on the scroll over the new creature "
	                   "again once it is back.",
	    .expected = "The click on the player's own creature cancels the offer and lets it go; the scroll comes back over "
	                "the new creature, and pressing it again brings the two together and the good advisor's question "
	                "again.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddRejuvenatorSwapTimedOut(std::vector<Scenario>& all)
{
	auto commands = SwapOfferToTheQuestion();
	// no answer: the question left unanswered for longer than the offer waits
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, k_PastTheConfirmSeconds));
	PressTheScrollAgain(commands, 1.0f);
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, 5.0f));
	all.push_back({
	    .id = "quest.the_rejuvenator_swap_timeout",
	    .name = "Land 3 quest run: The Rejuvenator, the creature swap left unanswered",
	    .facet = Facet::Story,
	    .description = "As quest.the_rejuvenator_swap up to the good advisor's question; then it clicks neither creature "
	                   "for 115 seconds, and presses on the scroll over the new creature again once it is back.",
	    .expected = "The good advisor asks again every 30 seconds; after 110 seconds with no answer the offer cancels "
	                "itself and the scroll comes back over the new creature, and pressing it again brings the question "
	                "again.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// Her single line when the villager brought is a child, and how long the timeline logs after it: her wait for the
/// child to go, at most 20 seconds, then her next look round the hut
constexpr uint32_t k_TooYoungLine = 1937;
constexpr float k_AfterTooYoungSeconds = 25.0f;
/// Seconds the hand is given at the old woman's spot once her introduction is over, and how high the camera looks
/// there: about her middle
constexpr float k_HandAtHutSeconds = 60.0f;
constexpr float k_WomanMiddle = 1.0f;

void AddRejuvenatorByHand(std::vector<Scenario>& all)
{
	// the scene glides the camera back over 5 seconds: the woman's spot is taken after that
	auto atWoman = CameraOn(k_SoapBox, k_EyeAbove, k_EyeBack, k_WomanMiddle);
	atWoman.delaySeconds = 6.0f;
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("SwapToApe"),
	    LogEvery(k_ApeGlobals, k_ApeScripts),
	    // [shortcut] the Egyptian village won by belief: its next fold hands it to the player
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_EgyptWinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_EgyptianVillage, .radius = k_VillagePickRadius}},
	    // [camera] the scroll over the hut in the middle of the screen, for the player's press
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_Hut, k_EyeAbove + k_HutRaised, k_EyeBack, k_HutRaised),
	    WaitText(k_HutIntroLastLine),
	    // [camera] once the scene has given the camera back, the old woman's spot in the middle of the screen
	    atWoman,
	    LogGlobals(k_ApeGlobals, k_ApeScripts, k_HandAtHutSeconds),
	};
	all.push_back({
	    .id = "realinput.the_rejuvenator_by_hand",
	    .name = "Land 3 real input: The Rejuvenator, the scroll and the old woman by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand3/SwapToApe on Land3.txt), with "
	                   "tools/checks/real_input_tap.sh's recorded presses (tools/checks/realinput/"
	                   "the_rejuvenator_by_hand.txt); it has no pointer commands. The texts that wait for a click are "
	                   "clicked on. It gives the Egyptian village the belief that wins it, puts the camera on the scroll "
	                   "over the hut once it appears, and after the old woman's introduction on her spot in front of the "
	                   "hut, for a minute.",
	    .expected = "A press on the scroll brings the old woman out with her scene. Whether the hand can take her depends "
	                "on the player's influence at the hut, some 440 from the Egyptian village.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddRejuvenatorTooYoung(std::vector<Scenario>& all)
{
	auto commands = RejuvenatorStart();
	// a child brought before any grown-up villager
	commands.push_back(BringVillager(true, 2.0f));
	commands.push_back(WaitText(k_TooYoungLine));
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, k_AfterTooYoungSeconds));
	all.push_back({
	    .id = "quest.the_rejuvenator_too_young",
	    .name = "Land 3 quest run: The Rejuvenator, a child brought first",
	    .facet = Facet::Story,
	    .description = "As quest.the_rejuvenator up to the old woman's introduction; then a child of the Egyptian "
	                   "village is put beside the hut, before any grown-up villager, and the timeline logs the quest's "
	                   "state 25 seconds after her line.",
	    .expected = "She says the child is not old enough, once, and waits until the child is more than 20 from the hut "
	                "or 20 seconds have passed; no rejuvenation scene.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddRejuvenatorKilled(std::vector<Scenario>& all)
{
	const ThingNear woman {
	    .type = script::ObjectType::Villager, .subtype = k_WomanSubtype, .point = k_SoapBox, .radius = k_WomanPickRadius};
	auto commands = RejuvenatorStart();
	// [shortcut] the player's influence over the hut, as owning the Egyptian village gives it
	commands.push_back({.kind = Kind::ScriptInfluence, .delaySeconds = 0.5f, .point = k_Hut, .radius = k_HutInfluenceRadius});
	// [shortcut] the hut hurt, as a player's miracle would hurt it, with the camera on it
	commands.push_back({.kind = Kind::EffectHit,
	                    .player = PlayerNames::PLAYER_ONE,
	                    .delaySeconds = 2.0f,
	                    .amount = k_HutHit,
	                    .thing = ThingNear {.type = script::ObjectType::Abode, .point = k_Hut, .radius = k_HutPickRadius}});
	commands.push_back(WaitText(k_HutHurtLine));
	// the woman taken into the hand as the player takes her: the pointer on her and the Action button held down once the
	// hand is over her, then let go; then the hand's own throw, away from the hut
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 2.0f, .thing = woman});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = woman});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 1.0f, .point = k_ThrowTo});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .value = 1});
	// [shortcut] once she has landed, her health set to nothing, as a player's blow would kill her
	commands.push_back({.kind = Kind::ScriptSetHealth,
	                    .delaySeconds = k_LandedSeconds,
	                    .amount = 0.0f,
	                    .thing = ThingNear {.type = script::ObjectType::Villager,
	                                        .subtype = k_WomanSubtype,
	                                        .point = k_ThrowTo,
	                                        .radius = k_LandedPickRadius}});
	commands.push_back(WaitTask("SwapToApe", TaskWait::Finished));
	commands.push_back(LogGlobals(k_ApeGlobals, k_ApeScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_rejuvenator_killed",
	    .name = "Land 3 quest run: The Rejuvenator, the hut hurt and the woman killed",
	    .facet = Facet::Story,
	    .description = "As quest.the_rejuvenator up to the old woman's introduction, with the player given an influence "
	                   "over the hut (a shortcut for owning the Egyptian village: the hand acts only in the player's "
	                   "influence). Then it hurts the hut with an effect's hit while the camera is on it, takes the "
	                   "woman into the hand with the pointer and the Action button and throws her away from the hut, "
	                   "sets her health to nothing once she has landed, and waits for the quest to end.",
	    .expected = "The hurt hut gives the woman's single line about her hut; thrown, she flies with the camera following "
	                "her at half speed until she lands; dead, the record closes as finished with alignment fully evil, "
	                "the evil and good advisors speak, no creature is made and the quest ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestLand3Scenarios(std::vector<Scenario>& all)
{
	AddRejuvenator(all);
	AddRejuvenatorSwap(all);
	AddRejuvenatorSwapCancelled(all);
	AddRejuvenatorSwapTimedOut(all);
	AddRejuvenatorKilled(all);
	AddRejuvenatorTooYoung(all);
	AddRejuvenatorByHand(all);
	AddShaolin(all);
	AddShaolinCaught(all);
	AddShaolinWonder(all);
	AddWolves(all);
	AddFire(all);
	AddFirePickedUp(all);
	AddFireWater(all);
	AddVortex(all);
}
