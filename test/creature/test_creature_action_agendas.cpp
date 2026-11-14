/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace plan_actions = openblack::creature_plan_actions;
namespace animations = openblack::creature_layers::animations;

namespace
{
uint32_t AlwaysFirst(uint32_t /*range*/)
{
	return 0;
}

uint32_t NeverFirst(uint32_t range)
{
	return range == 0 ? 0 : range - 1;
}
} // namespace

// "LookAtHand": watching the player's hand, curious, its head on the hand, for a turn and a half of its own
TEST(CreatureActionAgendas, LookingAtTheHandWatchesIt)
{
	const auto agenda = LookAtHand();
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::WatchHand);
	EXPECT_EQ(agenda[0].turns, std::optional<uint32_t>(15));
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Curiosity);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->hand);
	ASSERT_NE(plan_actions::For("LookAtHand"), nullptr);
	EXPECT_EQ(plan_actions::For("LookAtHand")->build, plan_actions::Build::LookAtHand);
}

// The watch is done once its whole seconds pass those of its turns, five at most, and never while it is on its way
// somewhere
TEST(CreatureActionAgendas, WatchingTheHandLastsItsTurnsUpToFiveSeconds)
{
	// A turn and a half is one whole second: done on the twentieth turn
	EXPECT_FALSE(WatchedHandEnough(19, 15, 10));
	EXPECT_TRUE(WatchedHandEnough(20, 15, 10));
	// A minute or more is five seconds: done on the sixtieth
	EXPECT_FALSE(WatchedHandEnough(59, 900, 10));
	EXPECT_TRUE(WatchedHandEnough(60, 900, 10));

	IdleMind mind;
	Plan(mind, Activity::Planned, LookAtHand());
	Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	senses.hasPlayer = true;
	// The hand is twenty ahead of it, the way it faces
	senses.hand = glm::vec2 {0.0f, 20.0f};
	senses.heading = creature_locomotion::HeadingOf(*senses.hand - senses.position);
	uint32_t turns = 0;
	while (mind.step == 0 && turns < 100)
	{
		const auto commands = Think(mind, senses, NeverFirst);
		EXPECT_FALSE(commands.move.has_value());
		++turns;
	}
	EXPECT_EQ(turns, 20u);
	// More than a quarter turn away it asks every turn to turn to the hand, a turn started only while it is free; its end
	// asks only that it is not on its way somewhere, so turning on the spot it is done on time all the same
	Plan(mind, Activity::Planned, LookAtHand());
	senses.heading += 2.0f;
	senses.moving = true;
	senses.turning = true;
	turns = 0;
	while (mind.step == 0 && turns < 100)
	{
		const auto commands = Think(mind, senses, NeverFirst);
		ASSERT_TRUE(commands.move.has_value());
		EXPECT_EQ(commands.move->kind, Movement::Kind::TurnToFace);
		EXPECT_TRUE(commands.move->whenFree);
		++turns;
	}
	EXPECT_EQ(turns, 20u);
	// On its way somewhere, it is not done
	Plan(mind, Activity::Planned, LookAtHand());
	senses.turning = false;
	for (int i = 0; i < 30; ++i)
	{
		static_cast<void>(Think(mind, senses, NeverFirst));
	}
	EXPECT_EQ(mind.step, 0u);
	senses.moving = false;
	// Without a player there is no hand to watch
	senses.hand.reset();
	static_cast<void>(Think(mind, senses, NeverFirst));
	EXPECT_EQ(mind.step, 1u);
}

// Each turn of the watch of a creature with a player takes one draw of thirty from the game's numbers, first, even the
// turn its hand is gone; a creature with no player draws nothing
TEST(CreatureActionAgendas, WatchingTheHandDrawsOnceATurn)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, LookAtHand());
	Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	senses.hasPlayer = true;
	senses.hand = glm::vec2 {0.0f, 20.0f};
	senses.heading = creature_locomotion::HeadingOf(*senses.hand - senses.position);
	uint32_t draws = 0;
	const auto counting = [&draws](uint32_t range) {
		if (range == k_HandWatchUnusedDraw)
		{
			++draws;
		}
		return NeverFirst(range);
	};
	for (uint32_t turn = 1; turn <= 5; ++turn)
	{
		static_cast<void>(Think(mind, senses, counting));
		EXPECT_EQ(draws, turn);
	}
	senses.hand.reset();
	static_cast<void>(Think(mind, senses, counting));
	EXPECT_EQ(draws, 6u);
	EXPECT_EQ(mind.step, 1u);
	// With no player: no draw
	IdleMind alone;
	Plan(alone, Activity::Planned, LookAtHand());
	senses.hasPlayer = false;
	senses.hand = glm::vec2 {0.0f, 20.0f};
	draws = 0;
	static_cast<void>(Think(alone, senses, counting));
	EXPECT_EQ(draws, 0u);
	EXPECT_EQ(alone.step, 1u);
}

// "InspectCreature": near it at four of its own heights, a tenth of a second facing it, curious, then watching it
TEST(CreatureActionAgendas, LookingACreatureOverGoesNearItFacesItAndWatchesIt)
{
	const auto agenda = InspectCreature(7, 10.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 40.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Curiosity);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Inspect);
	EXPECT_EQ(agenda[2].object, 7u);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Curiosity);
	ASSERT_TRUE(agenda[2].gaze.has_value());
	EXPECT_EQ(agenda[2].gaze->object, 7u);
}

