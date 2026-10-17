/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <numbers>
#include <vector>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Enums.h"

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

/// The bits of 5 times ModelInverseSquareRoot(1): a turn by nothing comes out of the normalisation of its rows a little
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
	Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
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
		// the pose's loop is held while the miracle is
		EXPECT_FALSE(commands.endSit) << turn;
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

TEST(CreatureCastAgenda, TheHoldIsTheStepsSecondsInTheGamesWholeTurns)
{
	const auto heldTurns = [](uint32_t turnsPerSecond) {
		IdleMind mind;
		Plan(mind, Activity::Planned, {CastAt(CastStyle::Lightning, 4, 0, 7, 15.0f, Always(1)).back()});
		Senses senses {.seconds = 0.1f, .turnsPerSecond = turnsPerSecond};
		Think(mind, senses, Always(0));
		senses.bodyBusy = true;
		senses.bodyLooping = true;
		const auto commands = Think(mind, senses, Always(0));
		EXPECT_TRUE(commands.cast.has_value());
		uint32_t turns = 0;
		while (!Think(mind, senses, Always(0)).releaseCast && turns < 1000)
		{
			++turns;
		}
		return turns;
	};
	EXPECT_EQ(heldTurns(10), 30u);
	// at 16 ms a turn the game has 62 whole turns a second, not 62.5
	EXPECT_EQ(heldTurns(1000 / 16), 186u);
	// with no turns a second it lets go on the next turn
	EXPECT_EQ(heldTurns(0), 0u);
}

TEST(CreatureCastAgenda, TheCastGrimacesAndItsPoseDrawsAVariantAsItStarts)
{
	const auto agenda = CastAt(CastStyle::Lightning, 4, 0, 7, 15.0f, Always(1));
	EXPECT_EQ(agenda.back().face, creature_face::Cue::Grimace);
	IdleMind mind;
	Plan(mind, Activity::Planned, {agenda.back()});
	std::vector<uint32_t> draws;
	const auto commands = Think(mind, Senses {.seconds = 0.1f, .turnsPerSecond = 10}, [&draws](uint32_t range) {
		draws.push_back(range);
		return 0u;
	});
	ASSERT_TRUE(commands.startSequence.has_value());
	ASSERT_TRUE(commands.face.has_value());
	// the face's own draws first, then the pose's variant
	ASSERT_FALSE(draws.empty());
	EXPECT_EQ(draws.back(), 2u);
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

TEST(CreatureCastAgenda, FoodIsCastKindlyFromTwoAwayInTheScatteringPose)
{
	const auto agenda = CastFoodAt(0, 7, k_FoodGoNear);
	ASSERT_EQ(agenda.size(), 3u);
	// no feeling shown first and no backing off: it goes near, kind, then turns to it, kind, and holds still
	EXPECT_EQ(agenda[0].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(*agenda[0].movement.object, 7u);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 2.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(*agenda[1].movement.object, 7u);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Compassion);
	// food, in the scattering pose, held three seconds, grimacing
	EXPECT_EQ(agenda[2].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[2].cast.magicType, static_cast<uint32_t>(MagicType::Food));
	EXPECT_EQ(*agenda[2].cast.object, 7u);
	EXPECT_EQ(agenda[2].animation, 47u);
	EXPECT_EQ(agenda[2].sequence, (std::array<size_t, 3> {46, 47, 48}));
	EXPECT_FLOAT_EQ(agenda[2].seconds, 3.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Grimace);
	// from twenty away at a sign that shows a need
	EXPECT_FLOAT_EQ(CastFoodAt(0, 7, k_FoodGoNearSign)[0].movement.maxDistance, 20.0f);
}

TEST(CreatureCastAgenda, FoodDrawsItsGestureOnlyWhenItHasOne)
{
	const auto agenda = CastFoodAt(3, 7, k_FoodGoNear);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Gesture);
	EXPECT_EQ(agenda[2].animation, 3u);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Cast);
	for (const auto& step : CastFoodAt(0, 7, k_FoodGoNear))
	{
		EXPECT_NE(step.kind, Step::Kind::Gesture);
	}
}

TEST(CreatureCastAgenda, WaterIsCastFromTwiceItsHeightSmilingInTheScatteringPose)
{
	const auto agenda = CastWaterAt(7, 15.0f);
	ASSERT_EQ(agenda.size(), 3u);
	// going near it pulls no face
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::None);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Smile);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[2].cast.magicType, static_cast<uint32_t>(MagicType::Water));
	EXPECT_EQ(*agenda[2].cast.object, 7u);
	EXPECT_EQ(agenda[2].sequence, k_ScatterPose);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 3.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Smile);
}

