/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// Land 2's silver scroll The Greedy Farmer as land timelines, each played on the game's own Land 2 in a quest run
// (land:Land2.txt:<frames>:quest=SetupLand2/GreedyFarmer:timeline=<id>, docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"): the land's control script is not started, so the eight-minute check on the town's owner that
// starts the quest in play is skipped. Each keeps the camera away from the field and the hideout until the herd and
// the gang are made, presses on the scroll at the farmer's house, waits for the introduction's last line, then drives
// one ending. The points are map coordinates, taken from the places the quest's script names.

#include <array>
#include <string_view>
#include <vector>

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

/// The waits for a task of a script: running
constexpr size_t k_Running = 0;
/// The scripts' sub-types of the quest's things: a cow, and a Celtic farmer (the farmer himself)
constexpr uint32_t k_Cow = 8;
constexpr uint32_t k_CelticFarmer = 3;

// The quest's places, from its script: the farmer's house (where his scroll is put up), his soap box, the field the
// herd is made in, and the gang's hideout by the campfire
constexpr glm::vec2 k_House {1741.64f, 3011.21f};
constexpr glm::vec2 k_SoapBox {1749.5812f, 3009.076f};
constexpr glm::vec2 k_Field {1722.0f, 3082.0f};
constexpr glm::vec2 k_HideOut {1840.4478f, 3165.9094f};
/// A spot of the land far east of the farm (a did-you-know sign's), where the camera looks while the herd and the gang
/// are made: each is made only once its place is off the screen
constexpr glm::vec2 k_FarAway {2739.382f, 3052.035f};
/// The herd keeps within 30 of its middle, which grazes within 20 of the field, so its cows stay more than 90 from the
/// hideout; a cow within 80 of it is one the gang is leading away
constexpr float k_StolenCowRadius = 80.0f;
/// How many times the hand reaches for a led cow, and how long apart: four minutes, a little over two thefts
constexpr int k_SaveTries = 60;
constexpr float k_SaveTrySeconds = 3.0f;
/// The scripts' sub-type of the gang's boys, Celtic farmer children
constexpr uint32_t k_CelticChild = 3;
/// Right after a slaughter the gang stands at the hideout; a boy within 20 of it is with the gang
constexpr float k_GangAtHideOutRadius = 20.0f;
/// Each boy is moved to a spot of his own before he is killed, so that he dies more than 10 from the gang and from
/// every other boy: spots south of the hideout, about 70 or more from the gang's way to the herd and from each other
/// at least 36 apart, each more than 60 from the hideout so that the next boy picked there is a living one
constexpr std::array<glm::vec2, 5> k_ApartSpots {{
    {1840.0f, 3080.0f},
    {1870.0f, 3100.0f},
    {1810.0f, 3060.0f},
    {1850.0f, 3040.0f},
    {1880.0f, 3060.0f},
}};
constexpr float k_PickAtHideOutRadius = 60.0f;

/// The globals and the scripts every end check logs
constexpr std::string_view k_Globals =
    "GreedyFarmerFinished GreedyFarmerCowKilled FirstCowSaved FarmerShowScared GreedyFarmerAlignmentTotal";
constexpr std::string_view k_Scripts = "GreedyFarmer StealCow MoveFlock AngryFarmer";

Command WaitTask(std::string_view script)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = k_Running, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

Command WaitGlobalAtLeast(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::AtLeast};
}

/// Logs the globals and scripts named, some seconds after the command before it; with a period, again every period
Command LogGlobals(float delay, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = k_Globals, .script = k_Scripts};
}

/// The camera put south of a point, some way up, looking at it
Command CameraSouthOf(glm::vec2 at, float back, float up, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, up, at.y - back}, .look = {at.x, 5.0f, at.y}};
}

/// The introduction: the camera kept far from the farm until the scroll is up (the herd and the gang are made first,
/// each only off the screen), then the hand pointed at the scroll at the farmer's house and the Action button pressed
/// once the hand is over it, as the scroll's click test asks; then the advisors' last line of the introduction, read
std::vector<Command> Introduction()
{
	return {{.kind = Kind::ClickTexts, .value = 1},
	        CameraSouthOf(k_FarAway, 70.0f, 45.0f, 0.5f),
	        WaitTask("ChallengeHighlightNotify"),
	        CameraSouthOf(k_House, 70.0f, 45.0f, 1.0f),
	        {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_House, .value = 1},
	        {.kind = Kind::PointerPress,
	         .delaySeconds = 0.5f,
	         .waitUntilFree = true,
	         .value = 3,
	         .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_House, .radius = 15.0f}},
	        {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	        WaitText(1733, true)};
}

