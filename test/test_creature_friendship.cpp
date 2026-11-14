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
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKiss.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_friendship;
using creature_desires::Desire;
using creature_learning::CreatureAttitude;
using creature_mind::Step;

namespace
{
constexpr uint32_t k_Asker = 7;
constexpr uint32_t k_Asked = 9;

creature_mind::Random NoRandom()
{
	return [](uint32_t) { return 0U; };
}

/// Desires about the asker, each with its own worth
AnswerFacts Asked(RequestKind kind, std::array<float, 5> worths)
{
	return {.kind = kind, .availability = Availability::Free, .worthAbout = [worths](Desire desire) {
		        switch (desire)
		        {
		        case Desire::Play:
			        return worths[0];
		        case Desire::BeFriends:
			        return worths[1];
		        case Desire::Fear:
			        return worths[2];
		        case Desire::Anger:
			        return worths[3];
		        case Desire::Curiosity:
			        return worths[4];
		        default:
			        return 0.0f;
		        }
	        }};
}
} // namespace

TEST(CreatureFriendship, AvailabilityOfTheAskedCreature)
{
	EXPECT_EQ(AvailabilityOf({.asker = k_Asker}), Availability::Free);
	EXPECT_EQ(AvailabilityOf({.dead = true, .asker = k_Asker}), Availability::Refuses);
	// Script control must be the same for both
	EXPECT_EQ(AvailabilityOf({.scriptControlled = true, .asker = k_Asker}), Availability::Refuses);
	EXPECT_EQ(AvailabilityOf({.scriptControlled = true, .askerScriptControlled = true, .asker = k_Asker}), Availability::Free);
	EXPECT_EQ(AvailabilityOf({.partner = k_Asker, .asker = k_Asker}), Availability::AlreadyPartner);
	EXPECT_EQ(AvailabilityOf({.partner = 3U, .asker = k_Asker}), Availability::Refuses);
}

TEST(CreatureFriendship, AFightIsAgreedToOnlyNearInHeight)
{
	AnswerFacts fight {.kind = RequestKind::Fight, .availability = Availability::Free, .heightRatio = 0.5f};
	EXPECT_TRUE(AnswerOf(fight).agrees);
	fight.heightRatio = 2.0f;
	EXPECT_TRUE(AnswerOf(fight).agrees);
	fight.heightRatio = 2.01f;
	EXPECT_FALSE(AnswerOf(fight).agrees);
	fight.heightRatio = 0.49f;
	EXPECT_FALSE(AnswerOf(fight).agrees);
	// A busy creature refuses, a partner agrees again, and a fight refusal tells the player nothing
	fight.heightRatio = 1.0f;
	fight.availability = Availability::Refuses;
	EXPECT_FALSE(AnswerOf(fight).agrees);
	EXPECT_FALSE(AnswerOf(fight).explanation.has_value());
	fight.availability = Availability::AlreadyPartner;
	EXPECT_TRUE(AnswerOf(fight).agrees);
	// The rule after a forced action does not change a fight
	fight.alwaysAgree = true;
	fight.heightRatio = 3.0f;
	EXPECT_FALSE(AnswerOf(fight).agrees);
}

TEST(CreatureFriendship, PlayIsAgreedToWhenPlayOrFriendshipIsWantedMost)
{
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::Play, {0.3f, 0.0f, 0.2f, 0.1f, 0.0f})).agrees);
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::Play, {0.1f, 0.3f, 0.2f, 0.0f, 0.0f})).agrees);
	const auto feared = AnswerOf(Asked(RequestKind::Play, {0.1f, 0.2f, 0.3f, 0.0f, 0.0f}));
	EXPECT_FALSE(feared.agrees);
	EXPECT_EQ(feared.explanation, k_RefusedToPlay);
	// With nothing worth more than nothing, it refuses
	EXPECT_FALSE(AnswerOf(Asked(RequestKind::Play, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f})).agrees);
	// A later desire must be worth more than an earlier one, not as much
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::Play, {0.3f, 0.0f, 0.3f, 0.3f, 0.0f})).agrees);
}

