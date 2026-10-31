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

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
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

// A task's place: the line of its next instruction and the native it called last, within a few instructions back
TEST(TestbedLandTimeline, ATasksPlaceIsItsLineAndItsLastCall)
{
	using lhvm::DataType;
	using lhvm::Opcode;
	using lhvm::VMMode;
	const std::vector<lhvm::NativeFunction> functions {{[] {}, 0, 0, "NONE"}, {[] {}, 1, 1, "GAME_THING_CLICKED"}};
	std::vector<lhvm::VMInstruction> instructions;
	instructions.emplace_back(Opcode::Push, VMMode::Immediate, DataType::Int, lhvm::VMValue(int32_t {0}), 12);
	instructions.emplace_back(Opcode::Sys, VMMode::Immediate, DataType::None, lhvm::VMValue(uint32_t {1}), 12);
	instructions.emplace_back(Opcode::Wait, VMMode::Immediate, DataType::None, lhvm::VMValue(uint32_t {0}), 13);
	auto task = Task("GuideImpressTownLesson");
	task.instructionAddress = 2;
	const auto place = timeline::WhereIs(task, instructions, &functions);
	EXPECT_EQ(place.line, 13u);
	EXPECT_EQ(place.lastCall, "GAME_THING_CLICKED");
	// before any call, or past the code, none
	task.instructionAddress = 0;
	EXPECT_TRUE(timeline::WhereIs(task, instructions, &functions).lastCall.empty());
	task.instructionAddress = 9;
	EXPECT_EQ(timeline::WhereIs(task, instructions, &functions).line, 0u);
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

TEST(TestbedLandTimeline, ATextIsSpokenOnceTheDialogueReadsItAsReadOrALaterOneIsShown)
{
	const std::array<uint32_t, 6> texts {4532, 4531, 0, 0, 0, 0};
	// the current text is still being said: shown, not yet spoken; the one before it was
	EXPECT_FALSE(timeline::TextSpoken(texts, false, 4532));
	EXPECT_TRUE(timeline::TextSpoken(texts, true, 4532));
	EXPECT_TRUE(timeline::TextSpoken(texts, false, 4531));
	// a text not shown, and text 0, are never spoken
	EXPECT_FALSE(timeline::TextSpoken(texts, true, 4533));
	EXPECT_FALSE(timeline::TextSpoken(texts, true, 0));
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

TEST(TestbedLandTimeline, KnockingATownDownTakesItsBuildingsAndItsPeople)
{
	constexpr uint32_t k_Town = 330;
	constexpr uint32_t k_OtherTown = 105;
	const std::vector<timeline::TownMember> buildings {
	    {.id = 1, .town = k_Town},
	    {.id = 2, .town = k_Town, .available = false},
	    {.id = 3, .town = k_Town},
	};
	const std::vector<timeline::TownMember> villagers {
	    {.id = 10, .town = k_Town},
	    {.id = 11, .town = k_OtherTown},
	    {.id = 12, .town = k_Town, .available = false},
	    {.id = 13, .town = k_Town},
	};
	const auto targets = timeline::KnockDownTargets(k_Town, buildings, villagers);
	// the town's own, still on the map, in their order; another town's villager stays
	EXPECT_EQ(targets.buildings, (std::vector<uint32_t> {1, 3}));
	EXPECT_EQ(targets.villagers, (std::vector<uint32_t> {10, 13}));
	// a town with nothing left takes nothing
	const auto none = timeline::KnockDownTargets(k_Town, {}, {});
	EXPECT_TRUE(none.buildings.empty());
	EXPECT_TRUE(none.villagers.empty());
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

// A town of the land changes hands by the game's own take-over, picked by its script type; a challenge's entry is
// replayed from the log by its number
TEST(TestbedLandTimeline, ATownChangesHandsAndAChallengeIsReplayedFromTheLog)
{
	using script::ObjectType;
	const timeline::ThingNear norse {.type = ObjectType::Town, .point = {2571.77f, 3068.29f}, .radius = 100.0f};
	const auto scenario = OnLand({
	    {.kind = Kind::TownOwner, .player = PlayerNames::PLAYER_ONE, .thing = norse},
	    {.kind = Kind::TownOwner, .player = PlayerNames::NEUTRAL, .thing = norse},
	    {.kind = Kind::ReplayChallenge, .value = 59},
	});
	const auto problems = Problems(scenario);
	EXPECT_TRUE(problems.empty()) << (problems.empty() ? "" : problems.front());
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::TownOwner}})), "takes over no town"));
	auto animal = norse;
	animal.type = ObjectType::Animal;
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::TownOwner, .thing = animal}})), "takes over no town"));
	EXPECT_TRUE(
	    Mentions(Problems(OnLand({{.kind = Kind::TownOwner, .player = PlayerNames::_COUNT, .thing = norse}})), "no player"));
	EXPECT_TRUE(NeedsNoCreature(Kind::TownOwner));
	EXPECT_TRUE(NeedsNoCreature(Kind::ReplayChallenge));
}