/// A thing of the land moved to a point by the scripts' own SET_POSITION, as a script moves it
Command MoveThingTo(ThingNear thing, glm::vec2 to, float delay)
{
	return {.kind = Kind::MoveThing, .delaySeconds = delay, .point = to, .thing = thing};
}

/// The camera on the farmer's soap box from the south, within 100 of it with the box and the field on the screen, as
/// the farmer's shouts and the endings' scenes ask
Command CameraOnTheFarm(float delay)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = delay,
	        .eye = {k_SoapBox.x - 4.0f, 50.0f, k_SoapBox.y - 64.0f},
	        .look = {k_SoapBox.x - 11.0f, 5.0f, k_SoapBox.y + 36.0f}};
}

std::vector<Command> HerdGone()
{
	auto all = Introduction();
	all.push_back(CameraOnTheFarm(1.0f));
	all.push_back(LogGlobals(1.0f, 60));
	// the gang's first theft runs to its end: the cow led to the hideout and slaughtered there
	all.push_back(WaitGlobalAtLeast("GreedyFarmerCowKilled", 1.0f));
	// then the hand, held over the field, reaches every few seconds for a cow the gang is leading away near the hideout,
	// and lets go of what it took there: the cow's own watcher puts it back into the herd
	all.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Field});
	for (int i = 0; i < k_SaveTries; ++i)
	{
		all.push_back(
		    {.kind = Kind::HandTake,
		     .delaySeconds = k_SaveTrySeconds,
		     .thing = ThingNear {
		         .type = script::ObjectType::Animal, .subtype = k_Cow, .point = k_HideOut, .radius = k_StolenCowRadius}});
		all.push_back({.kind = Kind::HandDrop, .delaySeconds = 1.0f});
	}
	all.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
	all.push_back(WaitGlobalAtLeast("FirstCowSaved", 1.0f));
	all.push_back(LogGlobals(1.0f));
	// the gang steals on until the herd is empty; the farmer's last scene waits for the camera on his soap box
	all.push_back(CameraOnTheFarm(1.0f));
	all.push_back(WaitGlobalAtLeast("GreedyFarmerFinished", 1.0f));
	all.push_back(WaitText(1740, true));
	all.push_back(LogGlobals(5.0f));
	return all;
}

std::vector<Command> FarmerDead()
{
	auto all = Introduction();
	all.push_back(CameraOnTheFarm(1.0f));
	// the farmer, wandering by his soap box, is taken out of the world as a killing blow would leave him: his health
	// reads 0 from then on
	all.push_back({.kind = Kind::ScriptDelete,
	               .delaySeconds = 2.0f,
	               .thing = ThingNear {
	                   .type = script::ObjectType::Villager, .subtype = k_CelticFarmer, .point = k_SoapBox, .radius = 8.0f}});
	all.push_back(WaitGlobalAtLeast("GreedyFarmerFinished", 1.0f));
	all.push_back(WaitText(2613, true));
	all.push_back(LogGlobals(2.0f));
	return all;
}

/// A script's SET_PROPERTY of HEALTH on the boy nearest a point: 0 kills him
Command KillBoyNear(glm::vec2 at, float radius, float delay)
{
	return {.kind = Kind::ScriptSetHealth,
	        .delaySeconds = delay,
	        .amount = 0.0f,
	        .thing =
	            ThingNear {.type = script::ObjectType::VillagerChild, .subtype = k_CelticChild, .point = at, .radius = radius}};
}

std::vector<Command> ChildrenScared()
{
	auto all = Introduction();
	all.push_back(CameraOnTheFarm(1.0f));
	// right after the first slaughter the gang stands at the hideout: a boy killed there lies beside the gang, which the
	// boys' watchers take as the gang's scare
	all.push_back(WaitGlobalAtLeast("GreedyFarmerCowKilled", 1.0f));
	all.push_back(KillBoyNear(k_HideOut, k_GangAtHideOutRadius, 0.0f));
	all.push_back(WaitGlobalAtLeast("FarmerShowScared", 1.0f));
	all.push_back(LogGlobals(0.5f));
	all.push_back(WaitGlobalAtLeast("GreedyFarmerFinished", 1.0f));
	all.push_back(WaitText(2613, true));
	all.push_back(LogGlobals(2.0f));
	return all;
}

