/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A testbed scenario's commands played on one of the game's own lands (Debug/TestbedLandTimeline.h): the points are map
// coordinates, the scenarios play in turn, and a scenario written for a land is checked by the land's rules. Pure: no
// game.

#include <cstdint>

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVMTypes.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Debug/TestbedLandTimeline.h"
#include "Debug/TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
namespace timeline = openblack::testbed_land_timeline;

namespace
{
using Kind = Command::Kind;

/// A scenario written for a land with these commands, and nothing else
Scenario OnLand(std::vector<Command> commands)
{
	return {
	    .id = "story.test",
	    .name = "A land timeline",
	    .facet = Facet::Examples,
	    .description = "Commands on a land.",
	    .expected = "They play in turn.",
	    .commands = std::move(commands),
	    .onLand = true,
	};
}

/// The scripts' globals as a land's challenge has them: the null variable first, then each by its name, as floats
std::vector<lhvm::VMVar> Globals()
{
	return {
	    {lhvm::DataType::Float, lhvm::VMValue(0.0f), "Null variable"},
	    {lhvm::DataType::Float, lhvm::VMValue(1.0f), "IsSkippingToCreatureSelect"},
	    {lhvm::DataType::Float, lhvm::VMValue(2.0f), "GateKey2Present"},
	    {lhvm::DataType::Int, lhvm::VMValue(int32_t {7}), "Counted"},
	    {lhvm::DataType::Object, lhvm::VMValue(uint32_t {4242}), "GateKey2"},
	    {lhvm::DataType::Float, lhvm::VMValue(9.0f), "GateKey2Present"},
	};
}

lhvm::VMTask Task(std::string name)
{
	lhvm::VMTask task;
	task.name = std::move(name);
	return task;
}

bool Mentions(const std::vector<std::string>& problems, std::string_view words)
{
	return std::ranges::any_of(problems,
	                           [words](const std::string& problem) { return problem.find(words) != std::string::npos; });
}
} // namespace

TEST(TestbedLandTimeline, PointsAreMapCoordinatesOnALandAndFromTheMiddleOnTheTestbed)
{
	const glm::vec2 middle {2560.0f, 2560.0f};
	EXPECT_EQ(timeline::Origin(false, middle), middle);
	EXPECT_EQ(timeline::Origin(true, middle), glm::vec2(0.0f));
	// the same command's point is then where the map has it
	EXPECT_EQ(MapPoint(timeline::Origin(true, middle), {1652.15f, 2322.91f}), glm::vec2(1652.15f, 2322.91f));
}

TEST(TestbedLandTimeline, APointIsOnTheLandBetweenItsCorners)
{
	EXPECT_TRUE(timeline::OnTheLand({0.0f, 0.0f}));
	EXPECT_TRUE(timeline::OnTheLand({1779.63f, 2805.83f}));
	EXPECT_TRUE(timeline::OnTheLand({timeline::k_LandSize, timeline::k_LandSize}));
	EXPECT_FALSE(timeline::OnTheLand({-1.0f, 100.0f}));
	EXPECT_FALSE(timeline::OnTheLand({100.0f, timeline::k_LandSize + 1.0f}));
}

TEST(TestbedLandTimeline, TheNextScenarioStartsOnceTheLastOnesCommandsAreAllGiven)
{
	EXPECT_EQ(timeline::NextChapter(0, 3, true), 1u);
	EXPECT_EQ(timeline::NextChapter(1, 3, true), 2u);
	// none after the last, and none while commands are still to be given
	EXPECT_FALSE(timeline::NextChapter(2, 3, true).has_value());
	EXPECT_FALSE(timeline::NextChapter(0, 3, false).has_value());
	EXPECT_FALSE(timeline::NextChapter(0, 1, true).has_value());
}

TEST(TestbedLandTimeline, ALandScenarioWithMapPointsAndThePlayersCreatureIsWellFormed)
{
	const auto scenario = OnLand({
	    {.kind = Kind::TapHighlight, .delaySeconds = 1.0f, .point = {1779.63f, 2805.83f}},
	    {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 1.0f, .point = {1896.5f, 2520.06f}},
	    {.kind = Kind::ChangeLand, .landScript = "Land2.txt"},
	});
	const auto problems = Problems(scenario);
	EXPECT_TRUE(problems.empty()) << (problems.empty() ? "" : problems.front());
	// on the testbed the same points would be off the map, from its middle
	auto testbed = scenario;
	testbed.onLand = false;
	EXPECT_FALSE(Problems(testbed).empty());
}