// Every building of a town of the land is knocked down by a script's deletion, the town picked by its script type
TEST(TestbedLandTimeline, ATownIsKnockedDownByItsPick)
{
	using script::ObjectType;
	const timeline::ThingNear norse {.type = ObjectType::Town, .point = {2571.77f, 3068.29f}, .radius = 100.0f};
	const auto problems = Problems(OnLand({{.kind = Kind::KnockDownTown, .thing = norse}}));
	EXPECT_TRUE(problems.empty()) << (problems.empty() ? "" : problems.front());
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::KnockDownTown}})), "knocks down no town"));
	auto abode = norse;
	abode.type = ObjectType::Abode;
	EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = Kind::KnockDownTown, .thing = abode}})), "knocks down no town"));
	EXPECT_TRUE(NeedsNoCreature(Kind::KnockDownTown));
}

// A throw at a point of the land is the player's mouse, and a script's building order and a gesture centred on the land
// need no creature; their points must be on the land
TEST(TestbedLandTimeline, AThrowABuildingOrderAndAGestureAimAtPointsOfTheLand)
{
	EXPECT_TRUE(IsPointerCommand(Kind::HandThrowAt));
	EXPECT_TRUE(NeedsNoCreature(Kind::HandThrowAt));
	EXPECT_FALSE(IsPointerCommand(Kind::ScriptBuildBuilding));
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptBuildBuilding));
	EXPECT_EQ(Name(Kind::HandThrowAt), "hand throws at a point");
	EXPECT_EQ(Name(Kind::ScriptBuildBuilding), "script builds a building");
	const glm::vec2 temple {1915.05f, 2508.89f};
	const auto problems = Problems(OnLand({{.kind = Kind::HandThrowAt, .point = temple},
	                                       {.kind = Kind::ScriptBuildBuilding, .point = temple, .amount = 1.0f},
	                                       {.kind = Kind::DrawGesture, .point = temple, .value = 1}}));
	EXPECT_TRUE(problems.empty()) << (problems.empty() ? "" : problems.front());
	for (const auto kind : {Kind::HandThrowAt, Kind::ScriptBuildBuilding, Kind::DrawGesture})
	{
		EXPECT_TRUE(Mentions(Problems(OnLand({{.kind = kind, .point = {-1.0f, 10.0f}, .value = 1}})), "off the land"))
		    << Name(kind);
	}
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

// A quest run's first chapter starts its scripts in turn, every one but the last waited for until none of its task is
// left; the chapter plays on the land
TEST(TestbedQuestChapter, StartsEachScriptOnceTheOneBeforeHasFinished)
{
	const QuestChapter chapter({{.name = "SetupLand1"}, {.name = "ChooseYourCreature"}});
	const auto& scenario = chapter.Get();
	EXPECT_TRUE(scenario.onLand);
	EXPECT_EQ(scenario.name, "Quest run: SetupLand1 ChooseYourCreature");
	const auto& commands = scenario.commands;
	ASSERT_EQ(commands.size(), 3u);
	EXPECT_EQ(commands[0].kind, Kind::StartScript);
	EXPECT_EQ(commands[0].script, "SetupLand1");
	EXPECT_EQ(commands[1].kind, Kind::WaitTask);
	EXPECT_EQ(commands[1].script, "SetupLand1");
	EXPECT_EQ(commands[1].value, static_cast<size_t>(timeline::TaskWait::NotRunning));
	EXPECT_EQ(commands[2].kind, Kind::StartScript);
	EXPECT_EQ(commands[2].script, "ChooseYourCreature");
	EXPECT_TRUE(commands[0].scriptArguments.empty());
}