TEST(CreatureFriendship, FriendshipAndBeingShownHaveTheirOwnDesire)
{
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::BeFriends, {0.0f, 0.4f, 0.3f, 0.0f, 0.0f})).agrees);
	// Play does not count for friendship
	const auto play = AnswerOf(Asked(RequestKind::BeFriends, {0.9f, 0.0f, 0.0f, 0.0f, 0.0f}));
	EXPECT_FALSE(play.agrees);
	EXPECT_EQ(play.explanation, k_RefusedToBeFriends);
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::BeShown, {0.0f, 0.0f, 0.1f, 0.1f, 0.2f})).agrees);
	const auto angry = AnswerOf(Asked(RequestKind::BeShown, {0.0f, 0.0f, 0.0f, 0.3f, 0.2f}));
	EXPECT_FALSE(angry.agrees);
	EXPECT_EQ(angry.explanation, k_RefusedToBeFriends);
}

TEST(CreatureFriendship, AfterAForcedActionAFreeCreatureAlwaysAgrees)
{
	auto facts = Asked(RequestKind::BeFriends, {0.0f, 0.0f, 0.9f, 0.0f, 0.0f});
	facts.alwaysAgree = true;
	EXPECT_TRUE(AnswerOf(facts).agrees);
	// It must still be free, and it says nothing when it is not
	facts.availability = Availability::Refuses;
	const auto busy = AnswerOf(facts);
	EXPECT_FALSE(busy.agrees);
	EXPECT_FALSE(busy.explanation.has_value());
	facts.availability = Availability::AlreadyPartner;
	EXPECT_TRUE(AnswerOf(facts).agrees);
}

TEST(CreatureFriendship, AttentionRisesForAnAgendaThatUsesTheCreature)
{
	AgendaUse use {.plan = {std::nullopt, k_Asked, std::nullopt}, .steps = {k_Asked}, .running = true};
	EXPECT_TRUE(AgendaUses(use, k_Asked));
	EXPECT_FALSE(AgendaUses(AgendaUse {.plan = {1U, 2U, 3U}, .steps = {4U}, .running = true}, k_Asked));
	use.running = false;
	EXPECT_FALSE(AgendaUses(use, k_Asked));
	// With no belief about the creature, a part of the plan that uses nothing counts, as in the game
	EXPECT_TRUE(AgendaUses(AgendaUse {.plan = {std::nullopt, 2U, 3U}, .running = true}, std::nullopt));
	EXPECT_TRUE(AttentionRises(false, false, true));
	EXPECT_FALSE(AttentionRises(true, false, true));
	EXPECT_FALSE(AttentionRises(false, true, true));
	EXPECT_FALSE(AttentionRises(false, false, false));
	CreatureAttitude attitude {.attention = 0.99f};
	ChangeAttention(attitude, k_AttentionStep);
	EXPECT_FLOAT_EQ(attitude.attention, 1.0f);
}

TEST(CreatureFriendship, FeelingsFadeAndFriendsAreMissed)
{
	std::vector<CreatureAttitude> attitudes {{.creature = 1, .howNice = 0.5f, .howImpressive = 1.0f, .attention = 1.0f}};
	Fade(attitudes);
	EXPECT_FLOAT_EQ(attitudes[0].howImpressive, k_ImpressiveFade);
	EXPECT_FLOAT_EQ(attitudes[0].attention, k_AttentionFade);
	EXPECT_FLOAT_EQ(attitudes[0].howNice, 0.5f);
	// Nice enough and given too little attention, it is missed by one less the attention
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.5f, .attention = 0.25f}}), 0.75f);
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.4f, .attention = 0.0f}}), 0.0f);
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.5f, .attention = 0.4f}}), 0.0f);
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.9f, .attention = 0.1f},
	                                                             {.howNice = 0.6f, .attention = 0.3f}}),
	                1.0f);
}

TEST(CreatureFriendship, AFriendGoneFromTheWorldIsTakenOutOfTheList)
{
	const auto a = static_cast<entt::entity>(3);
	const auto b = static_cast<entt::entity>(5);
	const auto c = static_cast<entt::entity>(9);
	std::vector<entt::entity> friends {a, b, c};
	ForgetGoneFriends(friends, [b](entt::entity other) { return other == b; });
	// the others keep their order, the newest first
	EXPECT_EQ(friends, (std::vector<entt::entity> {a, c}));
	ForgetGoneFriends(friends, [](entt::entity) { return false; });
	EXPECT_EQ(friends.size(), 2u);
	ForgetGoneFriends(friends, [](entt::entity) { return true; });
	EXPECT_TRUE(friends.empty());
}

