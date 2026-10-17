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
// miracle gone), and a creature switching to a reaction that matters more once it has followed its own long enough

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureReactionRules.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Systems/Implementations/CreatureReactions.h"
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
	EXPECT_FALSE(IsAvailableForReaction({.mimicking = true}, k_MimicPriority));
	EXPECT_TRUE(IsAvailableForReaction({.mimicking = true}, k_MimicPriority + 1));
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
	// the old reaction's record is left as it was, and its end no longer reaches the creature
	EXPECT_EQ(reactions::RecordTurn(creature, k_FleeType), k_Start);
	reactions::RemoveAllReactionsInitiatedByObject(nasty);
	EXPECT_EQ(ecs::creature_reactions::ReactionOf(creature), second);
}