TEST(CreatureCastAgenda, TheScatteringPoseStartsAsTheCastStepStarts)
{
	for (const auto& cast : {CastFoodAt(0, 7, k_FoodGoNear).back(), CastWaterAt(7, 15.0f).back()})
	{
		IdleMind mind;
		Plan(mind, Activity::Planned, {cast});
		const auto commands = Think(mind, Senses {.seconds = 0.1f, .turnsPerSecond = 10}, Always(0));
		ASSERT_TRUE(commands.startSequence.has_value());
		EXPECT_EQ(*commands.startSequence, (std::array<size_t, 3> {46, 47, 48}));
		EXPECT_FALSE(commands.cast.has_value());
	}
}

TEST(CreatureCastAgenda, WaterOnItselfIsCastFromAClearSpotItGoesToFirst)
{
	const auto agenda = CastWaterOnItself(glm::vec2(5.0f, 6.0f), 3, 15.0f);
	ASSERT_EQ(agenda.size(), 2u);
	// to within twice its height of the spot, pulling no face
	EXPECT_EQ(agenda[0].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(5.0f, 6.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::None);
	// then water on itself in the scattering pose, smiling
	EXPECT_EQ(agenda[1].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[1].cast.magicType, static_cast<uint32_t>(MagicType::Water));
	EXPECT_EQ(*agenda[1].cast.object, 3u);
	EXPECT_EQ(agenda[1].sequence, k_ScatterPose);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 3.0f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Smile);
	EXPECT_FLOAT_EQ(k_WaterOnItselfClearWidth, 1.7f);
}

TEST(CreatureCastMoves, RoutePlanRadiusAndArrival)
{
	// A thing as low as nothing: seven tenths of the creature's radius off its own
	EXPECT_EQ(cast_moves::RoutePlanRadius(10.0f, 0.0f, false, 15.0f, 5.0f), 10.0f - 5.0f * 0.7f);
	// Half of four fifths of the creature's height: the share of the height, then of the keep
	EXPECT_EQ(cast_moves::RoutePlanRadius(10.0f, 6.0f, false, 15.0f, 5.0f), 10.0f - 5.0f * (0.7f - (6.0f / 12.0f) * 0.3f));
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 6.0f, false, 15.0f, 5.0f), 10.0f - (0.7f - 0.3f * 0.5f) * 5.0f);
	EXPECT_EQ(cast_moves::RoutePlanRadius(10.0f, 20.0f, false, 15.0f, 5.0f), 10.0f - 5.0f * 0.4f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 20.0f, true, 15.0f, 5.0f), 0.25f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(1.0f, 20.0f, true, 15.0f, 5.0f), 0.1f);
	EXPECT_TRUE(cast_moves::Arrived(10.9f, 2.0f, 3.0f, 5.0f));
	EXPECT_FALSE(cast_moves::Arrived(11.0f, 2.0f, 3.0f, 5.0f));
}

TEST(CreatureCastMoves, RoutePlanKeepDividesFirst)
{
	// the height over four fifths of the creature's, then times 0.3, each step a float. Find a height where taking 0.3
	// of it first would give other bits, so the order is pinned
	const float creatureHeight = 15.0f;
	const float low = creatureHeight * 0.8f;
	float height = 0.0f;
	for (int i = 1; i < 1200; ++i)
	{
		const float h = static_cast<float>(i) * 0.01f;
		if (std::bit_cast<uint32_t>(0.7f - (h / low) * 0.3f) != std::bit_cast<uint32_t>(0.7f - 0.3f * h / low))
		{
			height = h;
			break;
		}
	}
	ASSERT_GT(height, 0.0f);
	const float keep = 0.7f - (height / low) * 0.3f;
	EXPECT_EQ(std::bit_cast<uint32_t>(cast_moves::RoutePlanKeep(height, creatureHeight)), std::bit_cast<uint32_t>(keep));
	EXPECT_EQ(std::bit_cast<uint32_t>(cast_moves::RoutePlanRadius(3.0f, height, false, creatureHeight, 2.5f)),
	          std::bit_cast<uint32_t>(3.0f - 2.5f * keep));
}

