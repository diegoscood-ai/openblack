/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureThrow.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_mind;
using creature_object_actions::Thing;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
uint32_t KindActionsFirst(uint32_t /*range*/)
{
	return 0;
}

uint32_t KindActionsLast(uint32_t range)
{
	return range - 1;
}
} // namespace

// "force MyCreature CREATURE_HURL TinyStone with Trainer": with its hand empty it may show its anger, picks up the trainer,
// goes to where it can throw at the stone and throws the trainer at it
TEST(CreatureKindActions, HurlingPicksUpWhatToThrowThenThrowsItAtTheThing)
{
	const auto agenda = HurlAt(4, 9, {10.0f, 20.0f}, 15.0f, false, KindActionsFirst);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_Angry);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, 4u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[2].movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 15.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[3].order.point, glm::vec2(10.0f, 20.0f));
	// The throw finishes the action, which sees to whichever desire it served
	EXPECT_EQ(agenda[3].effect, Effect::None);
	ASSERT_TRUE(agenda[3].gaze.has_value());
	EXPECT_EQ(agenda[3].gaze->object, 9u);
	// Most of the time it doesn't show its anger first; with something in its hand already it throws that
	EXPECT_EQ(HurlAt(4, 9, {}, 15.0f, false, KindActionsLast).size(), 3u);
	const auto holding = HurlAt(4, 9, {}, 15.0f, true, KindActionsFirst);
	ASSERT_EQ(holding.size(), 2u);
	EXPECT_EQ(holding[0].movement.kind, Movement::Kind::ToThrowPosition);
}

TEST(CreatureKindActions, LivingThingsAreVillagersAnimalsBirdsAndCreatures)
{
	for (const auto thing : {Thing::Villager, Thing::Animal, Thing::Bird, Thing::Creature})
	{
		EXPECT_TRUE(creature_object_actions::IsLiving(thing));
	}
	for (const auto thing : {Thing::Tree, Thing::Rock, Thing::MobileObject, Thing::Fixture, Thing::Other})
	{
		EXPECT_FALSE(creature_object_actions::IsLiving(thing));
	}
}

TEST(CreatureKindActions, StonesKnockDownTheLivingAndWhatIsSetIntoTheLand)
{
	EXPECT_TRUE(creature_object_actions::CanBeStoned(Thing::Villager, false));
	EXPECT_FALSE(creature_object_actions::CanBeStoned(Thing::Villager, true));
	EXPECT_TRUE(creature_object_actions::CanBeStoned(Thing::Creature, false));
	for (const auto thing :
	     {Thing::Fixture, Thing::Rock, Thing::Bonfire, Thing::DeadTree, Thing::Static, Thing::MagicMushroom, Thing::Shield})
	{
		EXPECT_TRUE(creature_object_actions::CanBeStoned(thing, false));
	}
	for (const auto thing : {Thing::Tree, Thing::Lantern, Thing::Vortex, Thing::TeleportStone, Thing::MobileObject, Thing::Pot,
	                         Thing::Pile, Thing::Ball, Thing::Creed, Thing::Other})
	{
		EXPECT_FALSE(creature_object_actions::CanBeStoned(thing, false));
	}
}

TEST(CreatureKindActions, OnlyStaticsOtherThanToysDoDamageWhenThrown)
{
	for (const auto thing : {Thing::Rock, Thing::Bonfire, Thing::DeadTree, Thing::Static, Thing::Vortex, Thing::TeleportStone})
	{
		EXPECT_TRUE(creature_object_actions::DoesThrowingDamage(thing, false));
		EXPECT_FALSE(creature_object_actions::DoesThrowingDamage(thing, true));
	}
	for (const auto thing : {Thing::Villager, Thing::Tree, Thing::MobileObject, Thing::Ball, Thing::Lantern, Thing::Shield})
	{
		EXPECT_FALSE(creature_object_actions::DoesThrowingDamage(thing, false));
	}
}

TEST(CreatureKindActions, WhatLiesAboutStaticsAndTreesGoIntoTheSea)
{
	for (const auto thing : {Thing::MobileObject, Thing::Ball, Thing::Creed, Thing::Pot, Thing::Pile, Thing::Rock,
	                         Thing::DeadTree, Thing::Static, Thing::Tree})
	{
		EXPECT_TRUE(creature_object_actions::CanBeThrownInTheSea(thing));
	}
	for (const auto thing :
	     {Thing::Villager, Thing::Animal, Thing::Creature, Thing::MagicMushroom, Thing::Fixture, Thing::Lantern, Thing::Shield})
	{
		EXPECT_FALSE(creature_object_actions::CanBeThrownInTheSea(thing));
	}
}

TEST(CreatureKindActions, AnythingButACreatureAVortexOrATeleportStoneIsLookedAt)
{
	for (const auto thing : {Thing::Other, Thing::Villager, Thing::Animal, Thing::Bird, Thing::Ball, Thing::Creed,
	                         Thing::MobileObject, Thing::Pot, Thing::Pile, Thing::Rock, Thing::Bonfire, Thing::DeadTree,
	                         Thing::Tree, Thing::MagicMushroom, Thing::Static, Thing::Lantern, Thing::Shield, Thing::Fixture})
	{
		EXPECT_TRUE(creature_object_actions::CanBeExaminedByLooking(thing));
	}
	for (const auto thing : {Thing::Creature, Thing::Vortex, Thing::TeleportStone})
	{
		EXPECT_FALSE(creature_object_actions::CanBeExaminedByLooking(thing));
	}
	// the action asks for such a thing, not for anything at all
	const auto* looking = plan_actions::For("ExamineByLooking");
	ASSERT_NE(looking, nullptr);
	EXPECT_EQ(looking->target, plan_actions::Target::Examinable);
}

TEST(CreatureKindActions, ACatchIsOfAThingItCouldPickUp)
{
	// only ever forced on it, never one of the planner's own actions
	EXPECT_EQ(plan_actions::For("Catch"), nullptr);
	const auto* catching = plan_actions::ForcedFor("Catch");
	ASSERT_NE(catching, nullptr);
	EXPECT_EQ(catching->target, plan_actions::Target::Pickable);
	EXPECT_EQ(catching->build, plan_actions::Build::Catch);
}

