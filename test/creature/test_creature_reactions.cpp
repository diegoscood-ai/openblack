/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature takes the miracles' reactions (Creature/CreatureReactionRules.h): the priorities, the turns, the
// availability, and what it does as it starts reacting; then the creatures' reaction handler with a fake mind: a nasty
// miracle is run from (or looked at) and learnt, never one it cast itself, a nice one of another player's is ignored,
// the impressive miracle reads the nice one's priority, the one miracle no creature learns, a seed learnt from once
// teaches no more, a computer player's miracle teaches nothing, a shield teaches the spiritual shield, and the end of a
// reaction stops the creature following it; each creature turn's end of the reaction it follows (its turns over, or its
// miracle gone), a creature switching to a reaction that matters more once it has followed its own long enough, and a
// fire taken up only within the burning thing's reach and the creature's own, leaving the reaction it followed

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureReactionRules.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/Systems/Implementations/CreatureReactions.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::creature_reaction_rules;
using openblack::ecs::components::SpellCreator;
using openblack::test::creature_loop_fakes::CallLog;
using openblack::test::creature_loop_fakes::FakeMind;

TEST(CreatureReactionRules, ANastyMiracleMattersMoreTheNearerItStrikes)
{
	EXPECT_EQ(FleeFromSpellPriority(false, k_FleeFromSpellRange, 60), 60);
	EXPECT_EQ(FleeFromSpellPriority(false, k_FleeFromSpellRange + 1, 60), 60);
	EXPECT_EQ(FleeFromSpellPriority(false, 0, 60), 160);
	EXPECT_EQ(FleeFromSpellPriority(false, k_FleeFromSpellRange / 2, 60), 110);
	// a hundredth of the range nearer adds one, truncated
	EXPECT_EQ(FleeFromSpellPriority(false, k_FleeFromSpellRange - 5999, 60), 60);
	EXPECT_EQ(FleeFromSpellPriority(false, k_FleeFromSpellRange - 6000, 60), 61);
	// the sum is cut to a byte
	EXPECT_EQ(FleeFromSpellPriority(false, 0, 200), 44);
	EXPECT_EQ(FleeFromSpellPriority(true, 0, 60), 0);
}

TEST(CreatureReactionRules, ANiceMiracleMattersByItsPriorityUnlessItsOwn)
{
	EXPECT_EQ(LookAtNiceSpellPriority(false, 70), 70);
	EXPECT_EQ(LookAtNiceSpellPriority(false, 300), 44);
	EXPECT_EQ(LookAtNiceSpellPriority(true, 70), 0);
}

TEST(CreatureReactionRules, TheTurnsGrowAndShrinkWithTheDistance)
{
	// ((64 - 16) x 0.5 / 64 + 0.5) x 100 = 87.5, and (0.5 x 16 / 64 + 0.5) x 200 = 125
	EXPECT_EQ(TurnsToReact(64.0f, 0.5f, 16.0f, 100), 87);
	EXPECT_EQ(TurnsBeforeReactingAgain(64.0f, 0.5f, 16.0f, 200), 125);
	EXPECT_EQ(TurnsToReact(64.0f, 0.5f, 96.0f, 100), 25);
	// beyond the reaction's distance the turns to react go below none
	EXPECT_EQ(TurnsToReact(64.0f, 0.5f, 200.0f, 100), -56);
	// distance not important: the table's turns
	EXPECT_EQ(TurnsToReact(64.0f, 0.0f, 16.0f, 100), 100);
	EXPECT_EQ(TurnsBeforeReactingAgain(64.0f, 0.0f, 16.0f, 200), 200);
	// all important: from none at the reaction's distance
	EXPECT_EQ(TurnsBeforeReactingAgain(64.0f, 1.0f, 0.0f, 200), 0);
	EXPECT_EQ(TurnsToReact(64.0f, 1.0f, 64.0f, 100), 0);
}

TEST(CreatureReactionRules, ACreatureBusyOrOutColdTakesNothingUpAndAMimicOnlyWhatMattersMost)
{
	EXPECT_TRUE(IsAvailableForReaction({}, 0));
	EXPECT_FALSE(IsAvailableForReaction({.available = false}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.reactionsOn = false}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.fighting = true}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.fainted = true}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.knockedOut = true}, 255));
	// nor in an emergency of its own, whatever the reaction
	EXPECT_FALSE(IsAvailableForReaction({.inEmergency = true}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.mimicking = true}, k_MimicPriority));
	EXPECT_TRUE(IsAvailableForReaction({.mimicking = true}, k_MimicPriority + 1));
}

// Nor does one under a script's control, carrying out its player's order, planning to fight, or whose opponent plans to
TEST(CreatureReactionRules, ACreatureScriptedOrderedOrAboutToFightTakesNothingUp)
{
	EXPECT_FALSE(IsAvailableForReaction({.scriptControlled = true}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.orderedByPlayer = true}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.plansFight = true}, 255));
	EXPECT_FALSE(IsAvailableForReaction({.opponentPlansFight = true}, 255));
}