// A quest's script is written as its name, then its arguments in parentheses if it takes any: numbers, markers made at
// a point, and the things of a script type nearest a point (within 10 unless a radius is given)
TEST(TestbedQuestScript, ANameThenItsNumbersMarkersAndThings)
{
	const auto plain = timeline::ParseQuestScript("SetupLand2");
	ASSERT_TRUE(plain.has_value());
	EXPECT_EQ(plain->name, "SetupLand2");
	EXPECT_TRUE(plain->arguments.empty());
	const auto full = timeline::ParseQuestScript("Land2VortexEntry(30,marker@1500;12.5;2000,thing@4;1520;2010;40,-2.5)");
	ASSERT_TRUE(full.has_value());
	ASSERT_EQ(full->arguments.size(), 4u);
	using ArgumentKind = timeline::ScriptArgument::Kind;
	EXPECT_EQ(full->arguments[0].kind, ArgumentKind::Number);
	EXPECT_EQ(full->arguments[0].number, 30.0f);
	EXPECT_EQ(full->arguments[1].kind, ArgumentKind::Marker);
	EXPECT_EQ(full->arguments[1].point, glm::vec3(1500.0f, 12.5f, 2000.0f));
	EXPECT_EQ(full->arguments[2].kind, ArgumentKind::Thing);
	EXPECT_EQ(full->arguments[2].thingType, 4u);
	EXPECT_EQ(full->arguments[2].thingPoint, glm::vec2(1520.0f, 2010.0f));
	EXPECT_EQ(full->arguments[2].thingRadius, 40.0f);
	EXPECT_EQ(full->arguments[3].number, -2.5f);
	EXPECT_EQ(timeline::ParseQuestScript("A(thing@4;1;2)")->arguments[0].thingRadius, 10.0f);
	// a global set as the story before the quest would have set it
	const auto global = timeline::ParseQuestScript("ProtectGateKeysFlag=1");
	ASSERT_TRUE(global.has_value());
	EXPECT_EQ(global->name, "ProtectGateKeysFlag");
	EXPECT_EQ(global->globalValue, 1.0f);
	EXPECT_FALSE(timeline::ParseQuestScript("A=x").has_value());
	EXPECT_FALSE(timeline::ParseQuestScript("A(1)=2").has_value());
	EXPECT_FALSE(timeline::ParseQuestScript("A=marker@1;2;3").has_value());
	// a script left running while the next starts
	const auto background = timeline::ParseQuestScript("Quest(1)&");
	ASSERT_TRUE(background.has_value());
	EXPECT_TRUE(background->background);
	EXPECT_EQ(background->arguments.size(), 1u);
	EXPECT_FALSE(timeline::ParseQuestScript("A&&").has_value());
	EXPECT_FALSE(timeline::ParseQuestScript("A=1&").has_value());
	const QuestChapter both({{.name = "First", .background = true}, {.name = "Second"}});
	ASSERT_EQ(both.Get().commands.size(), 2u);
	EXPECT_EQ(both.Get().commands[1].kind, Kind::StartScript);
	const QuestChapter chapter({{.name = "Flag", .globalValue = 1.0f}, {.name = "Quest"}});
	ASSERT_EQ(chapter.Get().commands.size(), 2u);
	EXPECT_EQ(chapter.Get().commands[0].kind, Kind::SetGlobal);
	EXPECT_EQ(chapter.Get().commands[0].global, "Flag");
	EXPECT_EQ(chapter.Get().commands[0].amount, 1.0f);
	for (const auto* bad :
	     {"", "A(", "A()", "A(1,)", "A(x)", "A(marker@1;2)", "A(thing@1.5;1;2)", "A(thing@300;1;2)", "A b", "A-b"})
	{
		EXPECT_FALSE(timeline::ParseQuestScript(bad).has_value()) << bad;
	}
}

namespace
{
/// Where a throw lands on flat land, falling from its release under gravity alone
glm::vec2 Landing(const timeline::HandThrow& plan)
{
	return plan.release + plan.velocity * plan.flightSeconds;
}
} // namespace

TEST(TestbedLandTimeline, HandThrowLandsOnTheTargetOverFlatLand)
{
	const auto flat = [](glm::vec2 /*at*/) { return 0.0f; };
	// let go 4.905 above the land, it falls for a second; with no trail it is carried the whole half-second run-up
	const auto plan = timeline::PlanHandThrow(
	    {.from = {100.0f, 200.0f}, .target = {130.0f, 200.0f}, .heightAboveLand = 4.905f, .runUpSeconds = 0.5f}, flat);
	ASSERT_TRUE(plan.has_value());
	EXPECT_NEAR(plan->flightSeconds, 1.0f, 1e-5f);
	EXPECT_NEAR(plan->velocity.x, 20.0f, 1e-4f);
	EXPECT_NEAR(plan->velocity.y, 0.0f, 1e-6f);
	EXPECT_NEAR(plan->release.x, 110.0f, 1e-4f);
	EXPECT_EQ(plan->upSpeed, 0.0f);
	EXPECT_TRUE(plan->withinTopSpeed);
	const auto landing = Landing(*plan);
	EXPECT_NEAR(landing.x, 130.0f, 1e-3f);
	EXPECT_NEAR(landing.y, 200.0f, 1e-3f);
}