TEST(CreatureKindActions, WhatIsBuiltIntoTheLandIsNeverPickedUp)
{
	EXPECT_EQ(creature_object_actions::RuleOf(Thing::Fixture), creature_object_actions::KindRule::Never);
	EXPECT_FALSE(creature_object_actions::Holdable(Thing::Fixture));
}

TEST(CreatureKindActions, TheThingUsedIsTheNearestOtherThanTheGoal)
{
	const std::array objects {
	    creature_planner::ObjectCandidate {.id = 1, .distance = 10.0f},
	    creature_planner::ObjectCandidate {.id = 2, .distance = 5.0f},
	    creature_planner::ObjectCandidate {.id = 3, .distance = 5.0f},
	};
	EXPECT_EQ(creature_planner::ChooseUsed(objects, 0.4f, std::nullopt), std::optional(2u));
	// Never the goal; of equals the first
	EXPECT_EQ(creature_planner::ChooseUsed(objects, 0.4f, 2u), std::optional(3u));
	// Something held comes first however far
	const std::array held {
	    creature_planner::ObjectCandidate {.id = 1, .distance = 1.0f},
	    creature_planner::ObjectCandidate {.id = 2, .distance = 150.0f, .held = true},
	};
	EXPECT_EQ(creature_planner::ChooseUsed(held, 0.4f, std::nullopt), std::optional(2u));
	// What the distance weighs fully against counts for nothing
	const std::array far {creature_planner::ObjectCandidate {.id = 1, .distance = 250.0f}};
	EXPECT_FALSE(creature_planner::ChooseUsed(far, 1.0f, std::nullopt).has_value());
	EXPECT_FALSE(creature_planner::ChooseUsed({}, 0.4f, std::nullopt).has_value());
}

TEST(CreatureKindActions, GivingWoodPullsUpTheTreeAndThrowsItIntoTheStore)
{
	const auto agenda = GiveWoodToStore(TreeToTake {.tree = 7, .point = {3.0f, 4.0f}, .radius = 2.0f}, 9, 5.0f, 15.0f);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(3.0f, 4.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, (2.0f + 5.0f) * 1.2f);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, 7u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[2].movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 15.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[3].seconds, 0.1f);
	EXPECT_EQ(agenda[3].effect, Effect::Completed);
	EXPECT_EQ(agenda[4].order.kind, ObjectOrder::Kind::ThrowInStore);
	EXPECT_EQ(agenda[4].order.object, 9u);
	// Holding a tree, it gives that
	EXPECT_EQ(GiveWoodToStore(std::nullopt, 9, 5.0f, 15.0f).size(), 3u);
}

TEST(CreatureKindActions, PlantingATreePutsItDownRightByTheThing)
{
	const auto agenda = PlantTreeBy(7u, 9u);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[0].order.object, 7u);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(agenda[1].movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 0.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::PutDown);
	EXPECT_EQ(PlantTreeBy(std::nullopt, 9u).size(), 3u);
}

TEST(CreatureKindActions, ThrowingToImpressTakesAnythingButTheLivingThatIsntTooTall)
{
	EXPECT_TRUE(creature_object_actions::CanBeThrownToImpress(Thing::Rock, 5.0f, 100.0f));
	EXPECT_TRUE(creature_object_actions::CanBeThrownToImpress(Thing::Tree, 63.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::Tree, 64.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::Villager, 1.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::Vortex, 1.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::TeleportStone, 1.0f, 100.0f));
}

TEST(CreatureKindActions, PractisingAThrowThrowsOffToOneSide)
{
	const auto agenda = PracticeThrow(7u, {100.0f, 200.0f}, 10.0f);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[0].order.object, 7u);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[1].order.point, glm::vec2(130.0f, 230.0f));
	// Holding something, it throws that
	EXPECT_EQ(PracticeThrow(std::nullopt, {0.0f, 0.0f}, 10.0f).size(), 1u);
}

// Where the head looks: at a thing it walks to pick up, at the store it goes to throw into and faces, at the thing it
// plants by, and on the way to where it throws in practice; not at a tree it walks up to
TEST(CreatureKindActions, TheKindActionsLookWhereTheGameLooks)
{
	const auto wood = GiveWoodToStore(TreeToTake {.tree = 7, .point = {3.0f, 4.0f}, .radius = 2.0f}, 9, 5.0f, 15.0f);
	ASSERT_EQ(wood.size(), 5u);
	EXPECT_FALSE(wood[0].gaze.has_value());
	ASSERT_TRUE(wood[1].gaze.has_value());
	EXPECT_EQ(wood[1].gaze->object, 7u);
	EXPECT_TRUE(wood[1].gaze->goingTowards);
	for (const size_t i : {2u, 3u})
	{
		ASSERT_TRUE(wood[i].gaze.has_value());
		EXPECT_EQ(wood[i].gaze->object, 9u);
		EXPECT_FALSE(wood[i].gaze->goingTowards);
	}
	EXPECT_FALSE(wood[4].gaze.has_value());
	const auto plant = PlantTreeBy(7u, 9u);
	ASSERT_EQ(plant.size(), 4u);
	ASSERT_TRUE(plant[1].gaze.has_value());
	EXPECT_EQ(plant[1].gaze->object, 9u);
	EXPECT_TRUE(plant[1].gaze->goingTowards);
	ASSERT_TRUE(plant[2].gaze.has_value());
	EXPECT_FALSE(plant[2].gaze->goingTowards);
	EXPECT_FALSE(plant[3].gaze.has_value());
	const auto practice = PracticeThrow(7u, {100.0f, 200.0f}, 10.0f);
	ASSERT_EQ(practice.size(), 2u);
	ASSERT_TRUE(practice[0].gaze.has_value());
	EXPECT_EQ(practice[0].gaze->object, 7u);
	ASSERT_TRUE(practice[1].gaze.has_value());
	EXPECT_EQ(practice[1].gaze->point, glm::vec3(130.0f, 0.0f, 230.0f));
	EXPECT_TRUE(practice[1].gaze->goingTowards);
	const auto high = GetHigh(7u, 7u);
	ASSERT_EQ(high.size(), 2u);
	ASSERT_TRUE(high[0].gaze.has_value());
	EXPECT_TRUE(high[0].gaze->goingTowards);
	EXPECT_FALSE(high[1].gaze.has_value());
}