TEST(CreatureCastMoves, RoutePlanKeepAtFourFifthsAndForNoNumber)
{
	// as tall as four fifths of the creature: still the curve, 0.7 - 0.3
	EXPECT_EQ(cast_moves::RoutePlanKeep(12.0f, 15.0f), 0.7f - 1.0f * 0.3f);
	// just taller: two fifths
	EXPECT_EQ(cast_moves::RoutePlanKeep(std::nextafter(12.0f, 13.0f), 15.0f), 0.4f);
	// a height that is not a number takes the curve, and so does the radius
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(std::isnan(cast_moves::RoutePlanKeep(nan, 15.0f)));
	EXPECT_TRUE(std::isnan(cast_moves::RoutePlanRadius(3.0f, nan, false, 15.0f, 2.5f)));
	// a creature with no height: anything with a height is taller
	EXPECT_EQ(cast_moves::RoutePlanKeep(1.0f, 0.0f), 0.4f);
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

TEST(CreaturePlanActionsCasting, ACastingActionNeedsWhatItCasts)
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
}

TEST(CreaturePlanActionsCasting, TheGamesCastsInTheirStyles)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using plan_actions::Build;
	using plan_actions::Target;
	const auto* lightning = plan_actions::For("CastLightningBolt");
	ASSERT_NE(lightning, nullptr);
	EXPECT_EQ(lightning->build, Build::CastLightning);
	EXPECT_EQ(lightning->target, Target::Anything);
	const auto* heal = plan_actions::For("CastHealSpell");
	ASSERT_NE(heal, nullptr);
	EXPECT_EQ(heal->build, Build::CastHelpful);
	EXPECT_EQ(heal->target, Target::HurtVillager);
	// the sixteen spells at creatures, playfully
	size_t playful = 0;
	for (const auto& executor : plan_actions::All())
	{
		if (executor.action.starts_with("CastMakeCreature"))
		{
			EXPECT_EQ(executor.build, Build::CastPlayful) << executor.action;
			EXPECT_EQ(executor.target, Target::Creature) << executor.action;
			++playful;
		}
	}
	EXPECT_EQ(playful, 16u);
	// the power-ups have none
	for (const auto* action : {"LightningBoltPU1", "LightningBoltPU2", "HealSpellPU1"})
	{
		EXPECT_EQ(plan_actions::For(action), nullptr) << action;
	}
}

TEST(CreaturePlanActionsCasting, TheFoodAndWoodRowsAndWhatTheyAreCastAt)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using plan_actions::Build;
	using plan_actions::Target;
	struct Row
	{
		const char* action;
		Target target;
		Build build;
	};
	for (const auto& row : {Row {"CastMagicFood", Target::FoodStore, Build::CastFood},
	                        Row {"CastMagicFoodByWorshipSite", Target::WorshipSite, Build::CastFood},
	                        Row {"CastMagicFoodPU1", Target::Anything, Build::CastHelpful},
	                        Row {"CastMagicWood", Target::WoodStore, Build::CastHelpful},
	                        Row {"CastMagicWoodByBuildingSite", Target::BuildingSite, Build::CastHelpful},
	                        Row {"CastMagicWoodByWorkshop", Target::Workshop, Build::CastHelpful}})
	{
		const auto* executor = plan_actions::For(row.action);
		ASSERT_NE(executor, nullptr) << row.action;
		EXPECT_EQ(executor->target, row.target) << row.action;
		EXPECT_EQ(executor->build, row.build) << row.action;
		EXPECT_TRUE(plan_actions::IsCast(*executor)) << row.action;
	}
}

TEST(CreaturePlanActionsCasting, FoodIsCastAsFoodWithNoDrawWhateverTheRowsMiracle)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* food = plan_actions::For("CastMagicFood");
	ASSERT_NE(food, nullptr);
	const Random never = [](uint32_t /*range*/) -> uint32_t {
		ADD_FAILURE() << "food is cast without a draw";
		return 0;
	};
	// without what it casts it can't be carried out
	EXPECT_FALSE(plan_actions::Agenda(*food, 7u, {}, {}, never).has_value());
	// the row's miracle, here the power-up's, does not change what is cast
	const auto agenda =
	    plan_actions::Agenda(*food, 7u, {}, {}, never, plan_actions::CastInfo {.magicType = 15, .gesture = 0, .height = 15.0f});
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3u);
	EXPECT_FLOAT_EQ(agenda->front().movement.maxDistance, 2.0f);
	EXPECT_EQ(agenda->back().cast.magicType, static_cast<uint32_t>(MagicType::Food));
	EXPECT_EQ(*agenda->back().cast.object, 7u);
	EXPECT_EQ(agenda->back().sequence, k_ScatterPose);
	// at a sign that shows a need it goes near to twenty, and a gesture is drawn when the miracle has one
	const auto atSign = plan_actions::Agenda(*food, 7u, {}, {}, never,
	                                         plan_actions::CastInfo {.magicType = 14, .gesture = 2, .atNeedsSign = true});
	ASSERT_TRUE(atSign.has_value());
	ASSERT_EQ(atSign->size(), 4u);
	EXPECT_FLOAT_EQ(atSign->front().movement.maxDistance, 20.0f);
	EXPECT_EQ((*atSign)[2].kind, Step::Kind::Gesture);
	EXPECT_EQ((*atSign)[2].animation, 2u);
}

