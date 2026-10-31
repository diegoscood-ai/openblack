/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureRestAndHome.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;
namespace actions = openblack::creature_plan_actions;

namespace
{
constexpr float k_Turn = 0.1f;

Random Draws(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0 : value % range; };
}

/// A body that plays what the mind tells it: a sequence loops until told to end, then ends at once
struct RestingBody
{
	bool looping {false};
	bool better {false};

	[[nodiscard]] Senses SensesNow() const
	{
		return {.seconds = k_Turn, .bodyBusy = looping, .bodyLooping = looping, .better = better};
	}

	void Obey(const Commands& commands)
	{
		if (commands.endSit)
		{
			looping = false;
		}
		if (commands.startSequence.has_value())
		{
			looping = true;
		}
	}
};

actions::Situation AtHome(float homeDistance)
{
	actions::Situation situation;
	situation.home = glm::vec2(100.0f, 0.0f);
	situation.homeDistance = homeDistance;
	situation.height = 10.0f;
	situation.self = 7;
	return situation;
}

const actions::Executor& ExecutorFor(std::string_view action)
{
	const auto* executor = actions::For(action);
	EXPECT_NE(executor, nullptr) << action;
	return *executor;
}
} // namespace

TEST(CreatureRestAndHome, AnotherCreatureNearerThanEightHeightsIsFrightening)
{
	const std::array<float, 2> far {80.0f, 200.0f};
	const std::array<float, 2> near {79.9f, 200.0f};
	EXPECT_TRUE(NothingFrighteningNear(10.0f, far));
	EXPECT_FALSE(NothingFrighteningNear(10.0f, near));
	EXPECT_TRUE(NothingFrighteningNear(10.0f, {}));
}

TEST(CreatureRestAndHome, HomeDistancesDecideGoingHomeAndPraying)
{
	EXPECT_FALSE(FarFromHome(20.0f));
	EXPECT_TRUE(FarFromHome(20.1f));
	EXPECT_TRUE(NotTooFarFromHome(true, 999.0f));
	EXPECT_FALSE(NotTooFarFromHome(true, 1000.0f));
	EXPECT_FALSE(NotTooFarFromHome(false, 10.0f));
}

TEST(CreatureRestAndHome, ItSleepsOnTheSpotOnlyOnALeashWithNoTempleOrAwayFromHome)
{
	// With a temple, off the leash: only further than 140 from home, and 140 itself is not further
	EXPECT_FALSE(MaySleepOnTheSpot(false, true, 10.0f));
	EXPECT_FALSE(MaySleepOnTheSpot(false, true, 140.0f));
	EXPECT_TRUE(MaySleepOnTheSpot(false, true, 140.5f));
	// A leash held or tied, no temple of its player, or no home at all lets it sleep anywhere
	EXPECT_TRUE(MaySleepOnTheSpot(true, true, 10.0f));
	EXPECT_TRUE(MaySleepOnTheSpot(false, false, 0.0f));
	EXPECT_TRUE(MaySleepOnTheSpot(false, true, std::nullopt));
}

TEST(CreatureRestAndHome, ItIsBetterOnlyAtFullLifeAndNoTiredness)
{
	EXPECT_TRUE(IsBetter(1.0f, 0.0f));
	EXPECT_FALSE(IsBetter(0.999f, 0.0f));
	EXPECT_FALSE(IsBetter(1.0f, 0.001f));
}

TEST(CreatureRestAndHome, RestingToGetBetterFacesDownTheSlopeRestsAndGetsUpDazed)
{
	const auto agenda = RestToGetBetter();
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Static);
	EXPECT_EQ(agenda[1].sequence,
	          (std::array<size_t, 3> {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}));
	EXPECT_TRUE(agenda[1].untilBetter);
	EXPECT_TRUE(agenda[1].mirrorAtRandom);
	EXPECT_EQ(agenda[1].effect, Effect::Completed);
	EXPECT_EQ(agenda[2].animation, animations::k_Confused);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Grimace);
}

TEST(CreatureRestAndHome, GoingHomeToRecoverWalksHomeWithinItsHeightButNoMoreThanFive)
{
	const auto agenda = GoHomeToRecover({3.0f, 4.0f}, 12.0f);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(3.0f, 4.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 5.0f);
	EXPECT_EQ(GoHomeToRecover({}, 2.0f)[0].movement.maxDistance, 2.0f);
	EXPECT_TRUE(agenda[2].untilBetter);
}

