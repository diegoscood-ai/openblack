/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <gtest/gtest.h>

#include "Creature/CreatureGiving.h"

using namespace openblack;
using namespace openblack::creature_giving;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;

GiveFacts Friendly()
{
	return {.creatureHasPlayer = true, .giverHasPlayer = true, .friendly = true, .spellSeed = false, .creatureCanPickUp = true};
}
} // namespace

TEST(CreatureGiving, TheOfferLastsElevenTenthsOfASecondAfterLeavingTheCreature)
{
	Offer offer;
	EXPECT_FALSE(offer.Active());
	offer.Renew();
	EXPECT_EQ(offer.ms, 1100);
	EXPECT_FALSE(offer.RunDown(1000, false));
	EXPECT_TRUE(offer.Active());
	EXPECT_FALSE(offer.RunDown(100, false));
	// At exactly nothing left it is over, but runs out only once below nothing
	EXPECT_FALSE(offer.Active());
	offer.Renew();
	EXPECT_TRUE(offer.RunDown(1200, false));
	EXPECT_EQ(offer.ms, 0);
}

TEST(CreatureGiving, TheOfferWaitsWhileTheCreatureReaches)
{
	Offer offer;
	offer.Renew();
	EXPECT_FALSE(offer.RunDown(5000, true));
	EXPECT_EQ(offer.ms, 1100);
}

TEST(CreatureGiving, ThePalmTurnsOverAndASideHoldAQuarterTurnTheOtherWayForTheRightHand)
{
	Offer offer;
	offer.Renew();
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, false), k_Pi);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, true, offer, false), -k_Pi);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Side, false, offer, false), k_Pi / 2.0f);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Villager, true, offer, false), -k_Pi / 2.0f);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Tree, false, offer, false), k_Pi / 2.0f);
	EXPECT_FALSE(OfferTurn(HoldType::Magic, false, offer, false).has_value());
}

TEST(CreatureGiving, NearTheEndOfTheOfferItTurnsBackUnlessTheCreatureReaches)
{
	Offer offer {.ms = 299};
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, false), 0.0f);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, true), k_Pi);
	offer.ms = 300;
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, false), k_Pi);
	// Nearly over, even a hold that keeps its turn turns back
	offer.ms = 0;
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Magic, false, offer, false), 0.0f);
}

TEST(CreatureGiving, OnlyItsOwnOrAnAlliedPlayerMayGiveAndNeverASeed)
{
	EXPECT_EQ(MayGive(Friendly()), Verdict::Give);
	auto facts = Friendly();
	facts.friendly = false;
	EXPECT_EQ(MayGive(facts), Verdict::No);
	facts = Friendly();
	facts.creatureHasPlayer = false;
	EXPECT_EQ(MayGive(facts), Verdict::No);
	facts = Friendly();
	facts.spellSeed = true;
	EXPECT_EQ(MayGive(facts), Verdict::No);
	facts = Friendly();
	facts.creatureCanPickUp = false;
	EXPECT_EQ(MayGive(facts), Verdict::CantPickUp);
	// Another god's creature says nothing of what it can't pick up
	facts.friendly = false;
	EXPECT_EQ(MayGive(facts), Verdict::No);
}

TEST(CreatureGiving, TheCreaturesOwnPlayerOrAnAllyOfItsPlayerIsFriendly)
{
	using ecs::components::PlayerAlliances;
	EXPECT_TRUE(IsFriendly(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO, PlayerAlliances {}));
	EXPECT_FALSE(IsFriendly(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, PlayerAlliances {}));
	PlayerAlliances allied {};
	allied.strength.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 1.0f;
	EXPECT_TRUE(IsFriendly(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, allied));
	// any strength above 0 makes an ally; 0 or below none
	allied.strength.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 0.01f;
	EXPECT_TRUE(IsFriendly(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, allied));
	allied.strength.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 0.0f;
	EXPECT_FALSE(IsFriendly(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, allied));
	// an alliance with another player is not one with the giver
	allied.strength.at(static_cast<size_t>(PlayerNames::PLAYER_THREE)) = 1.0f;
	EXPECT_FALSE(IsFriendly(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, allied));
}

TEST(CreatureGiving, TheHandMeetsTheCreaturesHandAtTheMomentOfTaking)
{
	const glm::vec3 creatureHand {0.0f, 10.0f, 0.0f};
	const glm::vec3 target {10.0f, 20.0f, 0.0f};
	const auto atTake = DrawToCreature(creatureHand, target, 0.0f, 2.0f);
	ASSERT_TRUE(atTake.has_value());
	EXPECT_FLOAT_EQ(atTake->x, 0.0f);
	EXPECT_FLOAT_EQ(atTake->y, 8.0f);
	const auto halfway = DrawToCreature(creatureHand, target, -250.0f, 2.0f);
	ASSERT_TRUE(halfway.has_value());
	EXPECT_FLOAT_EQ(halfway->x, 5.0f);
	EXPECT_FLOAT_EQ(halfway->y, 14.0f);
	EXPECT_FALSE(DrawToCreature(creatureHand, target, 500.0f, 2.0f).has_value());
}