TEST(CreatureKindActions, ThrowingAToyAtAThing)
{
	const auto agenda = ThrowToyAt(4u, 9u, {10.0f, 20.0f}, 15.0f, KindActionsFirst);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_Happy);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, 4u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[3].order.object, 9u);
	EXPECT_EQ(agenda[4].animation, creature_layers::animations::k_Happy);
	EXPECT_EQ(ThrowToyAt(4u, 9u, {10.0f, 20.0f}, 15.0f, KindActionsLast).size(), 3u);
}

TEST(CreatureKindActions, GettingHighEatsTheThingPickedUp)
{
	const auto agenda = GetHigh(7u, 7u);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda[1].effect, Effect::Eat);
	EXPECT_EQ(GetHigh(std::nullopt, 7u).size(), 1u);
}

TEST(CreatureKindActions, BeingHighWithoutSittingDown)
{
	bool looked = false;
	const auto agenda = BecomeHigh(
	    [&looked]() {
		    looked = true;
		    return std::optional<HighSeat> {};
	    },
	    KindActionsLast, [](float) { return 0.0f; });
	EXPECT_FALSE(looked);
	ASSERT_EQ(agenda.size(), 11u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::LookAtCamera);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 3.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Overlay);
	EXPECT_EQ(agenda[1].animation, creature_layers::animations::k_Talk);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 2.0f);
	EXPECT_EQ(agenda[2].heldEyes, Eyes::Stoned);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[3].seconds, 3.1f);
	EXPECT_EQ(agenda[4].animation, k_HighDazed);
	EXPECT_EQ(agenda[5].animation, k_HighConfused);
	EXPECT_FLOAT_EQ(agenda[6].seconds, 1.1f);
	EXPECT_EQ(agenda[7].animation, k_HighConfused);
	EXPECT_EQ(agenda[8].kind, Step::Kind::LookAtCamera);
	EXPECT_EQ(agenda[9].kind, Step::Kind::Overlay);
	EXPECT_EQ(agenda[10].animation, k_HighDazed);
}

TEST(CreatureKindActions, BeingHighSittingDownWhereItIsClear)
{
	const auto agenda = BecomeHigh(
	    []() {
		    return std::optional(HighSeat {.place = {5.0f, 6.0f}, .radius = 3.0f});
	    },
	    KindActionsFirst, [](float most) { return most; });
	ASSERT_EQ(agenda.size(), 18u);
	EXPECT_EQ(agenda[5].animation, creature_layers::animations::k_Happy);
	EXPECT_EQ(agenda[8].animation, k_HighDazedAgain);
	EXPECT_EQ(agenda[10].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[10].movement.point, glm::vec2(5.0f, 6.0f));
	EXPECT_FLOAT_EQ(agenda[10].movement.maxDistance, 3.0f);
	EXPECT_EQ(agenda[11].kind, Step::Kind::Static);
	EXPECT_FLOAT_EQ(agenda[11].seconds, 15.0f);
	EXPECT_EQ(agenda[11].sequence[1], creature_layers::animations::k_Sit);
	EXPECT_EQ(agenda[11].face, creature_face::Cue::Sad);
	EXPECT_EQ(agenda[12].animation, k_HighSitDazed);
	EXPECT_EQ(agenda[13].animation, k_HighFrightened);
	EXPECT_EQ(agenda[13].heldEyes, Eyes::Wide);
	EXPECT_EQ(agenda[14].animation, k_HighSitDazed);
	EXPECT_EQ(agenda.back().animation, k_HighDazed);
}

// The sit lasts ten seconds and a float draw over five more, drawn as the game draws it: over five at once, which
// rounds otherwise than five times a draw over one (seed 8: 0x3FC625C6 against 0x3FC625C7)
TEST(CreatureKindActions, TheHighSitDrawsItsExtraSecondsOverTheirRange)
{
	std::vector<float> asked;
	const auto agenda = BecomeHigh(
	    []() {
		    return std::optional(HighSeat {.place = {5.0f, 6.0f}, .radius = 3.0f});
	    },
	    KindActionsFirst,
	    [&asked](float most) {
		    asked.push_back(most);
		    return 1.5480278f;
	    });
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_EQ(asked[0], k_HighSitExtraSeconds);
	EXPECT_EQ(agenda[11].seconds, k_HighSitSeconds + 1.5480278f);
	uint32_t overFive = 8;
	uint32_t overOne = 8;
	EXPECT_EQ(std::bit_cast<uint32_t>(game_random::FloatRand(5.0f, overFive)), 0x3FC625C6u);
	EXPECT_EQ(std::bit_cast<uint32_t>(game_random::FloatRand(1.0f, overOne) * 5.0f), 0x3FC625C7u);
}

TEST(CreatureKindActions, GivingWoodWithNoTreeIdles)
{
	const auto* give = plan_actions::For("GiveWoodFromTreeToStoragePit");
	ASSERT_NE(give, nullptr);
	// only to a storage pit that is fully built, as the game's own test of the pit asks
	EXPECT_EQ(give->target, plan_actions::Target::BuiltStoragePit);
	EXPECT_EQ(give->use, plan_actions::Target::PickableTree);
	const auto idle = plan_actions::Agenda(*give, 9u, {0.0f, 0.0f}, {}, KindActionsFirst);
	ASSERT_TRUE(idle.has_value());
	EXPECT_EQ(idle->front().kind, Step::Kind::Wait);
	plan_actions::Situation holding;
	holding.holdingTree = true;
	const auto given = plan_actions::Agenda(*give, 9u, {0.0f, 0.0f}, holding, KindActionsFirst);
	ASSERT_TRUE(given.has_value());
	EXPECT_EQ(given->back().order.kind, ObjectOrder::Kind::ThrowInStore);
}

TEST(CreatureKindActions, PlantingNeedsATreeToPlant)
{
	const auto* plant = plan_actions::For("PlantTree");
	ASSERT_NE(plant, nullptr);
	EXPECT_EQ(plant->use, plan_actions::Target::PlantableTree);
	EXPECT_TRUE(plant->notWhatItHolds);
	// Nothing to plant, nothing done
	EXPECT_FALSE(plan_actions::Agenda(*plant, 9u, {0.0f, 0.0f}, {}, KindActionsFirst).has_value());
	plan_actions::Situation with;
	with.instrument = 7u;
	const auto agenda = plan_actions::Agenda(*plant, 9u, {0.0f, 0.0f}, with, KindActionsFirst);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->front().order.object, 7u);
}

