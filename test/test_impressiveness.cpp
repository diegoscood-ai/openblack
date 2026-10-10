/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How much a miracle impresses the villagers who react to it (Magic/Impressiveness and
// villager_reactions::UpdateHowImpressed, docs/bw1-notes/villagers.md "Belief from the miracles villagers react to"):
// the pure rules with made-up values (raffclar's tests of them, kept where our numbers agree), then the impression of
// one villager on a fake town, villager and miracle: the belief its town gains, the player's alignment, the town's
// boredom and the shield's overrides. The reactions are made with no map: CreateReaction does not spread them.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>
#include <utility>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Services/Guidance.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownStores.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Magic/Impressiveness.h"
#include "creature/CreatureSystemFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace tb = openblack::ecs::town_belief;
namespace vr = openblack::ecs::villager_reactions;
namespace reactions = openblack::ecs::effects::reactions;
namespace alignment = openblack::ecs::effects::alignment;

// ---- the pure rules ---------------------------------------------------------------------------------------------

TEST(Impressiveness, CloseByAllOfItAtTheEdgeAboutATenth)
{
	// the distance's share is our gutils::DistanceChangeToBelief (raffclar's own copy of the curve gives the same)
	EXPECT_FLOAT_EQ(gutils::DistanceChangeToBelief(0.0f, 60.0f), 1.0f);
	EXPECT_NEAR(gutils::DistanceChangeToBelief(60.0f, 60.0f), 0.11443728f, 1e-6f);
	EXPECT_NEAR(gutils::DistanceChangeToBelief(600.0f, 60.0f), 0.11443728f, 1e-6f);
	EXPECT_GT(gutils::DistanceChangeToBelief(20.0f, 60.0f), gutils::DistanceChangeToBelief(50.0f, 60.0f));
}

TEST(Impressiveness, ItMultipliesTheLandsBalanceTheMiracleTheReactionAndTheBoredom)
{
	const float value = magic::ImpressiveValue({.landBalance = 2.0f,
	                                            .impressiveValue = 3.0f,
	                                            .reactionMultiplier = 0.5f,
	                                            .distance = 0.0f,
	                                            .maxDistance = 60.0f,
	                                            .power = 1.0f,
	                                            .boredom = 0.25f});
	EXPECT_FLOAT_EQ(value, 2.0f * 3.0f * 0.5f * 0.25f);
}

TEST(Impressiveness, TheFactorsGoInTheOriginalsOrder)
{
	// the watcher's side (boredom, power, distance) times the miracle's side (reaction, value, land), each grouped
	const magic::ImpressionInputs in {.landBalance = 1.3f,
	                                  .impressiveValue = 0.7f,
	                                  .reactionMultiplier = 0.9f,
	                                  .distance = 17.0f,
	                                  .maxDistance = 50.0f,
	                                  .power = 1.0f,
	                                  .boredom = 0.83f};
	const float watcher = in.boredom * in.power * gutils::DistanceChangeToBelief(in.distance, in.maxDistance);
	const float miracle = in.reactionMultiplier * in.impressiveValue * in.landBalance;
	EXPECT_EQ(magic::ImpressiveValue(in), watcher * miracle);
}

TEST(Impressiveness, FoodAndWoodImpressByTheTownsWant)
{
	EXPECT_FLOAT_EQ(magic::ReactionMultiplier(Reaction::ReactToFood, 0.3f, 0.75f), 0.75f);
	EXPECT_FLOAT_EQ(magic::ReactionMultiplier(Reaction::ReactToWood, 0.3f, 1.5f), 1.5f);
	// no town or no desire named: exactly 1
	EXPECT_FLOAT_EQ(magic::ReactionMultiplier(Reaction::ReactToFood, 0.3f, std::nullopt), 1.0f);
	// every other reaction by its table's multiplier
	EXPECT_FLOAT_EQ(magic::ReactionMultiplier(Reaction::LookAtNiceSpell, 0.3f, 0.75f), 0.3f);
}

TEST(Impressiveness, AnImpressionMovesTheAlignmentByItsKindAndTheTownsDesire)
{
	EXPECT_FLOAT_EQ(magic::ImpressionAlignment(-0.01f, std::nullopt), -0.01f);
	EXPECT_FLOAT_EQ(magic::ImpressionAlignment(0.01f, 0.5f), 0.005f);
}

TEST(Impressiveness, ShieldOverrides)
{
	// standing or struck: nothing for a town whose last attacker is the shield's player
	EXPECT_EQ(magic::ShieldImpressiveValue(Reaction::ReactToMagicShield, true, 0.6f), 0.0f);
	EXPECT_EQ(magic::ShieldImpressiveValue(Reaction::ReactToMagicShieldStruck, true, 0.6f), 0.0f);
	EXPECT_EQ(magic::ShieldImpressiveValue(Reaction::ReactToMagicShield, false, 0.6f), 0.6f);
	EXPECT_EQ(magic::ShieldImpressiveValue(Reaction::ReactToMagicShieldStruck, false, 0.6f), 0.6f);
	// destroyed: four times, whoever attacked the town
	EXPECT_EQ(magic::ShieldImpressiveValue(Reaction::ReactToMagicShieldDestroyed, true, 0.6f), 0.6f * 4.0f);
	EXPECT_EQ(magic::ShieldImpressiveValue(Reaction::ReactToMagicShieldDestroyed, false, 0.6f), 0.6f * 4.0f);
}

TEST(Impressiveness, TownShare)
{
	EXPECT_EQ(magic::TownShare(30.0f, 20), (30.0f + 0.001f) / (20.0f + 0.001f));
	// an empty town divides by the small amount alone
	EXPECT_EQ(magic::TownShare(30.0f, 0), (30.0f + 0.001f) / 0.001f);
}

TEST(Impressiveness, BoredomNeverBelowNothing)
{
	// the step (the belief table's boredom, negative, times the share) is added, never below 0
	TownBelief belief {};
	belief.boredom.fill(1.0f);
	tb::AddToBoredomMultiplier(belief, 3, -0.02f * 0.5f);
	EXPECT_FLOAT_EQ(belief.boredom.at(3), 0.99f);
	belief.boredom.at(3) = 0.005f;
	tb::AddToBoredomMultiplier(belief, 3, -0.02f);
	EXPECT_EQ(belief.boredom.at(3), 0.0f);
}

// ---- one villager's impression ------------------------------------------------------------------------------------

namespace
{
constexpr size_t k_One = static_cast<size_t>(PlayerNames::PLAYER_ONE);
constexpr float k_FireballValue = 2.0f;
constexpr float k_ShieldValue = 0.5f;
constexpr float k_Unmodified = 30.0f;
constexpr float k_BoredomOfMe = -0.02f;

class ImpressionTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		info->town.populationForUnmodifiedBelief = k_Unmodified;
		info->town.beliefInNeutralPlayer = 0.5f;
		info->belief.defaultBoredomOfMe = k_BoredomOfMe;
		info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)).impressiveValue = k_FireballValue;
		info->magicEffect.at(static_cast<size_t>(MagicType::Shield)).impressiveValue = k_ShieldValue;
		// info.dat's rows 3 FLEE_FROM_SPELL and 13 REACT_TO_MAGIC_SHIELD (no desire, for the test, on 13)
		for (const auto [type, modifier] : {std::pair {3u, -0.01f}, std::pair {13u, 0.05f}})
		{
			auto& r = info->reaction.at(type);
			r.maxReactionDistance = type == 3 ? 50.0f : 35.0f;
			r.defaultReactionImpressiveMultiplier = 1.0f;
			r.correspondingTownDesire = TownDesireInfo::None;
			r.correspondingTownDesireForAlignment = TownDesireInfo::None;
			r.alignmentModifier = modifier;
			r.alignmentForSFX = GuidanceAlignment::Good;
		}
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		land_balance::Reset();
		reactions::Clear();
		tb::detail::ClearForTests();
		game_clock::SetTurn(100);
		ResetAlignment();
	}

	void TearDown() override
	{
		ResetAlignment();
		reactions::Clear();
		tb::detail::ClearForTests();
		land_balance::Reset();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static void ResetAlignment()
	{
		auto& one = alignment::Of(PlayerNames::PLAYER_ONE);
		one.value = 0.0f;
		one.pending = 0.0f;
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A town of `people` adults
	static entt::entity MakeTown(uint32_t people, int id = 1)
	{
		const auto town = ecs::archetypes::TownArchetype::Create(id, glm::vec3(50.0f * static_cast<float>(id), 0.0f, 50.0f),
		                                                         PlayerNames::NEUTRAL, Tribe::CELTIC);
		Reg().Get<Town>(town).stats.adults = people;
		return town;
	}

	static entt::entity MakeVillager(entt::entity town)
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.life = 1.0f;
		v.town = town;
		v.abode = entt::null;
		Reg().Assign<Transform>(e, glm::vec3(100.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}

	/// A miracle of PLAYER_ONE 10 m from the villager
	static entt::entity MakeMiracle(MagicType type, SpellClass spellClass, bool hasPlayer = true)
	{
		const auto e = Reg().Create();
		auto& spell = Reg().Assign<Spell>(e);
		spell.magicType = type;
		spell.spellClass = spellClass;
		spell.position = {110.0f, 0.0f, 130.0f};
		spell.player = hasPlayer ? PlayerNames::PLAYER_ONE : PlayerNames::NEUTRAL;
		spell.hasPlayer = hasPlayer;
		return e;
	}

	static TownBelief& BeliefOf(entt::entity town) { return Reg().Get<Town>(town).belief; }

	/// The share of a villager of a town of `people`, and the value at 10 m of a reach of `reach`
	static float Share(uint32_t people) { return magic::TownShare(k_Unmodified, people); }
	static float At10(float reach, float value, float boredom = 1.0f)
	{
		return boredom * 1.0f * gutils::DistanceChangeToBelief(10.0f, reach) * (1.0f * value * 1.0f);
	}
};
} // namespace

TEST_F(ImpressionTest, TheTownGainsBeliefInTheCaster)
{
	const auto town = MakeTown(20);
	const auto villager = MakeVillager(town);
	const auto spell = MakeMiracle(MagicType::Fireball, SpellClass::General);
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	vr::UpdateHowImpressed(villager, id, false);
	const float expected = Share(20) * At10(50.0f, k_FireballValue);
	auto& belief = BeliefOf(town);
	// into the pending belief (folded at the town's turn) and the recent one, stamped with the turn
	EXPECT_EQ(belief.pending.at(k_One), expected);
	EXPECT_EQ(belief.recent.at(k_One), expected);
	EXPECT_EQ(belief.lastAddedTurn.at(k_One), 100u);
	// no other player gains anything
	EXPECT_EQ(belief.pending.at(static_cast<size_t>(PlayerNames::NEUTRAL)), 0.0f);
	// the player's alignment moves by the reaction's own amount (here evil, with no lean yet: as it is)
	EXPECT_FLOAT_EQ(alignment::Of(PlayerNames::PLAYER_ONE).pending, -0.01f);
	// and the town tires of running from miracles by the villager's share
	EXPECT_EQ(belief.boredom.at(3), 1.0f + k_BoredomOfMe * Share(20));
}

TEST_F(ImpressionTest, AgainItImpressesLessForTheBoredom)
{
	const auto town = MakeTown(20);
	const auto villager = MakeVillager(town);
	const auto spell = MakeMiracle(MagicType::Fireball, SpellClass::General);
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::PLAYER_ONE, false);
	vr::UpdateHowImpressed(villager, id, false);
	const float first = BeliefOf(town).pending.at(k_One);
	const float boredom = BeliefOf(town).boredom.at(3);
	ASSERT_LT(boredom, 1.0f);
	vr::UpdateHowImpressed(villager, id, false);
	EXPECT_EQ(BeliefOf(town).pending.at(k_One), first + Share(20) * At10(50.0f, k_FireballValue, boredom));
}