TEST(CreatureGiving, FifteenTimesTheCreaturesSizeAwayCallsTheGivingOff)
{
	EXPECT_FALSE(CallsOff({0.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}, 2.0f));
	EXPECT_TRUE(CallsOff({0.0f, 0.0f, 0.0f}, {30.1f, 0.0f, 0.0f}, 2.0f));
}

TEST(CreatureGiving, TheBestScoringDesireDecides)
{
	const GiftFacts facts {.curiosity = {.desire = 0.5f, .usefulness = 0.2f},
	                       .hunger = {.desire = 0.4f, .usefulness = 0.5f},
	                       .playWithPlayer = {.desire = 0.3f, .usefulness = 0.9f}};
	EXPECT_EQ(ChooseUse(facts).use, Use::Throw);
	auto hungry = facts;
	hungry.hunger.usefulness = 1.0f;
	EXPECT_EQ(ChooseUse(hungry).use, Use::Eat);
	EXPECT_FALSE(ChooseUse(hungry).remembersKind);
}

TEST(CreatureGiving, HungerDoesntCountForAKindItCantEat)
{
	const GiftFacts facts {
	    .curiosity = {.desire = 0.2f, .usefulness = 0.5f}, .hunger = {.desire = 0.9f, .usefulness = 1.0f}, .cantEatKind = true};
	EXPECT_EQ(ChooseUse(facts).use, Use::Examine);
}

TEST(CreatureGiving, WithNoDesireATenthStrongTheFirstOfAKindIsExaminedAndLaterOnesActedOnAsWhenLed)
{
	GiftFacts facts {.curiosity = {.desire = 0.09f, .usefulness = 1.0f}, .hunger = {.desire = 0.05f, .usefulness = 1.0f}};
	const auto first = ChooseUse(facts);
	EXPECT_EQ(first.use, Use::Examine);
	EXPECT_TRUE(first.remembersKind);
	facts.givenKindBefore = true;
	EXPECT_EQ(ChooseUse(facts).use, Use::AsLed);
	// A strong desire for which the thing is of no use at all finds no winner either
	const GiftFacts useless {.curiosity = {.desire = 0.9f, .usefulness = 0.0f}, .givenKindBefore = true};
	EXPECT_EQ(ChooseUse(useless).use, Use::AsLed);
}

// The hand's part, frame by frame, as the hand system runs it (the press itself gives: OnPress)
namespace
{
constexpr auto k_Creature = static_cast<entt::entity>(7);
constexpr auto k_Held = static_cast<entt::entity>(3);

OfferFacts Over(std::optional<entt::entity> under, std::optional<Verdict> verdict, int32_t frameMs = 16)
{
	return {.under = under, .verdict = verdict, .frameMs = frameMs, .hold = HoldType::Above};
}
} // namespace

TEST(CreatureGivingHand, OverACreatureThatWouldTakeItTheHandOffersAndTurnsItOver)
{
	HandGiving giving;
	EXPECT_FALSE(KeepOffer(giving, Over(k_Creature, Verdict::Give)));
	EXPECT_EQ(giving.Offered(), k_Creature);
	// The palm turns over, eased towards the turn afresh every frame over 0.4 s, so it settles rather than arriving
	for (int i = 0; i < 120; ++i)
	{
		KeepOffer(giving, Over(k_Creature, Verdict::Give));
	}
	EXPECT_NEAR(giving.turn.GetValue(), k_Pi, 1e-3f);
	// Off the creature the offer lasts 1.1 s, turning back once less than 0.3 s is left
	KeepOffer(giving, Over(std::nullopt, std::nullopt, 700));
	EXPECT_EQ(giving.Offered(), k_Creature);
	KeepOffer(giving, Over(std::nullopt, std::nullopt, 200));
	KeepOffer(giving, Over(std::nullopt, std::nullopt, 200));
	EXPECT_FALSE(giving.Offered().has_value());
}