TEST(CreatureRestAndHome, TheRestEndsOnlyWhenBetterAndAfterThirtyTurns)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, RestToGetBetter());
	RestingBody body;
	// The turn to face down the slope, then the turn that the rest starts
	auto commands = Think(mind, body.SensesNow(), Draws(1));
	body.Obey(commands);
	int startTurn = -1;
	for (int turn = 0; turn < 5 && startTurn < 0; ++turn)
	{
		commands = Think(mind, body.SensesNow(), Draws(1));
		body.Obey(commands);
		if (commands.startSequence.has_value())
		{
			startTurn = turn;
			// Its eyes are the sleep's body's to close
			EXPECT_EQ(commands.eyes, Eyes::Unchanged);
			EXPECT_TRUE(commands.mirrored);
		}
	}
	ASSERT_GE(startTurn, 0);
	EXPECT_TRUE(IsResting(mind));
	EXPECT_FALSE(IsAsleep(mind));
	// Better at once, it still rests to its thirty-first turn
	body.better = true;
	int endTurn = 0;
	for (int turn = 2; turn < 100; ++turn)
	{
		commands = Think(mind, body.SensesNow(), Draws(1));
		EXPECT_TRUE(commands.coolDown);
		if (commands.endSit)
		{
			endTurn = turn;
			// The action counts as done only once it is up
			EXPECT_EQ(commands.effect, Effect::None);
			body.Obey(commands);
			break;
		}
		body.Obey(commands);
	}
	EXPECT_EQ(endTurn, 31);
	EXPECT_FALSE(IsResting(mind));
	commands = Think(mind, body.SensesNow(), Draws(1));
	EXPECT_EQ(commands.effect, Effect::Completed);
	EXPECT_FALSE(commands.coolDown);
}

TEST(CreatureRestAndHome, ANotBetterCreatureRestsOn)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {RestStep()});
	RestingBody body;
	auto commands = Think(mind, body.SensesNow(), Draws(0));
	body.Obey(commands);
	for (int turn = 0; turn < 1000; ++turn)
	{
		commands = Think(mind, body.SensesNow(), Draws(0));
		EXPECT_FALSE(commands.endSit);
		body.Obey(commands);
	}
	EXPECT_TRUE(IsResting(mind));
}

TEST(CreatureRestAndHome, HealingItselfCastsTheHealAtItselfThenHeals)
{
	const auto agenda = HealItself(static_cast<uint32_t>(MagicType::Heal), 7);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[0].sequence, k_CastPose);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 3.0f);
	EXPECT_EQ(agenda[0].cast.magicType, static_cast<uint32_t>(MagicType::Heal));
	EXPECT_EQ(agenda[0].cast.object, 7u);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Grimace);
	EXPECT_EQ(agenda[1].kind, Step::Kind::HealItself);

	IdleMind mind;
	Plan(mind, Activity::Planned, {agenda[1]});
	const auto commands = Think(mind, {.seconds = k_Turn}, Draws(0));
	EXPECT_TRUE(commands.healItself);
}

TEST(CreatureRestAndHome, RestingOnTheSpotWaitsExhaustedLookingAtItsFeet)
{
	const auto agenda = RestOnTheSpot(7, 5.5f);
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 5.5f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Exhausted);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[0].gaze->object, 7u);
	EXPECT_TRUE(agenda[0].gaze->bottom);
}

TEST(CreatureRestAndHome, RunningHomeRunsToTwoHeightsAfterAFrightHalfTheTime)
{
	const auto frightened = RunHome({5.0f, 6.0f}, 10.0f, true);
	ASSERT_EQ(frightened.size(), 2u);
	EXPECT_EQ(frightened[0].animation, animations::k_Frightened);
	EXPECT_EQ(frightened[0].heldEyes, Eyes::Wide);
	EXPECT_TRUE(frightened[1].movement.run);
	EXPECT_FLOAT_EQ(frightened[1].movement.maxDistance, 20.0f);
	EXPECT_EQ(RunHome({}, 10.0f, false).size(), 1u);
}

TEST(CreatureRestAndHome, GoingIntoTheTempleStopsShortOfTheHeart)
{
	// 100 away on the ground, heart radius 10: it stops 1.3 x 3.3 short
	const auto stop = TempleStop({0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, 10.0f);
	EXPECT_NEAR(stop.x, 100.0f - (1.3f * 3.3f), 1e-4f);
	EXPECT_NEAR(stop.y, 0.0f, 1e-6f);
	const auto agenda = EnterTemple({50.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, stop, 10.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 10.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Smile);
	EXPECT_EQ(agenda[2].movement.point, stop);
}

TEST(CreatureRestAndHome, PrayingCountsAsDoneOnArrivalThenPrays)
{
	const auto agenda = PrayAtTemple({1.0f, 2.0f}, 8.0f);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].effect, Effect::Completed);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 8.0f);
	EXPECT_EQ(agenda[1].animation, k_PrayAnimation);
}