TEST(CreatureReactionRules, ANastyMiracleIsLearntUnlessAComputersOrOneLearntFrom)
{
	// another thing started it: learnt, no seed to mark
	const auto thing = NastyMagic(std::nullopt);
	EXPECT_TRUE(thing.learn);
	EXPECT_FALSE(thing.markSeed);
	EXPECT_TRUE(thing.takeUp);
	const SpellFacts human {.hasCreator = true, .hasSeed = true, .playerType = 1};
	const auto learnt = NastyMagic(human);
	EXPECT_TRUE(learnt.learn);
	EXPECT_TRUE(learnt.markSeed);
	EXPECT_TRUE(learnt.takeUp);
	EXPECT_FALSE(learnt.examine);
	for (const int32_t type : {2, 3})
	{
		auto computer = human;
		computer.playerType = type;
		const auto outcome = NastyMagic(computer);
		EXPECT_FALSE(outcome.learn);
		EXPECT_FALSE(outcome.markSeed);
		EXPECT_TRUE(outcome.takeUp);
		// a creature's miracle teaches whatever its player
		computer.creatorIsCreature = true;
		EXPECT_TRUE(NastyMagic(computer).learn);
	}
	auto spent = human;
	spent.seedLearnedFrom = true;
	EXPECT_FALSE(NastyMagic(spent).learn);
	EXPECT_TRUE(NastyMagic(spent).takeUp);
	// cast by a spell icon with no seed: nothing more
	const auto icon = NastyMagic(SpellFacts {.hasCreator = true, .creatorIsSpellIcon = true});
	EXPECT_FALSE(icon.learn);
	EXPECT_FALSE(icon.takeUp);
	EXPECT_TRUE(NastyMagic(SpellFacts {.hasCreator = true, .creatorIsSpellIcon = true, .hasSeed = true}).takeUp);
}

TEST(CreatureReactionRules, ANiceMiracleIsLookedAtUnlessAnothersAComputersOrOneLearntFrom)
{
	const SpellFacts human {.hasCreator = true, .hasSeed = true, .playerType = 1};
	const auto look = NiceMagic(false, MagicType::Heal, human);
	EXPECT_TRUE(look.examine);
	EXPECT_TRUE(look.learn);
	EXPECT_TRUE(look.markSeed);
	EXPECT_TRUE(look.takeUp);
	const auto others = NiceMagic(true, MagicType::Heal, human);
	EXPECT_FALSE(others.examine);
	EXPECT_FALSE(others.takeUp);
	// a computer player's: taken up, not looked at
	auto computer = human;
	computer.playerType = 2;
	const auto ignored = NiceMagic(false, MagicType::Heal, computer);
	EXPECT_FALSE(ignored.examine);
	EXPECT_FALSE(ignored.learn);
	EXPECT_TRUE(ignored.takeUp);
	// type 3 does not stop a nice one
	computer.playerType = 3;
	EXPECT_TRUE(NiceMagic(false, MagicType::Heal, computer).examine);
	auto spent = human;
	spent.seedLearnedFrom = true;
	EXPECT_FALSE(NiceMagic(false, MagicType::Heal, spent).examine);
	EXPECT_TRUE(NiceMagic(false, MagicType::Heal, spent).takeUp);
	EXPECT_FALSE(NiceMagic(false, MagicType::Heal, SpellFacts {.hasCreator = true, .creatorIsSpellIcon = true}).takeUp);
	// another thing started it: anything but wood is looked at
	EXPECT_TRUE(NiceMagic(false, MagicType::Heal, std::nullopt).examine);
	EXPECT_FALSE(NiceMagic(false, MagicType::Heal, std::nullopt).markSeed);
	EXPECT_FALSE(NiceMagic(false, MagicType::Wood, std::nullopt).takeUp);
}

namespace
{
/// A creature of a human player that sees the thing, the table's urgency 100
Facts Seeing()
{
	return Facts {.tablePriority = 100, .humanPlayer = true, .seesThing = true};
}

uint32_t NoDraw(uint32_t /*range*/)
{
	ADD_FAILURE() << "no draw expected";
	return 0;
}
} // namespace

// Another creature is as urgent as can be once it is due another look over, unless the creature is busy with its own
// reaction, its player's order, a fight or that creature already
TEST(CreatureReactionRules, AnotherCreatureIsUrgentOnceDueALookOver)
{
	auto facts = Seeing();
	facts.thingIsCreature = true;
	facts.inspectDue = true;
	EXPECT_EQ(Priority(Reaction::ReactToCreature, facts, NoDraw), std::optional<uint8_t>(0xff));
	facts.inspectDue = false;
	EXPECT_EQ(Priority(Reaction::ReactToCreature, facts, NoDraw), std::optional<uint8_t>(0));
	facts.inspectDue = true;
	for (auto busy :
	     {&Facts::thingIsSelf, &Facts::orderedByPlayer, &Facts::hasOpponent, &Facts::planActsOnThing, &Facts::thingGone})
	{
		auto blocked = facts;
		blocked.*busy = true;
		EXPECT_TRUE(BlocksInspecting(blocked));
		EXPECT_EQ(Priority(Reaction::ReactToCreature, blocked, NoDraw), std::optional<uint8_t>(0));
	}
	EXPECT_FALSE(BlocksInspecting(facts));
	// Neither its player nor whether it sees the creature counts
	facts.humanPlayer = false;
	facts.seesThing = false;
	EXPECT_EQ(Priority(Reaction::ReactToCreature, facts, NoDraw), std::optional<uint8_t>(0xff));
}

// A creature never looked over is due at once; one looked over only once more turns than the wait have gone by
TEST(CreatureReactionRules, ACreatureIsDueALookOverOnlyAfterTheWait)
{
	EXPECT_TRUE(InspectDue(std::nullopt, 0, 600));
	EXPECT_FALSE(InspectDue(1000, 1600, 600));
	EXPECT_TRUE(InspectDue(1000, 1601, 600));
	// The game's wait of a minute, at ten turns a second
	EXPECT_EQ(InspectWaitTurns(60.0f, 10), 600u);
	EXPECT_EQ(InspectWaitTurns(1.25f, 10), 12u);
}