TEST(TestbedLandTimeline, ALandScenarioSetsNothingOutAndActsOnNothingOfItsOwn)
{
	auto withCreature = OnLand({{.kind = Kind::Stop}});
	withCreature.creatures.push_back({});
	EXPECT_TRUE(Mentions(Problems(withCreature), "sets nothing out"));
	EXPECT_TRUE(Mentions(Problems(OnLand({})), "no commands"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::Stop, .creature = 1}})), "the player's, creature 0"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::HandTake, .object = 0}})), "an object of the scenario's"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::Follow, .value = 1}})), "another creature"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::TownGainsBelief, .amount = 0.5f}})), "the scenario's town"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::ChangeLand}})), "no land's script"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WalkTo, .point = {-5.0f, 10.0f}}})), "off the land"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::Stop, .delaySeconds = -1.0f}})), "delay or hour"));
}

TEST(TestbedLandTimeline, AGlobalHoldsUpAsItsComparisonSays)
{
	using timeline::Compare;
	EXPECT_TRUE(timeline::Holds(Compare::Equal, 1.0f, 1.0f));
	EXPECT_FALSE(timeline::Holds(Compare::Equal, 0.0f, 1.0f));
	EXPECT_TRUE(timeline::Holds(Compare::NotEqual, 0.0f, 1.0f));
	EXPECT_TRUE(timeline::Holds(Compare::Below, 0.5f, 1.0f));
	EXPECT_FALSE(timeline::Holds(Compare::Below, 1.0f, 1.0f));
	EXPECT_TRUE(timeline::Holds(Compare::AtMost, 1.0f, 1.0f));
	EXPECT_TRUE(timeline::Holds(Compare::Above, 82.0f, 1.0f));
	EXPECT_FALSE(timeline::Holds(Compare::Above, 1.0f, 1.0f));
	EXPECT_TRUE(timeline::Holds(Compare::AtLeast, 1.0f, 1.0f));
	EXPECT_EQ(timeline::Name(Compare::Equal), "==");
	EXPECT_EQ(timeline::Name(Compare::AtLeast), ">=");
}

TEST(TestbedLandTimeline, AGlobalIsFoundByItsFirstNameAndReadByItsType)
{
	const auto globals = Globals();
	EXPECT_EQ(timeline::GlobalIndex(globals, "IsSkippingToCreatureSelect"), 1u);
	// the first of two of a name, and never the null variable
	EXPECT_EQ(timeline::GlobalIndex(globals, "GateKey2Present"), 2u);
	EXPECT_FALSE(timeline::GlobalIndex(globals, "Null variable").has_value());
	EXPECT_FALSE(timeline::GlobalIndex(globals, "NoSuchGlobal").has_value());
	EXPECT_FLOAT_EQ(timeline::NumberOf(globals[2]), 2.0f);
	EXPECT_FLOAT_EQ(timeline::NumberOf(globals[3]), 7.0f);
	EXPECT_FLOAT_EQ(timeline::NumberOf(globals[4]), 4242.0f);
}

TEST(TestbedLandTimeline, AGlobalSetKeepsItsType)
{
	EXPECT_FLOAT_EQ(timeline::ValueOf(lhvm::DataType::Float, 0.25f).floatVal, 0.25f);
	EXPECT_EQ(timeline::ValueOf(lhvm::DataType::Int, 3.0f).intVal, 3);
	EXPECT_EQ(timeline::ValueOf(lhvm::DataType::Boolean, 2.0f).intVal, 1);
	EXPECT_EQ(timeline::ValueOf(lhvm::DataType::Boolean, 0.0f).intVal, 0);
	EXPECT_EQ(timeline::ValueOf(lhvm::DataType::Object, 4242.0f).uintVal, 4242u);
}