TEST(CreatureGivingHand, APressWhileOfferingGivesItToTheCreatureOffered)
{
	HandGiving giving;
	KeepOffer(giving, Over(k_Creature, Verdict::Give));
	// The cursor has moved off it, but the offer stands, and it comes before what is under the hand
	KeepOffer(giving, Over(std::nullopt, std::nullopt));
	EXPECT_EQ(GiveTarget(giving, true, std::nullopt), k_Creature);
	EXPECT_EQ(GiveTarget(giving, true, k_Held), k_Creature);
	// Gone, what is under the hand is given to instead
	EXPECT_FALSE(GiveTarget(giving, false, std::nullopt).has_value());
	Given(giving, k_Creature);
	EXPECT_TRUE(giving.Reaching());
	EXPECT_EQ(giving.Offered(), k_Creature);
	// Given, a press gives it again to the same creature, whatever is under the hand
	EXPECT_EQ(GiveTarget(giving, false, std::nullopt), k_Creature);
	EXPECT_EQ(GiveTarget(giving, true, k_Held), k_Creature);
	// The giving and the offer last, whatever the creature does about it, while the creature is there; over another
	// creature that would take it, the giving goes on with that one
	KeepOffer(giving, {.givenStillThere = true, .frameMs = 5000, .hold = HoldType::Above});
	EXPECT_EQ(giving.Offered(), k_Creature);
	EXPECT_TRUE(giving.Reaching());
	constexpr auto k_Other = static_cast<entt::entity>(9);
	KeepOffer(giving,
	          {.givenStillThere = true, .under = k_Other, .verdict = Verdict::Give, .frameMs = 16, .hold = HoldType::Above});
	EXPECT_EQ(giving.Offered(), k_Other);
	EXPECT_EQ(GiveTarget(giving, true, std::nullopt), k_Other);
	// It is over once the creature is gone
	KeepOffer(giving, {.givenStillThere = false, .frameMs = 16, .hold = HoldType::Above});
	EXPECT_FALSE(giving.Reaching());
	// Taken from the hand, it offers nothing
	giving.Clear();
	EXPECT_FALSE(giving.Offered().has_value());
}

TEST(CreatureGivingHand, WhileTheCreatureReachesTheHandMeetsItsHandAtTheMomentOfTaking)
{
	HandGiving giving;
	KeepOffer(giving, Over(k_Creature, Verdict::Give));
	Given(giving, k_Creature);
	ReachFacts facts {.target = glm::vec3(0.0f),
	                  .takePoint = glm::vec3(5.0f, 3.0f, 0.0f),
	                  .creatureSize = 2.0f,
	                  .taking = k_Held,
	                  .msFromTake = 0.0f,
	                  .held = k_Held,
	                  .hang = 0.5f};
	// At the creature's hand, lowered by how far the held thing hangs
	const auto atTake = ReachFrame(giving, facts);
	EXPECT_FALSE(atTake.calledOff);
	EXPECT_NEAR(atTake.hand.x, 5.0f, 1e-4f);
	EXPECT_NEAR(atTake.hand.y, 2.5f, 1e-4f);
	// Half a second from the take it is where it should be
	facts.msFromTake = -500.0f;
	EXPECT_NEAR(ReachFrame(giving, facts).hand.x, 0.0f, 1e-4f);
	// Reaching for something else, it is where it should be
	facts.msFromTake = 0.0f;
	facts.taking = k_Creature;
	EXPECT_NEAR(ReachFrame(giving, facts).hand.x, 0.0f, 1e-4f);
}

TEST(CreatureGivingHand, MovingTheHandFifteenSizesFromTheReachingCreatureCallsItOff)
{
	HandGiving giving;
	KeepOffer(giving, Over(k_Creature, Verdict::Give));
	Given(giving, k_Creature);
	const ReachFacts facts {.target = glm::vec3(29.0f, 0.0f, 0.0f), .takePoint = glm::vec3(0.0f), .creatureSize = 2.0f};
	// It is measured from where the hand was drawn the frame before
	giving.lastHand = glm::vec3(29.0f, 0.0f, 0.0f);
	EXPECT_FALSE(ReachFrame(giving, facts).calledOff);
	giving.lastHand = glm::vec3(40.0f, 0.0f, 0.0f);
	EXPECT_TRUE(ReachFrame(giving, facts).calledOff);
	EXPECT_FALSE(giving.Offered().has_value());
	EXPECT_FALSE(giving.Reaching());
}

TEST(CreatureGivingHand, ACreatureThatCantPickItUpSaysSoAndIsNotOffered)
{
	HandGiving giving;
	EXPECT_TRUE(KeepOffer(giving, Over(k_Creature, Verdict::CantPickUp)));
	EXPECT_FALSE(giving.Offered().has_value());
	// Not offered, the press can only try the creature under the hand, which refuses it
	EXPECT_EQ(GiveTarget(giving, false, k_Creature), k_Creature);
	EXPECT_FALSE(KeepOffer(giving, Over(k_Creature, Verdict::No)));
	EXPECT_FALSE(giving.Offered().has_value());
}

TEST(CreatureGiving, ThePressItselfGivesAndAWaitingApplyUsesItUpWithNothingSent)
{
	// The original gives at the press, with no hold to wait for: to a creature that takes it, the Give packet at once
	EXPECT_EQ(OnPress(true, false), PressGive::Given);
	// with an apply packet still waiting to go out, nothing is sent, and the press does nothing else either
	EXPECT_EQ(OnPress(true, true), PressGive::UsedUp);
	// no creature takes it: the press is no give, and goes on to the apply and the release
	EXPECT_EQ(OnPress(false, false), PressGive::NotAGive);
	EXPECT_EQ(OnPress(false, true), PressGive::NotAGive);
}