TEST_F(ImpressionTest, ASmallerTownGainsMoreAVillager)
{
	const auto small = MakeTown(5, 1);
	const auto big = MakeTown(50, 2);
	const auto spell = MakeMiracle(MagicType::Fireball, SpellClass::General);
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::PLAYER_ONE, false);
	vr::UpdateHowImpressed(MakeVillager(small), id, false);
	vr::UpdateHowImpressed(MakeVillager(big), id, false);
	EXPECT_GT(BeliefOf(small).pending.at(k_One), BeliefOf(big).pending.at(k_One));
}

TEST_F(ImpressionTest, NoTownOrNoPlayerNothing)
{
	const auto town = MakeTown(20);
	// a villager with no town
	const auto homeless = MakeVillager(entt::null);
	const auto spell = MakeMiracle(MagicType::Fireball, SpellClass::General);
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::PLAYER_ONE, false);
	vr::UpdateHowImpressed(homeless, id, false);
	// a miracle with no player
	const auto villager = MakeVillager(town);
	const auto nobody = MakeMiracle(MagicType::Fireball, SpellClass::General, false);
	const auto none = reactions::CreateReaction(nobody, Reaction::FleeFromSpell, PlayerNames::NEUTRAL, false);
	vr::UpdateHowImpressed(villager, none, false);
	// a reaction to something that is not a miracle (pending: its value)
	const auto log = Reg().Create();
	Reg().Assign<Transform>(log, glm::vec3(105.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto wood = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::PLAYER_ONE, false);
	vr::UpdateHowImpressed(villager, wood, false);
	for (const float pending : BeliefOf(town).pending)
	{
		EXPECT_EQ(pending, 0.0f);
	}
	EXPECT_EQ(BeliefOf(town).boredom.at(3), 1.0f);
	EXPECT_EQ(alignment::Of(PlayerNames::PLAYER_ONE).pending, 0.0f);
}