TEST(CreatureKindActions, PractisingAndThrowingAroundPickWhatTheyThrow)
{
	const auto* around = plan_actions::For("ThrowAround");
	ASSERT_NE(around, nullptr);
	EXPECT_EQ(around->target, plan_actions::Target::AnyLiving);
	EXPECT_EQ(around->build, plan_actions::Build::PracticeThrow);
	const auto* practice = plan_actions::For("PracticeThrow");
	ASSERT_NE(practice, nullptr);
	EXPECT_EQ(practice->target, plan_actions::Target::Pickable);
	// Holding something, it throws that
	plan_actions::Situation holding;
	holding.held = 3u;
	const auto agenda = plan_actions::Agenda(*practice, 9u, {0.0f, 0.0f}, holding, KindActionsFirst);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->size(), 1u);
}

TEST(CreatureKindActions, HurlingAndThrowingToImpressNeedSomethingToThrow)
{
	for (const auto* name : {"Hurl", "ThrowToImpress"})
	{
		const auto* hurl = plan_actions::For(name);
		ASSERT_NE(hurl, nullptr);
		EXPECT_TRUE(hurl->notWhatItHolds);
		EXPECT_FALSE(plan_actions::Agenda(*hurl, 9u, {0.0f, 0.0f}, {}, KindActionsFirst).has_value());
		plan_actions::Situation with;
		with.instrument = 4u;
		const auto agenda = plan_actions::Agenda(*hurl, 9u, {0.0f, 0.0f}, with, KindActionsFirst);
		ASSERT_TRUE(agenda.has_value());
		EXPECT_EQ(agenda->back().order.kind, ObjectOrder::Kind::Throw);
		// Holding what it throws, it doesn't pick it up again
		with.held = 4u;
		const auto holding = plan_actions::Agenda(*hurl, 9u, {0.0f, 0.0f}, with, KindActionsFirst);
		ASSERT_TRUE(holding.has_value());
		EXPECT_EQ(holding->size(), 2u);
		// Holding something else, it picks nothing up either (and so shows no anger first): it throws what it holds
		with.held = 6u;
		const auto other = plan_actions::Agenda(*hurl, 9u, {0.0f, 0.0f}, with, KindActionsFirst);
		ASSERT_TRUE(other.has_value());
		ASSERT_EQ(other->size(), 2u);
		EXPECT_EQ(other->front().movement.kind, Movement::Kind::ToThrowPosition);
	}
	EXPECT_EQ(plan_actions::For("Hurl")->target, plan_actions::Target::Stonable);
	EXPECT_EQ(plan_actions::For("Hurl")->use, plan_actions::Target::ThrowingDamage);
	EXPECT_EQ(plan_actions::For("ThrowToImpress")->use, plan_actions::Target::ThrowableToImpress);
}

TEST(CreatureKindActions, ThrowingIntoTheSeaChoosesWhatMayGoInTheSea)
{
	const auto* sea = plan_actions::For("ThrowInTheSea");
	ASSERT_NE(sea, nullptr);
	EXPECT_EQ(sea->target, plan_actions::Target::SeaThrowable);
	EXPECT_EQ(sea->build, plan_actions::Build::ThrowInTheSea);
}

TEST(CreatureKindActions, BallGamesArePlayedWithToys)
{
	const auto* throwBall = plan_actions::For("ThrowBallAtObject");
	ASSERT_NE(throwBall, nullptr);
	EXPECT_EQ(throwBall->target, plan_actions::Target::Anything);
	EXPECT_EQ(throwBall->use, plan_actions::Target::Toy);
	const auto* kick = plan_actions::For("KickBallAround");
	ASSERT_NE(kick, nullptr);
	EXPECT_EQ(kick->target, plan_actions::Target::ToyBall);
}

TEST(CreatureKindActions, ThePlayersThingsAreOnesItMayPickUp)
{
	// What it throws at the camera, or shows the player, is chosen by the planner too, not only named by a script
	const auto* camera = plan_actions::For("ThrowAtCamera");
	ASSERT_NE(camera, nullptr);
	EXPECT_EQ(camera->target, plan_actions::Target::Pickable);
	EXPECT_EQ(camera->use, plan_actions::Target::Pickable);
	const auto* show = plan_actions::For("ShowPlayerAnObject");
	ASSERT_NE(show, nullptr);
	EXPECT_EQ(show->target, plan_actions::Target::None);
	EXPECT_EQ(show->use, plan_actions::Target::Pickable);
	// With the hand empty it picks the thing up first; holding anything, it throws or shows what it holds
	plan_actions::Situation situation;
	situation.camera = glm::vec2(10.0f, 20.0f);
	situation.hasPlayer = true;
	situation.instrument = 4u;
	for (const auto* action : {camera, show})
	{
		situation.held.reset();
		const auto empty = plan_actions::Agenda(*action, 9u, {0.0f, 0.0f}, situation, KindActionsFirst);
		ASSERT_TRUE(empty.has_value());
		EXPECT_EQ(empty->front().order.kind, ObjectOrder::Kind::PickUp);
		situation.held = 6u;
		const auto holding = plan_actions::Agenda(*action, 9u, {0.0f, 0.0f}, situation, KindActionsFirst);
		ASSERT_TRUE(holding.has_value());
		EXPECT_EQ(holding->size(), empty->size() - 1);
		EXPECT_TRUE(std::ranges::none_of(*holding, [](const auto& step) {
			return step.kind == Step::Kind::Object && step.order.kind == ObjectOrder::Kind::PickUp;
		}));
	}
}

TEST(CreatureKindActions, GettingHighEatsWhatItPicksUpUnlessAMushroomIsHeld)
{
	const auto* high = plan_actions::For("GetHigh");
	ASSERT_NE(high, nullptr);
	// only ever a mushroom, as the desire to get high accepts nothing else
	EXPECT_EQ(high->target, plan_actions::Target::Mushroom);
	EXPECT_EQ(high->build, plan_actions::Build::GetHigh);
	const auto picking = plan_actions::Agenda(*high, 7u, {0.0f, 0.0f}, {}, KindActionsFirst);
	ASSERT_TRUE(picking.has_value());
	EXPECT_EQ(picking->size(), 2u);
	plan_actions::Situation holding;
	holding.holdingMushroom = true;
	const auto eating = plan_actions::Agenda(*high, 7u, {0.0f, 0.0f}, holding, KindActionsFirst);
	ASSERT_TRUE(eating.has_value());
	EXPECT_EQ(eating->size(), 1u);
}