TEST(TestbedLandTimeline, AWaitForATaskIsOverWhenItRunsStopsOrHasFinished)
{
	using timeline::TaskWait;
	std::map<uint32_t, lhvm::VMTask> tasks;
	tasks.emplace(3, Task("FollowUs"));
	tasks.emplace(9, Task("SingingStoneCircle"));
	EXPECT_TRUE(timeline::ScriptRunning(tasks, "FollowUs"));
	EXPECT_FALSE(timeline::ScriptRunning(tasks, "ChooseYourCreature"));
	EXPECT_TRUE(timeline::TaskWaitHolds(TaskWait::Running, true, false));
	EXPECT_FALSE(timeline::TaskWaitHolds(TaskWait::Running, false, true));
	EXPECT_TRUE(timeline::TaskWaitHolds(TaskWait::NotRunning, false, false));
	EXPECT_FALSE(timeline::TaskWaitHolds(TaskWait::NotRunning, true, true));
	// finished: none runs now, and one was seen running since the wait began
	EXPECT_TRUE(timeline::TaskWaitHolds(TaskWait::Finished, false, true));
	EXPECT_FALSE(timeline::TaskWaitHolds(TaskWait::Finished, false, false));
	EXPECT_FALSE(timeline::TaskWaitHolds(TaskWait::Finished, true, true));
}

TEST(TestbedLandTimeline, ATextIsShownOnceAmongTheLastAndReadOnceItWaitsNoMore)
{
	const std::array<uint32_t, 6> texts {5219, 5192, 5191, 0, 0, 0};
	EXPECT_TRUE(timeline::TextHolds(texts, true, 5219, false));
	EXPECT_TRUE(timeline::TextHolds(texts, true, 5191, false));
	EXPECT_FALSE(timeline::TextHolds(texts, true, 5218, false));
	// the current text waits for its click: shown but not read; an earlier one was read
	EXPECT_FALSE(timeline::TextHolds(texts, true, 5219, true));
	EXPECT_TRUE(timeline::TextHolds(texts, false, 5219, true));
	EXPECT_TRUE(timeline::TextHolds(texts, true, 5192, true));
	// no text is text 0
	EXPECT_FALSE(timeline::TextHolds(texts, false, 0, false));
}

TEST(TestbedLandTimeline, TheCameraIsWithinARadiusOfAPointInThreeDimensions)
{
	// the tutorial's follow ends with the camera within 50 of its end marker
	const glm::vec3 marker {1633.72f, 18.4f, 2307.98f};
	EXPECT_TRUE(timeline::CameraWithin({1633.72f, 18.4f + 49.0f, 2307.98f}, marker, 50.0f));
	EXPECT_FALSE(timeline::CameraWithin({1633.72f, 18.4f + 50.0f, 2307.98f}, marker, 50.0f));
	// the height counts: 40 across and 40 up is beyond 50
	EXPECT_FALSE(timeline::CameraWithin({1633.72f + 40.0f, 18.4f + 40.0f, 2307.98f}, marker, 50.0f));
	EXPECT_TRUE(timeline::CameraWithin({1633.72f + 30.0f, 18.4f + 30.0f, 2307.98f}, marker, 50.0f));
}

TEST(TestbedLandTimeline, AListOfNamesIsSplitAtItsSpaces)
{
	EXPECT_EQ(timeline::Names("GateKey2Present  LostBrotherFinished "),
	          (std::vector<std::string_view> {"GateKey2Present", "LostBrotherFinished"}));
	EXPECT_TRUE(timeline::Names("").empty());
	EXPECT_TRUE(timeline::Names("   ").empty());
}