TEST_F(ImpressionTest, AShieldImpressesNothingTheTownItsPlayerAttacked)
{
	const auto town = MakeTown(20);
	const auto villager = MakeVillager(town);
	const auto shield = MakeMiracle(MagicType::Shield, SpellClass::Shield);
	const auto id = reactions::CreateReaction(shield, Reaction::ReactToMagicShield, PlayerNames::PLAYER_ONE, false);
	// never attacked: as another miracle
	vr::UpdateHowImpressed(villager, id, false);
	const float once = Share(20) * At10(35.0f, k_ShieldValue);
	EXPECT_EQ(BeliefOf(town).pending.at(k_One), once);
	// attacked last by the shield's player: nothing more, though the alignment and the boredom still move
	auto& record = Reg().Get<Town>(town).aggression;
	record.lastAggressor = PlayerNames::PLAYER_ONE;
	record.lastTurn = 90;
	const float pendingAlignment = alignment::Of(PlayerNames::PLAYER_ONE).pending;
	const float boredom = BeliefOf(town).boredom.at(13);
	vr::UpdateHowImpressed(villager, id, false);
	EXPECT_EQ(BeliefOf(town).pending.at(k_One), once);
	EXPECT_GT(alignment::Of(PlayerNames::PLAYER_ONE).pending, pendingAlignment);
	EXPECT_LT(BeliefOf(town).boredom.at(13), boredom);
	// attacked last by another player: impressed again
	record.lastAggressor = PlayerNames::PLAYER_TWO;
	vr::UpdateHowImpressed(villager, id, false);
	EXPECT_GT(BeliefOf(town).pending.at(k_One), once);
}