// An action's eyes are held as it plays and look as usual again as it ends; other steps leave the eyes as they are
TEST(CreatureKindActions, HeldEyesLastWhileTheActionPlays)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Action, .animation = 1, .heldEyes = Eyes::Stoned}});
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, KindActionsFirst);
	EXPECT_EQ(commands.eyes, Eyes::Stoned);
	senses.bodyBusy = true;
	commands = Think(mind, senses, KindActionsFirst);
	EXPECT_EQ(commands.eyes, Eyes::Unchanged);
	senses.bodyBusy = false;
	commands = Think(mind, senses, KindActionsFirst);
	EXPECT_EQ(commands.eyes, Eyes::Normal);
	EXPECT_EQ(mind.step, 1u);
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Wait, .seconds = 0.1f, .heldEyes = Eyes::Stoned}});
	commands = Think(mind, senses, KindActionsFirst);
	EXPECT_EQ(commands.eyes, Eyes::Unchanged);
}

TEST(CreatureKindActions, KickingABallAboutKicksItTwiceAtTheLeast)
{
	// Every chance comes up nought: it points after each kick, kicks twice and jumps for joy
	const auto agenda = KickBallAround(7, KindActionsFirst);
	ASSERT_EQ(agenda.size(), 11u);
	for (size_t kick = 0; kick < 2; ++kick)
	{
		const auto* steps = &agenda[kick * 5];
		EXPECT_EQ(steps[0].movement.kind, Movement::Kind::GoNearObject);
		EXPECT_EQ(steps[0].movement.object, 7u);
		EXPECT_FLOAT_EQ(steps[0].movement.maxDistance, 0.0f);
		EXPECT_EQ(steps[1].movement.kind, Movement::Kind::TurnToFaceObject);
		EXPECT_FLOAT_EQ(steps[1].seconds, 0.1f);
		EXPECT_EQ(steps[2].movement.kind, Movement::Kind::GoNearObject);
		EXPECT_EQ(steps[3].order.kind, ObjectOrder::Kind::Kick);
		EXPECT_EQ(steps[3].order.object, 7u);
		EXPECT_EQ(steps[4].order.kind, ObjectOrder::Kind::PointAt);
		EXPECT_EQ(steps[4].order.object, 7u);
		EXPECT_EQ(steps[4].order.seconds, 1.0f);
	}
	EXPECT_EQ(agenda[10].animation, creature_layers::animations::k_Happy);
}

TEST(CreatureKindActions, KickingATreeOrAThingKicksItAndAStompKnocksAtIt)
{
	// The kicks walk right up to the thing, face it a tenth of a second and kick it, as a ball is kicked; the stomp is
	// the knock-down
	for (const auto* name : {"KickTree", "Kick"})
	{
		const auto* kick = plan_actions::For(name);
		ASSERT_NE(kick, nullptr) << name;
		EXPECT_EQ(kick->build, plan_actions::Build::Kick) << name;
	}
	EXPECT_EQ(plan_actions::For("KickTree")->target, plan_actions::Target::Tree);
	const auto* stomp = plan_actions::For("Stomp");
	ASSERT_NE(stomp, nullptr);
	EXPECT_EQ(stomp->build, plan_actions::Build::Destroy);
	const auto agenda = plan_actions::Agenda(*plan_actions::For("KickTree"), 9u, {0.0f, 0.0f}, {}, KindActionsFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3u);
	EXPECT_EQ((*agenda)[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ((*agenda)[0].movement.object, 9u);
	EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 0.0f);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*agenda)[1].movement.object, 9u);
	EXPECT_FLOAT_EQ((*agenda)[1].seconds, 0.1f);
	EXPECT_EQ((*agenda)[2].kind, Step::Kind::Object);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::Kick);
	EXPECT_EQ((*agenda)[2].order.object, 9u);
	for (const auto& step : *agenda)
	{
		ASSERT_TRUE(step.gaze.has_value());
		EXPECT_EQ(step.gaze->object, std::optional(9u));
		EXPECT_EQ(step.face, creature_face::Cue::Anger);
	}
}

TEST(CreatureKindActions, KickingABallAboutWaitsAndDrawsTheCountAfreshEachKick)
{
	// Answers in turn: no point (1), wait 4 + 5 turns, count 2 + 2 (go on); no point, wait 4, count 2 + 0 (stop at 2);
	// no jump
	const std::array<uint32_t, 7> answers {1, 5, 2, 1, 0, 0, 1};
	size_t next = 0;
	const auto agenda = KickBallAround(3, [&](uint32_t) { return answers.at(next++); });
	ASSERT_EQ(agenda.size(), 10u);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[4].seconds, 0.9f);
	EXPECT_EQ(agenda[9].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[9].seconds, 0.4f);
	EXPECT_EQ(next, answers.size());
}

TEST(CreatureKindActions, AKickFliesHalfUpHalfAheadOnFlatLand)
{
	const auto lean = creature_object_actions::LeanOf(5.0f, 5.0f, 5.0f, 5.0f, 2.0f);
	EXPECT_EQ(lean, glm::vec2(0.0f));
	const auto velocity = creature_object_actions::KickVelocity({0.0f, -1.0f}, lean, 2.0f);
	const float component = 90.0f / std::sqrt(2.0f);
	EXPECT_NEAR(velocity.x, 0.0f, 1e-4f);
	EXPECT_NEAR(velocity.y, component, 1e-3f);
	EXPECT_NEAR(velocity.z, -component, 1e-3f);
}