TEST(CreatureFriendship, SmilingAtAFriendGoesNearFacesItAndHalfTheTimeShowsItIsHappy)
{
	// The draws in the original's order: the float for the hold first, then the one in two
	std::vector<uint32_t> asked;
	const auto drawing = [&asked](uint32_t answer) {
		return creature_mind::Random([&asked, answer](uint32_t n) {
			asked.push_back(n);
			return n == 2 ? answer : 0U;
		});
	};
	const auto happy = creature_mind::SmileAtFriend(k_Asked, 10.0f, drawing(0));
	ASSERT_EQ(happy.size(), 3U);
	EXPECT_EQ(happy[0].movement.kind, creature_mind::Movement::Kind::GoNearObject);
	EXPECT_EQ(happy[0].movement.object, k_Asked);
	EXPECT_FLOAT_EQ(happy[0].movement.maxDistance, 30.0f);
	EXPECT_TRUE(happy[0].flutterEyelids);
	EXPECT_EQ(happy[0].face, creature_face::Cue::Smile);
	EXPECT_EQ(happy[1].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_TRUE(happy[1].main);
	EXPECT_TRUE(happy[1].flutterEyelids);
	EXPECT_FLOAT_EQ(happy[1].seconds, creature_mind::k_SmileFaceSeconds);
	EXPECT_EQ(happy[2].animation, creature_layers::animations::k_Happy);
	ASSERT_EQ(asked.size(), 2U);
	EXPECT_EQ(asked.back(), 2U);
	// The other half: no happy show
	EXPECT_EQ(creature_mind::SmileAtFriend(k_Asked, 10.0f, drawing(1)).size(), 2U);
}

// Showing another creature it hates it: near it by the taller one's height, angry; facing it two seconds; half the time
// pointing at it with a grimace; one of two angry actions; half the time one more, the draws in that order
TEST(CreatureFriendship, ShowingACreatureItHatesItGoesNearFacesItAndPlaysAngryActions)
{
	const auto drawing = [](std::vector<uint32_t> answers) {
		auto queue = std::make_shared<std::vector<uint32_t>>(std::move(answers));
		return [queue](uint32_t range) {
			EXPECT_EQ(range, 2U);
			EXPECT_FALSE(queue->empty());
			const auto answer = queue->empty() ? 0U : queue->front();
			if (!queue->empty())
			{
				queue->erase(queue->begin());
			}
			return answer;
		};
	};
	// points, the first angry action, then one more
	const auto all = creature_mind::ShowCreatureItHatesIt(k_Asked, 10.0f, 16.0f, drawing({0, 1, 0}));
	ASSERT_EQ(all.size(), 5U);
	EXPECT_EQ(all[0].movement.kind, creature_mind::Movement::Kind::GoNearObject);
	EXPECT_EQ(all[0].movement.object, k_Asked);
	EXPECT_FLOAT_EQ(all[0].movement.maxDistance, 40.0f);
	EXPECT_EQ(all[0].face, creature_face::Cue::Anger);
	ASSERT_TRUE(all[0].gaze.has_value());
	EXPECT_TRUE(all[0].gaze->goingTowards);
	EXPECT_EQ(all[1].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(all[1].seconds, 2.0f);
	EXPECT_EQ(all[1].face, creature_face::Cue::Anger);
	EXPECT_EQ(all[2].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_EQ(all[2].order.object, k_Asked);
	ASSERT_TRUE(all[2].order.seconds.has_value());
	EXPECT_FLOAT_EQ(*all[2].order.seconds, 2.0f);
	EXPECT_EQ(all[2].face, creature_face::Cue::Grimace);
	EXPECT_EQ(all[3].animation, 222U);
	EXPECT_EQ(all[4].animation, 53U);
	EXPECT_FALSE(all[1].main);
	// no pointing, the second angry action and no more; its own height the taller
	const auto least = creature_mind::ShowCreatureItHatesIt(k_Asked, 10.0f, 4.0f, drawing({1, 0, 1}));
	ASSERT_EQ(least.size(), 3U);
	EXPECT_FLOAT_EQ(least[0].movement.maxDistance, 25.0f);
	EXPECT_EQ(least[2].animation, 70U);
	// it is done to another creature, by both heights
	const auto* show = creature_plan_actions::For("ShowCreatureYouHateHim");
	ASSERT_NE(show, nullptr);
	EXPECT_EQ(show->target, creature_plan_actions::Target::Creature);
}

// Showing another creature how nice it reckons it is: facing it two seconds, then, as the main step, the first silly face
// for neither nice nor nasty, clip 55 for nice, clip 222 or 70 by a draw for nasty; nothing for a thing that is no
// creature
TEST(CreatureFriendship, ShowingHowNiceItReckonsACreatureIsGoesByWhatItThinksOfIt)
{
	const auto none = [](uint32_t /*range*/) {
		ADD_FAILURE() << "no draw expected";
		return 0U;
	};
	const auto neither = creature_mind::ShowHowNiceItReckons(k_Asked, 0.0f, none);
	ASSERT_TRUE(neither.has_value());
	ASSERT_EQ(neither->size(), 2U);
	EXPECT_EQ((*neither)[0].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*neither)[0].movement.object, k_Asked);
	EXPECT_FLOAT_EQ((*neither)[0].seconds, 2.0f);
	EXPECT_FALSE((*neither)[0].main);
	EXPECT_EQ((*neither)[1].kind, Step::Kind::Face);
	EXPECT_EQ((*neither)[1].animation, 16U);
	EXPECT_TRUE((*neither)[1].main);
	const auto nice = creature_mind::ShowHowNiceItReckons(k_Asked, 0.4f, none);
	ASSERT_TRUE(nice.has_value());
	ASSERT_EQ(nice->size(), 2U);
	EXPECT_EQ((*nice)[1].animation, 55U);
	EXPECT_TRUE((*nice)[1].main);
	for (const auto [draw, clip] : {std::pair {1U, 222U}, std::pair {0U, 70U}})
	{
		const auto nasty = creature_mind::ShowHowNiceItReckons(k_Asked, -0.4f, [draw](uint32_t range) {
			EXPECT_EQ(range, 2U);
			return draw;
		});
		ASSERT_TRUE(nasty.has_value());
		ASSERT_EQ(nasty->size(), 2U);
		EXPECT_EQ((*nasty)[1].animation, clip);
		EXPECT_TRUE((*nasty)[1].main);
	}
	EXPECT_FALSE(creature_mind::ShowHowNiceItReckons(k_Asked, std::nullopt, none).has_value());
	const auto* show = creature_plan_actions::For("ShowHowNiceYouThinkCreatureIs");
	ASSERT_NE(show, nullptr);
	EXPECT_EQ(show->target, creature_plan_actions::Target::Creature);
}

// Telling another creature to go away: half the time it fights it; otherwise asks it as a fight, playful; points past it as
// far beyond it as it stands from it, for 3 seconds, as its main step; and sends it there, waiting for it to stand still
TEST(CreatureFriendship, TellingACreatureToGoAwayFightsItHalfTheTimeElsePointsItOnItsWay)
{
	const glm::vec3 here {100.0f, 5.0f, 100.0f};
	const glm::vec3 there {110.0f, 7.0f, 90.0f};
	const auto fight = creature_mind::TellCreatureToGoAway(k_Asker, k_Asked, here, there, 12.0f, [](uint32_t range) {
		EXPECT_EQ(range, 2U);
		return 0U;
	});
	EXPECT_EQ(fight.size(), creature_mind::FightCreature(k_Asked).size());
	EXPECT_EQ(fight.at(2).kind, Step::Kind::Fight);

	const auto away = creature_mind::TellCreatureToGoAway(k_Asker, k_Asked, here, there, 12.0f, [](uint32_t range) {
		EXPECT_EQ(range, 2U);
		return 1U;
	});
	ASSERT_EQ(away.size(), 13U);
	EXPECT_EQ(away[0].kind, Step::Kind::RequestPartner);
	EXPECT_EQ(away[0].request, creature_friendship::RequestKind::Fight);
	EXPECT_EQ(away[0].face, creature_face::Cue::Playfulness);
	EXPECT_EQ(away[1].kind, Step::Kind::Demobilise);
	ASSERT_EQ(away[2].kind, Step::Kind::Order);
	EXPECT_EQ(away[2].ordered->movement.object, k_Asker);
	EXPECT_FLOAT_EQ(away[3].seconds, 1.4f);
	EXPECT_EQ(away[3].face, creature_face::Cue::Smile);
	EXPECT_EQ(away[4].animation, 53U);
	EXPECT_EQ(away[4].face, creature_face::Cue::Amazed);
	EXPECT_EQ(away[5].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_EQ(away[5].order.point, glm::vec2(120.0f, 80.0f));
	ASSERT_TRUE(away[5].order.pointHeight.has_value());
	EXPECT_FLOAT_EQ(*away[5].order.pointHeight, 9.0f);
	ASSERT_TRUE(away[5].order.seconds.has_value());
	EXPECT_FLOAT_EQ(*away[5].order.seconds, 3.0f);
	EXPECT_EQ(away[5].face, creature_face::Cue::Grimace);
	EXPECT_TRUE(away[5].main);
	EXPECT_EQ(away[6].ordered->animation, 56U);
	EXPECT_EQ(away[7].animation, 53U);
	ASSERT_EQ(away[8].kind, Step::Kind::Order);
	EXPECT_EQ(away[8].ordered->movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_EQ(away[8].ordered->movement.point, glm::vec2(120.0f, 80.0f));
	EXPECT_FLOAT_EQ(away[8].ordered->movement.maxDistance, 24.0f);
	EXPECT_EQ(away[9].kind, Step::Kind::WaitForStill);
	EXPECT_EQ(away[10].ordered->animation, 56U);
	EXPECT_EQ(away[11].animation, 53U);
	EXPECT_EQ(away[12].kind, Step::Kind::Remobilise);
	const auto* sodOff = creature_plan_actions::For("TellCreatureToSodOff");
	ASSERT_NE(sodOff, nullptr);
	EXPECT_EQ(sodOff->target, creature_plan_actions::Target::CreatureFreeForJoint);
}

TEST(CreatureFriendship, WavingAtAFriendIsAJointAction)
{
	const auto agenda = creature_mind::WaveAtFriend(k_Asker, k_Asked);
	ASSERT_EQ(agenda.size(), 7U);
	EXPECT_EQ(agenda[0].kind, Step::Kind::RequestPartner);
	EXPECT_EQ(agenda[0].request, RequestKind::Play);
	EXPECT_EQ(agenda[0].object, k_Asked);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Demobilise);
	ASSERT_EQ(agenda[2].kind, Step::Kind::Order);
	ASSERT_NE(agenda[2].ordered, nullptr);
	EXPECT_EQ(agenda[2].ordered->movement.object, k_Asker);
	EXPECT_FLOAT_EQ(agenda[2].ordered->seconds, creature_mind::k_FriendFacesWaverSeconds);
	// An order has no face and no gaze of its own
	EXPECT_EQ(agenda[2].ordered->face, creature_face::Cue::None);
	EXPECT_FALSE(agenda[2].ordered->gaze.has_value());
	EXPECT_FLOAT_EQ(agenda[3].seconds, creature_mind::k_FaceFriendToWaveSeconds);
	ASSERT_EQ(agenda[4].kind, Step::Kind::Order);
	EXPECT_EQ(agenda[4].ordered->animation, creature_layers::animations::k_FriendlyWave);
	EXPECT_TRUE(agenda[5].main);
	EXPECT_EQ(agenda[5].animation, creature_layers::animations::k_FriendlyWave);
	EXPECT_EQ(agenda[6].kind, Step::Kind::Remobilise);
}

TEST(CreatureFriendship, MissingAFriend)
{
	const auto pine = creature_mind::PineForFriend(k_Asked);
	ASSERT_EQ(pine.size(), 3U);
	EXPECT_EQ(pine[1].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_TRUE(pine[2].main);
	EXPECT_EQ(pine[2].animation, creature_layers::animations::k_Sad);
	const auto look = creature_mind::LookForFriend(k_Asked, 10.0f);
	ASSERT_EQ(look.size(), 3U);
	EXPECT_TRUE(look[1].main);
	EXPECT_EQ(look[2].movement.kind, creature_mind::Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(look[2].movement.maxDistance, 30.0f);
}

TEST(CreatureFriendship, HowlingAtAFriendFacesItTwoSecondsThenHowls)
{
	const auto howl = creature_mind::HowlAtFriend(k_Asked);
	ASSERT_EQ(howl.size(), 2U);
	// It turns to face the friend for two seconds, looking at it, with no face of its own
	EXPECT_EQ(howl[0].kind, Step::Kind::Move);
	EXPECT_EQ(howl[0].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(howl[0].movement.object, std::optional<uint32_t>(k_Asked));
	EXPECT_FLOAT_EQ(howl[0].seconds, 2.0f);
	EXPECT_EQ(howl[0].face, creature_face::Cue::None);
	ASSERT_TRUE(howl[0].gaze.has_value());
	EXPECT_EQ(howl[0].gaze->object, std::optional<uint32_t>(k_Asked));
	// Then it plays the howl once, clip 217, and nothing more
	EXPECT_EQ(howl[1].kind, Step::Kind::Action);
	EXPECT_EQ(howl[1].animation, 217U);
	EXPECT_EQ(howl[1].face, creature_face::Cue::None);
	EXPECT_FALSE(howl[1].gaze.has_value());
}

TEST(CreatureFriendship, TheStepsOfAJointActionAreDoneAsTheyStart)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, creature_mind::WaveAtFriend(k_Asker, k_Asked));
	const creature_mind::Senses senses {.seconds = 0.1f};
	const auto request = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(request.requestPartner.has_value());
	EXPECT_EQ(request.requestPartner->creature, k_Asked);
	EXPECT_EQ(mind.step, 1U);
	const auto demobilise = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(demobilise.demobilise.has_value());
	EXPECT_TRUE(demobilise.demobilise->second);
	const auto order = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(order.order.has_value());
	EXPECT_EQ(order.order->partner, k_Asked);
	EXPECT_EQ(mind.step, 3U);
}

TEST(CreatureFriendship, AFightIsAskedForThenFought)
{
	const auto agenda = creature_mind::FightCreature(k_Asked);
	ASSERT_EQ(agenda.size(), 4U);
	EXPECT_EQ(agenda[0].kind, Step::Kind::RequestPartner);
	EXPECT_EQ(agenda[0].request, RequestKind::Fight);
	EXPECT_EQ(agenda[0].object, k_Asked);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Demobilise);
	EXPECT_EQ(agenda[1].object, k_Asked);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Fight);
	EXPECT_EQ(agenda[2].object, k_Asked);
	EXPECT_TRUE(agenda[2].main);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Remobilise);
}

TEST(CreatureFightStep, TheFightStartsAsTheStepStartsAndIsDoneWhenTheMindThinksAgain)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, creature_mind::FightCreature(k_Asked));
	const creature_mind::Senses senses {.seconds = 0.1f};
	std::ignore = creature_mind::Think(mind, senses, NoRandom());
	std::ignore = creature_mind::Think(mind, senses, NoRandom());
	// the fight is asked for as its step starts, and the step waits
	const auto start = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(start.fight.has_value());
	EXPECT_EQ(*start.fight, k_Asked);
	EXPECT_EQ(mind.step, 2U);
	EXPECT_FALSE(mind.gaveUp);
	// the mind thinks again only once the fight is over: the step is done, and the other is let go
	const auto after = creature_mind::Think(mind, senses, NoRandom());
	EXPECT_FALSE(after.fight.has_value());
	EXPECT_EQ(mind.step, 3U);
	const auto letGo = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(letGo.demobilise.has_value());
	EXPECT_FALSE(letGo.demobilise->second);
}