std::vector<Command> ChildrenKilled()
{
	auto all = Introduction();
	all.push_back(CameraOnTheFarm(1.0f));
	// right after the first slaughter, each boy in turn is taken from the gang at the hideout to a spot of his own by a
	// script's SET_POSITION and killed there, away from the gang, so none of the deaths scares the others; after the
	// first, the evil advisor's remark on the smaller gang is waited for
	all.push_back(WaitGlobalAtLeast("GreedyFarmerCowKilled", 1.0f));
	for (size_t i = 0; i < k_ApartSpots.size(); ++i)
	{
		all.push_back(MoveThingTo({.type = script::ObjectType::VillagerChild,
		                           .subtype = k_CelticChild,
		                           .point = k_HideOut,
		                           .radius = k_PickAtHideOutRadius},
		                          k_ApartSpots.at(i), i == 0 ? 0.0f : 0.3f));
		all.push_back(KillBoyNear(k_ApartSpots.at(i), 3.0f, 0.1f));
		if (i == 0)
		{
			all.push_back(WaitText(1741, false));
		}
	}
	// the farmer's scene waits for the camera on his soap box
	all.push_back(CameraOnTheFarm(0.5f));
	all.push_back(WaitGlobalAtLeast("GreedyFarmerFinished", 1.0f));
	all.push_back(WaitText(1743, true));
	all.push_back(WaitText(2613, true));
	all.push_back(LogGlobals(2.0f));
	return all;
}
} // namespace

void testbed_scenarios::AddQuestGreedyFarmerScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.q_greedy_farmer",
	    .name = "Land 2 quest run: The Greedy Farmer, a cow saved and the herd gone",
	    .facet = Facet::Story,
	    .description = "Keeps the camera away from the farm until the scroll is up, presses on it at the farmer's house "
	                   "and waits for the introduction's last line; then watches the gang's first theft end, takes a cow "
	                   "the gang is leading away into the hand and lets it go over the field, and lets the gang steal "
	                   "the rest, the camera on the farmer's soap box.",
	    .expected = "Ten cows are made in the field and five boys at the hideout; the scroll's press plays the "
	                "introduction (texts 1728 to 1733). Each theft ends with the cow slaughtered at the hideout "
	                "(GreedyFarmerCowKilled up by one); the cow taken from the gang goes back into the herd and the good "
	                "advisor says text 1739 (FirstCowSaved=1, GreedyFarmerAlignmentTotal=0.4). Once the herd is empty "
	                "the farmer's last scene plays text 1740 (GreedyFarmerFinished=1), with no reward.",
	    .commands = HerdGone(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.q_greedy_farmer_farmer_dead",
	    .name = "Land 2 quest run: The Greedy Farmer, the farmer killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.q_greedy_farmer does, then takes the farmer out of the world by "
	                   "a script's delete, standing in for the blow that would kill him.",
	    .expected = "The good advisor says text 1734 (GreedyFarmerAlignmentTotal=-0.6, GreedyFarmerFinished=1); the "
	                "reward follows: the Lightning miracle and its second level put into the local town, the camera "
	                "flown to the town's centre and the evil advisor's text 2613.",
	    .commands = FarmerDead(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.q_greedy_farmer_scared",
	    .name = "Land 2 quest run: The Greedy Farmer, the children scared off",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.q_greedy_farmer does, waits for the gang's first slaughter at "
	                   "the hideout and kills a boy there by a script's SET_PROPERTY of his health, standing in for the "
	                   "player's blow.",
	    .expected = "The dead boy lies beside the gang (FarmerShowScared=1): the boys gather round him and say texts 1748 "
	                "and 6107, a new snapshot is taken, the gang breaks up (GreedyFarmerFinished=1, "
	                "GreedyFarmerAlignmentTotal=0) and the reward follows with the evil advisor's text 2613.",
	    .commands = ChildrenScared(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.q_greedy_farmer_killed",
	    .name = "Land 2 quest run: The Greedy Farmer, all the children killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as story.q_greedy_farmer does, waits for the gang's first slaughter, then "
	                   "moves each boy from the hideout to a spot of his own by a script's SET_POSITION and kills him "
	                   "there by a script's SET_PROPERTY of his health, standing in for the player's blows; the camera "
	                   "then goes to the soap box.",
	    .expected = "The first death away from the gang draws the evil advisor's text 1741; once the gang is empty the "
	                "farmer's scene plays texts 1738 and 1743 (GreedyFarmerAlignmentTotal=-1, GreedyFarmerFinished=1) "
	                "and the reward follows with text 2613. No boy's death scares the gang (FarmerShowScared=0).",
	    .commands = ChildrenKilled(),
	    .onLand = true,
	});
}