// A flying thing thrown by a hand, still in the air, seen and past the wait after the last: the table's urgency; another
// player's only on a draw of 0 to 2 of 100
TEST(CreatureReactionRules, AThrownThingInTheAirCatchesItsEyeAfterTheWait)
{
	auto facts = Seeing();
	facts.thrownFromHand = true;
	facts.inTheAir = true;
	facts.turn = 1000;
	EXPECT_EQ(Priority(Reaction::ReactToFlyingObject, facts, NoDraw), std::optional<uint8_t>(100));
	facts.sawFlying = 101;
	EXPECT_EQ(Priority(Reaction::ReactToFlyingObject, facts, NoDraw), std::optional<uint8_t>(0));
	facts.sawFlying = 100;
	EXPECT_EQ(Priority(Reaction::ReactToFlyingObject, facts, NoDraw), std::optional<uint8_t>(100));
	for (auto unmet : {&Facts::thrownFromHand, &Facts::inTheAir, &Facts::seesThing, &Facts::humanPlayer})
	{
		auto missing = facts;
		missing.*unmet = false;
		EXPECT_EQ(Priority(Reaction::ReactToFlyingObject, missing, NoDraw), std::optional<uint8_t>(0));
	}
	auto catching = facts;
	catching.catching = true;
	EXPECT_EQ(Priority(Reaction::ReactToFlyingObject, catching, NoDraw), std::optional<uint8_t>(0));
	facts.otherPlayers = true;
	EXPECT_EQ(Priority(Reaction::ReactToFlyingObject, facts, [](uint32_t range) { return range == 100 ? 2u : 0u; }),
	          std::optional<uint8_t>(100));
	EXPECT_EQ(Priority(Reaction::ReactToFlyingObject, facts, [](uint32_t range) { return range == 100 ? 3u : 0u; }),
	          std::optional<uint8_t>(0));
}

// The hand of its own player picking something up, seen: three times the table's urgency, kept to a byte
TEST(CreatureReactionRules, AReactionOfAPlayerAlliedToTheCreaturesIsNotAnotherPlayers)
{
	using ecs::components::PlayerAlliances;
	EXPECT_TRUE(IsOthersPlayer(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, PlayerAlliances {}));
	// no player's, or the creature's own player's
	EXPECT_FALSE(IsOthersPlayer(false, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, PlayerAlliances {}));
	EXPECT_FALSE(IsOthersPlayer(true, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE, PlayerAlliances {}));
	// the reaction's player allied to the creature's, at any strength above 0
	PlayerAlliances twos {};
	twos.strength.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 0.01f;
	EXPECT_FALSE(IsOthersPlayer(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, twos));
	EXPECT_TRUE(IsOthersPlayer(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE, twos));
	twos.strength.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 0.0f;
	EXPECT_TRUE(IsOthersPlayer(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, twos));
}

TEST(CreatureReactionRules, ItsOwnHandPickingUpIsThreeTimesAsUrgent)
{
	auto facts = Seeing();
	facts.ownPlayersHand = true;
	facts.handHolds = true;
	EXPECT_EQ(Priority(Reaction::ReactToHandPickUp, facts, NoDraw), std::optional<uint8_t>(44));
	facts.tablePriority = 50;
	EXPECT_EQ(Priority(Reaction::ReactToHandPickUp, facts, NoDraw), std::optional<uint8_t>(150));
	facts.handHolds = false;
	EXPECT_EQ(Priority(Reaction::ReactToHandPickUp, facts, NoDraw), std::optional<uint8_t>(0));
	// The kinds a creature weighs as the miracles' rules do have none here
	EXPECT_FALSE(Priority(Reaction::FleeFromSpell, facts, NoDraw).has_value());
}

// The hand using a totem matters to a creature whose player the computer does not play, when it sees the totem, by the
// table's urgency; the hand putting things in a storage pit matters to it wherever it looks
TEST(CreatureReactionRules, TheHandUsingATotemOrFillingAStoragePitMattersUnlessTheComputerPlaysItsPlayer)
{
	auto facts = Seeing();
	EXPECT_EQ(Priority(Reaction::ReactToHandUsingTotem, facts, NoDraw), std::optional<uint8_t>(100));
	EXPECT_EQ(Priority(Reaction::ReactToHandPuttingStuffInStoragePit, facts, NoDraw), std::optional<uint8_t>(100));
	facts.seesThing = false;
	EXPECT_EQ(Priority(Reaction::ReactToHandUsingTotem, facts, NoDraw), std::optional<uint8_t>(0));
	EXPECT_EQ(Priority(Reaction::ReactToHandPuttingStuffInStoragePit, facts, NoDraw), std::optional<uint8_t>(100));
	// a player that is not a person but not the computer either still counts
	facts.seesThing = true;
	facts.humanPlayer = false;
	EXPECT_EQ(Priority(Reaction::ReactToHandUsingTotem, facts, NoDraw), std::optional<uint8_t>(100));
	facts.noPlayerOrComputers = true;
	EXPECT_EQ(Priority(Reaction::ReactToHandUsingTotem, facts, NoDraw), std::optional<uint8_t>(0));
	EXPECT_EQ(Priority(Reaction::ReactToHandPuttingStuffInStoragePit, facts, NoDraw), std::optional<uint8_t>(0));
}

// Taking up the hand using a totem, it comes across the totem, looks at it and watches the totem used; the hand putting
// things in a storage pit, it comes across the pit and watches it used, with no plan of its own
TEST(CreatureReactionRules, TheHandUsingATotemOrFillingAStoragePitShowsTheCreatureTheSkill)
{
	const auto totem = SetupFor(Reaction::ReactToHandUsingTotem, false);
	ASSERT_TRUE(totem.has_value() && totem->plan.has_value());
	EXPECT_TRUE(totem->keeps);
	EXPECT_TRUE(totem->comesAcross);
	EXPECT_EQ(totem->plan->desire, creature_desires::Desire::Curiosity);
	EXPECT_EQ(totem->plan->action, "ExamineByLooking");
	EXPECT_FALSE(totem->plan->onSelf);
	EXPECT_FALSE(totem->plan->onlyIfUseful);
	EXPECT_EQ(totem->watchesSkill, std::optional(CreatureActionKnownAbout::UseTotem));

	const auto pit = SetupFor(Reaction::ReactToHandPuttingStuffInStoragePit, false);
	ASSERT_TRUE(pit.has_value());
	EXPECT_TRUE(pit->keeps);
	EXPECT_TRUE(pit->comesAcross);
	EXPECT_FALSE(pit->plan.has_value());
	EXPECT_EQ(pit->watchesSkill, std::optional(CreatureActionKnownAbout::UseStoragePit));

	// the other kinds show it no skill
	const auto hand = SetupFor(Reaction::ReactToHandPickUp, false);
	ASSERT_TRUE(hand.has_value());
	EXPECT_FALSE(hand->watchesSkill.has_value());
}