// The watch lasts five seconds and a draw of up to ten more, in whole turns from the turn after it starts, and then it
// has looked the creature over
TEST(CreatureActionAgendas, LookingACreatureOverLastsFiveToFifteenSeconds)
{
	for (const auto& [draw, turns] : {std::pair {0u, 50u}, std::pair {0xfffeu, 149u}, std::pair {0x8000u, 100u}})
	{
		SCOPED_TRACE(draw);
		IdleMind mind;
		Plan(mind, Activity::Planned, InspectCreature(7, 10.0f));
		mind.step = 2;
		// The faces it pulls draw too; only the time's draw is fixed
		const auto random = [draw](uint32_t range) { return range == game_random::k_FloatRandRange ? draw : 0u; };
		const Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
		const auto faces = [](uint32_t /*range*/) { return 0u; };
		auto commands = Think(mind, senses, random);
		EXPECT_EQ(mind.inspectTurns, turns);
		uint32_t watched = 0;
		while (commands.effect != Effect::Inspected && watched < 1000)
		{
			commands = Think(mind, senses, faces);
			++watched;
		}
		EXPECT_EQ(watched, turns);
		EXPECT_EQ(commands.effectObject, 7u);
		EXPECT_EQ(mind.step, 3u);
	}
}

// "LookAtFlyingObject": half the time pointing at it two seconds first, then watching it, amazed, its head on the thing
// itself
TEST(CreatureActionAgendas, LookingAtAFlyingThingPointsHalfTheTimeThenWatches)
{
	const auto pointing = LookAtFlyingObject(4, [](uint32_t range) {
		EXPECT_EQ(range, 2u);
		return 0u;
	});
	ASSERT_EQ(pointing.size(), 2u);
	EXPECT_EQ(pointing[0].kind, Step::Kind::Object);
	EXPECT_EQ(pointing[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(pointing[0].order.object, 4u);
	EXPECT_EQ(pointing[0].order.seconds, std::optional(2.0f));
	EXPECT_EQ(pointing[0].face, creature_face::Cue::Amazed);
	EXPECT_EQ(pointing[1].kind, Step::Kind::WatchFlying);
	EXPECT_EQ(pointing[1].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(pointing[1].gaze.has_value());
	EXPECT_TRUE(pointing[1].gaze->itself);
	const auto watching = LookAtFlyingObject(4, [](uint32_t /*range*/) { return 1u; });
	ASSERT_EQ(watching.size(), 1u);
	EXPECT_EQ(watching[0].kind, Step::Kind::WatchFlying);
}

// The watch goes on while the thing flies and its whole seconds come under the limit drawn each turn
TEST(CreatureActionAgendas, WatchingAFlyingThingLastsWhileItFliesUpToFourSeconds)
{
	EXPECT_TRUE(KeepsWatchingFlying(19, 10, 2.0f, true));
	EXPECT_FALSE(KeepsWatchingFlying(20, 10, 2.0f, true));
	EXPECT_TRUE(KeepsWatchingFlying(39, 10, 3.5f, true));
	EXPECT_FALSE(KeepsWatchingFlying(1, 10, 3.5f, false));
	// Ten turns a second, a limit just above the lowest and the highest draw
	for (uint32_t turns = 0; turns < 30; ++turns)
	{
		EXPECT_TRUE(KeepsWatchingFlying(turns, 10, 2.01f, true));
	}
	EXPECT_FALSE(KeepsWatchingFlying(30, 10, 2.01f, true));
	EXPECT_TRUE(KeepsWatchingFlying(39, 10, 3.99f, true));
	EXPECT_FALSE(KeepsWatchingFlying(40, 10, 3.99f, true));
	// The lowest draw every turn: it counts the turn it starts on, and stops on the twentieth
	IdleMind mind;
	Plan(mind, Activity::Planned, LookAtFlyingObject(4, [](uint32_t /*range*/) { return 1u; }));
	Senses senses {.seconds = 0.1f, .turnsPerSecond = 10, .objectInTheAir = true};
	const auto lowest = [](uint32_t /*range*/) { return 0u; };
	uint32_t turns = 0;
	while (mind.step == 0 && turns < 100)
	{
		static_cast<void>(Think(mind, senses, lowest));
		++turns;
	}
	EXPECT_EQ(turns, 20u);
	// Down on the land, it stops at once
	Plan(mind, Activity::Planned, LookAtFlyingObject(4, [](uint32_t /*range*/) { return 1u; }));
	senses.objectInTheAir = false;
	static_cast<void>(Think(mind, senses, lowest));
	EXPECT_EQ(mind.step, 1u);
}

// "ExamineByFollowing": following the villager, amazed and looking at it, then the confused action, amazed, as its main
// step (docs/wiki/creature/plans-and-needs.md#pending)
TEST(CreatureActionAgendas, ExaminingByFollowingFollowsThenLooksConfused)
{
	const auto agenda = ExamineByFollowing(7);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::FollowInteresting);
	EXPECT_EQ(agenda[0].object, std::optional<uint32_t>(7));
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[0].gaze->object, std::optional<uint32_t>(7));
	EXPECT_FALSE(agenda[0].main);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[1].animation, animations::k_Confused);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Amazed);
	EXPECT_TRUE(agenda[1].main);
	ASSERT_NE(plan_actions::For("ExamineByFollowing"), nullptr);
	EXPECT_EQ(plan_actions::For("ExamineByFollowing")->build, plan_actions::Build::ExamineByFollowing);
}

