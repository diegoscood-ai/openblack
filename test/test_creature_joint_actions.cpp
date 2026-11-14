/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using creature_desires::Desire;
using creature_mind::Step;
namespace animations = creature_layers::animations;

namespace
{
constexpr uint32_t k_Asker = 7;
constexpr uint32_t k_Friend = 9;

creature_mind::Friends TwoFriends()
{
	return {.self = k_Asker,
	        .friendly = k_Friend,
	        .selfPoint = {100.0f, 200.0f},
	        .friendPoint = {140.0f, 260.0f},
	        .height = 30.0f,
	        .friendHeight = 20.0f};
}

/// A random source that always gives the same number
creature_mind::Random Always(uint32_t value)
{
	return [value](uint32_t) { return value; };
}

const Step& Ordered(const Step& step)
{
	EXPECT_EQ(step.kind, Step::Kind::Order);
	EXPECT_EQ(step.object, k_Friend);
	EXPECT_NE(step.ordered, nullptr);
	return *step.ordered;
}

/// The first steps of each joint action side by side: ask, send the friend to its place, go to its own, wait
void ExpectMeeting(const std::vector<Step>& agenda, size_t first, glm::vec2 selfPlace, glm::vec2 friendPlace)
{
	const auto& friends = TwoFriends();
	EXPECT_EQ(agenda[first].kind, Step::Kind::RequestPartner);
	EXPECT_EQ(agenda[first].request, creature_friendship::RequestKind::BeFriends);
	EXPECT_EQ(agenda[first].object, k_Friend);
	const auto& go = Ordered(agenda[first + 1]);
	EXPECT_EQ(go.movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ(go.movement.point.x, friendPlace.x);
	EXPECT_FLOAT_EQ(go.movement.point.y, friendPlace.y);
	EXPECT_FLOAT_EQ(go.movement.maxDistance, friends.friendHeight);
	EXPECT_FALSE(go.gaze.has_value());
	EXPECT_FLOAT_EQ(agenda[first + 2].movement.point.x, selfPlace.x);
	EXPECT_FLOAT_EQ(agenda[first + 2].movement.point.y, selfPlace.y);
	EXPECT_FLOAT_EQ(agenda[first + 2].movement.maxDistance, friends.height);
	EXPECT_EQ(agenda[first + 3].kind, Step::Kind::WaitForStill);
	EXPECT_EQ(agenda[first + 3].object, k_Friend);
}
} // namespace

TEST(CreatureJointActions, PlacesOfTwoFriends)
{
	const auto [self, other] = creature_mind::SideBySide(TwoFriends());
	// Halfway is (120, 230): the creature its own height along the first axis, the friend its own height back
	EXPECT_FLOAT_EQ(self.x, 150.0f);
	EXPECT_FLOAT_EQ(self.y, 230.0f);
	EXPECT_FLOAT_EQ(other.x, 100.0f);
	EXPECT_FLOAT_EQ(other.y, 230.0f);
	const auto [first, second] = creature_mind::AboutPlace({10.0f, 20.0f}, 40.0f);
	EXPECT_FLOAT_EQ(first.x, 20.0f);
	EXPECT_FLOAT_EQ(first.y, 30.0f);
	EXPECT_FLOAT_EQ(second.x, 0.0f);
	EXPECT_FLOAT_EQ(second.y, 10.0f);
	EXPECT_FLOAT_EQ(creature_mind::SitTogetherRadius(30.0f, 20.0f), 85.0f);
	EXPECT_FLOAT_EQ(creature_mind::SleepTogetherRadius(30.0f, 20.0f), 100.0f);
}

TEST(CreatureJointActions, BeingHappyWithAFriend)
{
	const auto agenda = creature_mind::BeHappyWithFriend(TwoFriends());
	ASSERT_EQ(agenda.size(), 8U);
	ExpectMeeting(agenda, 0, {150.0f, 230.0f}, {100.0f, 230.0f});
	EXPECT_EQ(Ordered(agenda[4]).movement.object, k_Asker);
	EXPECT_TRUE(agenda[5].flutterEyelids);
	EXPECT_EQ(Ordered(agenda[6]).animation, animations::k_Happy);
	EXPECT_EQ(agenda[7].animation, animations::k_Happy);
	// No step is marked as the main step, so the last one is
	EXPECT_TRUE(std::ranges::none_of(agenda, &Step::main));
}

TEST(CreatureJointActions, HavingAPooWithAFriend)
{
	const auto agenda = creature_mind::PooWithFriend(TwoFriends(), Always(0));
	ASSERT_EQ(agenda.size(), 10U);
	ExpectMeeting(agenda, 0, {150.0f, 230.0f}, {100.0f, 230.0f});
	EXPECT_EQ(Ordered(agenda[4]).sequence[1], animations::k_Poo);
	EXPECT_FLOAT_EQ(agenda[5].seconds, creature_mind::k_PooTogetherSeconds);
	EXPECT_TRUE(agenda[5].main);
	EXPECT_EQ(agenda[5].effect, creature_mind::Effect::None);
	EXPECT_EQ(Ordered(agenda[6]).effect, creature_mind::Effect::Poo);
	EXPECT_EQ(agenda[7].kind, Step::Kind::Apply);
	EXPECT_EQ(agenda[7].effect, creature_mind::Effect::Poo);
	EXPECT_FLOAT_EQ(agenda[8].seconds, creature_mind::k_FaceAfterPooSeconds);
	EXPECT_EQ(agenda[9].animation, animations::k_Happy);
}

TEST(CreatureJointActions, SittingWithAFriend)
{
	const auto agenda = creature_mind::SitWithFriend(TwoFriends(), {10.0f, 20.0f}, 40.0f, Always(0));
	ASSERT_EQ(agenda.size(), 10U);
	ExpectMeeting(agenda, 0, {20.0f, 30.0f}, {0.0f, 10.0f});
	EXPECT_TRUE(agenda[3].flutterEyelids);
	EXPECT_FALSE(agenda[5].flutterEyelids);
	EXPECT_FLOAT_EQ(Ordered(agenda[6]).seconds, creature_mind::k_SitTogetherSeconds);
	EXPECT_EQ(agenda[7].sequence[1], animations::k_Sit);
	EXPECT_TRUE(agenda[7].main);
	EXPECT_EQ(agenda[9].animation, animations::k_Happy);
}

TEST(CreatureJointActions, SleepingWithAFriendYawnsHalfTheTime)
{
	const auto yawn = creature_mind::SleepWithFriend(TwoFriends(), {10.0f, 20.0f}, 40.0f, Always(0));
	const auto noYawn = creature_mind::SleepWithFriend(TwoFriends(), {10.0f, 20.0f}, 40.0f, Always(1));
	ASSERT_EQ(yawn.size(), 10U);
	ASSERT_EQ(noYawn.size(), 9U);
	EXPECT_EQ(yawn[0].animation, animations::k_Tired);
	EXPECT_TRUE(yawn[0].sleepyEyes);
	ExpectMeeting(noYawn, 0, {20.0f, 30.0f}, {0.0f, 10.0f});
	EXPECT_EQ(Ordered(noYawn[6]).sequence[1], animations::k_Sleep);
	EXPECT_FLOAT_EQ(noYawn[7].seconds, creature_mind::k_SleepTogetherSeconds);
	EXPECT_TRUE(noYawn[7].main);
	// This sleep is a plain hold of the sleep animation: it does not rest the creature
	EXPECT_FALSE(noYawn[7].untilRested);
	EXPECT_EQ(noYawn[7].effect, creature_mind::Effect::None);
	EXPECT_EQ(noYawn[8].animation, animations::k_Confused);
}

TEST(CreatureJointActions, DrinkingWithAFriend)
{
	const auto agenda = creature_mind::DrinkWithFriend(TwoFriends(), {50.0f, 60.0f}, {0.0f, 60.0f}, Always(0));
	ASSERT_EQ(agenda.size(), 11U);
	// The friend stands its own height along the first axis from the coast
	ExpectMeeting(agenda, 0, {50.0f, 60.0f}, {70.0f, 60.0f});
	EXPECT_EQ(Ordered(agenda[4]).movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[7].animation, animations::k_Drink);
	EXPECT_TRUE(agenda[7].main);
	EXPECT_EQ(Ordered(agenda[8]).effect, creature_mind::Effect::Drink);
	EXPECT_EQ(agenda[9].kind, Step::Kind::Apply);
	EXPECT_TRUE(agenda[10].flutterEyelids);
	EXPECT_EQ(agenda[10].face, creature_face::Cue::Smile);
}

// Eating from a field with a friend, as the original's construct lays it: the friend 10 m west of the field and the
// creature 10 m east, each within the creature's height; both face the field and take a handful of wheat; the friend
// eats, then it eats (the main step); it faces the friend and is happy, lets it go, then faces it again fluttering its
// eyelids and is happy
TEST(CreatureJointActions, EatingFromAFieldWithAFriend)
{
	const auto agenda = creature_mind::EatFromFieldWithFriend(TwoFriends(), 5, {300.0f, 400.0f});
	ASSERT_EQ(agenda.size(), 14U);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::BeFriends);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Playfulness);
	EXPECT_EQ(Ordered(agenda[1]).movement.point, glm::vec2(290.0f, 400.0f));
	EXPECT_FLOAT_EQ(Ordered(agenda[1]).movement.maxDistance, 30.0f);
	EXPECT_EQ(agenda[2].movement.point, glm::vec2(310.0f, 400.0f));
	EXPECT_EQ(Ordered(agenda[3]).movement.object, std::optional<uint32_t>(5));
	EXPECT_EQ(agenda[4].movement.object, std::optional<uint32_t>(5));
	EXPECT_EQ(Ordered(agenda[5]).order.kind, creature_mind::ObjectOrder::Kind::PickUpPortion);
	EXPECT_EQ(Ordered(agenda[5]).order.portion, PotInfo::WheatInHand);
	EXPECT_EQ(agenda[6].order.kind, creature_mind::ObjectOrder::Kind::PickUpPortion);
	EXPECT_EQ(Ordered(agenda[7]).order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda[8].order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_TRUE(agenda[8].main);
	EXPECT_EQ(agenda[10].animation, animations::k_Happy);
	EXPECT_EQ(agenda[11].kind, Step::Kind::Remobilise);
	EXPECT_TRUE(agenda[12].flutterEyelids);
	EXPECT_EQ(agenda[13].animation, animations::k_Happy);
}

