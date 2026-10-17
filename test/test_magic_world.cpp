/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world as the miracles see it (magic::MagicWorldInterface): what its defaults do, on the tests' fake world; and
// the game's world over our own code: the map's edges, the sea and the low land on a small island, where objects stand,
// what a miracle may destroy, the heal's cure and its search in the spiral's order, and an effect at a point that
// reaches every object about it while the old call still answers the last of them. The test holds the registry, the
// island and the world lists; the locator only injects them.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/EffectSourceRules.h"
#include "Magic/MagicWorldInterface.h"
#include "support/LandFakes.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using ecs::components::Transform;
using ecs::systems::GameMagicWorld;

namespace
{
/// A 16 x 16 cell island: dry land at altitude 10 everywhere but the sea cell (2, 2), the low land (3, 3) at altitude 2
/// and the cell (4, 4) of high land with water in it
class GameMagicWorldLand: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& land = Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {1});
		auto& island = static_cast<test::WaterCellIsland&>(land);
		for (uint16_t x = 0; x < 16; ++x)
		{
			for (uint16_t z = 0; z < 16; ++z)
			{
				island.Cell({x, z}) = test::MakeWaterCell(10, false, false);
			}
		}
		island.Cell({2, 2}) = test::MakeWaterCell(0, true, false);
		island.Cell({3, 3}) = test::MakeWaterCell(2, false, false);
		island.Cell({4, 4}) = test::MakeWaterCell(10, true, false);
	}

	GameMagicWorld _world;

private:
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
};

/// Objects in the map's cells on no land, with the world lists the cells and the effects reach
class GameMagicWorldObjects: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		ecs::map_cells::Clear();
		ecs::object_index::OnLoadMap();
	}
	void TearDown() override
	{
		ecs::map_cells::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// An object standing at a point, not in the map's cells
	static entt::entity Make(const glm::vec3& position)
	{
		const auto entity = Reg().Create();
		ecs::object_index::Assign(entity);
		Reg().Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}
	/// A lion (a living thing the heal heals) at a point, put in its cell's mobile list, at the head
	static entt::entity LionAt(const glm::vec3& position)
	{
		const auto entity = Make(position);
		Reg().Assign<ecs::components::Animal>(entity).type = AnimalInfo::Lion;
		ecs::map_cells::InsertMapObject(entity);
		return entity;
	}

	GameMagicWorld _world;

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};

using Entities = std::vector<entt::entity>;
} // namespace

TEST(MagicWorld, TheHandIsInItsPlayersInfluenceWhereverThePointIs)
{
	test::FakeMagicWorld world;
	const magic::MagicWorldInterface& seen = world;
	EXPECT_TRUE(seen.HandInInfluence(PlayerNames::PLAYER_ONE, glm::vec3(10.0f, 0.0f, 10.0f)));
	world.influence = false;
	EXPECT_FALSE(seen.HandInInfluence(PlayerNames::PLAYER_ONE, glm::vec3(10.0f, 0.0f, 10.0f)));
}

