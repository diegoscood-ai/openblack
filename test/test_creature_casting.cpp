/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
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
	// the grimace draws only the variety's step, then the pose's variant
	ASSERT_GE(draws.size(), 2u);
	EXPECT_EQ(draws[draws.size() - 2], k_FaceVarietyStep);
	EXPECT_EQ(draws.back(), 2u);
	EXPECT_EQ(std::ranges::count(draws, creature_face::k_IdleFaceCount), 0);
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
	ASSERT_EQ(agenda.size(), 3u);
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
	// then its own fire stops
	EXPECT_EQ(agenda[2].kind, Step::Kind::CoolDown);
	EXPECT_FLOAT_EQ(k_WaterOnItselfClearWidth, 1.7f);
}

TEST(CreatureCastAgenda, AttacksFromFiveTimesItsHeightBackingOffToTwice)
{
	// A tornado, the lightning storm and the power-ups of the explosion
	const auto agenda = CastAt(CastStyle::Aggressive, 18, 0, 7, 10.0f, Always(0));
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].animation, 53u);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 50.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[3].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[4].cast.magicType, 18u);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
}

TEST(CreatureCastAgenda, AnExplosionFromAHundredAngryOneTimeInThreeShownAmazed)
{
	uint32_t asked = 0;
	const auto agenda = CastAt(CastStyle::Explosion, 7, 0, 7, 10.0f, [&asked](uint32_t range) {
		asked = range;
		return 0u;
	});
	EXPECT_EQ(asked, 3u);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].animation, 53u);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 100.0f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Anger);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[4].cast.magicType, 7u);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
}