TEST(CreatureJointActions, HelpingOrAttackingATownWithAFriend)
{
	const creature_mind::FriendsTown town {.town = 3, .pit = 4, .townPoint = {500.0f, 600.0f}, .pitRadius = 10.0f};
	const auto agenda = creature_mind::TownWithFriend(TwoFriends(), town, Desire::Anger);
	ASSERT_EQ(agenda.size(), 10U);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::BeFriends);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Playfulness);
	EXPECT_EQ(agenda[3].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_TRUE(agenda[3].main);
	// Both go near the pit, to twice its radius and the asker's height
	EXPECT_FLOAT_EQ(Ordered(agenda[4]).movement.maxDistance, 80.0f);
	EXPECT_EQ(agenda[5].movement.object, 4U);
	const auto& told = Ordered(agenda[8]);
	EXPECT_EQ(told.kind, Step::Kind::ApplyDesire);
	EXPECT_EQ(told.desire, Desire::Anger);
	EXPECT_EQ(agenda[9].object, 3U);
}

TEST(CreatureJointActions, WhatLetsEachJointActionStart)
{
	using creature_plan_actions::Situation;
	const auto possible = [](std::string_view action, const Situation& situation) {
		const auto* executor = creature_plan_actions::For(action);
		EXPECT_NE(executor, nullptr);
		return executor != nullptr && creature_plan_actions::Possible(*executor, situation);
	};
	Situation situation;
	EXPECT_FALSE(possible("DrinkWithFriend", situation));
	EXPECT_FALSE(possible("PooWithFriend", situation));
	EXPECT_FALSE(possible("SleepWithFriend", situation));
	EXPECT_TRUE(possible("BeHappyWithFriend", situation));
	EXPECT_TRUE(possible("SitWithFriend", situation));
	situation.waterDesire = 0.11f;
	situation.pooDesire = 0.11f;
	situation.tiredness = 0.11f;
	situation.sadness = 0.1f;
	EXPECT_TRUE(possible("DrinkWithFriend", situation));
	EXPECT_TRUE(possible("PooWithFriend", situation));
	EXPECT_TRUE(possible("SleepWithFriend", situation));
	EXPECT_FALSE(possible("BeHappyWithFriend", situation));
	// Without a belief about the nearest town, neither town action starts
	EXPECT_FALSE(possible("HelpTownWithFriend", situation));
	EXPECT_FALSE(possible("AttackWithFriend", situation));
	situation.attackNearestTown = false;
	EXPECT_TRUE(possible("HelpTownWithFriend", situation));
	EXPECT_FALSE(possible("AttackWithFriend", situation));
	situation.attackNearestTown = true;
	EXPECT_FALSE(possible("HelpTownWithFriend", situation));
	EXPECT_TRUE(possible("AttackWithFriend", situation));
}