// Taking up another creature's reaction, it looks it over, if that is of any use and isn't what it does already; a
// flying thing still up it looks at, noting the turn, and one already down it only notes; the hand it watches for long
TEST(CreatureReactionRules, WhatItDoesOnTakingUpACreatureAFlyingThingOrTheHand)
{
	const auto creature = SetupFor(Reaction::ReactToCreature, false);
	ASSERT_TRUE(creature.has_value());
	EXPECT_TRUE(creature->keeps);
	EXPECT_FALSE(creature->comesAcross);
	ASSERT_TRUE(creature->plan.has_value());
	EXPECT_EQ(creature->plan->desire, creature_desires::Desire::Curiosity);
	EXPECT_EQ(creature->plan->action, "InspectCreature");
	EXPECT_TRUE(creature->plan->onlyIfUseful);
	EXPECT_FALSE(creature->plan->onSelf);

	const auto up = SetupFor(Reaction::ReactToFlyingObject, true);
	ASSERT_TRUE(up.has_value() && up->plan.has_value());
	EXPECT_TRUE(up->keeps);
	EXPECT_TRUE(up->notesFlying);
	EXPECT_TRUE(up->comesAcross);
	EXPECT_EQ(up->plan->action, "LookAtFlyingObject");
	const auto down = SetupFor(Reaction::ReactToFlyingObject, false);
	ASSERT_TRUE(down.has_value());
	EXPECT_FALSE(down->keeps);
	EXPECT_TRUE(down->notesFlying);
	EXPECT_FALSE(down->plan.has_value());

	const auto hand = SetupFor(Reaction::ReactToHandPickUp, false);
	ASSERT_TRUE(hand.has_value() && hand->plan.has_value());
	EXPECT_EQ(hand->plan->action, "LookAtHand");
	EXPECT_TRUE(hand->plan->onSelf);
	EXPECT_TRUE(hand->longHandWatch);

	EXPECT_FALSE(SetupFor(Reaction::FleeFromSpell, false).has_value());
}

TEST(CreatureReactionRules, OnTheRopeOrUnafraidItGoesToLook)
{
	EXPECT_FALSE(CuriousAboutNastyMagic(false, 0.1f));
	EXPECT_TRUE(CuriousAboutNastyMagic(true, 0.1f));
	EXPECT_TRUE(CuriousAboutNastyMagic(false, 0.0f));
	EXPECT_TRUE(CuriousAboutNastyMagic(false, std::numeric_limits<float>::quiet_NaN()));
	EXPECT_FALSE(LearntBySight(k_NotLearntBySight).has_value());
	EXPECT_EQ(LearntBySight(MagicType::Fireball), MagicType::Fireball);
}

namespace
{
namespace reactions = openblack::ecs::effects::reactions;

/// A registry of its own with a creature of the first player's, the game's reactions with the creatures' handlers, a
/// fake mind, and the reaction table's rows of the four miracle types
class CreatureReactionHandlerTest: public ::testing::Test
{
protected:
	CreatureReactionHandlerTest()
	{
		Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
		ecs::creature_reactions::RegisterHandlers();
		Locator::creatureMindSystem::emplace<FakeMind>(log);
		Locator::creatureFightSystem::reset();
		auto& info = world.Info();
		for (const auto type : {openblack::Reaction::FleeFromSpell, openblack::Reaction::ReactToMagicShield,
		                        openblack::Reaction::LookAtNiceSpell, openblack::Reaction::ReactToImpressiveSpell})
		{
			auto& row = info.reaction.at(static_cast<size_t>(type));
			row.priority = 40;
			row.maxReactionDistance = 64.0f;
			row.howImportantIsDistance = 0.5f;
			row.numGameTurnsForCreatureToReact = 100;
			row.numGameTurnsForCreatureBeforeReactingAgain = 200;
		}
		auto& reacting = info.creature.at(0).isReacting;
		reacting.isFleeingFromSpell = 1;
		reacting.isReactingToMagicShield = 1;
		reacting.isReactingToNiceSpell = 1;
		reacting.isReactingToImpressiveSpell = 1;
		creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	}

	/// A miracle of the player's, cast by the player at a point near the creature, with no seed
	entt::entity Miracle(MagicType magic, PlayerNames player = PlayerNames::PLAYER_ONE)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto spell = registry.Create();
		registry.Assign<ecs::components::Spell>(spell, ecs::components::Spell {
		                                                   .magicType = magic,
		                                                   .position = glm::vec3(110.0f, 0.0f, 100.0f),
		                                                   .creator = {.kind = SpellCreator::Kind::Player, .player = player},
		                                                   .player = player,
		                                                   .hasPlayer = true,
		                                               });
		return spell;
	}