// The belief sound of AddToBelief with a thing (the next step of an impression): asked for only with the local
// interface's hand and for a player below the town's strongest belief. Its gate is seen through the local draws: the
// sound's time check draws twice (the interval) whenever it is asked
TEST_F(ImpressionTest, TheBeliefSoundIsAskedOnlyForAPlayerBelowTheStrongest)
{
	int draws = 0;
	audio::guidance::SetRandom([&draws](uint32_t) {
		++draws;
		return 0u;
	});
	const auto town = MakeTown(20);
	const auto villager = MakeVillager(town);
	BeliefOf(town).belief.at(static_cast<size_t>(PlayerNames::NEUTRAL)) = 0.5f;
	// no hand: no sound
	ecs::town_stores::AddToBelief(town, PlayerNames::PLAYER_ONE, 0.1f, villager, false, 1);
	EXPECT_EQ(draws, 0);
	Locator::handSystem::emplace<test::creature_fakes::FakeHand>();
	// below the neutral player's belief: asked for (at least the interval's two draws)
	ecs::town_stores::AddToBelief(town, PlayerNames::PLAYER_ONE, 0.1f, villager, false, 1);
	EXPECT_GE(draws, 2);
	const int asked = draws;
	// the strongest: nothing
	BeliefOf(town).belief.at(k_One) = 1.0f;
	ecs::town_stores::AddToBelief(town, PlayerNames::PLAYER_ONE, 0.1f, villager, false, 1);
	EXPECT_EQ(draws, asked);
	// no thing: nothing
	BeliefOf(town).belief.at(k_One) = 0.0f;
	ecs::town_stores::AddToBelief(town, PlayerNames::PLAYER_ONE, 0.1f, entt::null, false, 1);
	EXPECT_EQ(draws, asked);
	Locator::handSystem::reset();
	audio::guidance::SetRandom({});
}