// Each turn it walks up to within 15 metres of the thing when it is farther; once the thing does nothing interesting it
// waits a second and a draw of up to three more in whole turns, the last ending the step
TEST(CreatureActionAgendas, FollowingAnInterestingThingKeepsNearItThenWaits)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, ExamineByFollowing(7));
	Senses senses {
	    .seconds = 0.1f, .turnsPerSecond = 10, .follow = Senses::FollowView {.interesting = true, .distance = 20.0f}};
	const auto far = Think(mind, senses, AlwaysFirst);
	ASSERT_TRUE(far.move.has_value());
	EXPECT_EQ(far.move->kind, Movement::Kind::ToObject);
	EXPECT_EQ(far.move->object, std::optional<uint32_t>(7));
	EXPECT_FLOAT_EQ(far.move->maxDistance, 15.0f);
	senses.follow->distance = 15.0f;
	EXPECT_FALSE(Think(mind, senses, AlwaysFirst).move.has_value());
	// The lowest draw: ten turns of waiting
	senses.follow->interesting = false;
	senses.follow->distance = 20.0f;
	EXPECT_FALSE(Think(mind, senses, AlwaysFirst).move.has_value());
	EXPECT_TRUE(mind.followEnding);
	uint32_t turns = 0;
	while (mind.step == 0 && turns < 100)
	{
		EXPECT_FALSE(Think(mind, senses, AlwaysFirst).move.has_value());
		++turns;
	}
	EXPECT_EQ(turns, 10u);
	EXPECT_FALSE(mind.gaveUp);
	// The highest draw: 39 turns
	Plan(mind, Activity::Planned, ExamineByFollowing(7));
	static_cast<void>(Think(mind, senses, NeverFirst));
	EXPECT_EQ(mind.heldTurns, 39u);
}

// Still interesting and near, it follows for 60 whole seconds, the turn it starts on counted, and stops on the 610th
// turn at ten a turn; with the thing gone the agenda is given up
TEST(CreatureActionAgendas, FollowingAnInterestingThingLastsAMinuteAtMost)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, ExamineByFollowing(7));
	Senses senses {.seconds = 0.1f, .turnsPerSecond = 10, .follow = Senses::FollowView {.interesting = true, .distance = 5.0f}};
	for (uint32_t turn = 1; turn < 610; ++turn)
	{
		static_cast<void>(Think(mind, senses, AlwaysFirst));
	}
	EXPECT_FALSE(mind.followEnding);
	static_cast<void>(Think(mind, senses, AlwaysFirst));
	EXPECT_TRUE(mind.followEnding);
	Plan(mind, Activity::Planned, ExamineByFollowing(7));
	senses.follow.reset();
	static_cast<void>(Think(mind, senses, AlwaysFirst));
	EXPECT_TRUE(mind.gaveUp);
}

// "FishAndEat": to within its reach of the shoal, looking where it goes, the food brought out of the sea and picked up,
// then eaten, the main step; chosen only with a fish farm near, and made only with somewhere to fish
// (docs/wiki/creature/plans-and-needs.md)
TEST(CreatureActionAgendas, FishingWalksToTheShoalBringsFoodOutOfTheSeaAndEatsIt)
{
	const auto agenda = FishAndEat({100.0f, 200.0f}, 15.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 15.0f);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->goingTowards);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::FishFromSea);
	EXPECT_EQ(agenda[2].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_TRUE(agenda[2].main);
	const auto* executor = plan_actions::For("FishAndEat");
	ASSERT_NE(executor, nullptr);
	EXPECT_EQ(executor->build, plan_actions::Build::FishAndEat);
	plan_actions::Situation situation;
	EXPECT_FALSE(plan_actions::Possible(*executor, situation));
	situation.fishFarmNearby = true;
	EXPECT_TRUE(plan_actions::Possible(*executor, situation));
}

// Eating from a food pile: up to it, turning to face the pile itself a tenth of a second, the portion taken, then eaten;
// no step pulls a face (docs/wiki/creature/plans-and-needs.md)
TEST(CreatureActionAgendas, EatingFromAPileTurnsToThePileAndPullsNoFace)
{
	const auto agenda = EatPortion(4, {10.0f, 20.0f}, 6.0f);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 6.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[1].movement.object, std::optional(4u));
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[2].order.kind, ObjectOrder::Kind::PickUpPortion);
	EXPECT_EQ(agenda[3].effect, Effect::Eat);
	for (const auto& step : agenda)
	{
		EXPECT_EQ(step.face, creature_face::Cue::None);
	}
}

// Pooing discreetly: it walks home to within two of its heights, looking home as it goes, then poos by itself as a poo
// by a thing; only when no further than 20 from home (docs/wiki/creature/plans-and-needs.md#a-poo-by-a-thing)
TEST(CreatureActionAgendas, PooingAtHomeWalksHomeFirstThenPoosAndOnlyNearHome)
{
	const auto agenda = PooAtHome(7, {100.0f, 200.0f}, 30.0f, AlwaysFirst);
	const auto poo = PooBy(7, AlwaysFirst);
	ASSERT_EQ(agenda.size(), poo.size() + 1);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 60.0f);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->goingTowards);
	for (size_t i = 0; i < poo.size(); ++i)
	{
		EXPECT_EQ(agenda[i + 1].kind, poo[i].kind);
		EXPECT_EQ(agenda[i + 1].animation, poo[i].animation);
		EXPECT_EQ(agenda[i + 1].effect, poo[i].effect);
	}
	const auto* atHome = plan_actions::For("PooAtHome");
	ASSERT_NE(atHome, nullptr);
	EXPECT_TRUE(plan_actions::WalksByItsHeight(atHome->build));
	plan_actions::Situation near;
	near.home = glm::vec2(0.0f);
	near.homeDistance = 20.0f;
	EXPECT_TRUE(plan_actions::Possible(*atHome, near));
	near.homeDistance = 20.5f;
	EXPECT_FALSE(plan_actions::Possible(*atHome, near));
	EXPECT_FALSE(plan_actions::Possible(*atHome, {}));
}