TEST(CreatureRestAndHome, LookingAtTheTemplePointsOneTimeInThree)
{
	const auto pointing = LookAtTemple(4, {30.0f, 40.0f}, true);
	ASSERT_EQ(pointing.size(), 5u);
	EXPECT_EQ(pointing[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(pointing[0].seconds, 2.0f);
	EXPECT_FLOAT_EQ(pointing[1].seconds, 3.0f);
	EXPECT_EQ(pointing[2].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(pointing[2].order.point, glm::vec2(30.0f, 40.0f));
	EXPECT_TRUE(pointing[3].gaze->bottom);
	EXPECT_FLOAT_EQ(pointing[4].seconds, 2.0f);
	EXPECT_EQ(LookAtTemple(4, {30.0f, 40.0f}, false).size(), 4u);
}

TEST(CreatureRestAndHome, ThePlannerHasTheVanillaPreconditions)
{
	// Go home only when more than 20 away
	EXPECT_FALSE(actions::Possible(ExecutorFor("GoHome"), AtHome(20.0f)));
	EXPECT_TRUE(actions::Possible(ExecutorFor("GoHome"), AtHome(21.0f)));
	// Pray at home only with a temple and within 1000
	auto withTemple = AtHome(500.0f);
	withTemple.hasTemple = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("PrayAtCitadel"), withTemple));
	EXPECT_FALSE(actions::Possible(ExecutorFor("PrayAtCitadel"), AtHome(500.0f)));
	withTemple.homeDistance = 1000.0f;
	EXPECT_FALSE(actions::Possible(ExecutorFor("PrayAtCitadel"), withTemple));
	// Sleep at home by the same test as praying: a temple, and less than 1000 from home
	auto sleepy = AtHome(999.0f);
	sleepy.hasTemple = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("SleepAtHome"), sleepy));
	EXPECT_FALSE(actions::Possible(ExecutorFor("SleepAtHome"), AtHome(10.0f)));
	sleepy.homeDistance = 1000.0f;
	EXPECT_FALSE(actions::Possible(ExecutorFor("SleepAtHome"), sleepy));
	// A sleep at home forced on it (a script's) is not asked, with no temple or far away too, but it needs its home
	auto forcedHome = AtHome(5000.0f);
	forcedHome.forced = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("SleepAtHome"), forcedHome));
	forcedHome.home.reset();
	EXPECT_FALSE(actions::Possible(ExecutorFor("SleepAtHome"), forcedHome));
	// Sleep on the spot near home only on a leash, or with no temple
	auto nearTemple = AtHome(100.0f);
	nearTemple.hasTemple = true;
	EXPECT_FALSE(actions::Possible(ExecutorFor("SleepOnTheSpot"), nearTemple));
	EXPECT_TRUE(actions::Possible(ExecutorFor("SleepOnTheSpot"), AtHome(100.0f)));
	nearTemple.leashed = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("SleepOnTheSpot"), nearTemple));
	// A sleep on the spot forced on it (a script's) is not asked, near its temple and off the leash too
	auto forced = AtHome(100.0f);
	forced.hasTemple = true;
	forced.forced = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("SleepOnTheSpot"), forced));
	// Rest only with nothing frightening near
	auto frightened = AtHome(0.0f);
	frightened.nothingFrighteningNear = false;
	EXPECT_FALSE(actions::Possible(ExecutorFor("RestToGetBetter"), frightened));
	EXPECT_FALSE(actions::Possible(ExecutorFor("RestOnTheSpot"), frightened));
	EXPECT_TRUE(actions::Possible(ExecutorFor("GoHomeToRecover"), frightened));
	// Healing itself is a cast
	EXPECT_TRUE(actions::IsCast(ExecutorFor("HealHimself")));
}

