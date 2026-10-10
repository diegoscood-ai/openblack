/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <numbers>
#include <vector>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace cast_moves = openblack::creature_cast_moves;

namespace
{
Random Always(uint32_t value)
{
	return [value](uint32_t range) { return std::min(value, range - 1); };
}

/// A stand of one keyframe that turns every bone by `angles` (x, y, z) and moves it by its translation
skeletal_animation::Animation StandOf(const std::vector<glm::vec3>& translations, const std::vector<glm::vec3>& angles)
{
	skeletal_animation::Animation stand {.duration = 1000, .looping = true};
	for (uint32_t bone = 0; bone < translations.size(); ++bone)
	{
		stand.rotatedJoints.push_back(bone);
		stand.translatedJoints.push_back(bone);
	}
	stand.frames.push_back({.eulerAngles = angles, .translations = translations});
	return stand;
}

/// A root and a bone under it, both at rest where their parents are
const std::vector<uint32_t> k_TwoBones {skeletal_animation::k_NoParent, 0};

/// The bits of 5 times InverseSquareRoot(1): a turn by nothing comes out of the normalisation of its rows a little
/// short, so a bone 5 out under a parent reaches out a little less
constexpr uint32_t k_FiveNormalised = 0x409FFFC4u;
} // namespace

TEST(CreatureCastAgenda, LightningFromFiftyBackingOffToTenMoreThanItsHeight)
{
	const auto agenda = CastAt(CastStyle::Lightning, 4, 0, 7, 15.0f, Always(1));
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 50.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 25.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 0.1f);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[3].cast.magicType, 4u);
	EXPECT_EQ(*agenda[3].cast.object, 7u);
	EXPECT_EQ(agenda[3].sequence, (std::array<size_t, 3> {40, 41, 42}));
	EXPECT_FLOAT_EQ(agenda[3].seconds, 3.0f);
}

TEST(CreatureCastAgenda, OneTimeInFiveItShowsHowItFeelsFirst)
{
	const auto playful = CastAt(CastStyle::Playful, 26, 0, 7, 10.0f, Always(0));
	ASSERT_EQ(playful.size(), 5u);
	EXPECT_EQ(playful[0].kind, Step::Kind::Action);
	EXPECT_EQ(playful[0].animation, 67u);
	EXPECT_FLOAT_EQ(playful[1].movement.maxDistance, 50.0f);
	EXPECT_FLOAT_EQ(playful[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(playful[3].face, creature_face::Cue::Compassion);
	const auto helpful = CastAt(CastStyle::Helpful, 10, 0, 7, 10.0f, Always(0));
	EXPECT_EQ(helpful[0].animation, 64u);
	EXPECT_FLOAT_EQ(helpful[1].movement.maxDistance, 20.0f);
	// A power-up draws its gesture before casting
	const auto powered = CastAt(CastStyle::Helpful, 11, 1, 7, 10.0f, Always(1));
	ASSERT_EQ(powered.size(), 5u);
	EXPECT_EQ(powered[3].kind, Step::Kind::Gesture);
	EXPECT_EQ(powered[3].animation, 1u);
}

TEST(CreatureCastAgenda, TheMiracleIsCastAsThePoseLoopsAndHeldThirtyTurns)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {CastAt(CastStyle::Lightning, 4, 0, 7, 15.0f, Always(1)).back()});
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.startSequence.has_value());
	EXPECT_FALSE(commands.cast.has_value());
	senses.bodyBusy = true;
	commands = Think(mind, senses, Always(0));
	EXPECT_FALSE(commands.cast.has_value());
	senses.bodyLooping = true;
	commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.cast.has_value());
	EXPECT_EQ(commands.cast->magicType, 4u);
	for (int turn = 0; turn < 30; ++turn)
	{
		commands = Think(mind, senses, Always(0));
		EXPECT_FALSE(commands.releaseCast) << turn;
		// As the code stands, the pose is told to end its loop on every held turn: the rule that ends a sit
		// counts only a still step as one. Pinned as it is, so the commit that first builds a casting step has
		// to change it on purpose (the creature page's Pending says so).
		EXPECT_TRUE(commands.endSit) << turn;
	}
	commands = Think(mind, senses, Always(0));
	EXPECT_TRUE(commands.releaseCast);
	EXPECT_TRUE(commands.endSit);
	senses.bodyBusy = false;
	senses.bodyLooping = false;
	commands = Think(mind, senses, Always(0));
	EXPECT_FALSE(commands.releaseCast);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.gaveUp);
}