	/// A player's entity of a type (1 a human, 2 a computer player)
	static void MakePlayer(PlayerNames player, int32_t type)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Player>(entity, player);
		registry.Assign<ecs::components::PlayerMagic>(entity).playerType = type;
	}

	/// The reaction the miracle makes, in the game's list, applied to the creature
	uint32_t React(entt::entity spell, openblack::Reaction type, float distance = 10.0f)
	{
		const auto& player = std::as_const(test::creature_world::World::Registry()).Get<ecs::components::Spell>(spell).player;
		const auto id = Locator::reactionSystem::value().Add({.initiator = spell, .type = type, .player = player});
		ecs::creature_reactions::HandleReaction(creature, *reactions::Find(id), distance);
		return id;
	}

	test::creature_world::World world;
	CallLog log;
	const test::RestoreService<Locator::reactionSystem> restoreReactions;
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	const test::RestoreService<Locator::creatureFightSystem> restoreFight;
	entt::entity creature {entt::null};
};

float Id(entt::entity entity)
{
	return static_cast<float>(entt::to_integral(entity));
}
} // namespace

TEST_F(CreatureReactionHandlerTest, ItRunsFromANastyMiracleAndLearnsIt)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	const auto id = React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "mind.ReactToNastyMagic");
	EXPECT_EQ(log.front().args,
	          (std::vector<float> {Id(creature), 110.0f, 0.0f, 100.0f, static_cast<float>(MagicType::Fireball)}));
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
}

// Each take of a reaction is stamped with the order it joined in, against every other follower's
TEST_F(CreatureReactionHandlerTest, ATakeIsStampedWithItsJoinOrder)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	const auto id = React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell);
	ASSERT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
	const auto& registry = test::creature_world::World::Registry();
	EXPECT_GT(registry.Get<const ecs::components::CreatureReaction>(creature).joinOrder, 0u);
}

// Carrying out its player's order it takes no reaction up, unless the leash leads it or sent it somewhere
TEST_F(CreatureReactionHandlerTest, CarryingOutItsPlayersOrderItTakesNoReactionUnlessTheLeashLeadsOrSendsIt)
{
	auto& registry = test::creature_world::World::Registry();
	const auto type = static_cast<uint8_t>(openblack::Reaction::FleeFromSpell);
	ASSERT_TRUE(ecs::creature_reactions::IsAvailableForReaction(creature, type));
	registry.Get<ecs::components::CreatureMindState>(creature).commanded = true;
	EXPECT_FALSE(ecs::creature_reactions::IsAvailableForReaction(creature, type));
	auto& leash = registry.AssignOrReplace<ecs::components::CreatureLeash>(creature);
	leash.control = ecs::components::CreatureLeash::Control::Sent;
	EXPECT_TRUE(ecs::creature_reactions::IsAvailableForReaction(creature, type));
	leash.control = ecs::components::CreatureLeash::Control::Idle;
	EXPECT_FALSE(ecs::creature_reactions::IsAvailableForReaction(creature, type));
	registry.Get<ecs::components::CreatureMindState>(creature).commanded = false;
	EXPECT_TRUE(ecs::creature_reactions::IsAvailableForReaction(creature, type));
}

// A creature a script has told to take no notice of reactions takes none up, until a script tells it to again
TEST_F(CreatureReactionHandlerTest, ACreatureToldToTakeNoNoticeTakesNothingUp)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<ecs::components::CreatureReactionsOff>(creature);
	React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell);
	EXPECT_TRUE(log.empty());
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
	registry.Remove<ecs::components::CreatureReactionsOff>(creature);
	const auto id = React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
}

TEST_F(CreatureReactionHandlerTest, ItIgnoresAMiracleItCastItself)
{
	const auto spell = Miracle(MagicType::Fireball);
	auto& own = test::creature_world::World::Registry().Get<ecs::components::Spell>(spell);
	own.creator = {.kind = SpellCreator::Kind::Creature, .player = PlayerNames::PLAYER_ONE, .entity = creature};
	React(spell, openblack::Reaction::FleeFromSpell);
	EXPECT_TRUE(log.empty());
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
}

TEST_F(CreatureReactionHandlerTest, BeyondItsDistanceOrWithoutItsInfoReactingItIgnoresIt)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell, 65.0f);
	world.Info().creature.at(0).isReacting.isFleeingFromSpell = 0;
	React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell);
	EXPECT_TRUE(log.empty());
}

TEST_F(CreatureReactionHandlerTest, TheOneMiracleNoCreatureLearnsIsStillRunFrom)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	React(Miracle(k_NotLearntBySight), openblack::Reaction::FleeFromSpell);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().args.back(), -1.0f);
}

TEST_F(CreatureReactionHandlerTest, ASeedTeachesOnce)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	auto& registry = test::creature_world::World::Registry();
	const auto seed = registry.Create();
	registry.Assign<ecs::components::SpellSeed>(seed);
	const auto first = Miracle(MagicType::Fireball);
	registry.Get<ecs::components::Spell>(first).seed = seed;
	React(first, openblack::Reaction::FleeFromSpell);
	EXPECT_TRUE(std::as_const(registry).Get<ecs::components::SpellSeed>(seed).learnedFrom);
	// a nice miracle of the same seed is taken up, but not looked at
	ecs::creature_reactions::StopReacting(creature);
	const auto second = Miracle(MagicType::Heal);
	registry.Get<ecs::components::Spell>(second).seed = seed;
	const auto id = React(second, openblack::Reaction::LookAtNiceSpell);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "mind.ReactToNastyMagic");
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
}

TEST_F(CreatureReactionHandlerTest, AComputerPlayersMiracleTeachesNothing)
{
	MakePlayer(PlayerNames::PLAYER_TWO, 2);
	React(Miracle(MagicType::Fireball, PlayerNames::PLAYER_TWO), openblack::Reaction::FleeFromSpell);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().args.back(), -1.0f);
}

TEST_F(CreatureReactionHandlerTest, AnotherPlayersNiceMiracleIsIgnored)
{
	MakePlayer(PlayerNames::PLAYER_TWO, 1);
	React(Miracle(MagicType::Heal, PlayerNames::PLAYER_TWO), openblack::Reaction::LookAtNiceSpell);
	EXPECT_TRUE(log.empty());
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
}