TEST(TestbedLandTimeline, HandThrowTrailsThePointer)
{
	const auto flat = [](glm::vec2 /*at*/) { return 0.0f; };
	// a thing trailing the pointer by 0.1 s of motion is let go that far behind the run-up's end
	const auto plan = timeline::PlanHandThrow(
	    {.from = {0.0f, 0.0f}, .target = {0.0f, 50.0f}, .heightAboveLand = 4.905f, .runUpSeconds = 0.5f, .trailSeconds = 0.1f},
	    flat);
	ASSERT_TRUE(plan.has_value());
	EXPECT_NEAR(plan->velocity.y, 50.0f / 1.4f, 1e-3f);
	EXPECT_NEAR(plan->release.y, plan->velocity.y * 0.4f, 1e-3f);
	EXPECT_NEAR(Landing(*plan).y, 50.0f, 1e-3f);
	const auto end = timeline::RunUpPoint(*plan, 0.5f);
	EXPECT_NEAR(end.y - plan->release.y, plan->velocity.y * 0.1f, 1e-3f);
}

TEST(TestbedLandTimeline, HandThrowFallsToTheTargetsLand)
{
	// the target's land 4.905 lower than the hand's: the thing falls twice the height, for sqrt(2) seconds
	const auto stepDown = [](glm::vec2 at) { return at.x > 20.0f ? 0.0f : 4.905f; };
	const auto plan = timeline::PlanHandThrow(
	    {.from = {0.0f, 0.0f}, .target = {40.0f, 0.0f}, .heightAboveLand = 4.905f, .runUpSeconds = 0.5f}, stepDown);
	ASSERT_TRUE(plan.has_value());
	EXPECT_NEAR(plan->flightSeconds, std::sqrt(2.0f), 1e-4f);
	EXPECT_NEAR(Landing(*plan).x, 40.0f, 1e-3f);
}

TEST(TestbedLandTimeline, HandThrowRisesWithTheSlope)
{
	// land rising 0.1 a unit eastwards: the hand climbs as it moves, and the thing leaves it rising as fast
	const auto slope = [](glm::vec2 at) { return at.x * 0.1f; };
	const auto plan = timeline::PlanHandThrow(
	    {.from = {0.0f, 0.0f}, .target = {30.0f, 0.0f}, .heightAboveLand = 6.0f, .runUpSeconds = 0.5f}, slope);
	ASSERT_TRUE(plan.has_value());
	EXPECT_NEAR(plan->upSpeed, plan->velocity.x * 0.1f, 0.05f);
	// its height at the landing is the target's land, by the rise and fall the plan works out
	const float height = 6.0f + slope(plan->release) + plan->upSpeed * plan->flightSeconds -
	                     0.5f * 9.81f * plan->flightSeconds * plan->flightSeconds;
	EXPECT_NEAR(height, slope({30.0f, 0.0f}), 0.05f);
	EXPECT_NEAR(Landing(*plan).x, 30.0f, 0.5f);
}

TEST(TestbedLandTimeline, HandThrowBeyondTheHandsSpeedOrWithNoRunUp)
{
	const auto flat = [](glm::vec2 /*at*/) { return 0.0f; };
	const auto far = timeline::PlanHandThrow(
	    {.from = {0.0f, 0.0f}, .target = {1000.0f, 0.0f}, .heightAboveLand = 4.905f, .runUpSeconds = 0.5f}, flat);
	ASSERT_TRUE(far.has_value());
	EXPECT_FALSE(far->withinTopSpeed);
	EXPECT_FALSE(
	    timeline::PlanHandThrow({.target = {10.0f, 0.0f}, .heightAboveLand = 5.0f, .runUpSeconds = 0.0f}, flat).has_value());
	// a thing below the target's land with no rise can't reach it
	const auto wall = [](glm::vec2 at) { return at.x > 5.0f ? 10.0f : 0.0f; };
	EXPECT_FALSE(
	    timeline::PlanHandThrow({.target = {10.0f, 0.0f}, .heightAboveLand = 1.0f, .runUpSeconds = 0.5f}, wall).has_value());
}

TEST(TestbedLandTimeline, HandThrowRunUpOnTheScreen)
{
	const timeline::HandThrow plan {.from = {10.0f, 20.0f}, .velocity = {4.0f, -2.0f}, .runUpSeconds = 0.5f};
	EXPECT_EQ(timeline::RunUpPoint(plan, -1.0f), glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(timeline::RunUpPoint(plan, 0.25f), glm::vec2(11.0f, 19.5f));
	// held at the run-up's end
	EXPECT_EQ(timeline::RunUpPoint(plan, 3.0f), glm::vec2(12.0f, 19.0f));
	// a camera seeing the land at twice its x and three times its z, and nothing west of x 11
	const auto project = [](glm::vec2 at) -> std::optional<glm::vec2> {
		if (at.x < 11.0f)
		{
			return std::nullopt;
		}
		return glm::vec2(at.x * 2.0f, at.y * 3.0f);
	};
	EXPECT_FALSE(timeline::RunUpPixel(plan, 0.0f, project).has_value());
	const auto pixel = timeline::RunUpPixel(plan, 0.5f, project);
	ASSERT_TRUE(pixel.has_value());
	EXPECT_EQ(*pixel, glm::vec2(24.0f, 57.0f));
}