TEST(CreatureKindActions, AKickTipsUpWithTheLandAheadRisingAndTheLeanIsCapped)
{
	// The land rises 1 over 4 along x: 1 x 0.6 / 4 = 0.15; along z it rises 10, capped at 0.3
	const auto lean = creature_object_actions::LeanOf(6.0f, 5.0f, 15.0f, 5.0f, 2.0f);
	EXPECT_NEAR(lean.x, 0.15f, 1e-6f);
	EXPECT_NEAR(lean.y, 0.3f, 1e-6f);
	EXPECT_NEAR(creature_object_actions::LeanOf(0.0f, 10.0f, 0.0f, 0.0f, 1.0f).x, -0.3f, 1e-6f);
	// A body of no radius on flat land leans the lowest both ways, as the body's placement does
	EXPECT_EQ(creature_object_actions::LeanOf(0.0f, 0.0f, 0.0f, 0.0f, 0.0f), glm::vec2(-0.3f));
	// Facing +x, the up part is 1 + 0.15
	const auto velocity = creature_object_actions::KickVelocity({1.0f, 0.0f}, lean, 1.0f);
	const auto length = std::sqrt(1.0f + (1.15f * 1.15f));
	EXPECT_NEAR(velocity.x, 45.0f / length, 1e-3f);
	EXPECT_NEAR(velocity.y, 45.0f * 1.15f / length, 1e-3f);
	EXPECT_NEAR(velocity.z, 0.0f, 1e-4f);
}

TEST(CreatureKindActions, WhatACreatureSetsFlyingIsItsPlayersThrow)
{
	// The game hands the physics the interface of the creature's player: any player's, the local one or not
	EXPECT_EQ(creature_object_actions::ThrowerPlayerOf(PlayerNames::PLAYER_ONE), std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(creature_object_actions::ThrowerPlayerOf(PlayerNames::PLAYER_TWO), std::optional(PlayerNames::PLAYER_TWO));
	// a creature of no player's throws for no one
	EXPECT_FALSE(creature_object_actions::ThrowerPlayerOf(PlayerNames::NEUTRAL).has_value());
}

TEST(CreatureKindActions, AKickOnAHomeStrikesTwiceLowOnItHalfWayToTheFoot)
{
	using creature_object_actions::HomeKickBlows;
	EXPECT_EQ(creature_object_actions::HomeKickCentre({0.0f, 2.0f, 0.0f}, {4.0f, 6.0f, -8.0f}), glm::vec3(2.0f, 4.0f, -4.0f));
	// A creature of size 2 reaches 7.5 above the land; a home of radius 5 makes each blow 3 wide. The first blow goes the
	// kick's way at twice it, at half its height above the land; the second, unturned (a draw of 5), straight ahead along
	// the land on the land
	const glm::vec3 way {0.0f, 0.6f, -0.8f};
	const auto blows = HomeKickBlows({10.0f, 5.0f, 20.0f}, 4.0f, way, 0.0f, 2.0f, 5.0f, 5u, 50u, 0u);
	EXPECT_EQ(blows.firstPoint, glm::vec3(10.0f, 4.5f, 20.0f));
	EXPECT_EQ(blows.secondPoint, glm::vec3(10.0f, 4.0f, 20.0f));
	EXPECT_EQ(blows.firstVelocity, way * 2.0f);
	EXPECT_EQ(blows.secondVelocity, glm::vec3(0.0f, 0.0f, -2.0f));
	EXPECT_EQ(blows.radius, 3.0f);
	// Below the land the point is raised to it; high above, the blows go no higher than the reach; a wide home's blows
	// are as wide as the reach
	const auto low = HomeKickBlows({10.0f, 1.0f, 20.0f}, 4.0f, way, 0.0f, 2.0f, 5.0f, 5u, 99u, 99u);
	EXPECT_EQ(low.firstPoint.y, 4.0f);
	const auto high = HomeKickBlows({10.0f, 100.0f, 20.0f}, 4.0f, way, 0.0f, 2.0f, 20.0f, 5u, 50u, 50u);
	EXPECT_EQ(high.firstPoint.y, 7.75f);
	EXPECT_EQ(high.radius, 7.5f);
	// The game's heading faces sin, -cos of it: a quarter turn faces +x; a draw of 10 turns the second blow an eighth
	// of a turn on
	const auto turned = HomeKickBlows({0.0f, 0.0f, 0.0f}, 0.0f, way, 1.5707964f, 2.0f, 5.0f, 5u, 0u, 0u);
	EXPECT_NEAR(turned.secondVelocity.x, 2.0f, 1e-6f);
	EXPECT_NEAR(turned.secondVelocity.z, 0.0f, 1e-6f);
	const auto eighth = HomeKickBlows({0.0f, 0.0f, 0.0f}, 0.0f, way, 0.0f, 2.0f, 5.0f, 10u, 0u, 0u);
	EXPECT_NEAR(eighth.secondVelocity.x, std::sqrt(2.0f), 1e-5f);
	EXPECT_NEAR(eighth.secondVelocity.z, -std::sqrt(2.0f), 1e-5f);
	// A draw below five is taken unsigned: not a small turn back, but a vast angle, the blow still along the land at
	// twice a unit
	const auto vast = HomeKickBlows({0.0f, 0.0f, 0.0f}, 0.0f, way, 0.0f, 2.0f, 5.0f, 4u, 0u, 0u);
	const glm::vec2 small {2.0f * std::sin(-0.1f * 1.5707964f), -2.0f * std::cos(-0.1f * 1.5707964f)};
	EXPECT_GT(glm::distance(glm::vec2(vast.secondVelocity.x, vast.secondVelocity.z), small), 0.01f);
	EXPECT_EQ(vast.secondVelocity.y, 0.0f);
	EXPECT_NEAR(glm::length(vast.secondVelocity), 2.0f, 1e-5f);
}

TEST(CreatureKindActions, AKickIsDoneWithEitherHand)
{
	using creature_object_actions::Kind;
	EXPECT_EQ(creature_object_actions::HandsFor(Kind::Kick), creature_object_actions::Hands::Either);
	// letting go by the put-down's animation lays the thing down; by the toss's, or any other, it is tossed away
	EXPECT_TRUE(creature_object_actions::DiscardPutsDown(creature_throw::k_PutDown));
	EXPECT_FALSE(creature_object_actions::DiscardPutsDown(creature_throw::k_Discard));
	EXPECT_FALSE(creature_object_actions::DiscardPutsDown(0));
	EXPECT_EQ(creature_object_actions::Name(Kind::Kick), "Kicking");
}

TEST(CreatureKindActions, BreakingARockThrowsItOffToOneSideAndBreaksItAsItFlies)
{
	const auto agenda = BreakRock(7u, 7u, {100.0f, 200.0f}, 10.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[1].order.point, glm::vec2(130.0f, 230.0f));
	EXPECT_EQ(agenda[2].kind, Step::Kind::Shatter);
	EXPECT_EQ(agenda[2].object, 7u);
	// Holding something else, it throws that and breaks the rock where it lies
	const auto holding = BreakRock(std::nullopt, 9u, {0.0f, 0.0f}, 10.0f);
	ASSERT_EQ(holding.size(), 2u);
	EXPECT_EQ(holding[1].object, 9u);
}

TEST(CreatureKindActions, StrokingAToyPicksItUpAndStrokesIt)
{
	const auto agenda = StrokeToy(5);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Keep);
	EXPECT_EQ(agenda[1].order.animation, creature_object_actions::k_StrokeObject);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Compassion);
}