TEST(CreatureCastAgenda, AMoveTheBodySaysFailedGivesUpTheRest)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, CastAt(CastStyle::Playful, 30, 0, 7, 15.0f, Always(1)));
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::GoNearObject);
	senses.subMove = SubMove::Running;
	(void)Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 0u);
	senses.subMove = SubMove::Done;
	(void)Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
	senses.subMove = SubMove::Running;
	(void)Think(mind, senses, Always(0));
	senses.subMove = SubMove::Failed;
	(void)Think(mind, senses, Always(0));
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_EQ(mind.step, mind.agenda.size());
}

TEST(CreatureCastMoves, RoutePlanRadiusAndArrival)
{
	// A thing as low as nothing: seven tenths of the creature's radius off its own
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 0.0f, false, 15.0f, 5.0f), 10.0f - 0.7f * 5.0f);
	// Half of four fifths of the creature's height
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 6.0f, false, 15.0f, 5.0f), 10.0f - (0.7f - 0.3f * 0.5f) * 5.0f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 20.0f, false, 15.0f, 5.0f), 10.0f - 0.4f * 5.0f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 20.0f, true, 15.0f, 5.0f), 0.25f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(1.0f, 20.0f, true, 15.0f, 5.0f), 0.1f);
	EXPECT_TRUE(cast_moves::Arrived(10.9f, 2.0f, 3.0f, 5.0f));
	EXPECT_FALSE(cast_moves::Arrived(11.0f, 2.0f, 3.0f, 5.0f));
}