namespace
{
std::vector<Step::Kind> KindsOf(const std::vector<Step>& agenda)
{
	std::vector<Step::Kind> kinds;
	for (const auto& step : agenda)
	{
		kinds.push_back(step.kind);
	}
	return kinds;
}

size_t MainStep(const std::vector<Step>& agenda)
{
	for (size_t i = 0; i < agenda.size(); ++i)
	{
		if (agenda[i].main)
		{
			return i;
		}
	}
	return agenda.size();
}
} // namespace

TEST(CreatureJointActions, TalkingAndTellingAJoke)
{
	const auto talk = creature_mind::TalkToFriend(k_Asker, k_Friend, 10.0f, Always(0));
	ASSERT_EQ(talk.size(), 6);
	EXPECT_FLOAT_EQ(talk[0].movement.maxDistance, 30.0f);
	EXPECT_TRUE(talk[0].flutterEyelids);
	EXPECT_TRUE(talk[1].main);
	EXPECT_FLOAT_EQ(talk[1].seconds, 4.0f);
	// The request comes only after it has turned to the friend
	EXPECT_EQ(talk[2].kind, Step::Kind::RequestPartner);
	EXPECT_FLOAT_EQ(Ordered(talk[3]).seconds, creature_mind::k_FriendFacesTalkerSeconds);
	EXPECT_EQ(talk[4].kind, Step::Kind::Overlay);
	EXPECT_EQ(talk[4].animation, animations::k_Talk);
	EXPECT_EQ(Ordered(talk[5]).kind, Step::Kind::Overlay);
	const auto joke = creature_mind::TellFriendAJoke(k_Asker, k_Friend, 10.0f, Always(0));
	EXPECT_EQ(Ordered(joke[5]).kind, Step::Kind::Action);
	EXPECT_EQ(Ordered(joke[5]).animation, creature_mind::k_Laugh);
}