TEST(CreatureKindActions, TakingAToyHomePutsItDownNearHomeMostly)
{
	// Fractions 0 and 1 put it 5 short along x and 5 past along z; a draw of 2 or more out of 10 puts it down
	std::array<float, 2> fractions {0.0f, 1.0f};
	size_t next = 0;
	const auto agenda = TakeToyHome(
	    5, {100.0f, 200.0f}, 8.0f, [](uint32_t) { return 2u; }, [&] { return fractions.at(next++); });
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(95.0f, 205.0f));
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 5.0f);
	EXPECT_EQ(agenda[2].order.kind, ObjectOrder::Kind::PutDown);
	// A draw of 0 or 1 tosses it away; a creature shorter than 5 arrives within its height
	next = 0;
	const auto tossed = TakeToyHome(
	    5, {0.0f, 0.0f}, 3.0f, [](uint32_t) { return 1u; }, [&] { return fractions.at(next++); });
	ASSERT_EQ(tossed.size(), 3u);
	EXPECT_EQ(tossed[2].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_FLOAT_EQ(tossed[1].movement.maxDistance, 3.0f);
}

TEST(CreatureKindActions, ThrowingADieWatchesItFlyThenJumpsOrSulks)
{
	const auto agenda = ThrowDie(6u, 6u, {10.0f, 20.0f}, 10.0f, [](uint32_t) { return 1u; });
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[1].order.point, glm::vec2(22.0f, 32.0f));
	EXPECT_EQ(agenda[2].kind, Step::Kind::WatchFlying);
	EXPECT_EQ(agenda[2].object, 6u);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Amazed);
	EXPECT_EQ(agenda[3].animation, creature_layers::animations::k_Happy);
	const auto sulk = ThrowDie(std::nullopt, 6u, {0.0f, 0.0f}, 10.0f, KindActionsFirst);
	ASSERT_EQ(sulk.size(), 3u);
	EXPECT_EQ(sulk[2].animation, creature_layers::animations::k_Sad);
}

TEST(CreatureKindActions, WatchingAThingFlyEndsWhenItLands)
{
	IdleMind mind;
	mind.agenda = {{.kind = Step::Kind::WatchFlying, .object = 3}};
	Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	senses.objectInTheAir = true;
	const auto random = [](uint32_t) { return 0u; };
	static_cast<void>(Think(mind, senses, random));
	EXPECT_EQ(mind.step, 0u);
	static_cast<void>(Think(mind, senses, random));
	EXPECT_EQ(mind.step, 0u);
	senses.objectInTheAir = false;
	static_cast<void>(Think(mind, senses, random));
	EXPECT_EQ(mind.step, 1u);
}

TEST(CreatureKindActions, PlayingAThrowingGameThrowsTheToyToThePlayersHand)
{
	const auto agenda = ThrowToPlayer(4, {10.0f, 30.0f, 20.0f}, false);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[1].order.point, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(agenda[1].order.pointHeight, 30.0f);
	EXPECT_EQ(agenda[2].animation, creature_layers::animations::k_Happy);
	// Holding something as it sets about it, it picks the toy up but throws nothing
	EXPECT_EQ(ThrowToPlayer(4, {}, true).size(), 2u);
}

TEST(CreatureKindActions, HelpingRepairAHouseBringsATreeAndUsesItUp)
{
	const auto agenda = HelpRepairHouse(8u, 3, 8);
	ASSERT_EQ(agenda.size(), 6u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(agenda[1].movement.object, 3u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 0.1f);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::PutDown);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Repair);
	EXPECT_EQ(agenda[4].object, 3u);
	EXPECT_EQ(agenda[5].kind, Step::Kind::UseUp);
	EXPECT_EQ(agenda[5].object, 8u);
	// Holding something already, it puts that down instead, and the tree is used up all the same
	EXPECT_EQ(HelpRepairHouse(std::nullopt, 3, 8).size(), 5u);
}

// The original's own test of what wants mending covers the totem as well as the buildings, on the totem's own life
TEST(CreatureKindActions, ADamagedBuildingOrTotemNeedsRepair)
{
	// building, temple, totem, life
	EXPECT_TRUE(plan_actions::NeedsRepair(true, false, false, 0.6f));
	EXPECT_FALSE(plan_actions::NeedsRepair(true, false, false, 1.0f));
	EXPECT_FALSE(plan_actions::NeedsRepair(true, true, false, 0.6f));
	EXPECT_TRUE(plan_actions::NeedsRepair(false, false, true, 0.6f));
	EXPECT_FALSE(plan_actions::NeedsRepair(false, false, true, 1.0f));
	EXPECT_FALSE(plan_actions::NeedsRepair(false, false, false, 0.6f));
}

TEST(CreatureKindActions, ShatteringRepairingAndUsingUpAreDoneAtOnce)
{
	for (const auto kind : {Step::Kind::Shatter, Step::Kind::Repair, Step::Kind::UseUp})
	{
		IdleMind mind;
		mind.agenda = {{.kind = kind, .object = 5}};
		const auto commands = Think(mind, Senses {.seconds = 0.1f}, KindActionsFirst);
		EXPECT_EQ(mind.step, 1u);
		const auto& given = kind == Step::Kind::Shatter  ? commands.shatter
		                    : kind == Step::Kind::Repair ? commands.repair
		                                                 : commands.useUp;
		ASSERT_TRUE(given.has_value());
		EXPECT_EQ(*given, 5u);
	}
}