// "force Guide CREATURE_SLEEP Guide": the guide goes home to sleep
TEST(CreatureActionAgendas, SleepingAtHomeWalksHomeFacesDownTheSlopeAndSleepsUntilRested)
{
	const auto agenda = SleepAtHome({100.0f, 200.0f}, 30.0f, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].animation, animations::k_Tired);
	EXPECT_TRUE(agenda[0].sleepyEyes);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(100.0f, 200.0f));
	// No further than 5 from home, however tall it is
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 5.0f);
	// It looks where it is going
	ASSERT_TRUE(agenda[1].gaze.has_value());
	EXPECT_TRUE(agenda[1].gaze->goingTowards);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Static);
	EXPECT_TRUE(agenda[3].untilRested);
	EXPECT_TRUE(agenda[3].closedEyes);
	EXPECT_EQ(agenda[3].effect, Effect::Slept);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_TRUE(agenda[4].sleepyEyes);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
	EXPECT_FLOAT_EQ(SleepAtHome({}, 3.0f, NeverFirst).front().movement.maxDistance, 3.0f);
	// Without a home it can't
	const auto* sleep = plan_actions::For("SleepAtHome");
	ASSERT_NE(sleep, nullptr);
	EXPECT_FALSE(plan_actions::Possible(*sleep, {}));
}