TEST(CreaturePlanActionsCasting, WoodAndThePowerUpsFoodAreCastAsHelpfulMiracles)
{
	namespace plan_actions = openblack::creature_plan_actions;
	for (const auto* name : {"CastMagicWood", "CastMagicWoodByBuildingSite", "CastMagicFoodPU1"})
	{
		const auto* executor = plan_actions::For(name);
		ASSERT_NE(executor, nullptr) << name;
		const auto agenda =
		    plan_actions::Agenda(*executor, 7u, {}, {}, Always(1), plan_actions::CastInfo {.magicType = 21, .height = 10.0f});
		ASSERT_TRUE(agenda.has_value()) << name;
		ASSERT_EQ(agenda->size(), 4u) << name;
		EXPECT_FLOAT_EQ(agenda->front().movement.maxDistance, 20.0f) << name;
		EXPECT_EQ(agenda->back().sequence, k_CastPose) << name;
		EXPECT_EQ(agenda->back().cast.magicType, 21u) << name;
	}
}

TEST(CreaturePlanActionsCasting, TheWaterRowsAndWhatTheyAreCastAt)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using plan_actions::Build;
	using plan_actions::Target;
	struct Row
	{
		const char* action;
		Target target;
		Build build;
	};
	for (const auto& row :
	     {Row {"SprinkleMagicWaterOnCrops", Target::Sprinklable, Build::CastWater},
	      Row {"WaterTree", Target::AnyTree, Build::CastWater}, Row {"WaterField", Target::DryField, Build::CastWater},
	      Row {"SprinkleMagicWaterPU1OnCrops", Target::Sprinklable, Build::CastWater},
	      Row {"WaterTreeForTown", Target::Anything, Build::CastWaterOnInstrument},
	      Row {"WaterTreePU1ForTown", Target::Anything, Build::CastWaterOnInstrument}})
	{
		const auto* executor = plan_actions::For(row.action);
		ASSERT_NE(executor, nullptr) << row.action;
		EXPECT_EQ(executor->target, row.target) << row.action;
		EXPECT_EQ(executor->build, row.build) << row.action;
		EXPECT_TRUE(plan_actions::IsCast(*executor)) << row.action;
	}
	EXPECT_EQ(plan_actions::k_TargetCount, static_cast<size_t>(Target::DryField) + 1);
}

TEST(CreaturePlanActionsCasting, WaterIsPlainWaterEvenForThePowerUpsRows)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const Random never = [](uint32_t /*range*/) -> uint32_t {
		ADD_FAILURE() << "water is cast without a draw";
		return 0;
	};
	for (const auto* name : {"SprinkleMagicWaterOnCrops", "SprinkleMagicWaterPU1OnCrops", "WaterTree", "WaterField"})
	{
		const auto* executor = plan_actions::For(name);
		ASSERT_NE(executor, nullptr) << name;
		// the power-up's row gives the power-up's miracle, and plain water is cast all the same
		const auto agenda =
		    plan_actions::Agenda(*executor, 7u, {}, {}, never, plan_actions::CastInfo {.magicType = 23, .height = 15.0f});
		ASSERT_TRUE(agenda.has_value()) << name;
		ASSERT_EQ(agenda->size(), 3u) << name;
		EXPECT_FLOAT_EQ(agenda->front().movement.maxDistance, 30.0f) << name;
		EXPECT_EQ(agenda->back().cast.magicType, static_cast<uint32_t>(MagicType::Water)) << name;
		EXPECT_EQ(*agenda->back().cast.object, 7u) << name;
		EXPECT_EQ(agenda->back().sequence, k_ScatterPose) << name;
	}
}