TEST(CreatureFightStep, AFightWithNobodyGivesTheAgendaUp)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, {Step {.kind = Step::Kind::Fight, .main = true}});
	const auto commands = creature_mind::Think(mind, creature_mind::Senses {.seconds = 0.1f}, NoRandom());
	EXPECT_FALSE(commands.fight.has_value());
	EXPECT_TRUE(mind.gaveUp);
}

// A kissing point is a share of the way from the low kiss's point to the high kiss's, at the creature's size
TEST(CreatureKiss, AKissingPointIsBetweenTheLowAndTheHighKiss)
{
	const creature_kiss::KissPoints points {.high = {0.0f, 10.0f, -2.0f}, .low = {0.0f, 0.0f, -1.0f}};
	EXPECT_EQ(creature_kiss::PointAt(points, 0.5f, 2.0f), glm::vec3(0.0f, 10.0f, -3.0f));
	EXPECT_EQ(creature_kiss::PointAt(points, 0.0f, 1.0f), points.low);
	EXPECT_EQ(creature_kiss::PointAt(points, 1.0f, 1.0f), points.high);
}

// The taller of two creatures kisses at the share of its rise from its own low point that brings it down to the other's
// high point; the shorter kisses at its full height; and a taller one whose low point is above the other's high point
// can't kiss it, nor it the taller
TEST(CreatureKiss, TheTallerKissesDownToTheShorter)
{
	const creature_kiss::KissPoints tall {.high = {0.0f, 10.0f, -2.0f}, .low = {0.0f, 0.0f, -1.0f}};
	const creature_kiss::KissPoints shortOne {.high = {0.0f, 6.0f, -1.0f}, .low = {0.0f, 2.0f, -1.0f}};
	const auto tallShare = creature_kiss::KissShare(tall, 1.0f, shortOne, 1.0f);
	ASSERT_TRUE(tallShare.has_value());
	EXPECT_FLOAT_EQ(*tallShare, 0.6f);
	const auto shortShare = creature_kiss::KissShare(shortOne, 1.0f, tall, 1.0f);
	ASSERT_TRUE(shortShare.has_value());
	EXPECT_FLOAT_EQ(*shortShare, 1.0f);
	// how far ahead its lips are: 0.6 of its rise from (0, 0, -1) to (0, 10, -2)
	EXPECT_FLOAT_EQ(creature_kiss::KissingDistance(tall, 1.0f, shortOne, 1.0f), 1.6f);
	EXPECT_FLOAT_EQ(creature_kiss::KissingDistance(shortOne, 1.0f, tall, 1.0f), 1.0f);
	// sizes scale the points: at twice its size the short one is the taller, from 4 up to 12, down to 10
	EXPECT_FLOAT_EQ(*creature_kiss::KissShare(shortOne, 2.0f, tall, 1.0f), 0.75f);
	EXPECT_FLOAT_EQ(creature_kiss::KissingDistance(shortOne, 2.0f, tall, 1.0f), 2.0f);
	const creature_kiss::KissPoints highUp {.high = {0.0f, 10.0f, -1.0f}, .low = {0.0f, 7.0f, -1.0f}};
	EXPECT_FALSE(creature_kiss::KissShare(highUp, 1.0f, shortOne, 1.0f).has_value());
	EXPECT_FALSE(creature_kiss::KissShare(shortOne, 1.0f, highUp, 1.0f).has_value());
	EXPECT_FLOAT_EQ(creature_kiss::KissingDistance(shortOne, 1.0f, highUp, 1.0f), 0.0f);
}