// "force MyCreature CREATURE_WAVE_AT_OBJECT Guide"
TEST(CreatureActionAgendas, WavingGoesNearAndFacesTheOther)
{
	const auto wave = WaveAt(5, 10.0f);
	ASSERT_EQ(wave.size(), 3u);
	EXPECT_EQ(wave[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(wave[0].movement.object, std::optional<uint32_t>(5));
	EXPECT_FLOAT_EQ(wave[0].movement.maxDistance, 35.0f);
	EXPECT_EQ(wave[0].face, creature_face::Cue::Smile);
	ASSERT_TRUE(wave[0].gaze.has_value());
	EXPECT_TRUE(wave[0].gaze->goingTowards);
	EXPECT_EQ(wave[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(wave[1].seconds, 1.4f);
	EXPECT_EQ(wave[1].face, creature_face::Cue::Smile);
	ASSERT_TRUE(wave[1].gaze.has_value());
	EXPECT_EQ(wave[1].gaze->object, std::optional<uint32_t>(5));
	// The wave itself pulls no face, and watches the thing with fluttering eyelids
	EXPECT_EQ(wave[2].kind, Step::Kind::Action);
	EXPECT_EQ(wave[2].animation, animations::k_FriendlyWave);
	EXPECT_EQ(wave[2].face, creature_face::Cue::None);
	EXPECT_TRUE(wave[2].flutterEyelids);
	ASSERT_TRUE(wave[2].gaze.has_value());
	EXPECT_EQ(wave[2].gaze->object, std::optional<uint32_t>(5));
	EXPECT_FALSE(wave[2].gaze->goingTowards);
}

// "BeSad": a wait of two seconds and a float draw of up to three more, looking down, sad, then the sad clip or the
// crying one by a draw of two, the main step with no face of its own
TEST(CreatureActionAgendas, BeingSadWaitsLookingDownThenPlaysASadClip)
{
	for (const auto& [draw, clip] : {std::pair {0u, size_t {56}}, std::pair {1u, size_t {219}}})
	{
		SCOPED_TRACE(draw);
		// The float draw first, then the clip's
		std::vector<uint32_t> ranges;
		const auto random = [&ranges, draw](uint32_t range) {
			ranges.push_back(range);
			return range == game_random::k_FloatRandRange ? 0u : draw;
		};
		const auto sad = BeSad(9, random);
		ASSERT_EQ(ranges, (std::vector<uint32_t> {game_random::k_FloatRandRange, 2u}));
		ASSERT_EQ(sad.size(), 2u);
		EXPECT_EQ(sad[0].kind, Step::Kind::Wait);
		EXPECT_FLOAT_EQ(sad[0].seconds, 2.0f);
		EXPECT_EQ(sad[0].face, creature_face::Cue::Sad);
		ASSERT_TRUE(sad[0].gaze.has_value());
		EXPECT_EQ(sad[0].gaze->object, std::optional<uint32_t> {9u});
		EXPECT_TRUE(sad[0].gaze->bottom);
		EXPECT_FALSE(sad[0].main);
		EXPECT_EQ(sad[1].kind, Step::Kind::Action);
		EXPECT_EQ(sad[1].animation, clip);
		EXPECT_EQ(sad[1].face, creature_face::Cue::None);
		EXPECT_TRUE(sad[1].main);
	}
	// The longest wait: a float draw of the whole range adds three seconds, as the game's float scale gives it
	const auto longest = BeSad(9, [](uint32_t range) { return range == game_random::k_FloatRandRange ? range : 0u; });
	EXPECT_FLOAT_EQ(longest[0].seconds,
	                2.0f + (static_cast<float>(game_random::k_FloatRandRange) * 3.0f * game_random::FloatRandScale()));

	// The plan action builds it
	const auto* executor = plan_actions::For("BeSad");
	ASSERT_NE(executor, nullptr);
	plan_actions::Situation situation;
	situation.self = 9;
	const auto agenda = plan_actions::Agenda(*executor, std::nullopt, {}, situation, AlwaysFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 2u);
	EXPECT_EQ((*agenda)[1].animation, 56u);
}

TEST(CreatureActionAgendas, FrightenedPuttingDownAndLyingDead)
{
	const auto frightened = BeFrightenedOnTheSpot();
	ASSERT_EQ(frightened.size(), 3u);
	for (const size_t start : {0u, 2u})
	{
		SCOPED_TRACE(start);
		EXPECT_EQ(frightened[start].kind, Step::Kind::Action);
		EXPECT_EQ(frightened[start].animation, animations::k_Frightened);
		// Wide eyes, and no face of its own
		EXPECT_EQ(frightened[start].heldEyes, Eyes::Wide);
		EXPECT_EQ(frightened[start].face, creature_face::Cue::None);
	}
	EXPECT_EQ(frightened[1].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(frightened[1].seconds, 3.0f);
	EXPECT_EQ(frightened[1].face, creature_face::Cue::Frightened);
	EXPECT_EQ(frightened[1].heldEyes, Eyes::Unchanged);

	// The put-down sets down what it holds with the put-down animation, not a toss
	const auto* put = plan_actions::For("PutDown");
	ASSERT_NE(put, nullptr);
	const auto putDown = plan_actions::Agenda(*put, std::nullopt, {}, {}, NeverFirst);
	ASSERT_TRUE(putDown.has_value());
	ASSERT_EQ(putDown->size(), 1u);
	EXPECT_EQ((*putDown)[0].kind, Step::Kind::Object);
	EXPECT_EQ((*putDown)[0].order.kind, ObjectOrder::Kind::PutDown);

	// The faint, done as it lies, then the wait of ten days lying on
	const auto dead = DeadForever();
	ASSERT_EQ(dead.size(), 2u);
	EXPECT_EQ(dead[0].kind, Step::Kind::Static);
	EXPECT_EQ(dead[0].sequence[0], animations::k_Faint);
	EXPECT_TRUE(dead[0].holdLoop);
	EXPECT_TRUE(dead[0].closedEyes);
	EXPECT_TRUE(dead[0].faint);
	EXPECT_TRUE(dead[0].liesOn);
	EXPECT_TRUE(dead[0].main);
	EXPECT_EQ(dead[0].effect, Effect::None);
	EXPECT_EQ(dead[1].kind, Step::Kind::Wait);
	EXPECT_TRUE(dead[1].staysDown);
	EXPECT_FALSE(dead[1].main);
	EXPECT_FLOAT_EQ(dead[1].seconds, 864000.0f);
	EXPECT_FALSE(dead[1].gaze.has_value());
}

// A script forces CREATURE_DEAD_FOREVER: it faints, and once it lies the faint is done and it waits on, lying,
// unconscious and looking about at nothing, never got up
TEST(CreatureActionAgendas, LyingDeadForGoodFaintsAndStaysDown)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, DeadForever());
	const auto body = [](bool busy, bool lying) { return Senses {.seconds = 0.1f, .bodyBusy = busy, .bodyLooping = lying}; };
	auto commands = Think(mind, body(false, false), NeverFirst);
	ASSERT_TRUE(commands.startSequence.has_value());
	EXPECT_EQ((*commands.startSequence)[0], animations::k_Faint);
	// Those near it react as to a death, and it is out cold
	EXPECT_TRUE(commands.fainted);
	EXPECT_TRUE(mind.outCold);
	for (int turn = 0; turn < 4; ++turn)
	{
		commands = Think(mind, body(true, false), NeverFirst);
		EXPECT_FALSE(commands.endSit);
	}
	// The first turn it lies ends the faint
	commands = Think(mind, body(true, true), NeverFirst);
	EXPECT_FALSE(commands.endSit);
	EXPECT_EQ(mind.step, 1u);
	// Lying, it waits on, down and unconscious, turn after turn
	for (int turn = 0; turn < 600; ++turn)
	{
		commands = Think(mind, body(true, true), NeverFirst);
		EXPECT_FALSE(commands.endSit);
		EXPECT_FALSE(commands.lookAbout);
		EXPECT_EQ(commands.eyes, Eyes::Unchanged);
	}
	EXPECT_TRUE(IsUnconscious(mind));
	EXPECT_TRUE(mind.outCold);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_TRUE(mind.stepStarted);
	EXPECT_FALSE(ComesTo(mind));
}

// A script forces CREATURE_DIE_PERMANENTLY (Land 2's death ray on Khazar's creature, Land 5's opening): the faint and
// the wait of lying dead for good, the wait 999999 seconds instead of ten days
TEST(CreatureActionAgendas, DyingForGoodIsLyingDeadForGoodForLonger)
{
	const auto* row = plan_actions::For("DiePermanently");
	ASSERT_NE(row, nullptr);
	const auto dying = plan_actions::Agenda(*row, std::nullopt, {}, {}, NeverFirst);
	ASSERT_TRUE(dying.has_value());
	const auto dead = DeadForever();
	ASSERT_EQ(dying->size(), 2u);
	ASSERT_EQ(dead.size(), 2u);
	const auto& faint = (*dying)[0];
	EXPECT_EQ(faint.kind, dead[0].kind);
	EXPECT_EQ(faint.sequence, dead[0].sequence);
	EXPECT_TRUE(faint.holdLoop);
	EXPECT_TRUE(faint.closedEyes);
	EXPECT_TRUE(faint.faint);
	EXPECT_TRUE(faint.liesOn);
	EXPECT_TRUE(faint.main);
	EXPECT_EQ(faint.effect, Effect::None);
	const auto& wait = (*dying)[1];
	EXPECT_EQ(wait.kind, Step::Kind::Wait);
	EXPECT_TRUE(wait.staysDown);
	EXPECT_FLOAT_EQ(wait.seconds, 999999.0f);
}

// A script forces CREATURE_DIE on the creature itself after a fight it lost (Land4Ogre, TheBigFightMain). Only forcing
// reaches the row: the planner never weighs it. The faint, the lie, the carry home and the rest are the knock-out's;
// the agenda's one step, the main one, waits for the body to be free again
TEST(CreatureActionAgendas, DyingIsOnlyForcedAndWaitsOutTheKnockOut)
{
	EXPECT_EQ(plan_actions::For("Die"), nullptr);
	const auto* row = plan_actions::ForcedFor("Die");
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->build, plan_actions::Build::Die);
	EXPECT_EQ(row->target, plan_actions::Target::None);
	const auto dying = plan_actions::Agenda(*row, std::nullopt, {}, {}, NeverFirst);
	ASSERT_TRUE(dying.has_value());
	ASSERT_EQ(dying->size(), 1u);
	EXPECT_EQ((*dying)[0].kind, Step::Kind::WaitUntilFree);
	EXPECT_TRUE((*dying)[0].main);
	EXPECT_EQ((*dying)[0].effect, Effect::None);
}

// The actions Land 1's lesson scripts force have a row, so a forced plan of them has an agenda
TEST(CreatureActionAgendas, TheForcedActionsOfLandOneHaveTheirRows)
{
	for (const auto* name : {"SleepAtHome", "SmileAtFriend", "PutDown", "WaveAtObject", "BeFrightenedOnTheSpot", "DeadForever"})
	{
		SCOPED_TRACE(name);
		const auto* executor = plan_actions::For(name);
		ASSERT_NE(executor, nullptr);
	}
	const auto* put = plan_actions::For("PutDown");
	ASSERT_NE(put, nullptr);
	EXPECT_TRUE(plan_actions::Agenda(*put, std::nullopt, {}, {}, NeverFirst).has_value());
	const auto* wave = plan_actions::For("WaveAtObject");
	ASSERT_NE(wave, nullptr);
	EXPECT_FALSE(plan_actions::Agenda(*wave, std::nullopt, {}, {}, NeverFirst).has_value());
	EXPECT_TRUE(plan_actions::Agenda(*wave, 3, {}, {}, NeverFirst).has_value());
}

// "HoldObject": with an empty hand it picks the thing up, turns to the camera, plays with it the first two times, then
// watches the camera 40 seconds and up to 10 more; the play's draw comes before the watch's
TEST(CreatureActionAgendas, HoldingAThingUpPicksItUpTurnsToTheCameraPlaysAndWatches)
{
	std::vector<uint32_t> ranges;
	const Random record = [&ranges](uint32_t range) {
		ranges.push_back(range);
		return range == 4 ? 2u : 0xFFFFu / 2u;
	};
	const auto agenda =
	    HoldObject({.thing = 7, .camera = glm::vec2(5.0f, 6.0f), .playsWithIt = true, .turnsPerSecond = 10.0f}, record);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[0].order.object, std::optional<uint32_t>(7));
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->goingTowards);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::None);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(5.0f, 6.0f));
	EXPECT_EQ(agenda[1].face, creature_face::Cue::AttitudeToPlayer);
	EXPECT_TRUE(agenda[1].main);
	// The play: stroke, shake, smell or examine, the first of the keep animations and the draw of four
	EXPECT_EQ(agenda[2].order.kind, ObjectOrder::Kind::Keep);
	EXPECT_EQ(agenda[2].order.animation, 102u);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Smile);
	EXPECT_FALSE(agenda[2].gaze.has_value());
	// The watch: 40 seconds and the float draw of up to 10, cut to whole turns
	EXPECT_EQ(agenda[3].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[3].seconds, 44.9f);
	EXPECT_EQ(agenda[3].face, creature_face::Cue::AttitudeToPlayer);
	ASSERT_TRUE(agenda[3].gaze.has_value());
	EXPECT_TRUE(agenda[3].gaze->camera);
	EXPECT_EQ(ranges, (std::vector<uint32_t> {4u, game_random::k_FloatRandRange}));
	// Holding it already, or the third time, neither pick-up nor play, and only the watch's draw
	ranges.clear();
	const auto held = HoldObject({.thing = 7, .handFull = true, .camera = glm::vec2(5.0f, 6.0f)}, record);
	ASSERT_EQ(held.size(), 2u);
	EXPECT_EQ(held[0].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(held[1].kind, Step::Kind::Wait);
	EXPECT_EQ(ranges, (std::vector<uint32_t> {game_random::k_FloatRandRange}));
	// The row holds up what it can pick up
	const auto* row = plan_actions::For("HoldObject");
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->target, plan_actions::Target::Pickable);
	EXPECT_FALSE(plan_actions::Agenda(*row, std::nullopt, {}, {}, NeverFirst).has_value());
}

