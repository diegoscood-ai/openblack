/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's cases, with the original's rules. Not taken from his file: the script speed's conversion (our villager's
// script speed is VillagerSpeed's, ECS/VillagerSpeed.h) and the walking states that carry on a walk (ours stops every
// walk but MOVE_TO_POS's, VillagerScript.cpp)

#include <string>
#include <vector>

#include <ECS/HighDetailRules.h>
#include <ECS/VillagerScriptRules.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace rules = openblack::ecs::villager_script_rules;

TEST(VillagerScriptRules, DistanceIsAlongTheGround)
{
	// The game's own hypotenuse, a close approximation
	EXPECT_NEAR(rules::ScriptDistance({0.0f, 0.0f, 0.0f}, {3.0f, 50.0f, 4.0f}), 5.0f, 0.01f);
}

TEST(VillagerScriptRules, UnderHalfAMetreIsNoDistance)
{
	EXPECT_EQ(rules::ScriptDistance({10.0f, 2.0f, 10.0f}, {10.4f, 0.0f, 10.0f}), 0.0f);
	EXPECT_EQ(rules::ScriptDistance({10.0f, 2.0f, 10.0f}, {10.0f, 0.0f, 10.0f}), 0.0f);
	EXPECT_NEAR(rules::ScriptDistance({10.0f, 0.0f, 10.0f}, {10.5f, 0.0f, 10.0f}), 0.5f, 0.01f);
}

TEST(VillagerScriptRules, ClipPlaysCountDownThenWait)
{
	auto step = rules::StepScriptClip(2);
	EXPECT_EQ(step.playsLeft, 1u);
	EXPECT_EQ(step.then, VillagerStates::ScriptPlayAnim);
	step = rules::StepScriptClip(step.playsLeft);
	EXPECT_EQ(step.playsLeft, 0u);
	EXPECT_EQ(step.then, VillagerStates::InScript);
	step = rules::StepScriptClip(0);
	EXPECT_EQ(step.playsLeft, 0u);
	EXPECT_FALSE(step.then.has_value());
}

TEST(VillagerScriptRules, EndlessPlaysNeverRunOut)
{
	const auto step = rules::StepScriptClip(0xffffffffu);
	EXPECT_EQ(step.then, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(step.playsLeft, 0xfffffffeu);
}

TEST(VillagerScriptRules, PlayedOnceTheLastPlayEnds)
{
	EXPECT_FALSE(rules::HasPlayedScriptClip(VillagerStates::WaitForAnimation, 0));
	EXPECT_FALSE(rules::HasPlayedScriptClip(VillagerStates::ScriptPlayAnim, 1));
	EXPECT_TRUE(rules::HasPlayedScriptClip(VillagerStates::ScriptPlayAnim, 0));
	EXPECT_TRUE(rules::HasPlayedScriptClip(VillagerStates::InScript, 3));
	EXPECT_TRUE(rules::HasPlayedScriptClip(VillagerStates::MoveToPos, 3));
}

TEST(VillagerScriptRules, ScriptLetsGoOnlyForItsOwnStates)
{
	// The game asks whether the state it works towards and the next one share their exit, rather than listing the
	// script's states that leave the same way
	constexpr rules::StateRules k_Plain {.isScriptState = false, .isScriptInterruptable = false, .isFinalState = true};
	EXPECT_FALSE(rules::ScriptLetsGo(VillagerStates::DecideWhatToDo, k_Plain, false));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::InHand, k_Plain, false));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::DecideWhatToDo,
	                                {.isScriptState = true, .isScriptInterruptable = false, .isFinalState = true}, false));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::DecideWhatToDo,
	                                {.isScriptState = false, .isScriptInterruptable = true, .isFinalState = true}, false));
	// IN_SCRIPT and MOVE_ALONG_PATH share their exit
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::MoveAlongPath, k_Plain, true));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::ScriptPlayAnim, k_Plain, true));
}

TEST(VillagerScriptRules, ScriptLetsGoIntoAStateItOnlyPassesThrough)
{
	// A state that is not a final one lets it go whatever its exit
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::DecideWhatToDo,
	                                {.isScriptState = false, .isScriptInterruptable = false, .isFinalState = false}, false));
}