// Two kissers each stand the mean of their kissing distances from the point halfway between them, on their own sides;
// two on the same point stand on it
TEST(CreatureKiss, KissersStandTheMeanDistanceFromTheirMiddle)
{
	const creature_kiss::KissPoints tall {.high = {0.0f, 10.0f, -2.0f}, .low = {0.0f, 0.0f, -1.0f}};
	const creature_kiss::KissPoints shortOne {.high = {0.0f, 6.0f, -1.0f}, .low = {0.0f, 2.0f, -1.0f}};
	EXPECT_FLOAT_EQ(creature_kiss::MeanKissingDistance(tall, 1.0f, shortOne, 1.0f), 1.3f);
	EXPECT_FLOAT_EQ(creature_kiss::MeanKissingDistance(shortOne, 1.0f, tall, 1.0f), 1.3f);
	const auto [self, other] = creature_kiss::KissPlaces({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f);
	EXPECT_FLOAT_EQ(self.x, 3.0f);
	EXPECT_FLOAT_EQ(other.x, 7.0f);
	EXPECT_FLOAT_EQ(self.y, 0.0f);
	const auto [same, sameOther] = creature_kiss::KissPlaces({4.0f, 5.0f}, {4.0f, 5.0f}, 2.0f);
	EXPECT_EQ(same, glm::vec2(4.0f, 5.0f));
	EXPECT_EQ(sameOther, glm::vec2(4.0f, 5.0f));
}

// Kissing a friend: it asks the friend to be friends, sends it puzzled to its place a tenth of a metre near, goes to its
// own with compassion, both face each other a tenth of a second fluttering their eyelids, the friend kisses it and it
// kisses the friend as the main step; half the time it then smiles at the friend ten seconds and shows it is happy
TEST(CreatureFriendship, KissingAFriendIsAJointAction)
{
	const creature_mind::Friends friends {
	    .self = k_Asker, .friendly = k_Asked, .selfPoint = {0.0f, 0.0f}, .friendPoint = {10.0f, 0.0f}, .height = 5.0f};
	std::vector<uint32_t> asked;
	const auto drawing = [&asked](uint32_t answer) {
		return creature_mind::Random([&asked, answer](uint32_t n) {
			asked.push_back(n);
			return answer;
		});
	};
	const auto agenda = creature_mind::KissFriend(friends, 2.0f, drawing(0));
	ASSERT_EQ(agenda.size(), 9U);
	EXPECT_EQ(agenda[0].kind, Step::Kind::RequestPartner);
	EXPECT_EQ(agenda[0].request, RequestKind::BeFriends);
	EXPECT_EQ(agenda[0].object, k_Asked);
	ASSERT_EQ(agenda[1].kind, Step::Kind::Order);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Puzzled);
	EXPECT_EQ(agenda[1].ordered->movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ(agenda[1].ordered->movement.point.x, 7.0f);
	EXPECT_FLOAT_EQ(agenda[1].ordered->movement.maxDistance, creature_mind::k_KissPlaceWithin);
	EXPECT_EQ(agenda[2].movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ(agenda[2].movement.point.x, 3.0f);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, creature_mind::k_KissPlaceWithin);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Compassion);
	ASSERT_EQ(agenda[3].kind, Step::Kind::Order);
	EXPECT_EQ(agenda[3].ordered->movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[3].ordered->movement.object, k_Asker);
	EXPECT_TRUE(agenda[3].ordered->flutterEyelids);
	EXPECT_FLOAT_EQ(agenda[3].ordered->seconds, creature_mind::k_FaceToKissSeconds);
	EXPECT_EQ(agenda[4].movement.object, k_Asked);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Compassion);
	EXPECT_TRUE(agenda[4].flutterEyelids);
	ASSERT_EQ(agenda[5].kind, Step::Kind::Order);
	EXPECT_EQ(agenda[5].ordered->kind, Step::Kind::Kiss);
	EXPECT_EQ(agenda[5].ordered->object, k_Asker);
	EXPECT_EQ(agenda[6].kind, Step::Kind::Kiss);
	EXPECT_EQ(agenda[6].object, k_Asked);
	EXPECT_TRUE(agenda[6].main);
	EXPECT_FLOAT_EQ(agenda[7].seconds, creature_mind::k_FaceAfterKissSeconds);
	EXPECT_EQ(agenda[7].face, creature_face::Cue::Smile);
	EXPECT_TRUE(agenda[7].flutterEyelids);
	EXPECT_EQ(agenda[8].animation, creature_layers::animations::k_Happy);
	ASSERT_EQ(asked.size(), 1U);
	EXPECT_EQ(asked.front(), 2U);
	// The other half: no smile after
	EXPECT_EQ(creature_mind::KissFriend(friends, 2.0f, drawing(1)).size(), 7U);
	// The plan action is the friend kiss's, done with a friend free for a joint action
	const auto* kiss = creature_plan_actions::For("KissFriend");
	ASSERT_NE(kiss, nullptr);
	EXPECT_EQ(kiss->build, creature_plan_actions::Build::KissFriend);
}