// "SleepByObject": in a clear place by the thing it walks there first and only then draws the yawn; with no clear place
// it sleeps at home, the yawn drawn first
TEST(CreatureActionAgendas, SleepingByAThingWalksToTheClearPlaceThenYawns)
{
	const auto agenda = SleepByObject(glm::vec2(10.0f, 20.0f), 3.0f, glm::vec2(40.0f, 50.0f), 30.0f, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(10.0f, 20.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 3.0f);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->goingTowards);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::None);
	EXPECT_EQ(agenda[1].animation, animations::k_Tired);
	EXPECT_TRUE(agenda[1].sleepyEyes);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::None);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_TRUE(agenda[3].untilRested);
	EXPECT_TRUE(agenda[3].closedEyes);
	EXPECT_EQ(agenda[3].effect, Effect::Slept);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
	// No yawn the other half of the time
	EXPECT_EQ(SleepByObject(glm::vec2(10.0f, 20.0f), 3.0f, std::nullopt, 30.0f, NeverFirst).size(), 4u);
	// No clear place: home within its height, at most 5, down the slope
	const auto home = SleepByObject(std::nullopt, 3.0f, glm::vec2(40.0f, 50.0f), 30.0f, NeverFirst);
	ASSERT_EQ(home.size(), 4u);
	EXPECT_EQ(home[0].movement.point, glm::vec2(40.0f, 50.0f));
	EXPECT_FLOAT_EQ(home[0].movement.maxDistance, 5.0f);
	EXPECT_EQ(home[1].movement.kind, Movement::Kind::FaceDownSlope);
}