TEST(CreatureCastMoves, GettingAway)
{
	EXPECT_FLOAT_EQ(cast_moves::GetAwayDistance(20.0f, 5.0f), 26.0f);
	EXPECT_FLOAT_EQ(cast_moves::GetAwayDistance(20.0f, std::nullopt), 20.0f);
	const auto away = cast_moves::GetAwayPoint({10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 26.0f);
	EXPECT_FLOAT_EQ(away.x, 36.0f);
	const auto onTop = cast_moves::GetAwayPoint({10.0f, 0.0f, 10.0f}, {10.0f, 0.0f, 10.0f}, 5.0f);
	EXPECT_FLOAT_EQ(onTop.x, 15.0f);
}

TEST(CreatureCastMoves, TheNearestClearAreaIsTheMiddleOfAClearSquare)
{
	// All clear: a square of two cells about the point, its middle at a cell corner
	const auto open = cast_moves::FindClearArea({125.0f, 125.0f}, 15.0f, [](int32_t, int32_t) { return true; });
	ASSERT_TRUE(open.has_value());
	EXPECT_FLOAT_EQ(open->x, 127.5f);
	EXPECT_FLOAT_EQ(open->y, 127.5f);
	// Nothing clear west of x = 200
	const auto east = cast_moves::FindClearArea({125.0f, 125.0f}, 15.0f, [](int32_t x, int32_t) { return x >= 20; });
	ASSERT_TRUE(east.has_value());
	EXPECT_FLOAT_EQ(east->x, 207.5f);
	EXPECT_FALSE(cast_moves::FindClearArea({125.0f, 125.0f}, 15.0f, [](int32_t, int32_t) { return false; }).has_value());
}

TEST(CreatureCastMoves, AThingKeepsCirclesClear)
{
	const glm::mat2 still(1.0f);
	// Squarish: one circle as wide as its longer side
	const auto square = cast_moves::CollideCircles({3.0f, 2.5f}, {10.0f, 10.0f}, still);
	ASSERT_EQ(square.size(), 1u);
	EXPECT_FLOAT_EQ(square[0].radius, 3.0f);
	// Long: a row of three as wide as its shorter side, along its length
	const auto wall = cast_moves::CollideCircles({6.0f, 2.5f}, {10.0f, 10.0f}, still);
	ASSERT_EQ(wall.size(), 3u);
	EXPECT_FLOAT_EQ(wall[0].radius, 2.5f);
	EXPECT_FLOAT_EQ(wall[0].centre.x, 10.0f - 4.0f);
	EXPECT_FLOAT_EQ(wall[2].centre.x, 10.0f + 4.0f);
	// Sides under 1 count as 1
	EXPECT_FLOAT_EQ(cast_moves::CollideCircles({0.2f, 0.2f}, {0.0f, 0.0f}, still)[0].radius, 1.0f);
	// A cell is blocked by a circle coming within 5 of its middle
	EXPECT_TRUE(cast_moves::BlocksCell({.centre = {19.5f, 5.0f}, .radius = 1.0f}, 1, 0));
	EXPECT_FALSE(cast_moves::BlocksCell({.centre = {5.0f, 5.0f}, .radius = 0.3f}, 1, 0));
}

TEST(CreatureCastMoves, TheBonesReachInTheStandsFirstKeyframe)
{
	// the bone 3 along and 4 back, under a root that is not turned: 5 out, less the rows' shortfall
	const std::vector<glm::mat4> rest(2, glm::mat4(1.0f));
	const auto stand = StandOf({glm::vec3(0.0f), glm::vec3(3.0f, 2.0f, 4.0f)}, std::vector<glm::vec3>(2, glm::vec3(0.0f)));
	const float reach = cast_moves::BoneReach(stand, k_TwoBones, rest);
	EXPECT_EQ(std::bit_cast<uint32_t>(reach), k_FiveNormalised);
	EXPECT_LT(reach, 5.0f);
	// the root's own move is not turned: 5 exactly
	const std::vector<uint32_t> root {skeletal_animation::k_NoParent};
	const std::vector<glm::mat4> rootRest(1, glm::mat4(1.0f));
	EXPECT_EQ(cast_moves::BoneReach(StandOf({glm::vec3(3.0f, 7.0f, 4.0f)}, {glm::vec3(0.0f)}), root, rootRest), 5.0f);
}

TEST(CreatureCastMoves, TheBonesReachTheSameWithTheRootTurned)
{
	// the root's rest pose a quarter turn about the upright axis: rows (0, 0, -1), (0, 1, 0), (1, 0, 0)
	std::vector<glm::mat4> rest(2, glm::mat4(1.0f));
	rest[0][0] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
	rest[0][2] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	const auto stand = StandOf({glm::vec3(0.0f), glm::vec3(3.0f, 2.0f, 4.0f)}, std::vector<glm::vec3>(2, glm::vec3(0.0f)));
	EXPECT_EQ(std::bit_cast<uint32_t>(cast_moves::BoneReach(stand, k_TwoBones, rest)), k_FiveNormalised);
	// turned a quarter by the keyframe instead
	const std::vector<glm::mat4> still(2, glm::mat4(1.0f));
	const auto turned = StandOf({glm::vec3(0.0f), glm::vec3(3.0f, 2.0f, 4.0f)},
	                            {glm::vec3(0.0f, std::numbers::pi_v<float> / 2.0f, 0.0f), glm::vec3(0.0f)});
	EXPECT_NEAR(cast_moves::BoneReach(turned, k_TwoBones, still), 5.0f, 1e-4f);
}

TEST(CreatureCastMoves, NoBonesOrNoKeyframeReachNothing)
{
	const auto stand = StandOf({glm::vec3(3.0f, 0.0f, 4.0f)}, {glm::vec3(0.0f)});
	EXPECT_EQ(cast_moves::BoneReach(stand, {}, {}), 0.0f);
	const std::vector<uint32_t> root {skeletal_animation::k_NoParent};
	const std::vector<glm::mat4> rest(1, glm::mat4(1.0f));
	skeletal_animation::Animation empty {.duration = 1000, .looping = true};
	empty.rotatedJoints = {0};
	empty.translatedJoints = {0};
	EXPECT_EQ(cast_moves::BoneReach(empty, root, rest), 0.0f);
}

TEST(CreaturePlanActionsCasting, ACastingActionNeedsWhatItCastsAndNoneIsInTheTableYet)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const plan_actions::Executor lightning {
	    .action = "Made-up", .target = plan_actions::Target::Anything, .build = plan_actions::Build::CastLightning};
	EXPECT_TRUE(plan_actions::IsCast(lightning));
	EXPECT_FALSE(plan_actions::IsCast(*plan_actions::For("Scratch")));
	// Without what it casts it can't be carried out
	EXPECT_FALSE(plan_actions::Agenda(lightning, 7u, {}, {}, Always(1)).has_value());
	const auto agenda =
	    plan_actions::Agenda(lightning, 7u, {}, {}, Always(1), plan_actions::CastInfo {.magicType = 4, .height = 15.0f});
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4u);
	EXPECT_EQ(agenda->back().kind, Step::Kind::Cast);
	EXPECT_EQ(agenda->back().cast.magicType, 4u);
	// The game's casting actions get their rows with the casting itself, so the planner can't pick one yet
	for (const auto* action : {"CastLightningBolt", "CastHealSpell", "CastMakeCreatureFreeze", "CastMakeCreatureItchy"})
	{
		EXPECT_EQ(plan_actions::For(action), nullptr) << action;
	}
}