TEST_F(CreatureReactionHandlerTest, AnAlliedPlayersNiceMiracleIsLookedAtAsItsOwnPlayers)
{
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	MakePlayer(PlayerNames::PLAYER_TWO, 1);
	// the miracle's player allied to the creature's
	ecs::alliances::SetAlliance(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 1.0f);
	const auto id = React(Miracle(MagicType::Heal, PlayerNames::PLAYER_TWO), openblack::Reaction::LookAtNiceSpell);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "mind.ReactToNiceMagic");
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
	// the alliance ended: ignored again
	ecs::creature_reactions::StopReacting(creature);
	log.clear();
	ecs::alliances::SetAlliance(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 0.0f);
	React(Miracle(MagicType::Heal, PlayerNames::PLAYER_TWO), openblack::Reaction::LookAtNiceSpell);
	EXPECT_TRUE(log.empty());
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
}

TEST_F(CreatureReactionHandlerTest, TheImpressiveMiracleReadsTheNiceOnesPriority)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	auto& info = world.Info();
	info.reaction.at(static_cast<size_t>(openblack::Reaction::LookAtNiceSpell)).priority = 0;
	info.reaction.at(static_cast<size_t>(openblack::Reaction::ReactToImpressiveSpell)).priority = 200;
	React(Miracle(MagicType::Heal), openblack::Reaction::ReactToImpressiveSpell);
	EXPECT_TRUE(log.empty());
	info.reaction.at(static_cast<size_t>(openblack::Reaction::LookAtNiceSpell)).priority = 40;
	info.reaction.at(static_cast<size_t>(openblack::Reaction::ReactToImpressiveSpell)).priority = 0;
	React(Miracle(MagicType::Heal), openblack::Reaction::ReactToImpressiveSpell);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "mind.ReactToNiceMagic");
	EXPECT_EQ(log.front().args.back(), static_cast<float>(MagicType::Heal));
}

TEST_F(CreatureReactionHandlerTest, AShieldTeachesTheSpiritualShield)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	React(Miracle(MagicType::PhysicalShield), openblack::Reaction::ReactToMagicShield);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "mind.ReactToNiceMagic");
	EXPECT_EQ(log.front().args.back(), static_cast<float>(MagicType::Shield));
}

TEST_F(CreatureReactionHandlerTest, ItTakesTheSameKindOnlyOnceWhileItFollowsOne)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	const auto id = React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell);
	React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell, 0.0f);
	EXPECT_EQ(log.size(), 1u);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
}

TEST_F(CreatureReactionHandlerTest, TheEndOfTheReactionStopsTheCreature)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	const auto spell = Miracle(MagicType::Fireball);
	React(spell, openblack::Reaction::FleeFromSpell);
	ASSERT_NE(ecs::creature_reactions::ReactionOf(creature), 0u);
	reactions::RemoveAllReactionsInitiatedByObject(spell);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
}

namespace
{
/// The game's turn, set for one test and put back at its end
class TurnAt
{
public:
	explicit TurnAt(uint32_t turn)
	    : _previous(game_clock::Turn())
	{
		game_clock::SetTurn(turn);
	}
	~TurnAt() { game_clock::SetTurn(_previous); }
	TurnAt(const TurnAt&) = delete;
	TurnAt& operator=(const TurnAt&) = delete;
	TurnAt(TurnAt&&) = delete;
	TurnAt& operator=(TurnAt&&) = delete;

private:
	uint32_t _previous;
};

constexpr auto k_FleeType = static_cast<uint8_t>(openblack::Reaction::FleeFromSpell);
constexpr auto k_ShieldType = static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield);
} // namespace

TEST_F(CreatureReactionHandlerTest, ItStopsReactingWhenItsTurnsToReactAreOver)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	constexpr uint32_t k_Start = 1000;
	const TurnAt turn(k_Start);
	const auto id = React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell);
	ASSERT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start);
	// within the reaction's distance its turns to react are from half the table's 100 to all of them
	game_clock::SetTurn(k_Start + 50);
	ecs::creature_reactions::ProcessReaction(creature);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
	game_clock::SetTurn(k_Start + 101);
	ecs::creature_reactions::ProcessReaction(creature);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
	// the record is refreshed to when it stopped
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start + 101);
}

TEST_F(CreatureReactionHandlerTest, ItStopsReactingWhenTheMiracleHasGone)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	constexpr uint32_t k_Start = 1000;
	const TurnAt turn(k_Start);
	const auto spell = Miracle(MagicType::Fireball);
	React(spell, openblack::Reaction::FleeFromSpell);
	ASSERT_NE(ecs::creature_reactions::ReactionOf(creature), 0u);
	test::creature_world::World::Registry().Assign<ecs::components::Unavailable>(spell);
	game_clock::SetTurn(k_Start + 1);
	ecs::creature_reactions::ProcessReaction(creature);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start + 1);
	// nothing is held any more: another turn changes nothing
	game_clock::SetTurn(k_Start + 2);
	ecs::creature_reactions::ProcessReaction(creature);
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start + 1);
}

