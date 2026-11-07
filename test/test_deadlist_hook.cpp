/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::ToBeDeleted calls `abodes::OnToBeDeleted(entity)`: a script
// OBJECT_DELETE (CHL, ScriptHeld: ecs::ToBeDeleted) of a building runs the buildings' class part (the graveyard,
// the storage pit, the multi-map fixed part), and a one-shot orb's takes the seed inside with it, which also leaves the
// draw while its orb is not drawn. Same fixture as test_deadlist_buildings.cpp; register it
// with openblack_setup_and_add_test(test_deadlist_hook test_deadlist_hook.cpp).

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownStores.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/SpellSeedGraphic.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace abodes = openblack::ecs::abodes;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr auto k_Pit = static_cast<AbodeInfo>(1);
constexpr auto k_Graveyard = static_cast<AbodeInfo>(2);
constexpr auto k_Creche = static_cast<AbodeInfo>(3);
constexpr auto k_Field = static_cast<AbodeInfo>(4);
constexpr uint32_t k_TownId = 1;

class DeadlistHookTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		const auto set = [&info](AbodeInfo index, AbodeType type, AbodeNumber number) {
			auto& abode = info->abode.at(static_cast<size_t>(index));
			abode.abodeType = type;
			abode.abodeNumber = number;
			abode.tribeType = Tribe::CELTIC;
			abode.thresholdForStopBeingFunctional = 0.75f;
		};
		set(k_House, AbodeType::LivingQuarters, AbodeNumber::A);
		set(k_Pit, AbodeType::StoragePit, AbodeNumber::StoragePit);
		set(k_Graveyard, AbodeType::Graveyard, AbodeNumber::Graveyard);
		set(k_Creche, AbodeType::Creche, AbodeNumber::Creche);
		set(k_Field, AbodeType::Field, AbodeNumber::Field);
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
		game_clock::SetTurn(0);
		reactions::Clear();
	}

	void TearDown() override
	{
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	Town& TownData() { return Reg().Get<Town>(_town); }

	/// A built abode of the town (life 1: functional), without a mesh nor a Transform (MoveAbodeToPlannedAbodes then
	/// makes no plan)
	entt::entity MakeAbode(AbodeInfo info)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		const auto& record = Locator::infoConstants::value().abode.at(static_cast<size_t>(info));
		registry.Assign<Abode>(e, record.abodeNumber, k_TownId, 0u, 0u).info = info;
		return e;
	}

	/// A storage pit with its six pile slots empty
	entt::entity MakePit()
	{
		const auto pit = MakeAbode(k_Pit);
		auto& slots = Reg().Assign<StoragePit>(pit);
		slots.foodPile = entt::null;
		slots.woodPiles.fill(entt::null);
		return pit;
	}

	/// A pot of a GPotInfo row with `amount`
	static entt::entity MakePot(uint16_t amount)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& pot = registry.Assign<Pot>(e);
		pot.amount = amount;
		pot.maxAmount = 100;
		pot.type = PotInfo::StoragePitFoodPile;
		return e;
	}

	/// A one-shot orb with its seed graphic inside, the graphic with a power-up band and one more band (no effect)
	static entt::entity MakeOrb()
	{
		auto& registry = Reg();
		const auto graphic = registry.Create();
		auto& seed = registry.Assign<SpellSeedGraphic>(graphic);
		seed.band = registry.Create();
		seed.extraBands.push_back(registry.Create());
		const auto orb = registry.Create();
		registry.Assign<OneOffSpellSeed>(orb).graphic = graphic;
		return orb;
	}

	entt::entity _town {entt::null};
};

// ---- ecs::ToBeDeleted -> abodes::OnToBeDeleted ----------------------------------------------------------------------

TEST_F(DeadlistHookTest, ObjectDeleteOfAGraveyardUnlinksIt)
{
	const auto first = MakeAbode(k_Graveyard);
	Reg().Assign<Graveyard>(first);
	TownData().graveyard = first;
	// somebody lives in the town, so it stays when its only building goes
	TownData().stats.adults = 1;
	ecs::ToBeDeleted(first);
	EXPECT_FALSE(Reg().Valid(first)); // the deferral is off: destroyed at once
	EXPECT_TRUE(TownData().graveyard == entt::null);
}

TEST_F(DeadlistHookTest, ObjectDeleteOfAPitDeletesItsPilesAndHandsItOn)
{
	const auto pit = MakePit();
	const auto other = MakePit();
	TownData().storagePit = pit;
	const auto food = MakePot(10);
	Reg().Get<StoragePit>(pit).foodPile = food;
	ecs::ToBeDeleted(pit);
	EXPECT_FALSE(Reg().Valid(pit));
	EXPECT_FALSE(Reg().Valid(food));
	EXPECT_EQ(TownData().storagePit, other);
}

TEST_F(DeadlistHookTest, ObjectDeleteOfAHouseRemovesItsReactions)
{
	const auto house = MakeAbode(k_House);
	reactions::CreateReaction(house, openblack::Reaction::LookAtObject, PlayerNames::NEUTRAL, false);
	ecs::ToBeDeleted(house);
	EXPECT_EQ(reactions::GetReactionInitiatedBy(house), 0u);
}

// ---- ecs::ToBeDeleted of a one-shot orb ------------------------------------------------------------------------------

TEST_F(DeadlistHookTest, ADeletedOneShotOrbTakesTheSeedInsideWithIt)
{
	// a creature that eats the orb deletes it as any other meal: the seed graphic, its bands with it, goes first
	const auto orb = MakeOrb();
	const auto graphic = Reg().Get<const OneOffSpellSeed>(orb).graphic;
	const auto& seed = Reg().Get<const SpellSeedGraphic>(graphic);
	const auto band = seed.band;
	const auto extra = seed.extraBands.front();
	ecs::ToBeDeleted(orb);
	EXPECT_FALSE(Reg().Valid(orb));
	EXPECT_FALSE(Reg().Valid(graphic));
	EXPECT_FALSE(Reg().Valid(band));
	EXPECT_FALSE(Reg().Valid(extra));
	// an orb whose seed is already gone is deleted alone
	const auto empty = Reg().Create();
	Reg().Assign<OneOffSpellSeed>(empty);
	const auto other = Reg().Create();
	ecs::ToBeDeleted(empty);
	EXPECT_FALSE(Reg().Valid(empty));
	EXPECT_TRUE(Reg().Valid(other));
}

TEST_F(DeadlistHookTest, AnOrbsSeedLeavesTheDrawWithItsBandsAndComesBackWithThem)
{
	// the seed is drawn only by its orb's draw: while the orb is not drawn (a creature eating it), the seed's mesh and
	// bands are out of the draw, and they come back together
	const auto orb = MakeOrb();
	const auto graphic = Reg().Get<const OneOffSpellSeed>(orb).graphic;
	const auto band = Reg().Get<const SpellSeedGraphic>(graphic).band;
	const auto extra = Reg().Get<const SpellSeedGraphic>(graphic).extraBands.front();
	worship::seed_graphic::SetDrawn(graphic, false);
	EXPECT_TRUE(Reg().AllOf<NotDrawn>(graphic));
	EXPECT_TRUE(Reg().AllOf<NotDrawn>(band));
	EXPECT_TRUE(Reg().AllOf<NotDrawn>(extra));
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(orb));
	worship::seed_graphic::SetDrawn(graphic, true);
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(graphic));
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(band));
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(extra));
	// asked again, nothing changes; a gone graphic is no error
	worship::seed_graphic::SetDrawn(graphic, true);
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(graphic));
	worship::seed_graphic::SetDrawn(entt::null, false);
}
} // namespace