// The planner sleeps by a thing only as it sleeps on the spot, and with nothing frightening near; forced, it is not asked
TEST(CreatureActionAgendas, SleepingByAThingHasTheRowsTests)
{
	const auto* row = plan_actions::For("SleepByObject");
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->target, plan_actions::Target::Anything);
	plan_actions::Situation away;
	away.homeDistance = 141.0f;
	away.hasTemple = true;
	away.nothingFrighteningNear = true;
	EXPECT_TRUE(plan_actions::Possible(*row, away));
	away.nothingFrighteningNear = false;
	EXPECT_FALSE(plan_actions::Possible(*row, away));
	plan_actions::Situation near = away;
	near.nothingFrighteningNear = true;
	near.homeDistance = 100.0f;
	EXPECT_FALSE(plan_actions::Possible(*row, near));
	near.forced = true;
	near.nothingFrighteningNear = false;
	EXPECT_TRUE(plan_actions::Possible(*row, near));
}

TEST(CreatureActionAgendas, SillyFacesAtAVillagerGoUpToItFaceItAndPullThreeFacesAfterAPlayfulOneTwoTimesInThree)
{
	using creature_mind::Step;
	std::vector<uint32_t> bounds;
	const creature_mind::Random random = [&bounds](uint32_t bound) {
		bounds.push_back(bound);
		return bound == 3 ? 1u : 4u;
	};
	const auto agenda = creature_mind::PullSillyFaces(7, 10.0f, random);
	// the draw of three first, then one of ten for each face
	EXPECT_EQ(bounds, (std::vector<uint32_t> {3, 10, 10, 10}));
	ASSERT_EQ(agenda.size(), 6u);
	EXPECT_EQ(agenda[0].movement.kind, creature_mind::Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[1].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[2].animation, 67u);
	for (size_t face = 3; face < 6; ++face)
	{
		EXPECT_EQ(agenda[face].kind, Step::Kind::Face);
		EXPECT_EQ(agenda[face].animation, 16u + 4u);
	}
	// a draw of 0 of three plays no playful clip
	EXPECT_EQ(creature_mind::PullSillyFaces(7, 10.0f, [](uint32_t) { return 0u; }).size(), 5u);
}