// A magic tree's reaction, with the game's row (priority 50 within 35 m) and every species reacting to it: a creature of
// a player the computer does not play notices it, so the reaction is stamped and the creature's record of it set, but
// as a tree is no miracle the creature neither looks nor learns nor takes it up; a computer player's creature scores it
// nothing (docs/wiki/creature/mimicry.md#reactions-to-miracles)
TEST_F(CreatureReactionHandlerTest, AMagicTreeIsNoticedButNeverTakenUpAndAComputersCreatureIgnoresIt)
{
	constexpr auto k_TreeType = static_cast<uint8_t>(openblack::Reaction::ReactToMagicTree);
	auto& row = world.Info().reaction.at(k_TreeType);
	row.priority = 50;
	row.maxReactionDistance = 35.0f;
	row.howImportantIsDistance = 1.0f;
	row.numGameTurnsForCreatureToReact = 2000;
	row.numGameTurnsForCreatureBeforeReactingAgain = 600;
	world.Info().creature.at(0).isReacting.isReactingToMagicTree = 1;
	constexpr uint32_t k_Start = 1000;
	const TurnAt turn(k_Start);
	auto& registry = test::creature_world::World::Registry();
	const auto tree = registry.Create();
	registry.Assign<ecs::components::Transform>(tree, glm::vec3(110.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto treeReaction = [&](entt::entity who) {
		const auto id = Locator::reactionSystem::value().Add(
		    {.initiator = tree, .type = openblack::Reaction::ReactToMagicTree, .player = PlayerNames::PLAYER_ONE});
		ecs::creature_reactions::HandleReaction(who, *reactions::Find(id), 10.0f);
		return id;
	};
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	MakePlayer(PlayerNames::PLAYER_TWO, 2);
	const auto computers = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 120.0f), PlayerNames::PLAYER_TWO);
	// a computer player's creature: nothing at all
	const auto ignored = treeReaction(computers);
	EXPECT_EQ(reactions::RecordTurn(computers, k_TreeType), 0u);
	EXPECT_EQ(reactions::Find(ignored)->turnCreated, 0u);
	// the human's: noticed, not taken up
	const auto noticed = treeReaction(creature);
	EXPECT_EQ(reactions::RecordTurn(creature, k_TreeType), k_Start);
	EXPECT_EQ(reactions::Find(noticed)->turnCreated, k_Start);
	EXPECT_TRUE(log.empty());
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(computers), 0u);
}

TEST_F(CreatureReactionHandlerTest, ItSwitchesToAReactionThatMattersMoreOnceItHasFollowedItsOwnLongEnough)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	world.Info().reaction.at(k_ShieldType).priority = 250;
	constexpr uint32_t k_Start = 1000;
	const TurnAt turn(k_Start);
	const auto nasty = Miracle(MagicType::Fireball);
	const auto first = React(nasty, openblack::Reaction::FleeFromSpell, 60.0f);
	ASSERT_EQ(ecs::creature_reactions::ReactionOf(creature), first);
	ASSERT_EQ(log.size(), 1u);
	// the shield matters more, but the reaction it follows has lasted less than ten seconds
	game_clock::SetTurn(k_Start + 99);
	React(Miracle(MagicType::PhysicalShield), openblack::Reaction::ReactToMagicShield, 0.0f);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), first);
	EXPECT_EQ(log.size(), 1u);
	// ten seconds on it takes the shield up, its record set to now
	game_clock::SetTurn(k_Start + 100);
	const auto second = React(Miracle(MagicType::PhysicalShield), openblack::Reaction::ReactToMagicShield, 0.0f);
	ASSERT_EQ(log.size(), 2u);
	EXPECT_EQ(log.back().name, "mind.ReactToNiceMagic");
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), second);
	EXPECT_EQ(reactions::RecordTurn(creature, k_ShieldType), k_Start + 100);
	// the plan forced on it to look stopped the old reaction, its record refreshed, and its end no longer reaches the
	// creature
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start + 100);
	reactions::RemoveAllReactionsInitiatedByObject(nasty);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), second);
}

// A nice miracle it only takes up, without going to look, forces no plan: the reaction it followed keeps its record
TEST_F(CreatureReactionHandlerTest, ItLeavesTheReactionItFollowedAsItWasForANiceMiracleItDoesNotLookAt)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	world.Info().reaction.at(k_ShieldType).priority = 250;
	constexpr uint32_t k_Start = 1000;
	const TurnAt turn(k_Start);
	const auto first = React(Miracle(MagicType::Fireball), openblack::Reaction::FleeFromSpell, 60.0f);
	ASSERT_EQ(ecs::creature_reactions::ReactionOf(creature), first);
	// a shield of a seed already learnt from: taken up, not looked at
	auto& registry = test::creature_world::World::Registry();
	const auto seed = registry.Create();
	registry.Assign<ecs::components::SpellSeed>(seed).learnedFrom = true;
	const auto shield = Miracle(MagicType::PhysicalShield);
	registry.Get<ecs::components::Spell>(shield).seed = seed;
	game_clock::SetTurn(k_Start + 100);
	const auto second = React(shield, openblack::Reaction::ReactToMagicShield, 0.0f);
	EXPECT_EQ(log.size(), 1u);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), second);
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start);
}

namespace
{
constexpr auto k_FireType = static_cast<uint8_t>(openblack::Reaction::ReactToFire);

/// The fire's row as the game's data has it (priority 210 within 35 m, the distance of no weight, turns to react
/// 100000 and none before reacting again), every species reacting to it, and a burning field at a point: a field
/// reaches 5 m about it and, having no mesh, stands no height, so any creature is big enough beside it
entt::entity BurningField(test::creature_world::World& world, glm::vec3 position)
{
	auto& row = world.Info().reaction.at(k_FireType);
	row.priority = 210;
	row.maxReactionDistance = 35.0f;
	row.howImportantIsDistance = 0.0f;
	row.numGameTurnsForCreatureToReact = 100000;
	row.numGameTurnsForCreatureBeforeReactingAgain = 0;
	world.Info().creature.at(0).isReacting.isReactingToAbodeBurning = 1;
	auto& registry = test::creature_world::World::Registry();
	const auto field = registry.Create();
	registry.Assign<ecs::components::Transform>(field, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Field>(field).town = -1;
	return field;
}

uint32_t FireReaction(entt::entity creature, entt::entity burning, float distance)
{
	const auto id = Locator::reactionSystem::value().Add(
	    {.initiator = burning, .type = openblack::Reaction::ReactToFire, .player = PlayerNames::PLAYER_ONE});
	ecs::creature_reactions::HandleReaction(creature, *reactions::Find(id), distance);
	return id;
}
} // namespace

// A creature takes a fire up only when it stands strictly within the burning thing's reach and its own; then its mind is
// told of the burning thing and it follows the fire's reaction (docs/wiki/world/fire.md#the-creature-and-a-fire)
TEST_F(CreatureReactionHandlerTest, AFireIsTakenUpOnlyWithinTheBurningThingsReachAndItsOwn)
{
	const float reach = 5.0f + ecs::object::Get2DRadius(creature);
	const auto far = BurningField(world, glm::vec3(100.0f + reach + 1.0f, 0.0f, 100.0f));
	FireReaction(creature, far, 10.0f);
	EXPECT_TRUE(log.empty());
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), 0u);
	const auto near = BurningField(world, glm::vec3(100.0f + reach - 1.0f, 0.0f, 100.0f));
	const auto id = FireReaction(creature, near, 10.0f);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "mind.ReactToFire");
	EXPECT_EQ(log.front().args, (std::vector<float> {Id(creature), Id(near)}));
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), id);
	const auto& state = test::creature_world::World::Registry().Get<const ecs::components::CreatureReaction>(creature);
	EXPECT_EQ(state.object, near);
}