TEST(CreatureCastAgenda, AFireballFromAClearAreaNearItGettingAwayOnlyFromTheLiving)
{
	// At a living thing, angry: to the clear area, the anger (with an amazed face), away from the thing, the turn and the
	// cast
	const auto living = CastFireballAt(1, 0, 7, true, {30.0f, 40.0f}, 10.0f, Always(0));
	ASSERT_EQ(living.size(), 5u);
	EXPECT_EQ(living[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(living[0].movement.point, glm::vec2(30.0f, 40.0f));
	EXPECT_FLOAT_EQ(living[0].movement.maxDistance, 20.0f);
	ASSERT_TRUE(living[0].gaze.has_value());
	EXPECT_TRUE(living[0].gaze->goingTowards);
	EXPECT_EQ(living[1].kind, Step::Kind::Action);
	EXPECT_EQ(living[1].animation, 53u);
	EXPECT_EQ(living[1].face, creature_face::Cue::Amazed);
	EXPECT_EQ(living[2].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(living[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(living[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(living[3].face, creature_face::Cue::Anger);
	EXPECT_EQ(living[4].kind, Step::Kind::Cast);
	EXPECT_EQ(living[4].cast.magicType, 1u);
	EXPECT_EQ(living[4].face, creature_face::Cue::Grimace);
	// At a thing not living, not angry: to the clear area, the turn and the cast
	const auto still = CastFireballAt(2, 0, 7, false, {30.0f, 40.0f}, 10.0f, Always(1));
	ASSERT_EQ(still.size(), 3u);
	EXPECT_EQ(still[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(still[2].cast.magicType, 2u);
}

TEST(CreatureCastAgenda, AShieldIsCastFromWhereItStandsTurningKindly)
{
	const auto agenda = CastShieldAt(19, 7);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[1].cast.magicType, 19u);
	EXPECT_EQ(agenda[1].sequence, (std::array<size_t, 3> {40, 41, 42}));
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Grimace);
}

TEST(CreatureCastAgenda, AForestIsScatteredAtThePlaceOfTheVillagerForFourSeconds)
{
	const glm::vec3 place {40.0f, 3.0f, 50.0f};
	const auto agenda = CastForestAt(13, 7, place, 15.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(*agenda[0].movement.object, 7u);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 37.5f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::None);
	// It looks where it goes as it walks towards the villager
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[0].gaze->object, std::optional<uint32_t>(7u));
	EXPECT_TRUE(agenda[0].gaze->goingTowards);
	// It turns to the villager, kind, and looks at it
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(*agenda[1].movement.object, 7u);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Compassion);
	ASSERT_TRUE(agenda[1].gaze.has_value());
	EXPECT_EQ(agenda[1].gaze->object, std::optional<uint32_t>(7u));
	// It casts at the place, not at the villager, in the scattering pose, grimacing, for four seconds
	EXPECT_EQ(agenda[2].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[2].cast.magicType, 13u);
	EXPECT_FALSE(agenda[2].cast.object.has_value());
	ASSERT_TRUE(agenda[2].cast.place.has_value());
	EXPECT_EQ(*agenda[2].cast.place, place);
	EXPECT_EQ(agenda[2].sequence, k_ScatterPose);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 4.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Grimace);
	// It draws no gesture
	EXPECT_TRUE(std::ranges::none_of(agenda, [](const Step& step) { return step.kind == Step::Kind::Gesture; }));
	// Its build casts at the place of the cast, and can't without one
	namespace plan_actions = openblack::creature_plan_actions;
	const plan_actions::Executor forest {
	    .action = "Forest", .target = plan_actions::Target::Villager, .build = plan_actions::Build::CastForest};
	EXPECT_TRUE(plan_actions::IsCast(forest));
	plan_actions::CastInfo cast {.magicType = 13, .height = 15.0f};
	EXPECT_FALSE(plan_actions::Agenda(forest, 7u, {}, {}, Always(1), cast).has_value());
	cast.place = place;
	const auto built = plan_actions::Agenda(forest, 7u, {}, {}, Always(1), cast);
	ASSERT_TRUE(built.has_value());
	ASSERT_EQ(built->size(), 3u);
	EXPECT_EQ(*built->back().cast.place, place);
	// The game's row of the forest is cast that way, at a villager
	const auto* row = plan_actions::For("CastMagicForest");
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->target, plan_actions::Target::Villager);
	EXPECT_EQ(row->build, plan_actions::Build::CastForest);
}

TEST(CreatureCastAgenda, ToImpressItPointsBeforeOrAfterAndBeckonsTwoTimesInThree)
{
	// Draws of 0: it points first and does not beckon
	const auto first = CastToImpress(10, 7, 10.0f, Always(0));
	ASSERT_EQ(first.size(), 5u);
	EXPECT_EQ(first[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(first[0].movement.maxDistance, 40.0f);
	EXPECT_EQ(first[1].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(first[1].movement.maxDistance, 20.0f);
	EXPECT_EQ(first[2].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(*first[2].order.object, 7u);
	EXPECT_FLOAT_EQ(*first[2].order.seconds, 1.0f);
	EXPECT_EQ(first[2].face, creature_face::Cue::Amazed);
	EXPECT_EQ(first[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(first[4].kind, Step::Kind::Cast);
	EXPECT_EQ(first[4].cast.magicType, 10u);
	EXPECT_EQ(first[4].face, creature_face::Cue::Grimace);
	// Draws of 1: it beckons, casts, then points
	const auto after = CastToImpress(16, 7, 10.0f, Always(1));
	ASSERT_EQ(after.size(), 6u);
	EXPECT_EQ(after[2].kind, Step::Kind::Action);
	EXPECT_EQ(after[2].animation, 52u);
	EXPECT_EQ(after[2].face, creature_face::Cue::Playfulness);
	EXPECT_EQ(after[4].cast.magicType, 16u);
	EXPECT_EQ(after[5].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(after[5].face, creature_face::Cue::Smile);
	// When it knows no miracle that impresses, it casts nothing
	const auto none = CastToImpress(std::nullopt, 7, 10.0f, Always(1));
	EXPECT_TRUE(std::ranges::none_of(none, [](const Step& step) { return step.kind == Step::Kind::Cast; }));
}

TEST(CreatureCastAgenda, ItWatchesWhatItCastsAtGoingNearBackingOffAndTurningButNotToShieldIt)
{
	// Going near, the look of the walk towards the thing; backing off and turning, its head on the thing
	const auto watched = [](const Step& step, bool going) {
		return step.gaze.has_value() && step.gaze->object == std::optional<uint32_t>(7u) && step.gaze->goingTowards == going &&
		       !step.gaze->bottom && !step.gaze->itself;
	};
	for (const auto style :
	     {CastStyle::Lightning, CastStyle::Helpful, CastStyle::Aggressive, CastStyle::Explosion, CastStyle::Playful})
	{
		const auto agenda = CastAt(style, 4, 0, 7, 10.0f, Always(1));
		ASSERT_EQ(agenda.size(), 4u);
		EXPECT_TRUE(watched(agenda[0], true));
		EXPECT_TRUE(watched(agenda[1], false));
		EXPECT_TRUE(watched(agenda[2], false));
		EXPECT_FALSE(agenda[3].gaze.has_value());
	}
	const auto fireball = CastFireballAt(1, 0, 7, true, {30.0f, 40.0f}, 10.0f, Always(1));
	ASSERT_EQ(fireball.size(), 4u);
	EXPECT_TRUE(watched(fireball[1], false));
	EXPECT_TRUE(watched(fireball[2], false));
	const auto impress = CastToImpress(10, 7, 10.0f, Always(1));
	ASSERT_EQ(impress.size(), 6u);
	EXPECT_TRUE(watched(impress[0], true));
	EXPECT_TRUE(watched(impress[1], false));
	EXPECT_TRUE(watched(impress[3], false));
	// Food and water: going near and turning, without backing off
	for (const auto& agenda : {CastFoodAt(0, 7, k_FoodGoNear), CastWaterAt(7, 10.0f)})
	{
		ASSERT_EQ(agenda.size(), 3u);
		EXPECT_TRUE(watched(agenda[0], true));
		EXPECT_TRUE(watched(agenda[1], false));
		EXPECT_FALSE(agenda[2].gaze.has_value());
	}
	// Turning to what it shields, it has no look of its own
	EXPECT_FALSE(CastShieldAt(19, 7)[0].gaze.has_value());
}

TEST(CreatureCastAgenda, ToImpressItCastsTheMiracleCastTheLongestTimeAgo)
{
	EXPECT_EQ(PickImpressiveMiracle({false, false, false, false}, {0, 0, 0, 0}), std::nullopt);
	// The last of equals
	EXPECT_EQ(PickImpressiveMiracle({true, true, true, true}, {0, 0, 0, 0}), 3u);
	EXPECT_EQ(PickImpressiveMiracle({true, true, true, false}, {50, 20, 20, 0}), 2u);
	EXPECT_EQ(PickImpressiveMiracle({true, false, true, false}, {10, 0, 20, 0}), 0u);
}

TEST(CreatureCastAgenda, ItChoosesToImpressKnowingAMiracleSeenMoreThanHalfItsTimes)
{
	const ImpressiveMiracleState known {.known = true, .canPay = true, .seen = 3.0f, .needed = 5.0f};
	EXPECT_TRUE(CanCastToImpress(std::array {known}));
	auto half = known;
	half.seen = 2.5f;
	EXPECT_FALSE(CanCastToImpress(std::array {half}));
	auto unpaid = known;
	unpaid.canPay = false;
	EXPECT_FALSE(CanCastToImpress(std::array {unpaid}));
	auto unknown = known;
	unknown.known = false;
	EXPECT_FALSE(CanCastToImpress(std::array {unknown}));
	EXPECT_TRUE(CanCastToImpress(std::array {unknown, known}));
	// None seen of none needed is no share, and fails
	auto nothing = known;
	nothing.seen = 0.0f;
	nothing.needed = 0.0f;
	EXPECT_FALSE(CanCastToImpress(std::array {nothing}));
	// Food lets it choose to impress, but it never casts food to impress
	EXPECT_EQ(k_ImpressMiraclesItMayKnow.front(), 14u);
	EXPECT_TRUE(std::ranges::find(k_ImpressiveMiracles, 14u) == k_ImpressiveMiracles.end());
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

TEST(CreatureCastMoves, AConfinedCreatureTakesNoClearAreaOutsideItsKeptArea)
{
	const glm::vec2 centre(100.0f, 100.0f);
	// Kept within 10: a middle 12 away is refused, ones 8 and 9.5 away are taken
	EXPECT_FALSE(cast_moves::WithinKeptArea(glm::vec2(112.0f, 100.0f), centre, 10.0f, true).has_value());
	EXPECT_TRUE(cast_moves::WithinKeptArea(glm::vec2(108.0f, 100.0f), centre, 10.0f, true).has_value());
	EXPECT_TRUE(cast_moves::WithinKeptArea(glm::vec2(100.0f, 109.5f), centre, 10.0f, true).has_value());
	// Not kept: any middle; none found stays none
	EXPECT_TRUE(cast_moves::WithinKeptArea(glm::vec2(112.0f, 100.0f), centre, 10.0f, false).has_value());
	EXPECT_FALSE(cast_moves::WithinKeptArea(std::nullopt, centre, 10.0f, true).has_value());
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
	// the power-ups of lightning and of the heal are cast as their miracles are
	for (const auto* action : {"CastLightningBoltPU1", "CastLightningBoltPU2"})
	{
		const auto* executor = plan_actions::For(action);
		ASSERT_NE(executor, nullptr) << action;
		EXPECT_EQ(executor->build, Build::CastLightning) << action;
		EXPECT_EQ(executor->target, Target::Anything) << action;
	}
	const auto* healPowerUp = plan_actions::For("CastHealSpellPU1");
	ASSERT_NE(healPowerUp, nullptr);
	EXPECT_EQ(healPowerUp->build, Build::CastHelpful);
	EXPECT_EQ(healPowerUp->target, Target::HurtVillager);
}

TEST(CreaturePlanActionsCasting, EachShieldIsCastAtAnythingItsOwnWay)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using plan_actions::Build;
	using plan_actions::Target;
	const auto executorOf = [](std::string_view action) {
		const auto* executor = plan_actions::For(action);
		EXPECT_NE(executor, nullptr) << action;
		return executor != nullptr ? *executor : plan_actions::Executor {};
	};
	// The physical shield and the shield's power-up are cast the way the heal is
	EXPECT_EQ(executorOf("CastPhysicalShield").build, Build::CastHelpful);
	EXPECT_EQ(executorOf("CastPhysicalShield").target, Target::Anything);
	EXPECT_EQ(executorOf("CastShieldPU1").build, Build::CastHelpful);
	EXPECT_EQ(executorOf("CastShieldPU1").target, Target::Anything);
	// The shield from where it stands
	const auto shield = executorOf("CastShield");
	EXPECT_EQ(shield.build, Build::CastShield);
	EXPECT_EQ(shield.target, Target::Anything);
	EXPECT_TRUE(plan_actions::IsCast(shield));
	const auto agenda = plan_actions::Agenda(shield, 7u, {}, plan_actions::Situation {.activity = 3u}, Always(1),
	                                         plan_actions::CastInfo {.magicType = 19, .height = 10.0f});
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 2u);
	EXPECT_EQ(agenda->front().movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda->back().cast.magicType, 19u);
	// For a plan about nothing, with nothing to shield too, it idles, as the idle action does, and casts nothing
	const auto idle = plan_actions::Agenda(shield, std::nullopt, {}, {}, Always(1),
	                                       plan_actions::CastInfo {.magicType = 19, .height = 10.0f});
	ASSERT_TRUE(idle.has_value());
	const auto beIdle = creature_mind::BeIdle(Always(1));
	ASSERT_EQ(idle->size(), beIdle.size());
	for (size_t i = 0; i < beIdle.size(); ++i)
	{
		EXPECT_EQ((*idle)[i].kind, beIdle[i].kind);
		EXPECT_EQ((*idle)[i].animation, beIdle[i].animation);
	}
	EXPECT_TRUE(std::ranges::none_of(*idle, [](const Step& step) { return step.kind == Step::Kind::Cast; }));
}

TEST(CreaturePlanActionsCasting, ATownsShieldIsCastAtABuiltStoragePitAsTheShieldIs)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* town = plan_actions::For("CastShieldAroundTown");
	ASSERT_NE(town, nullptr);
	EXPECT_EQ(town->target, plan_actions::Target::BuiltStoragePit);
	EXPECT_EQ(town->build, plan_actions::Build::CastShield);
	EXPECT_TRUE(plan_actions::IsCast(*town));
	EXPECT_FALSE(town->castMagic.has_value());
	const auto agenda = plan_actions::Agenda(*town, 7u, {}, plan_actions::Situation {.activity = 3u}, Always(1),
	                                         plan_actions::CastInfo {.magicType = 19, .height = 10.0f});
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 2u);
	EXPECT_EQ(*agenda->back().cast.object, 7u);
	EXPECT_EQ(agenda->back().cast.magicType, 19u);
}

TEST(CreaturePlanActionsCasting, TheWorkshopsRowCastsWoodThoughTheGamesTableGivesItNoMiracle)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* workshop = plan_actions::For("CastMagicWoodByWorkshop");
	ASSERT_NE(workshop, nullptr);
	EXPECT_EQ(workshop->target, plan_actions::Target::Workshop);
	EXPECT_EQ(workshop->build, plan_actions::Build::CastHelpful);
	EXPECT_EQ(workshop->castMagic, std::optional<uint32_t>(static_cast<uint32_t>(MagicType::Wood)));
	// every other row casts the miracle of its row in the game's table
	EXPECT_EQ(std::ranges::count_if(plan_actions::All(),
	                                [](const plan_actions::Executor& executor) { return executor.castMagic.has_value(); }),
	          1);
}

TEST(CreaturePlanActionsCasting, AShieldForAPlanAboutNothingIdlesInstead)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* executor = plan_actions::For("CastShield");
	ASSERT_NE(executor, nullptr);
	const plan_actions::CastInfo cast {.magicType = 19, .height = 15.0f};
	plan_actions::Situation situation;
	const auto idle = plan_actions::Agenda(*executor, 7u, {}, situation, Always(0), cast);
	ASSERT_TRUE(idle.has_value());
	EXPECT_EQ(idle->size(), BeIdle(Always(0)).size());
	EXPECT_TRUE(std::ranges::none_of(*idle, [](const Step& step) { return step.kind == Step::Kind::Cast; }));
	situation.activity = 3u;
	const auto shield = plan_actions::Agenda(*executor, 7u, {}, situation, Always(0), cast);
	ASSERT_TRUE(shield.has_value());
	ASSERT_EQ(shield->size(), 2u);
	EXPECT_EQ(shield->back().kind, Step::Kind::Cast);
	EXPECT_EQ(shield->back().cast.magicType, 19u);
}

TEST(CreaturePlanActionsCasting, ToImpressItCastsAtAVillagerAndGoesThroughTheMotionsWithNoMiracle)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* impress = plan_actions::For("CastImpressiveSpell");
	ASSERT_NE(impress, nullptr);
	EXPECT_EQ(impress->build, plan_actions::Build::CastToImpress);
	EXPECT_EQ(impress->target, plan_actions::Target::Villager);
	EXPECT_TRUE(plan_actions::IsCast(*impress));
	// With a miracle: the heal is cast, after the walk, the backing off, the point and the turn
	const auto heal =
	    plan_actions::Agenda(*impress, 7u, {}, {}, Always(0), plan_actions::CastInfo {.magicType = 10, .height = 10.0f});
	ASSERT_TRUE(heal.has_value());
	EXPECT_TRUE(std::ranges::any_of(*heal, [](const Step& step) { return step.kind == Step::Kind::Cast; }));
	// With none (0), no cast
	const auto none =
	    plan_actions::Agenda(*impress, 7u, {}, {}, Always(0), plan_actions::CastInfo {.magicType = 0, .height = 10.0f});
	ASSERT_TRUE(none.has_value());
	EXPECT_TRUE(std::ranges::none_of(*none, [](const Step& step) { return step.kind == Step::Kind::Cast; }));
}

TEST(CreaturePlanActionsCasting, EachMiracleThatAttacksIsCastItsOwnWay)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using Build = plan_actions::Build;
	const auto buildOf = [](std::string_view action) {
		const auto* executor = plan_actions::For(action);
		return executor != nullptr ? std::optional(executor->build) : std::nullopt;
	};
	EXPECT_EQ(buildOf("CastFireball"), Build::CastFireball);
	EXPECT_EQ(buildOf("CastFireballPU1"), Build::CastFireball);
	EXPECT_EQ(buildOf("CastFireballPU2"), Build::CastFireball);
	EXPECT_EQ(buildOf("CastExplosion"), Build::CastExplosion);
	EXPECT_EQ(buildOf("CastExplosionPU1"), Build::CastAggressive);
	EXPECT_EQ(buildOf("CastExplosionPU2"), Build::CastAggressive);
	EXPECT_EQ(buildOf("CastTornado"), Build::CastAggressive);
	EXPECT_EQ(buildOf("CastLightningStorm"), Build::CastAggressive);
	// With no clear area near it, the creature can't cast a fireball
	const auto* fireball = plan_actions::For("CastFireball");
	ASSERT_NE(fireball, nullptr);
	EXPECT_TRUE(plan_actions::IsCast(*fireball));
	const plan_actions::CastInfo noRoom {.magicType = 1, .height = 10.0f};
	EXPECT_FALSE(plan_actions::Agenda(*fireball, 7u, {}, {}, Always(1), noRoom).has_value());
	auto room = noRoom;
	room.clearArea = glm::vec2(5.0f, 5.0f);
	const auto agenda = plan_actions::Agenda(*fireball, 7u, {}, {}, Always(1), room);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->front().movement.kind, Movement::Kind::ToPoint);
	// At a living thing it gets away from it first
	room.targetLiving = true;
	const auto living = plan_actions::Agenda(*fireball, 7u, {}, {}, Always(1), room);
	ASSERT_TRUE(living.has_value());
	EXPECT_EQ(living->size(), agenda->size() + 1);
	// The explosion and the miracles that attack cast at the thing in their own styles
	const auto* explosion = plan_actions::For("CastExplosion");
	ASSERT_NE(explosion, nullptr);
	const auto blast = plan_actions::Agenda(*explosion, 7u, {}, {}, Always(1), noRoom);
	ASSERT_TRUE(blast.has_value());
	EXPECT_FLOAT_EQ(blast->front().movement.maxDistance, 100.0f);
	const auto* tornado = plan_actions::For("CastTornado");
	ASSERT_NE(tornado, nullptr);
	const auto storm = plan_actions::Agenda(*tornado, 7u, {}, {}, Always(1), noRoom);
	ASSERT_TRUE(storm.has_value());
	EXPECT_FLOAT_EQ(storm->front().movement.maxDistance, 50.0f);
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
	EXPECT_EQ(plan_actions::k_TargetCount, static_cast<size_t>(Target::Burning) + 1);
}

TEST(CreaturePlanActionsCasting, AFireIsPutOutWithWaterScatteredOnTheThingThatBurns)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* put = plan_actions::ForcedFor("PutOutFireWithMagicWater");
	ASSERT_NE(put, nullptr);
	EXPECT_EQ(put, plan_actions::For("PutOutFireWithMagicWater"));
	EXPECT_EQ(put->target, plan_actions::Target::Burning);
	EXPECT_EQ(put->build, plan_actions::Build::CastWater);
	EXPECT_TRUE(plan_actions::IsCast(*put));
	// it needs its miracle, whether or not the creature itself burns
	EXPECT_TRUE(plan_actions::MayTry(*put, true, false));
	EXPECT_FALSE(plan_actions::MayTry(*put, false, true));
	// the agenda is the water cast at the burning thing, as for a tree or a field
	const auto agenda =
	    plan_actions::Agenda(*put, 7u, {}, {}, Always(0), plan_actions::CastInfo {.magicType = 22, .height = 15.0f});
	ASSERT_TRUE(agenda.has_value());
	const auto expected = CastWaterAt(7, 15.0f);
	ASSERT_EQ(agenda->size(), expected.size());
	EXPECT_EQ(agenda->back().cast.magicType, static_cast<uint32_t>(MagicType::Water));
	EXPECT_EQ(*agenda->back().cast.object, 7u);
	EXPECT_FLOAT_EQ(agenda->front().movement.maxDistance, expected.front().movement.maxDistance);
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
	ASSERT_EQ(agenda->size(), 3u);
	EXPECT_EQ(agenda->front().movement.point, glm::vec2(1.0f, 2.0f));
	EXPECT_FLOAT_EQ(agenda->front().movement.maxDistance, 20.0f);
	EXPECT_EQ(*agenda->at(1).cast.object, 4u);
	EXPECT_EQ(agenda->at(1).cast.magicType, static_cast<uint32_t>(MagicType::Water));
	EXPECT_EQ(agenda->back().kind, Step::Kind::CoolDown);
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

TEST(CreatureCastAgenda, ToAmuseItPicksASpellAtRandomAndCastsFromFourTimesItsHeight)
{
	const std::array spells {AmusingSpell {.magicType = 26}, AmusingSpell {.magicType = 31, .gesture = 5}};
	// The first number picks the spell. The second number decides whether it shows how it feels first.
	std::array<uint32_t, 2> draws {1, 0};
	size_t next = 0;
	const Random random = [&](uint32_t range) { return std::min(draws.at(next++), range - 1); };
	const auto agenda = CastAmusingAt(spells, 7, 10.0f, random);
	ASSERT_EQ(agenda.size(), 6u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_FeelPlayful);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 40.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Gesture);
	EXPECT_EQ(agenda[4].animation, 5u);
	EXPECT_EQ(agenda[5].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[5].cast.magicType, 31u);
	EXPECT_EQ(agenda[5].cast.object, 7u);

	// Four times in five it does not show how it feels first
	const auto quiet = CastAmusingAt(spells, 7, 10.0f, Always(1));
	ASSERT_EQ(quiet.size(), 5u);
	EXPECT_EQ(quiet[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(quiet[4].cast.magicType, 31u);
}

TEST(CreatureCastAgenda, WithNoSpellToAmuseItOnlyShowsThatItFeelsPlayful)
{
	const Random never = [](uint32_t /*range*/) -> uint32_t {
		ADD_FAILURE() << "with no spell nothing is drawn";
		return 0;
	};
	const auto agenda = CastAmusingAt({}, 7, 10.0f, never);
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_FeelPlayful);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Playfulness);
}

TEST(CreatureCastAgenda, AnotherCreatureSeemsFriendlyUnlessItIsAngryOrAfraidOrTheCreatureIsAngry)
{
	using creature_desires::Desire;
	// The creature's own desire first, then the other creature's about it
	EXPECT_TRUE(SeemsFriendly(Desire::Play, Desire::Play));
	EXPECT_TRUE(SeemsFriendly(std::nullopt, std::nullopt));
	// The creature being afraid of the other does not stop it
	EXPECT_TRUE(SeemsFriendly(Desire::Fear, Desire::Play));
	EXPECT_FALSE(SeemsFriendly(Desire::Anger, Desire::Play));
	// The other being angry with it or afraid of it does
	EXPECT_FALSE(SeemsFriendly(Desire::Play, Desire::Anger));
	EXPECT_FALSE(SeemsFriendly(Desire::Play, Desire::Fear));
	EXPECT_FALSE(SeemsFriendly(std::nullopt, Desire::Fear));
}

TEST(CreatureCastAgenda, TheAmusingSpellIsACastAtACreatureThatSeemsFriendly)
{
	const auto* amuse = creature_plan_actions::For("CastAmusingSpellOnCreature");
	ASSERT_NE(amuse, nullptr);
	EXPECT_TRUE(creature_plan_actions::IsCast(*amuse));
	EXPECT_EQ(amuse->target, creature_plan_actions::Target::FriendlyCreature);
	EXPECT_EQ(amuse->build, creature_plan_actions::Build::CastAmusing);
	const creature_plan_actions::CastInfo cast {.height = 10.0f, .amusingSpells = {AmusingSpell {.magicType = 33}}};
	const auto agenda = creature_plan_actions::Agenda(*amuse, 7u, {}, {}, Always(1), cast);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->back().cast.magicType, 33u);
	// It grimaces as it casts, as every cast does
	EXPECT_EQ(agenda->back().face, creature_face::Cue::Grimace);
}