TEST(CreatureJointActions, DancingWithAFriend)
{
	const auto agenda = creature_mind::DanceWithFriend(k_Asker, k_Friend, 10.0f);
	const std::vector expected {
	    Step::Kind::Move,  Step::Kind::Move,         Step::Kind::Action, Step::Kind::RequestPartner, Step::Kind::Demobilise,
	    Step::Kind::Order, Step::Kind::WaitForStill, Step::Kind::Order,  Step::Kind::PracticeDance,  Step::Kind::Remobilise};
	EXPECT_EQ(KindsOf(agenda), expected);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 20.0f);
	EXPECT_EQ(Ordered(agenda[7]).kind, Step::Kind::PracticeDance);
	EXPECT_FLOAT_EQ(agenda[8].seconds, creature_mind::k_DanceTogetherSeconds);
	EXPECT_EQ(MainStep(agenda), 8u);
}

TEST(CreatureJointActions, ArguingMopingAndShowingOff)
{
	const auto argue = creature_mind::ArgueWithFriend(k_Asker, k_Friend, 10.0f, Always(0));
	ASSERT_EQ(argue.size(), 16);
	EXPECT_FLOAT_EQ(Ordered(argue[1]).movement.maxDistance, 30.0f);
	EXPECT_EQ(Ordered(argue[1]).movement.object, k_Asker);
	EXPECT_EQ(argue[6].face, creature_face::Cue::Anger);
	EXPECT_EQ(argue[9].animation, animations::k_Angry);
	EXPECT_FLOAT_EQ(argue[11].seconds, creature_mind::k_ArgueWaitSeconds);
	EXPECT_EQ(MainStep(argue), 13u);
	EXPECT_EQ(argue[15].animation, animations::k_Happy);

	const auto mope = creature_mind::MopeAboutWithFriend(k_Asker, k_Friend, 10.0f, Always(1));
	ASSERT_EQ(mope.size(), 7);
	EXPECT_EQ(mope[2].face, creature_face::Cue::Sad);
	EXPECT_EQ(MainStep(mope), 6u);
	EXPECT_EQ(creature_mind::MopeAboutWithFriend(k_Asker, k_Friend, 10.0f, Always(0)).size(), 9);

	const auto show = creature_mind::ShowOffToFriend(k_Asker, k_Friend, 10.0f, {50.0f, 3.0f, 60.0f}, Always(0));
	ASSERT_EQ(show.size(), 11);
	EXPECT_TRUE(show[5].main);
	EXPECT_FLOAT_EQ(show[5].order.point.x, 50.0f);
	EXPECT_FLOAT_EQ(*show[5].order.pointHeight, 3.0f);
	EXPECT_EQ(show[5].face, creature_face::Cue::Grimace);
	EXPECT_EQ(Ordered(show[6]).movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(Ordered(show[8]).animation, animations::k_Confused);
}

// Confusing a friend is showing off without its last two steps: it looks at the friend as it faces it, flutters its
// eyelids as it points at it, and ends with the friend's confused action
// Showing a friend a destructive miracle: turned to the friend, with a thing that can burn it asks to show the friend,
// both go near the thing, it backs off, points at it, waits for the friend and casts at it as the main step; with no
// such thing it only turns to the friend. A creating miracle waits for the friend to come before it points
TEST(CreatureJointActions, ShowingAFriendAMiracle)
{
	constexpr uint32_t k_Tree = 11;
	const creature_mind::Friends friends {.self = k_Asker, .friendly = k_Friend, .height = 10.0f};
	const auto fire = creature_mind::ShowFriendSpell(
	    friends, {.magicType = 3, .destructive = true, .target = k_Tree, .head = {1.0f, 2.0f, 3.0f}, .seesFriend = false});
	ASSERT_EQ(fire.size(), 13u);
	EXPECT_FLOAT_EQ(fire[0].movement.maxDistance, 15.0f);
	EXPECT_EQ(fire[2].request, creature_friendship::RequestKind::BeShown);
	EXPECT_FLOAT_EQ(Ordered(fire[3]).movement.maxDistance, 25.0f);
	EXPECT_EQ(fire[5].movement.kind, creature_mind::Movement::Kind::GetAwayFromObject);
	EXPECT_EQ(fire[6].face, creature_face::Cue::Fear);
	EXPECT_EQ(fire[9].kind, Step::Kind::WaitForFree);
	EXPECT_TRUE(fire[11].main);
	EXPECT_EQ(fire[11].kind, Step::Kind::Cast);
	EXPECT_EQ(fire[11].cast.magicType, 3u);
	EXPECT_EQ(fire[12].face, creature_face::Cue::Amazed);
	const auto none = creature_mind::ShowFriendSpell(friends, {.magicType = 3, .destructive = true});
	ASSERT_EQ(none.size(), 1u);
	EXPECT_EQ(none[0].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	const auto food = creature_mind::ShowFriendSpell(friends, {.magicType = 7, .destructive = false, .target = k_Tree});
	ASSERT_EQ(food.size(), 13u);
	EXPECT_EQ(food[0].face, creature_face::Cue::Compassion);
	EXPECT_EQ(food[4].kind, Step::Kind::WaitForStill);
	EXPECT_TRUE(food[11].main);
	EXPECT_EQ(food[11].face, creature_face::Cue::Compassion);
}

TEST(CreatureJointActions, ConfusingAFriend)
{
	const auto confuse = creature_mind::ConfuseFriend(k_Asker, k_Friend, 10.0f, {50.0f, 3.0f, 60.0f}, Always(0));
	ASSERT_EQ(confuse.size(), 9);
	EXPECT_FALSE(confuse[4].flutterEyelids);
	ASSERT_TRUE(confuse[4].gaze.has_value());
	EXPECT_EQ(confuse[4].gaze->object, std::optional<uint32_t> {k_Friend});
	EXPECT_TRUE(confuse[5].main);
	EXPECT_EQ(confuse[5].face, creature_face::Cue::Grimace);
	EXPECT_EQ(Ordered(confuse[6]).movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_TRUE(confuse[7].flutterEyelids);
	EXPECT_EQ(confuse[7].face, creature_face::Cue::Sad);
	EXPECT_EQ(Ordered(confuse[8]).animation, animations::k_Confused);
}

TEST(CreatureJointActions, GettingTheAttentionOfAFriend)
{
	const auto all = creature_mind::GetAttentionFromFriend(k_Friend, Always(0));
	ASSERT_EQ(all.size(), 6);
	EXPECT_EQ(all[1].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_EQ(all[2].animation, animations::k_FriendlyWave);
	EXPECT_EQ(all[3].animation, creature_mind::k_LookAtMe);
	EXPECT_TRUE(all[5].main);
	EXPECT_EQ(all[5].animation, animations::k_Sad);
	EXPECT_EQ(creature_mind::GetAttentionFromFriend(k_Friend, Always(1)).size(), 3);
}

TEST(CreatureJointActions, ExaminingAnotherCreatureWithAFriend)
{
	const auto agenda = creature_mind::ExamineOtherCreatureWithFriend(k_Friend, 3, Always(0));
	ASSERT_EQ(agenda.size(), 8);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::Play);
	EXPECT_EQ(Ordered(agenda[4]).order.object, 3U);
	// It never lets the mind of the friend go free
	for (const auto& step : agenda)
	{
		EXPECT_NE(step.kind, Step::Kind::Remobilise);
	}
	EXPECT_EQ(creature_mind::ExamineOtherCreatureWithFriend(k_Friend, 3, Always(1)).size(), 6);
}

// An order's face is the asker's, pulled as it gives the order; the partner does the ordered step with none
TEST(CreatureJointActions, TheAskerPullsAnOrdersFaceAndThePartnerNone)
{
	const auto wave = creature_mind::WaveAtFriend(k_Asker, k_Friend);
	const auto order = std::ranges::find(wave, Step::Kind::Order, &Step::kind);
	ASSERT_NE(order, wave.end());
	EXPECT_EQ(order->face, creature_face::Cue::Grimace);
	EXPECT_EQ(Ordered(*order).face, creature_face::Cue::None);
	const auto argue = creature_mind::ArgueWithFriend(k_Asker, k_Friend, 10.0f, Always(0));
	EXPECT_EQ(argue[1].face, creature_face::Cue::Smile);
	EXPECT_EQ(argue[5].face, creature_face::Cue::Anger);
	EXPECT_EQ(Ordered(argue[5]).face, creature_face::Cue::None);
	const auto mope = creature_mind::MopeAboutWithFriend(k_Asker, k_Friend, 10.0f, Always(1));
	EXPECT_EQ(mope[1].face, creature_face::Cue::Sad);
}

TEST(CreatureJointActions, SittingOnTopOfAHillWithAFriend)
{
	const auto friends = TwoFriends();
	const float radius = creature_mind::HillTogetherRadius(4.0f, 2.0f);
	EXPECT_FLOAT_EQ(radius, 24.0f);
	const auto agenda = creature_mind::SitOnTopOfHillWithFriend(friends, {100.0f, 100.0f}, radius, true, Always(1));
	ASSERT_EQ(agenda.size(), 9);
	EXPECT_FLOAT_EQ(Ordered(agenda[2]).movement.point.x, 94.0f);
	EXPECT_FLOAT_EQ(agenda[3].movement.point.x, 106.0f);
	EXPECT_FLOAT_EQ(agenda[3].movement.maxDistance, 2.5f * friends.height);
	EXPECT_EQ(Ordered(agenda[5]).movement.kind, creature_mind::Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[7].movement.kind, creature_mind::Movement::Kind::FaceDownSlope);
	EXPECT_TRUE(agenda[8].main);
	EXPECT_EQ(agenda[8].kind, Step::Kind::Static);
	// When it cannot see the friend it goes near it first, and half the time it is happy first
	EXPECT_EQ(creature_mind::SitOnTopOfHillWithFriend(friends, {}, radius, false, Always(0)).size(), 11);
}

TEST(CreatureJointActions, SeeingAnObject)
{
	EXPECT_TRUE(creature_mind::CanSee(9.0f, 0.0f));
	EXPECT_FALSE(creature_mind::CanSee(10.0f, 0.0f));
	EXPECT_TRUE(creature_mind::CanSee(400.0f, 10.0f));
	EXPECT_FALSE(creature_mind::CanSee(500.0f, 10.0f));
}

TEST(CreatureJointActions, PlayingAThrowingGameWithAFriend)
{
	const auto friends = TwoFriends();
	const glm::vec3 atSelf {1.0f, 2.0f, 3.0f};
	const glm::vec3 atFriend {4.0f, 5.0f, 6.0f};
	const auto agenda = creature_mind::PlayThrowingGameWithFriend(friends, 11, atSelf, atFriend, false);
	ASSERT_EQ(agenda.size(), 15);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 5.0f * friends.height);
	EXPECT_EQ(agenda[2].request, creature_friendship::RequestKind::Play);
	// the order to face it, given with a grimace
	EXPECT_EQ(agenda[6].face, creature_face::Cue::Grimace);
	EXPECT_EQ(agenda[7].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_TRUE(agenda[8].main);
	EXPECT_FLOAT_EQ(agenda[8].order.point.x, 4.0f);
	EXPECT_FLOAT_EQ(*agenda[8].order.pointHeight, 5.0f);
	EXPECT_FLOAT_EQ(agenda[9].seconds, creature_mind::k_WaitAfterThrowSeconds);
	EXPECT_EQ(Ordered(agenda[12]).order.kind, creature_mind::ObjectOrder::Kind::Throw);
	EXPECT_FLOAT_EQ(Ordered(agenda[12]).order.point.y, 3.0f);
	EXPECT_EQ(agenda[14].kind, Step::Kind::Remobilise);
	// holding something already, it picks nothing up
	EXPECT_EQ(creature_mind::PlayThrowingGameWithFriend(friends, 11, atSelf, atFriend, true).size(), 14);
	// thrown at 0.82 of fifteen times the drawn size above where it stands
	EXPECT_FLOAT_EQ(creature_mind::ThrowPointAt({0.0f, 1.0f, 0.0f}, 2.0f).y, 1.0f + 0.82f * 30.0f);
}

TEST(CreatureJointActions, GivingAFriendAToy)
{
	const auto agenda = creature_mind::GiveFriendToy(k_Asker, k_Friend, 11, false, Always(2));
	ASSERT_EQ(agenda.size(), 12);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::BeFriends);
	// the friend points at it, the smile is the giver's own
	EXPECT_EQ(Ordered(agenda[1]).order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_EQ(Ordered(agenda[1]).order.object, std::optional(k_Asker));
	EXPECT_FLOAT_EQ(*Ordered(agenda[1]).order.seconds, 5.0f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Smile);
	EXPECT_EQ(agenda[2].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_FLOAT_EQ(agenda[3].movement.maxDistance, 0.0f);
	EXPECT_TRUE(agenda[4].flutterEyelids);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Compassion);
	EXPECT_TRUE(agenda[5].main);
	EXPECT_EQ(agenda[5].order.kind, creature_mind::ObjectOrder::Kind::Discard);
	EXPECT_EQ(agenda[5].order.animation, 97u);
	EXPECT_EQ(agenda[6].kind, Step::Kind::WaitInMap);
	EXPECT_EQ(Ordered(agenda[7]).order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_FLOAT_EQ(agenda[9].seconds, 5.0f);
	EXPECT_EQ(agenda[10].kind, Step::Kind::WaitForFree);
	EXPECT_EQ(agenda[10].turns, std::optional(4u));
	// the way it plays with the toy: one of the four, drawn as the agenda is made
	EXPECT_EQ(Ordered(agenda[11]).order.kind, creature_mind::ObjectOrder::Kind::Keep);
	EXPECT_EQ(Ordered(agenda[11]).order.animation, 102u);
	// holding something already, it picks nothing up
	EXPECT_EQ(creature_mind::GiveFriendToy(k_Asker, k_Friend, 11, true, Always(0)).size(), 11);
}

TEST(CreatureJointActions, WaitingForAFriendToBeFreeCountsItsTurnsFirst)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::WaitForFree, .turns = 4u, .object = k_Friend}});
	creature_mind::Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	// the turn it starts, and four more while the friend is free
	for (int turn = 0; turn < 5; ++turn)
	{
		std::ignore = creature_mind::Think(mind, senses, Always(0));
		EXPECT_EQ(mind.step, 0u) << turn;
	}
	// then it waits while the friend's body is busy
	senses.objectBusy = true;
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 0u);
	senses.objectBusy = false;
	std::ignore = creature_mind::Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
}