namespace
{
/// What a script's creation of a villager did, in order, with a fake villager numbered 7
std::vector<std::string> CreateLog(bool child, uint32_t grownUpAge)
{
	std::vector<std::string> log;
	const int made = rules::CreateScriptVillager(
	    child, grownUpAge,
	    [&log](uint32_t age) {
		    log.push_back("create " + std::to_string(age));
		    return 7;
	    },
	    [&log](int villager, uint32_t age) { log.push_back("age " + std::to_string(villager) + " " + std::to_string(age)); },
	    [&log](int villager, VillagerStates state) {
		    log.push_back("state " + std::to_string(villager) + " " + std::to_string(static_cast<int>(state)));
	    });
	log.push_back("made " + std::to_string(made));
	return log;
}
} // namespace

TEST(VillagerScriptRules, ScriptCreatesAnAdultAgesItAgainThenHoldsIt)
{
	// One year past growing up, the same age given twice, then IN_SCRIPT (4)
	EXPECT_EQ(CreateLog(false, 13), (std::vector<std::string> {"create 14", "age 7 14", "state 7 4", "made 7"}));
}

TEST(VillagerScriptRules, ScriptCreatesAChildOfTenAgesItAgainThenHoldsIt)
{
	// A child is 10 whatever its growing-up age
	EXPECT_EQ(CreateLog(true, 13), (std::vector<std::string> {"create 10", "age 7 10", "state 7 4", "made 7"}));
	EXPECT_EQ(rules::ScriptCreateAge(true, 30), 10u);
	EXPECT_EQ(rules::ScriptCreateAge(false, 30), 31u);
}

TEST(VillagerScriptRules, ARandomVillagerOfATribeIsOneOfItsSevenRows)
{
	// the Norse (7) start at row 49: the draw 0..6 picks one of 49..55
	EXPECT_EQ(rules::RandomVillagerRow(7, 0, 84), 49u);
	EXPECT_EQ(rules::RandomVillagerRow(7, 6, 84), 55u);
	EXPECT_EQ(rules::RandomVillagerRow(0, 3, 84), 3u);
	// the last tribe's last row is the table's last
	EXPECT_EQ(rules::RandomVillagerRow(11, 6, 84), 83u);
}

TEST(VillagerScriptRules, ARandomVillagerOutsideTheTableIsNone)
{
	EXPECT_FALSE(rules::RandomVillagerRow(12, 0, 84).has_value());
	EXPECT_FALSE(rules::RandomVillagerRow(-1, 6, 84).has_value());
	EXPECT_FALSE(rules::RandomVillagerRow(1 << 30, 0, 84).has_value());
	static_assert(rules::k_VillagerRowsPerTribe == 7);
}

TEST(HighDetail, OnlyTheOpeningsFamilyAndTheTrainerHaveDetailedModels)
{
	namespace hd = openblack::ecs::high_detail_rules;
	const auto man = hd::DetailedModelFor(openblack::MeshId::PersonNorseMaleA1);
	ASSERT_TRUE(man.has_value());
	EXPECT_EQ(man->file, "Intro/nors_man.l3d");
	EXPECT_EQ(man->face, hd::Face::Man);
	EXPECT_EQ(hd::DetailedModelFor(openblack::MeshId::PersonNorseFemaleA1)->face, hd::Face::Woman);
	EXPECT_EQ(hd::DetailedModelFor(openblack::MeshId::PersonBoyWhite1)->file, "Intro/nors_boy.l3d");
	// The trainer wears the woman's face
	EXPECT_EQ(hd::DetailedModelFor(openblack::MeshId::PersonAnimalTrainer)->face, hd::Face::Woman);
	EXPECT_FALSE(hd::DetailedModelFor(openblack::MeshId::PersonNorseMaleA2).has_value());
}

TEST(HighDetail, TheOpeningsOrdersTurnTheBoyAndEaseHisTurning)
{
	namespace hd = openblack::ecs::high_detail_rules;
	const hd::DrawOrders none;
	// Easing switched off, then on again
	auto result = hd::ApplyThingSpecial(hd::ThingSpecial::EaseTurning, false, none, 1.0f);
	EXPECT_TRUE(result.orders.turnAtOnce);
	EXPECT_FALSE(result.yAngle.has_value());
	result = hd::ApplyThingSpecial(hd::ThingSpecial::EaseTurning, true, result.orders, 1.0f);
	EXPECT_FALSE(result.orders.turnAtOnce);
	// Mirrored, he faces the other way round and turns at once
	result = hd::ApplyThingSpecial(hd::ThingSpecial::FaceMirrored, true, none, 1.0f);
	ASSERT_TRUE(result.yAngle.has_value());
	EXPECT_FLOAT_EQ(*result.yAngle, -1.0f);
	EXPECT_TRUE(result.orders.turnAtOnce);
	// Following the hand doesn't look at on
	result = hd::ApplyThingSpecial(hd::ThingSpecial::FollowIntroHand, false, none, 1.0f);
	EXPECT_TRUE(result.orders.followIntroHand);
	result = hd::ApplyThingSpecial(hd::ThingSpecial::TakeQuarterTurn, true, none, 1.0f);
	EXPECT_FLOAT_EQ(*result.yAngle, 1.0f - hd::k_QuarterTurn);
}