// Switching to a fire, the plan the fire forces first stops the reaction the creature followed, so that reaction's
// record is refreshed to the turn it left it, unlike a switch to a shield
TEST_F(CreatureReactionHandlerTest, ACreatureLeavesTheReactionItFollowedForAFireAndRefreshesItsRecord)
{
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	constexpr uint32_t k_Start = 1000;
	const TurnAt turn(k_Start);
	const auto nasty = Miracle(MagicType::Fireball);
	const auto first = React(nasty, openblack::Reaction::FleeFromSpell, 60.0f);
	ASSERT_EQ(ecs::creature_reactions::ReactionOf(creature), first);
	const auto field = BurningField(world, glm::vec3(101.0f, 0.0f, 100.0f));
	game_clock::SetTurn(k_Start + 100);
	const auto fire = FireReaction(creature, field, 1.0f);
	ASSERT_EQ(log.size(), 2u);
	EXPECT_EQ(log.back().name, "mind.ReactToFire");
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), fire);
	EXPECT_EQ(reactions::RecordTurn(creature, k_FireType), k_Start + 100);
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start + 100);
	// the old reaction's end no longer reaches it
	reactions::RemoveAllReactionsInitiatedByObject(nasty);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), fire);
}

namespace
{
/// The reaction table's row of a kind the hand starts, and every species reacting to it
void HandReactionRow(test::creature_world::World& world, openblack::Reaction type)
{
	auto& row = world.Info().reaction.at(static_cast<size_t>(type));
	row.priority = 40;
	row.maxReactionDistance = 64.0f;
	row.howImportantIsDistance = 0.5f;
	row.numGameTurnsForCreatureToReact = 100;
	row.numGameTurnsForCreatureBeforeReactingAgain = 200;
	auto& reacting = world.Info().creature.at(0).isReacting;
	reacting.isReactingToHandUsingTotem = 1;
	reacting.isReactingToHandPuttingStuffInStoragePit = 1;
}

/// A thing at a point, as the totem or the storage pit the hand used
entt::entity ThingAt(glm::vec3 point)
{
	auto& registry = test::creature_world::World::Registry();
	const auto thing = registry.Create();
	registry.Assign<ecs::components::Transform>(thing, point, glm::mat3(1.0f), glm::vec3(1.0f));
	return thing;
}
} // namespace

// The hand using a totem the creature sees (in its own cell): it takes the reaction up and watches the totem skill; the
// hand putting things in a storage pit: it takes that up and watches the storage pit skill; a computer player's creature
// takes neither up (docs/wiki/creature/mind.md#who-reports-a-sighting)
TEST_F(CreatureReactionHandlerTest, TheHandUsingATotemOrAStoragePitShowsItsCreatureTheSkill)
{
	HandReactionRow(world, openblack::Reaction::ReactToHandUsingTotem);
	HandReactionRow(world, openblack::Reaction::ReactToHandPuttingStuffInStoragePit);
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	MakePlayer(PlayerNames::PLAYER_TWO, 2);
	const auto computers = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f), PlayerNames::PLAYER_TWO);
	const auto handUses = [&](entt::entity who, entt::entity thing, openblack::Reaction type) {
		const auto id =
		    Locator::reactionSystem::value().Add({.initiator = thing, .type = type, .player = PlayerNames::PLAYER_ONE});
		ecs::creature_reactions::HandleReaction(who, *reactions::Find(id), 1.0f);
		return id;
	};
	const auto totem = ThingAt(glm::vec3(101.0f, 0.0f, 101.0f));
	const auto used = handUses(creature, totem, openblack::Reaction::ReactToHandUsingTotem);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "mind.WatchSkill");
	EXPECT_EQ(log.front().args, (std::vector<float> {Id(creature), static_cast<float>(CreatureActionKnownAbout::UseTotem)}));
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), used);

	const auto pit = ThingAt(glm::vec3(301.0f, 0.0f, 301.0f));
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 140.0f), PlayerNames::PLAYER_ONE);
	const auto filled = handUses(other, pit, openblack::Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_EQ(log.size(), 2u);
	EXPECT_EQ(log.back().name, "mind.WatchSkill");
	EXPECT_EQ(log.back().args, (std::vector<float> {Id(other), static_cast<float>(CreatureActionKnownAbout::UseStoragePit)}));
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(other), filled);

	// the computer's creature: neither
	handUses(computers, totem, openblack::Reaction::ReactToHandUsingTotem);
	handUses(computers, pit, openblack::Reaction::ReactToHandPuttingStuffInStoragePit);
	EXPECT_EQ(log.size(), 2u);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(computers), 0u);
}