TEST(CreatureJointActions, ShowingAFriendItsCitadel)
{
	const auto friends = TwoFriends();
	const creature_mind::Citadel citadel {.point = {500.0f, 30.0f, 600.0f}, .radius = 10.0f};
	const glm::vec3 head {1.0f, 2.0f, 3.0f};
	// the friend's wait is drawn first, then its own
	std::vector<uint32_t> draws {4, 6};
	size_t drawn = 0;
	const creature_mind::Random random = [&](uint32_t) { return draws[drawn++]; };
	const auto agenda = creature_mind::ShowFriendCitadel(friends, citadel, 3.0f, 2.0f, head, true, random);
	ASSERT_EQ(agenda.size(), 11);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[1].request, creature_friendship::RequestKind::BeShown);
	// the friend goes to five of its radii and 1.2 of the heart's, the giver puzzled as it gives the order
	EXPECT_FLOAT_EQ(Ordered(agenda[2]).movement.maxDistance, 2.0f * 5.0f + 10.0f * 1.2f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Puzzled);
	EXPECT_FLOAT_EQ(agenda[3].movement.maxDistance, 3.0f * 5.0f + 10.0f * 1.2f);
	EXPECT_FLOAT_EQ(agenda[3].movement.point.x, 500.0f);
	EXPECT_EQ(agenda[4].kind, Step::Kind::WaitForStill);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
	EXPECT_TRUE(agenda[5].main);
	EXPECT_FLOAT_EQ(*agenda[5].order.seconds, 3.0f);
	EXPECT_FLOAT_EQ(*agenda[5].order.pointHeight, 30.0f);
	EXPECT_FLOAT_EQ(Ordered(agenda[6]).movement.point.x, 1.0f);
	EXPECT_EQ(agenda[7].kind, Step::Kind::WaitForFree);
	EXPECT_FLOAT_EQ(Ordered(agenda[8]).movement.point.y, 600.0f);
	EXPECT_EQ(agenda[9].face, creature_face::Cue::Puzzled);
	EXPECT_FLOAT_EQ(Ordered(agenda[9]).seconds, 5.0f + creature_emote::FloatRandom(Always(4), 20.0f));
	EXPECT_EQ(agenda[10].face, creature_face::Cue::Compassion);
	EXPECT_FLOAT_EQ(agenda[10].seconds, 5.0f + creature_emote::FloatRandom(Always(6), 20.0f));
	EXPECT_TRUE(agenda[10].gaze.has_value() && agenda[10].gaze->feet);
	// not seeing the friend, it goes near it first, to three of its heights
	const auto far = creature_mind::ShowFriendCitadel(friends, citadel, 3.0f, 2.0f, head, false, Always(0));
	ASSERT_EQ(far.size(), 12);
	EXPECT_FLOAT_EQ(far[0].movement.maxDistance, 3.0f * friends.height);
}