TEST(CreaturePlanActionsCasting, WateringATreeForATownIsCastOnTheTreeItIsDoneWith)
{
	namespace plan_actions = openblack::creature_plan_actions;
	for (const auto* name : {"WaterTreeForTown", "WaterTreePU1ForTown"})
	{
		const auto* executor = plan_actions::For(name);
		ASSERT_NE(executor, nullptr) << name;
		const plan_actions::CastInfo cast {.magicType = 23, .height = 10.0f};
		// with nothing to use it has no object to use, and can't be carried out
		EXPECT_FALSE(plan_actions::Possible(*executor, {})) << name;
		EXPECT_FALSE(plan_actions::Agenda(*executor, 7u, {}, {}, Always(1), cast).has_value()) << name;
		// with a tree to use it goes to the tree and waters it, whatever the town's member it is done to
		const plan_actions::Situation withTree {.instrument = 9u};
		EXPECT_TRUE(plan_actions::Possible(*executor, withTree)) << name;
		const auto agenda = plan_actions::Agenda(*executor, 7u, {}, withTree, Always(1), cast);
		ASSERT_TRUE(agenda.has_value()) << name;
		ASSERT_EQ(agenda->size(), 3u) << name;
		EXPECT_EQ(*agenda->front().movement.object, 9u) << name;
		EXPECT_FLOAT_EQ(agenda->front().movement.maxDistance, 20.0f) << name;
		EXPECT_EQ(*(*agenda)[1].movement.object, 9u) << name;
		EXPECT_EQ(*agenda->back().cast.object, 9u) << name;
		EXPECT_EQ(agenda->back().cast.magicType, static_cast<uint32_t>(MagicType::Water)) << name;
	}
}

TEST(CreaturePlanActionsCasting, WaterOnItselfNeedsAClearSpotAndItself)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* water = plan_actions::For("CastMagicWater");
	ASSERT_NE(water, nullptr);
	EXPECT_EQ(water->target, plan_actions::Target::None);
	EXPECT_EQ(water->build, plan_actions::Build::CastWaterOnItself);
	EXPECT_TRUE(plan_actions::IsCast(*water));
	const plan_actions::CastInfo cast {.magicType = 22, .height = 10.0f, .caster = 4u};
	// with no clear spot near it, it can't
	EXPECT_FALSE(plan_actions::Agenda(*water, std::nullopt, {}, {}, Always(1), cast).has_value());
	const plan_actions::Situation spot {.clearSpot = glm::vec2(1.0f, 2.0f)};
	// nor without knowing who it is
	EXPECT_FALSE(plan_actions::Agenda(*water, std::nullopt, {}, spot, Always(1),
	                                  plan_actions::CastInfo {.magicType = 22, .height = 10.0f})
	                 .has_value());
	const auto agenda = plan_actions::Agenda(*water, std::nullopt, {}, spot, Always(1), cast);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 2u);
	EXPECT_EQ(agenda->front().movement.point, glm::vec2(1.0f, 2.0f));
	EXPECT_FLOAT_EQ(agenda->front().movement.maxDistance, 20.0f);
	EXPECT_EQ(*agenda->back().cast.object, 4u);
	EXPECT_EQ(agenda->back().cast.magicType, static_cast<uint32_t>(MagicType::Water));
}

TEST(CreaturePlanActionsCasting, ACastIsTriedOnlyWhenItsMiracleMayBeAndWaterOnItselfOnlyOnFire)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* scratch = plan_actions::For("Scratch");
	const auto* lightning = plan_actions::For("CastLightningBolt");
	const auto* water = plan_actions::For("CastMagicWater");
	ASSERT_NE(scratch, nullptr);
	ASSERT_NE(lightning, nullptr);
	ASSERT_NE(water, nullptr);
	// an action that casts nothing is not tested
	EXPECT_TRUE(plan_actions::MayTry(*scratch, false, false));
	// a cast needs its miracle, whether or not the creature burns
	EXPECT_TRUE(plan_actions::MayTry(*lightning, true, false));
	EXPECT_FALSE(plan_actions::MayTry(*lightning, false, true));
	// water on itself needs the creature on fire as well
	EXPECT_FALSE(plan_actions::MayTry(*water, true, false));
	EXPECT_FALSE(plan_actions::MayTry(*water, false, true));
	EXPECT_TRUE(plan_actions::MayTry(*water, true, true));
}