// The joint action stops when either agenda changes or comes to an end, but an agenda whose last step gave the partner
// an order stops it one turn later, so the partner starts that order first
TEST(CreatureFriendship, AJointActionThatEndsWithAnOrderStopsOnTheNextTurn)
{
	const JointFacts running {
	    .otherAgenda = false, .hadStepsLeft = true, .hasStepsLeft = true, .endedWithOrder = false, .endSeen = false};
	EXPECT_EQ(JointTurnOf(running), JointTurn::Continues);
	auto other = running;
	other.otherAgenda = true;
	EXPECT_EQ(JointTurnOf(other), JointTurn::Ends);
	// The last step was not an order: the joint action stops at once
	auto done = running;
	done.hasStepsLeft = false;
	EXPECT_EQ(JointTurnOf(done), JointTurn::Ends);
	// The last step gave an order: the partner gets one turn to start it
	done.endedWithOrder = true;
	EXPECT_EQ(JointTurnOf(done), JointTurn::EndsNextTurn);
	done.endSeen = true;
	EXPECT_EQ(JointTurnOf(done), JointTurn::Ends);
	// An agenda that had no steps left when the joint action noted it does not stop the joint action
	auto empty = running;
	empty.hadStepsLeft = false;
	empty.hasStepsLeft = false;
	EXPECT_EQ(JointTurnOf(empty), JointTurn::Continues);
}