TEST(TestbedLandTimeline, TheScriptCommandsNameWhatTheyActOnAndWaitUntilFree)
{
	using timeline::Compare;
	const auto scenario = OnLand({
	    {.kind = Kind::ClickTexts, .value = 1},
	    {.kind = Kind::WaitTask, .waitUntilFree = true, .value = 0, .script = "FollowUs"},
	    {.kind = Kind::WaitText, .waitUntilFree = true, .value = 5219, .read = true},
	    {.kind = Kind::WaitGlobal,
	     .waitUntilFree = true,
	     .amount = 1.0f,
	     .global = "GateKey2Present",
	     .compare = Compare::AtLeast},
	    {.kind = Kind::SetGlobal, .amount = 1.0f, .global = "CreatureDevelopment_Fight"},
	    {.kind = Kind::StartScript, .script = "MiracleCastHandDemo"},
	    {.kind = Kind::LogGlobals, .global = "GateKey2Present LostBrotherFinished", .script = "LostBrother"},
	    {.kind = Kind::WaitCamera, .waitUntilFree = true, .point = {1633.72f, 2307.98f}, .radius = 45.0f},
	});
	const auto problems = Problems(scenario);
	EXPECT_TRUE(problems.empty()) << (problems.empty() ? "" : problems.front());
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WaitGlobal, .global = "A"}})), "does not wait until free"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WaitGlobal, .waitUntilFree = true}})), "names no global"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::SetGlobal, .global = "A B"}})), "names no global"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WaitTask, .waitUntilFree = true}})), "names no script"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WaitTask, .waitUntilFree = true, .value = 3, .script = "A"}})),
	                     "no such wait"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WaitText, .waitUntilFree = true}})), "no such text"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::StartScript}})), "names no script"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::LogGlobals, .global = " "}})), "logs nothing"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::ClickTexts, .value = 2}})), "neither start nor stop"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WaitCamera, .point = {1633.72f, 2307.98f}, .radius = 45.0f}})),
	                     "does not wait until free"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::WaitCamera, .waitUntilFree = true, .point = {1633.72f, 2307.98f}}})),
	                     "within no distance"));
	EXPECT_TRUE(Mentions(
	    Problems(OnLand({{.kind = Kind::WaitCamera, .waitUntilFree = true, .point = {-5.0f, 10.0f}, .radius = 45.0f}})),
	    "off the land"));
}

TEST(TestbedLandTimeline, AThingIsPickedAsTheNearestOfItsKindWithinTheRadius)
{
	using script::ObjectType;
	const std::vector<timeline::ThingFacts> things {
	    // a rock and two tiger stones by the dance site, one a little nearer; an ape stone further off
	    {.id = 10, .type = ObjectType::Rock, .subtype = 0, .at = {1652.0f, 2323.0f}},
	    {.id = 11, .type = ObjectType::MobileStatic, .subtype = 52, .at = {1660.0f, 2330.0f}},
	    {.id = 12, .type = ObjectType::MobileStatic, .subtype = 52, .at = {1655.0f, 2325.0f}},
	    {.id = 13, .type = ObjectType::MobileStatic, .subtype = 49, .at = {1700.0f, 2590.0f}},
	};
	const glm::vec2 point {1652.15f, 2322.91f};
	const timeline::ThingNear tiger {.type = ObjectType::MobileStatic, .subtype = 52, .point = point, .radius = 30.0f};
	EXPECT_EQ(timeline::Nearest(things, tiger, point), 12u);
	// any sub-type takes the nearest static, and too small a radius none
	auto any = tiger;
	any.subtype = ecs::script_type::k_AnySubtype;
	EXPECT_EQ(timeline::Nearest(things, any, point), 12u);
	auto close = tiger;
	close.radius = 1.0f;
	EXPECT_FALSE(timeline::Nearest(things, close, point).has_value());
	// a thing of no sub-type is never one of a sub-type asked for
	const std::vector<timeline::ThingFacts> unknown {{.id = 20, .type = ObjectType::MobileStatic, .at = point}};
	EXPECT_FALSE(timeline::Nearest(unknown, tiger, point).has_value());
	EXPECT_TRUE(timeline::WithinReach({1660.0f, 2330.0f}, tiger, point));
	EXPECT_FALSE(timeline::WithinReach({1700.0f, 2590.0f}, tiger, point));
}

TEST(TestbedLandTimeline, APlacedCameraStandsAboveTheGroundUnderItsEyeAndLook)
{
	const auto ground = [](glm::vec2 at) { return at.x > 1800.0f ? 30.0f : 10.0f; };
	const auto onLand = Placed({1774.5f, 60.0f, 2713.3f}, {1900.0f, 0.0f, 2793.3f}, timeline::Origin(true, {}), ground);
	EXPECT_EQ(onLand.origin, glm::vec3(1774.5f, 70.0f, 2713.3f));
	EXPECT_EQ(onLand.focus, glm::vec3(1900.0f, 30.0f, 2793.3f));
	// on the testbed, from the middle of the map
	const auto testbed = Placed({0.0f, 20.0f, -50.0f}, {0.0f, 0.0f, 0.0f}, {2000.0f, 2000.0f}, ground);
	EXPECT_EQ(testbed.origin, glm::vec3(2000.0f, 50.0f, 1950.0f));
	EXPECT_EQ(testbed.focus, glm::vec3(2000.0f, 30.0f, 2000.0f));
}