TEST(CreatureActionAgendas, AnImpressivePoseGoesWithinThreeHeightsFacesTheVillagerAndPlaysTheImpressThenOneOfTwo)
{
	const auto one = creature_mind::ShowImpressiveAnimation(7, 10.0f, [](uint32_t) { return 0u; });
	ASSERT_EQ(one.size(), 4u);
	EXPECT_FLOAT_EQ(one[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(one[2].animation, 69u);
	EXPECT_EQ(one[3].animation, 65u);
	// the main step is the last pose's
	EXPECT_FALSE(one[2].main);
	EXPECT_TRUE(one[3].main);
	EXPECT_EQ(creature_mind::ShowImpressiveAnimation(7, 10.0f, [](uint32_t) { return 1u; })[3].animation, 52u);
}

// The impressive pose is chosen again only more than 200 game turns after it was last carried out, or when it never
// was; a plan forced on it is not asked
TEST(CreatureActionAgendas, AnImpressivePoseIsNotChosenAgainWithin200Turns)
{
	EXPECT_TRUE(plan_actions::HasntShownImpressivePoseRecently(50, 0));
	EXPECT_FALSE(plan_actions::HasntShownImpressivePoseRecently(1200, 1000));
	EXPECT_TRUE(plan_actions::HasntShownImpressivePoseRecently(1201, 1000));
	// counted on whole numbers that may wrap: a turn before the last one is not long after it
	EXPECT_FALSE(plan_actions::HasntShownImpressivePoseRecently(900, 1000));
	EXPECT_TRUE(plan_actions::HasntShownImpressivePoseRecently(100, 0xFFFFFF00u));

	const auto* pose = plan_actions::For("ShowImpressiveAnimation");
	ASSERT_NE(pose, nullptr);
	plan_actions::Situation situation;
	EXPECT_TRUE(plan_actions::Possible(*pose, situation));
	situation.impressivePoseRested = false;
	EXPECT_FALSE(plan_actions::Possible(*pose, situation));
	situation.forced = true;
	EXPECT_TRUE(plan_actions::Possible(*pose, situation));
}

TEST(CreatureActionAgendas, RunningFromThePlayerLooksFrightenedOneTimeInTwoBeforeAndAfterTheRun)
{
	using creature_mind::Step;
	// draws of 0: frightened before and after
	const auto both = creature_mind::RunFromPlayer({0.0f, -100.0f}, [](uint32_t) { return 0u; });
	ASSERT_EQ(both.size(), 3u);
	EXPECT_EQ(both[0].animation, 61u);
	EXPECT_EQ(both[1].movement.kind, creature_mind::Movement::Kind::FleeFrom);
	EXPECT_TRUE(both[1].movement.run);
	EXPECT_EQ(both[1].face, creature_face::Cue::Fear);
	EXPECT_EQ(both[2].animation, 61u);
	// draws of 1: the run alone
	const auto run = creature_mind::RunFromPlayer({0.0f, -100.0f}, [](uint32_t) { return 1u; });
	ASSERT_EQ(run.size(), 1u);
	EXPECT_EQ(run[0].movement.kind, creature_mind::Movement::Kind::FleeFrom);
}

TEST(CreatureActionAgendas, RunningFromAThingLooksFrightenedOneTimeInTwoBeforeAndAfterTheRun)
{
	// draws of 0: frightened before and after, the run between them the main step, afraid
	const auto both = creature_mind::RunFromThing({10.0f, 20.0f}, [](uint32_t) { return 0u; });
	ASSERT_EQ(both.size(), 3u);
	EXPECT_EQ(both[0].animation, 61u);
	EXPECT_EQ(both[0].heldEyes, creature_mind::Eyes::Wide);
	EXPECT_FALSE(both[0].main);
	EXPECT_EQ(both[1].movement.kind, creature_mind::Movement::Kind::FleeFrom);
	EXPECT_EQ(both[1].movement.point, glm::vec2(10.0f, 20.0f));
	EXPECT_TRUE(both[1].movement.run);
	EXPECT_TRUE(both[1].main);
	EXPECT_EQ(both[1].face, creature_face::Cue::Fear);
	EXPECT_EQ(both[2].animation, 61u);
	// a draw of 0 then 1: frightened before only
	uint32_t draws = 0;
	const auto before = creature_mind::RunFromThing({10.0f, 20.0f}, [&draws](uint32_t) { return draws++ == 0 ? 0u : 1u; });
	ASSERT_EQ(before.size(), 2u);
	EXPECT_EQ(before[0].animation, 61u);
	EXPECT_EQ(before[1].movement.kind, creature_mind::Movement::Kind::FleeFrom);
	// draws of 1: the run alone
	const auto run = creature_mind::RunFromThing({10.0f, 20.0f}, [](uint32_t) { return 1u; });
	ASSERT_EQ(run.size(), 1u);
	EXPECT_EQ(run[0].movement.kind, creature_mind::Movement::Kind::FleeFrom);
}

// A fight achieves its plan's aim only when it is the last step of the agenda: with a step after it, the end of the fight
// achieves nothing
TEST(CreatureActionAgendas, AFightAchievesItsAimOnlyAsTheLastStep)
{
	const Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	IdleMind last;
	Plan(last, Activity::Planned, {{.kind = Step::Kind::Fight, .object = 9U}});
	const auto started = Think(last, senses, NeverFirst);
	EXPECT_EQ(started.fight, std::optional(9U));
	EXPECT_FALSE(started.achievedAim);
	const auto ended = Think(last, senses, NeverFirst);
	EXPECT_TRUE(ended.achievedAim);
	EXPECT_EQ(last.step, 1U);

	IdleMind before;
	Plan(before, Activity::Planned, {{.kind = Step::Kind::Fight, .object = 9U}, {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	static_cast<void>(Think(before, senses, NeverFirst));
	const auto ends = Think(before, senses, NeverFirst);
	EXPECT_FALSE(ends.achievedAim);
	EXPECT_EQ(before.step, 1U);
}

// The mending of a building achieves its aim once the building has all of its life again
TEST(CreatureActionAgendas, MendingAchievesItsAimAtFullLife)
{
	EXPECT_FALSE(RepairedToFullLife(0.8f));
	EXPECT_FALSE(RepairedToFullLife(0.99f));
	EXPECT_TRUE(RepairedToFullLife(1.0f));
	EXPECT_TRUE(RepairedToFullLife(1.2f));
}

// A pick-up needs empty hands or the thing it picks up: holding that thing, the step is done at once; holding another,
// it puts that down first, each turn until the put-down starts, and picks up once its hands are free
TEST(CreatureActionAgendas, APickUpPutsDownAnotherThingFirstAndSkipsTheThingItHolds)
{
	constexpr uint32_t k_Target = 7;
	constexpr uint32_t k_Other = 9;
	const std::vector<Step> agenda {
	    Step {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = k_Target}}};

	IdleMind mind;
	Plan(mind, Activity::Planned, agenda);
	Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	senses.holding = true;
	senses.held = k_Target;
	const auto skipped = Think(mind, senses, NeverFirst);
	EXPECT_FALSE(skipped.object.has_value());
	EXPECT_EQ(mind.step, 1u);

	Plan(mind, Activity::Planned, agenda);
	senses.held = k_Other;
	for (int turn = 0; turn < 2; ++turn)
	{
		SCOPED_TRACE(turn);
		const auto putDown = Think(mind, senses, NeverFirst);
		ASSERT_TRUE(putDown.object.has_value());
		EXPECT_EQ(putDown.object->kind, ObjectOrder::Kind::PutDown);
		EXPECT_FALSE(mind.stepStarted);
	}
	// the put-down under way keeps the hands and the body busy: the pick-up waits
	senses.hands = HandsState::Busy;
	senses.bodyBusy = true;
	EXPECT_FALSE(Think(mind, senses, NeverFirst).object.has_value());
	EXPECT_FALSE(mind.stepStarted);
	// its hands empty, the pick-up starts
	senses.hands = HandsState::Done;
	senses.bodyBusy = false;
	senses.holding = false;
	senses.held.reset();
	const auto pickUp = Think(mind, senses, NeverFirst);
	ASSERT_TRUE(pickUp.object.has_value());
	EXPECT_EQ(pickUp.object->kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(pickUp.object->object, k_Target);
	EXPECT_TRUE(mind.stepStarted);

	// with no plan its hands do nothing: it keeps what it holds
	IdleMind idle;
	senses.holding = true;
	senses.held = k_Other;
	senses.hands = HandsState::Idle;
	EXPECT_FALSE(Think(idle, senses, NeverFirst).object.has_value());
}