TEST(CreatureRestAndHome, AForcedPlanIsNotAskedTheTestsOfChoosingIt)
{
	// Praying at its temple far from home: refused when chosen, carried out when a script forces it
	auto farAway = AtHome(1500.0f);
	farAway.hasTemple = true;
	EXPECT_FALSE(actions::Possible(ExecutorFor("PrayAtCitadel"), farAway));
	farAway.forced = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("PrayAtCitadel"), farAway));
	// Forced with no temple, too
	auto noTemple = AtHome(500.0f);
	noTemple.forced = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("PrayAtCitadel"), noTemple));
	// A forced plan still needs what the action is made from: a home to walk to
	auto homeless = AtHome(0.0f);
	homeless.home.reset();
	homeless.homeDistance.reset();
	homeless.forced = true;
	EXPECT_FALSE(actions::Possible(ExecutorFor("PrayAtCitadel"), homeless));
	EXPECT_FALSE(actions::Possible(ExecutorFor("GoHome"), homeless));
	// Going home from near it, and resting with something frightening near
	auto near = AtHome(5.0f);
	near.nothingFrighteningNear = false;
	EXPECT_FALSE(actions::Possible(ExecutorFor("GoHome"), near));
	EXPECT_FALSE(actions::Possible(ExecutorFor("RestToGetBetter"), near));
	EXPECT_FALSE(actions::Possible(ExecutorFor("RestOnTheSpot"), near));
	near.forced = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("GoHome"), near));
	EXPECT_TRUE(actions::Possible(ExecutorFor("RestToGetBetter"), near));
	EXPECT_TRUE(actions::Possible(ExecutorFor("RestOnTheSpot"), near));
	// The tests of the player's hand and camera: a still hand near it, a camera on it with no player, no crossness
	auto watched = AtHome(0.0f);
	watched.lookedAt = true;
	for (const auto* action :
	     {"LookAtHand", "GoToHand", "GoToMiddleOfScreen", "FollowPlayer", "ShowPlayerAnObject", "BeCrossWithPlayer"})
	{
		watched.forced = false;
		EXPECT_FALSE(actions::Possible(ExecutorFor(action), watched)) << action;
		watched.forced = true;
		EXPECT_TRUE(actions::Possible(ExecutorFor(action), watched)) << action;
	}
}

TEST(CreatureRestAndHome, EnteringTheTempleFailsWithoutAHeart)
{
	const auto& enter = ExecutorFor("EnterCitadel");
	auto situation = AtHome(50.0f);
	EXPECT_FALSE(actions::Agenda(enter, std::nullopt, {}, situation, Draws(0)).has_value());
	situation.templeHeart = actions::Situation::TempleHeart {.temple = 3, .point = {100.0f, 0.0f, 0.0f}, .radius = 10.0f};
	EXPECT_TRUE(actions::Agenda(enter, std::nullopt, {}, situation, Draws(0)).has_value());
}

TEST(CreatureRestAndHome, TheSunIsLookedAtOnlyByDayAndTheMoonOnlyAtNight)
{
	auto day = AtHome(0.0f);
	auto night = AtHome(0.0f);
	night.visualNight = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("LookAtSun"), day));
	EXPECT_FALSE(actions::Possible(ExecutorFor("LookAtSun"), night));
	EXPECT_FALSE(actions::Possible(ExecutorFor("LookAtMoon"), day));
	EXPECT_TRUE(actions::Possible(ExecutorFor("LookAtMoon"), night));
	// The other looks about are not asked the time of day
	EXPECT_TRUE(actions::Possible(ExecutorFor("LookAtMountains"), night));
	// A look forced on it is not asked either
	night.forced = true;
	day.forced = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("LookAtSun"), night));
	EXPECT_TRUE(actions::Possible(ExecutorFor("LookAtMoon"), day));
}

TEST(CreatureRestAndHome, ACreatureWavesOnlyAtAnotherCreature)
{
	EXPECT_EQ(ExecutorFor("WaveAtObject").target, actions::Target::Creature);
}

TEST(CreatureRestAndHome, ACreatureIsFrightenedOnTheSpotOnlyAboutSomethingThatFrightensIt)
{
	// What a frightening thing is (and that a cow or a villager is not) is pinned by the frightening rule's own tests
	const auto& frightened = ExecutorFor("BeFrightenedOnTheSpot");
	EXPECT_EQ(frightened.target, actions::Target::Frightening);
	// Its agenda never uses the thing: a fright forced on it with none, as after a fizzled miracle, is still made
	EXPECT_TRUE(actions::Agenda(frightened, std::nullopt, {}, AtHome(0.0f), Draws(0)).has_value());
	EXPECT_TRUE(actions::Agenda(frightened, 3, {}, AtHome(0.0f), Draws(0)).has_value());
	// A wave still needs the creature it waves at
	EXPECT_FALSE(actions::Agenda(ExecutorFor("WaveAtObject"), std::nullopt, {}, AtHome(0.0f), Draws(0)).has_value());
}