TEST(TestbedLandTimeline, ALandsObjectCommandsPickAThingOfTheLand)
{
	using script::ObjectType;
	const timeline::ThingNear stone {
	    .type = ObjectType::MobileStatic, .subtype = 52, .point = {1652.15f, 2322.91f}, .radius = 30.0f};
	const timeline::ThingNear plinth {.type = ObjectType::Feature, .point = {1774.5f, 2793.3f}, .radius = 5.0f};
	const auto scenario = OnLand({
	    {.kind = Kind::HandTake, .thing = stone},
	    {.kind = Kind::CameraTo, .eye = {1774.5f, 60.0f, 2713.3f}, .look = {1774.5f, 0.0f, 2793.3f}},
	    {.kind = Kind::HandOver, .point = {1774.5f, 2793.3f}},
	    {.kind = Kind::HandDrop},
	    {.kind = Kind::LayGateStone, .thing = stone, .other = plinth},
	    {.kind = Kind::TieLeash,
	     .creature = 0,
	     .thing = timeline::ThingNear {.type = ObjectType::Villager, .point = {1865.0f, 2641.0f}}},
	    {.kind = Kind::MoveThing, .point = {1896.5f, 2520.06f}},
	    {.kind = Kind::CameraFollow, .amount = 1.5f},
	    {.kind = Kind::SetCreatureSize, .creature = 0, .amount = 0.5f},
	});
	const auto problems = Problems(scenario);
	EXPECT_TRUE(problems.empty()) << (problems.empty() ? "" : problems.front());
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::LayGateStone, .thing = stone}})), "no plinth"));
	EXPECT_TRUE(
	    Mentions(Problems(OnLand({{.kind = Kind::HandTake, .thing = timeline::ThingNear {.point = stone.point}}})), "no type"));
	EXPECT_TRUE(Mentions(
	    Problems(OnLand(
	        {{.kind = Kind::HandTake, .thing = timeline::ThingNear {.type = ObjectType::Animal, .point = {-1.0f, 0.0f}}}})),
	    "off the land"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::CameraTo, .eye = {-10.0f, 50.0f, 0.0f}}})), "eye or look"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::HandOver, .value = 2}})), "neither starts nor stops"));
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::SetCreatureSize, .amount = 0.0f}})), "a size of nothing"));
}

TEST(TestbedLandTimeline, TheStorysChaptersArePlayedOnALandAndEndByLoggingTheirGlobals)
{
	EXPECT_EQ(Name(Facet::Story), "Story");
	for (const auto* id : {"story.t1_opening", "story.c1_tiger_stone"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_TRUE(scenario->onLand) << id;
		EXPECT_EQ(scenario->facet, Facet::Story) << id;
		ASSERT_FALSE(scenario->commands.empty()) << id;
		// the texts are clicked on from the start, and the end is a check
		EXPECT_EQ(scenario->commands.front().kind, Kind::ClickTexts) << id;
		EXPECT_EQ(scenario->commands.back().kind, Kind::LogGlobals) << id;
	}
	// the opening ends once the hand-over's text is read
	const auto& opening = Find("story.t1_opening")->commands;
	EXPECT_TRUE(std::ranges::any_of(opening, [](const Command& command) {
		return command.kind == Kind::WaitText && command.value == 5219 && command.read;
	}));
	// the tiger stone is the land's own, picked by its kind where the welcome's dance is held, and laid by the hand
	const auto& tiger = Find("story.c1_tiger_stone")->commands;
	const auto take = std::ranges::find(tiger, Kind::HandTake, &Command::kind);
	ASSERT_NE(take, tiger.end());
	ASSERT_TRUE(take->thing.has_value());
	EXPECT_EQ(take->thing->type, script::ObjectType::MobileStatic);
	EXPECT_EQ(take->thing->subtype, 52u);
	EXPECT_TRUE(std::ranges::any_of(tiger, [](const Command& command) {
		return command.kind == Kind::WaitGlobal && command.global == "GateKey2Present" && command.amount == 1.0f;
	}));
}