TEST(HighDetail, KeptOnlyWhileAScriptHoldsTheBars)
{
	namespace hd = openblack::ecs::high_detail_rules;
	EXPECT_TRUE(hd::KeepsHighDetail(true, 12));
	EXPECT_FALSE(hd::KeepsHighDetail(true, 0));
	EXPECT_FALSE(hd::KeepsHighDetail(false, 12));
}

TEST(HighDetail, DrawnAndItsEyesPlacedInTheHandsGripWhileHeld)
{
	namespace hd = openblack::ecs::high_detail_rules;
	const glm::vec3 standsAt {1494.895f, 0.0f, 2092.996f};
	const glm::vec3 grip {1416.0f, 1.2f, 2053.0f};
	EXPECT_EQ(hd::DrawnAt(std::nullopt, standsAt), standsAt);
	EXPECT_EQ(hd::DrawnAt(grip, standsAt), grip);
}

TEST(VillagerScriptRules, AWandererSetsOffOnceItsTurnsAtAStopHaveRunOut)
{
	namespace rules = openblack::ecs::villager_script_rules;
	// Put into the wandering state it has no turns to stand, so it sets off on its first turn
	EXPECT_TRUE(rules::StepWander(0).setsOff);
	// With turns left it stands, one fewer each turn, and sets off on the turn after the last
	auto step = rules::StepWander(2);
	EXPECT_FALSE(step.setsOff);
	EXPECT_EQ(step.turnsLeft, 1);
	step = rules::StepWander(step.turnsLeft);
	EXPECT_FALSE(step.setsOff);
	EXPECT_EQ(step.turnsLeft, 0);
	EXPECT_TRUE(rules::StepWander(step.turnsLeft).setsOff);
}

TEST(VillagerScriptRules, AWandererReadsItsTurnsAsASignedCount)
{
	// A count past the signed range sets off at once, as none does
	EXPECT_TRUE(rules::StepWander(0x8000).setsOff);
	EXPECT_TRUE(rules::StepWander(0xFFFF).setsOff);
	// The largest signed count still stands, one turn less
	const auto step = rules::StepWander(0x7FFF);
	EXPECT_FALSE(step.setsOff);
	EXPECT_EQ(step.turnsLeft, 0x7FFE);
}

TEST(VillagerScriptRules, AWandererStandsFromTheFewestToOneUnderTheMostTurns)
{
	namespace rules = openblack::ecs::villager_script_rules;
	uint32_t asked = 99;
	const auto lowest = [&asked](uint32_t limit) {
		asked = limit;
		return 0u;
	};
	const auto highest = [](uint32_t limit) { return limit - 1; };
	// The Lost Brother's sister stands 4 to 19 turns at each stop round her soapbox
	EXPECT_EQ(rules::WanderPause(4, 20, lowest), 4);
	EXPECT_EQ(asked, 16u);
	EXPECT_EQ(rules::WanderPause(4, 20, highest), 19);
}

TEST(VillagerScriptRules, AWandererDrawsItsBearingThenItsDistanceFromThePlace)
{
	std::vector<float> asked;
	const auto halfOfEach = [&asked](float most) {
		asked.push_back(most);
		return most / 2.0f;
	};
	const glm::vec2 place {1200.0f, 1500.0f};
	const auto stop = rules::NextWanderStop(place, 8.0f, halfOfEach);
	// The bearing is drawn first, from a whole turn, then the distance, from the script's
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[0], rules::k_FullTurn);
	EXPECT_EQ(asked[1], 8.0f);
	// Half a turn and half the distance: the stop is 4 m from the place, the way x falls
	auto expected = map_coords::FromMetres(place);
	gutils::AddDistanceFromAngle(expected, rules::k_FullTurn / 2.0f, 4.0f);
	EXPECT_EQ(stop.x, expected.x);
	EXPECT_EQ(stop.z, expected.z);
	EXPECT_NEAR(map_coords::ToMetres(stop).x, 1196.0f, 0.01f);
	EXPECT_NEAR(map_coords::ToMetres(stop).y, 1500.0f, 0.01f);
}