TEST(CreatureKindActions, TheToyGamesNeedAHomeAndAHandOfThePlayers)
{
	const plan_actions::Executor home {
	    .action = "TakeToyHome", .target = plan_actions::Target::Anything, .build = plan_actions::Build::TakeToyHome};
	EXPECT_FALSE(plan_actions::Agenda(home, 4u, {0.0f, 0.0f}, {}, KindActionsFirst).has_value());
	plan_actions::Situation withHome;
	withHome.home = glm::vec2(10.0f, 20.0f);
	withHome.height = 8.0f;
	const auto taken = plan_actions::Agenda(home, 4u, {0.0f, 0.0f}, withHome, KindActionsFirst);
	ASSERT_TRUE(taken.has_value());
	EXPECT_EQ(taken->size(), 3u);

	const plan_actions::Executor game {
	    .action = "PlayThrowingGameWithPlayer", .use = plan_actions::Target::Toy, .build = plan_actions::Build::ThrowToPlayer};
	plan_actions::Situation withToy;
	withToy.instrument = 6u;
	EXPECT_FALSE(plan_actions::Agenda(game, std::nullopt, {0.0f, 0.0f}, withToy, KindActionsFirst).has_value());
	withToy.playerHand = glm::vec3(1.0f, 2.0f, 3.0f);
	const auto thrown = plan_actions::Agenda(game, std::nullopt, {0.0f, 0.0f}, withToy, KindActionsFirst);
	ASSERT_TRUE(thrown.has_value());
	ASSERT_EQ(thrown->size(), 3u);
	EXPECT_EQ((*thrown)[0].order.object, 6u);
	EXPECT_EQ((*thrown)[1].order.pointHeight, 2.0f);
}

TEST(CreatureKindActions, TalkingWhenHighFluttersTheEyelidsUntilItTakesUpSomethingElse)
{
	const auto agenda = BecomeHigh([] { return std::optional<HighSeat> {}; }, KindActionsFirst, [](float) { return 0.0f; });
	ASSERT_GE(agenda.size(), 2u);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Overlay);
	EXPECT_TRUE(agenda[1].flutterEyelids);
	IdleMind mind;
	mind.agenda = {{.kind = Step::Kind::Overlay, .animation = creature_layers::animations::k_Talk, .flutterEyelids = true}};
	const auto started = Think(mind, Senses {.seconds = 0.1f}, KindActionsFirst);
	EXPECT_EQ(started.blinkIntervalMs, k_FlutterBlinkIntervalMs);
	EXPECT_TRUE(mind.eyesHeld);
	Plan(mind, Activity::BeIdle, {{.kind = Step::Kind::Wait, .seconds = 1.0f}});
	const auto next = Think(mind, Senses {.seconds = 0.1f}, KindActionsFirst);
	EXPECT_EQ(next.eyes, Eyes::Normal);
	EXPECT_FALSE(mind.eyesHeld);
}

TEST(CreatureKindActions, AFightIsOnlyForcedAndOfAnotherCreature)
{
	// only ever forced on it: the anger that picks fights starts them through the fights themselves
	EXPECT_EQ(plan_actions::For("Fight"), nullptr);
	const auto* fighting = plan_actions::ForcedFor("Fight");
	ASSERT_NE(fighting, nullptr);
	EXPECT_EQ(fighting->target, plan_actions::Target::Creature);
	EXPECT_EQ(fighting->build, plan_actions::Build::Fight);
	const creature_mind::Random random = [](uint32_t) { return 0U; };
	const auto agenda = plan_actions::Agenda(*fighting, 9U, glm::vec2(0.0f), plan_actions::Situation {}, random);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4U);
	EXPECT_EQ((*agenda)[2].kind, Step::Kind::Fight);
	EXPECT_EQ((*agenda)[2].object, std::optional(9U));
	// with nobody to fight, there is no agenda
	EXPECT_FALSE(plan_actions::Agenda(*fighting, std::nullopt, glm::vec2(0.0f), plan_actions::Situation {}, random));
}

TEST(CreatureKindActions, SmashingAStoneInHalfStampsOnItAngrily)
{
	const auto* smash = plan_actions::For("SmashStoneInHalf");
	ASSERT_NE(smash, nullptr);
	EXPECT_EQ(smash->target, plan_actions::Target::Rock);
	EXPECT_EQ(smash->build, plan_actions::Build::Destroy);
	const auto agenda = DestroyThing(4);
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::Destroy);
	EXPECT_EQ(agenda[0].order.object, 4u);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Anger);
}

TEST(CreatureKindActions, GivingWoodToABuildingSiteIsGivingWoodToAStore)
{
	const auto* site = plan_actions::For("GiveWoodFromTreeToBuildingSite");
	ASSERT_NE(site, nullptr);
	EXPECT_EQ(site->target, plan_actions::Target::BuildingSite);
	EXPECT_EQ(site->use, plan_actions::Target::PickableTree);
	EXPECT_EQ(site->build, plan_actions::Build::GiveWoodToStore);
}

TEST(CreatureKindActions, TheRockToyAndRepairActionsAreThePlannersRows)
{
	const auto check = [](std::string_view action, plan_actions::Target target, plan_actions::Build build) {
		const auto* executor = plan_actions::For(action);
		ASSERT_NE(executor, nullptr) << action;
		EXPECT_EQ(executor->target, target) << action;
		EXPECT_EQ(executor->build, build) << action;
	};
	check("BreakRock", plan_actions::Target::PickableRock, plan_actions::Build::BreakRock);
	check("StrokToy", plan_actions::Target::CuddlyToy, plan_actions::Build::StrokeToy);
	check("TakeToyHome", plan_actions::Target::ToyAwayFromHome, plan_actions::Build::TakeToyHome);
	check("ThrowDie", plan_actions::Target::Die, plan_actions::Build::ThrowDie);
	check("PlayThrowingGameWithPlayer", plan_actions::Target::None, plan_actions::Build::ThrowToPlayer);
	check("HelpRepairHouse", plan_actions::Target::NeedsRepair, plan_actions::Build::HelpRepairHouse);
	check("KickBallAround", plan_actions::Target::ToyBall, plan_actions::Build::KickBallAround);
	EXPECT_EQ(plan_actions::For("PlayThrowingGameWithPlayer")->use, plan_actions::Target::Toy);
	EXPECT_EQ(plan_actions::For("HelpRepairHouse")->use, plan_actions::Target::PickableTree);
}