TEST(MagicWorld, ByDefaultNoMiracleDestroysAnythingAndTheOtherHooksDoNothing)
{
	test::FakeMagicWorld world;
	magic::MagicWorldInterface& seen = world;
	EXPECT_FALSE(seen.CanBeDestroyedBySpell(entt::entity {1}));
	seen.HeldThingUsedOnLand(PlayerNames::PLAYER_ONE);
	seen.CurePoison(entt::entity {1});
	seen.PlayerAffected(entt::entity {1}, MagicType::Heal, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(world.applied.empty());
	EXPECT_TRUE(world.appliedAt.empty());
}

TEST(MagicWorld, TheFakeRecordsWhatTheMiraclesDo)
{
	test::FakeMagicWorld world;
	world.objects[entt::entity {3}] = glm::vec3(1.0f, 0.0f, 2.0f);
	world.objects[entt::entity {4}] = glm::vec3(5.0f, 0.0f, 6.0f);
	magic::MagicWorldInterface& seen = world;
	EXPECT_EQ(seen.PositionOf(entt::entity {3}), glm::vec3(1.0f, 0.0f, 2.0f));
	EXPECT_FALSE(seen.PositionOf(entt::entity {9}).has_value());
	EXPECT_EQ(seen.HealTargets(glm::vec3(0.0f), 7.0f, 1).size(), 1u);
	EXPECT_EQ(world.lastHealRadius, 7.0f);
	seen.Water({.position = glm::vec3(2.0f, 0.0f, 2.0f), .ringGrowth = 0.5f, .extreme = true});
	ASSERT_EQ(world.drops.size(), 1u);
	EXPECT_EQ(world.rings.front(), 0.5f);
	EXPECT_TRUE(world.extremeDrops.front());
	EXPECT_TRUE(seen.AddResource(ResourceType::Food, glm::vec3(1.0f), 10, true, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(world.resources.size(), 1u);
	EXPECT_EQ(world.resources.front().amount, 10u);
	EXPECT_FALSE(seen.IsDryLand(glm::vec3(-1.0f, 0.0f, 1.0f)));
}

TEST(MagicWorld, TheMiraclesServiceHandsOutItsOneWorldAndTheInertOneTheFake)
{
	ecs::systems::MagicSystem miracles;
	EXPECT_EQ(&miracles.World(), &miracles.World());
	EXPECT_NE(dynamic_cast<GameMagicWorld*>(&miracles.World()), nullptr);
	test::InertMagicSystem inert;
	EXPECT_EQ(&inert.World(), &inert.World());
	EXPECT_NE(dynamic_cast<test::FakeMagicWorld*>(&inert.World()), nullptr);
}

TEST_F(GameMagicWorldLand, TheLandsHeightIsTheIslandsAndNoneWithoutOne)
{
	EXPECT_EQ(_world.LandHeight(glm::vec2(55.0f, 55.0f)), test::WaterCellIsland::k_HeightMarker);
	Locator::terrainSystem::reset();
	EXPECT_EQ(_world.LandHeight(glm::vec2(55.0f, 55.0f)), 0.0f);
}

TEST_F(GameMagicWorldLand, OnTheMapIsWithinItsCellsAHairWestOfTheEdgeTruncatingOntoIt)
{
	EXPECT_TRUE(_world.InBounds(glm::vec3(0.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(_world.InBounds(glm::vec3(159.9f, 0.0f, 159.9f)));
	EXPECT_FALSE(_world.InBounds(glm::vec3(160.0f, 0.0f, 5.0f)));
	EXPECT_FALSE(_world.InBounds(glm::vec3(5.0f, 0.0f, 160.0f)));
	// a point whose map coordinate truncates to 0 is on the first cell; a metre west of it is off the map
	EXPECT_TRUE(_world.InBounds(glm::vec3(-0.0001f, 0.0f, 5.0f)));
	EXPECT_FALSE(_world.InBounds(glm::vec3(-1.0f, 0.0f, 5.0f)));
	EXPECT_FALSE(_world.InBounds(glm::vec3(5.0f, 0.0f, -1.0f)));
}

TEST_F(GameMagicWorldLand, LandIsACellWithoutWaterAndDryLandOneHighAboveTheSea)
{
	// the dry land
	EXPECT_TRUE(_world.IsLand(glm::vec3(55.0f, 0.0f, 55.0f)));
	EXPECT_TRUE(_world.IsDryLand(glm::vec3(55.0f, 0.0f, 55.0f)));
	// the sea
	EXPECT_FALSE(_world.IsLand(glm::vec3(25.0f, 0.0f, 25.0f)));
	EXPECT_FALSE(_world.IsDryLand(glm::vec3(25.0f, 0.0f, 25.0f)));
	// low land: land, but not high enough for a pile
	EXPECT_TRUE(_world.IsLand(glm::vec3(35.0f, 0.0f, 35.0f)));
	EXPECT_FALSE(_world.IsDryLand(glm::vec3(35.0f, 0.0f, 35.0f)));
	// high land with water in its cell: dry land by its height, but no land for a miracle held over land
	EXPECT_FALSE(_world.IsLand(glm::vec3(45.0f, 0.0f, 45.0f)));
	EXPECT_TRUE(_world.IsDryLand(glm::vec3(45.0f, 0.0f, 45.0f)));
	// off the map neither
	EXPECT_FALSE(_world.IsLand(glm::vec3(-5.0f, 0.0f, 55.0f)));
	EXPECT_FALSE(_world.IsDryLand(glm::vec3(-5.0f, 0.0f, 55.0f)));
	EXPECT_FALSE(_world.IsLand(glm::vec3(165.0f, 0.0f, 55.0f)));
	EXPECT_FALSE(_world.IsDryLand(glm::vec3(165.0f, 0.0f, 55.0f)));
}

TEST_F(GameMagicWorldObjects, AnObjectStandsWhereItsTransformIsAndNowhereOnceGone)
{
	const auto object = Make(glm::vec3(12.0f, 3.0f, 14.0f));
	EXPECT_EQ(_world.PositionOf(object), glm::vec3(12.0f, 3.0f, 14.0f));
	const auto placeless = Reg().Create();
	EXPECT_FALSE(_world.PositionOf(placeless).has_value());
	Reg().Destroy(object);
	EXPECT_FALSE(_world.PositionOf(object).has_value());
	EXPECT_FALSE(_world.PositionOf(entt::null).has_value());
}

TEST_F(GameMagicWorldObjects, NoMiracleDestroysACreatureAFieldOrATemple)
{
	const auto creature = Make(glm::vec3(10.0f, 0.0f, 10.0f));
	Reg().Assign<ecs::components::Creature>(creature);
	const auto field = Make(glm::vec3(20.0f, 0.0f, 10.0f));
	Reg().Assign<ecs::components::Field>(field);
	const auto temple = Make(glm::vec3(30.0f, 0.0f, 10.0f));
	Reg().Assign<ecs::components::Temple>(temple, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_world.CanBeDestroyedBySpell(creature));
	EXPECT_FALSE(_world.CanBeDestroyedBySpell(field));
	EXPECT_FALSE(_world.CanBeDestroyedBySpell(temple));
	// anything else that takes effects may be destroyed, but not once it is going
	const auto rock = Make(glm::vec3(40.0f, 0.0f, 10.0f));
	EXPECT_TRUE(_world.CanBeDestroyedBySpell(rock));
	Reg().Assign<ecs::components::Unavailable>(rock);
	EXPECT_FALSE(_world.CanBeDestroyedBySpell(rock));
}

TEST_F(GameMagicWorldObjects, TheHealCuresPoisonAndNothingElse)
{
	const auto poisoned = Make(glm::vec3(10.0f, 0.0f, 10.0f));
	Reg().Assign<ecs::components::Poisoned>(poisoned);
	const auto healthy = Make(glm::vec3(20.0f, 0.0f, 10.0f));
	_world.CurePoison(poisoned);
	_world.CurePoison(healthy);
	EXPECT_FALSE(Reg().AllOf<ecs::components::Poisoned>(poisoned));
	EXPECT_FALSE(Reg().AllOf<ecs::components::Poisoned>(healthy));
	EXPECT_TRUE(Reg().Valid(poisoned));
	Reg().Destroy(healthy);
	_world.CurePoison(healthy); // gone: nothing to cure
}

TEST_F(GameMagicWorldObjects, TheHealFindsItsTargetsInTheSpiralsOrderNoMoreThanItsMaximum)
{
	// one in the next cell west, filed first; two in the point's cell, the later at the head of its list; one far away
	const auto west = LionAt(glm::vec3(97.0f, 0.0f, 105.0f));
	const auto first = LionAt(glm::vec3(106.0f, 0.0f, 106.0f));
	const auto second = LionAt(glm::vec3(104.0f, 0.0f, 104.0f));
	LionAt(glm::vec3(140.0f, 0.0f, 140.0f));
	const glm::vec3 point(105.0f, 0.0f, 105.0f);
	// the point's cell first, its list from the head, then the spiral's next cells
	EXPECT_EQ(_world.HealTargets(point, 12.0f, 10), (Entities {second, first, west}));
	EXPECT_EQ(magic::cast_rules::HealTargetsAround(point, 12.0f, 10), (Entities {second, first, west}));
	EXPECT_EQ(_world.HealTargets(point, 12.0f, 2), (Entities {second, first}));
	EXPECT_TRUE(_world.HealTargets(point, 12.0f, 0).empty());
	// one that is going is not healed
	Reg().Assign<ecs::components::Unavailable>(first);
	EXPECT_EQ(_world.HealTargets(point, 12.0f, 10), (Entities {second, west}));
}

TEST_F(GameMagicWorldObjects, AnEffectAtAPointReachesEveryObjectAboutItAndTheOldCallAnswersTheLast)
{
	const auto first = LionAt(glm::vec3(105.5f, 0.0f, 105.0f));
	const auto second = LionAt(glm::vec3(106.0f, 0.0f, 105.5f));
	LionAt(glm::vec3(130.0f, 0.0f, 130.0f));
	ecs::effects::EffectValues values;
	values.radius = 2.0f;
	// the mobile list from its head: the later lion is hit first
	EXPECT_EQ(_world.ApplyEffectAt(glm::vec3(105.0f, 0.0f, 105.0f), values, {}), (Entities {second, first}));
	auto copy = values;
	Entities hits;
	copy.ApplyEffectToMapPos(glm::vec3(105.0f, 0.0f, 105.0f), hits);
	EXPECT_EQ(hits, (Entities {second, first}));
	EXPECT_EQ(values.ApplyEffectToMapPos(glm::vec3(105.0f, 0.0f, 105.0f)), first);
	// nothing about the point: none
	EXPECT_EQ(values.ApplyEffectToMapPos(glm::vec3(60.0f, 0.0f, 60.0f)), entt::entity {entt::null});
	EXPECT_TRUE(_world.ApplyEffectAt(glm::vec3(60.0f, 0.0f, 60.0f), values, {}).empty());
}

TEST_F(GameMagicWorldObjects, AnObjectTakesAnEffectUntilItIsGone)
{
	const auto lion = LionAt(glm::vec3(50.0f, 0.0f, 50.0f));
	const ecs::effects::EffectValues values;
	EXPECT_TRUE(_world.ApplyEffect(lion, values, {}));
	// taken out of reach in the same turn, it still takes it: whether it takes effects is the caller's to ask
	Reg().Assign<ecs::components::Unavailable>(lion);
	EXPECT_TRUE(_world.ApplyEffect(lion, values, {}));
	ecs::map_cells::RemoveMapObject(lion);
	Reg().Destroy(lion);
	EXPECT_FALSE(_world.ApplyEffect(lion, values, {}));
}

TEST_F(GameMagicWorldObjects, TheMiraclesServiceAppliesAnEffectToAnObjectThroughItsWorld)
{
	const auto lion = LionAt(glm::vec3(50.0f, 0.0f, 50.0f));
	const ecs::effects::EffectValues values;
	ecs::systems::MagicSystem miracles;
	EXPECT_TRUE(miracles.ApplyEffectToObject(lion, values, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(miracles.ApplyEffectToObject(lion, values, magic::EffectSource {.player = PlayerNames::PLAYER_ONE}));
	Reg().Assign<ecs::components::Unavailable>(lion);
	EXPECT_TRUE(miracles.ApplyEffectToObject(lion, values, PlayerNames::PLAYER_ONE));
	ecs::map_cells::RemoveMapObject(lion);
	Reg().Destroy(lion);
	EXPECT_FALSE(miracles.ApplyEffectToObject(lion, values, magic::EffectSource {}));
}

TEST_F(GameMagicWorldObjects, OnlyTheHumanPlayerHasAnInterfaceToPutAPileDown)
{
	ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	ecs::archetypes::PlayerArchetype::Create(PlayerNames::NEUTRAL);
	// the world builds its dropper from the player alone: a miracle with no player is the neutral one's, which has none
	EXPECT_TRUE(magic::players::IsHuman(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(magic::players::IsHuman(PlayerNames::NEUTRAL));
}

TEST(EffectSourceRules, ValuesThatNameAnApplierWinAndFreshOnesTakeItFromTheSource)
{
	magic::EffectValues named;
	named.player = PlayerNames::PLAYER_TWO;
	EXPECT_FALSE(magic::NamesNoApplier(named));
	EXPECT_TRUE(magic::NamesNoApplier(magic::EffectValues {}));
	// a player's own miracle
	const auto byPlayer = magic::ApplierFrom({}, {.player = PlayerNames::PLAYER_ONE}, false, false, std::nullopt);
	EXPECT_TRUE(byPlayer.appliedByPlayer);
	EXPECT_EQ(byPlayer.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(byPlayer.appliedBy, entt::entity {entt::null});
	// with no player at all
	const auto nobody =
	    magic::ApplierFrom({}, {.player = PlayerNames::PLAYER_ONE, .playerless = true}, false, false, std::nullopt);
	EXPECT_FALSE(nobody.appliedByPlayer);
	EXPECT_FALSE(nobody.player.has_value());
}

TEST(EffectSourceRules, ACreaturesMiracleIsAppliedByTheCreatureWithItsPlayerOrNone)
{
	const auto creature = static_cast<entt::entity>(42);
	const auto withPlayer =
	    magic::ApplierFrom({}, {.player = PlayerNames::PLAYER_ONE, .casterCreature = creature}, false, false, std::nullopt);
	EXPECT_EQ(withPlayer.appliedBy, creature);
	EXPECT_TRUE(withPlayer.appliedByCreature);
	EXPECT_FALSE(withPlayer.appliedByPlayer);
	EXPECT_EQ(withPlayer.player, PlayerNames::PLAYER_ONE);
	const auto without = magic::ApplierFrom(
	    {}, {.player = PlayerNames::PLAYER_ONE, .casterCreature = creature, .playerless = true}, false, false, std::nullopt);
	EXPECT_EQ(without.appliedBy, creature);
	EXPECT_FALSE(without.player.has_value());
}

TEST(EffectSourceRules, ABlowIsAppliedByWhatStruckWhileTheThrowerIsBlamed)
{
	const auto rock = static_cast<entt::entity>(7);
	// a rock thrown by the hand: the rock answers for itself with its own player (none), the thrower is blamed
	const auto thrown =
	    magic::ApplierFrom({}, {.player = PlayerNames::PLAYER_ONE, .appliedBy = rock, .blow = true}, true, false, std::nullopt);
	EXPECT_EQ(thrown.appliedBy, rock);
	EXPECT_FALSE(thrown.appliedByCreature);
	EXPECT_EQ(thrown.causedPlayer, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(thrown.player.has_value());
	// a villager striking, of a player's town: its player answers
	const auto villager =
	    magic::ApplierFrom({}, {.appliedBy = rock, .playerless = true, .blow = true}, true, false, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(villager.player, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(villager.causedPlayer.has_value());
	// a fall onto the land nobody caused: nothing applied it, and the neutral player is the one
	const auto fall = magic::ApplierFrom({}, {.appliedBy = entt::entity {entt::null}, .playerless = true, .blow = true}, false,
	                                     false, std::nullopt);
	EXPECT_EQ(fall.appliedBy, entt::entity {entt::null});
	EXPECT_EQ(fall.player, PlayerNames::NEUTRAL);
	// a fall from a throw: the thrower
	const auto thrownFall =
	    magic::ApplierFrom({}, {.player = PlayerNames::PLAYER_ONE, .appliedBy = entt::entity {entt::null}, .blow = true}, false,
	                       false, std::nullopt);
	EXPECT_EQ(thrownFall.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(thrownFall.causedPlayer, PlayerNames::PLAYER_ONE);
}

TEST_F(GameMagicWorldObjects, ASpellWithNoPlayerIsAPlayerlessSource)
{
	const auto spell = Reg().Create();
	auto& component = Reg().Assign<ecs::components::Spell>(spell);
	component.player = PlayerNames::PLAYER_ONE;
	component.hasPlayer = true;
	EXPECT_FALSE(magic::spell_event::SourceOf(spell).playerless);
	component.hasPlayer = false;
	EXPECT_TRUE(magic::spell_event::SourceOf(spell).playerless);
}
