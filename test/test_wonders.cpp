/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::wonders: a built wonder adds its power to its player's most tribal power for its tribe, and takes it off again.
// Its player is its town's owner, and the neutral player when it has no town. No game data: a hand-made info.dat.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Town/Wonders.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace wonders = openblack::ecs::wonders;

namespace
{
constexpr AbodeInfo k_Wonder = AbodeInfo::EgyptianWonder;
constexpr size_t k_Tribe = static_cast<size_t>(Tribe::EGYPTIAN);
constexpr uint32_t k_TownId = 3;
constexpr float k_Power = 2.5f;

class WondersTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		auto& wonder = info->abode.at(static_cast<size_t>(k_Wonder));
		wonder.abodeType = AbodeType::Wonder;
		wonder.abodeNumber = AbodeNumber::Wonder;
		wonder.tribeType = Tribe::EGYPTIAN;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_TWO;
		registry.Context().towns[k_TownId] = _town;
		MakePlayer(PlayerNames::PLAYER_TWO);
		MakePlayer(PlayerNames::NEUTRAL);
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A player on its own entity, as the player archetype makes it
	static void MakePlayer(PlayerNames name)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Player>(e, name);
		registry.Assign<PlayerMagic>(e);
	}

	/// A built wonder abode of the town of key `townId`, made a wonder with its power
	static entt::entity MakeBuiltWonder(uint32_t townId)
	{
		const auto e = Reg().Create();
		auto& abode = Reg().Assign<Abode>(e);
		abode.townId = townId;
		abode.info = k_Wonder;
		wonders::Create(e, k_Power);
		return e;
	}

	/// The player's most tribal power for the wonder's tribe
	static float MostPower(PlayerNames player)
	{
		float found = 0.0f;
		Reg().Each<const Player>([&found, player](entt::entity e, const Player& p) {
			if (p.name == player)
			{
				found = Reg().Get<PlayerMagic>(e).maxTribalPower.at(k_Tribe);
			}
		});
		return found;
	}

	entt::entity _town {entt::null};
};
} // namespace

TEST_F(WondersTest, AWonderOfATownRaisesItsOwnersPowerForItsTribe)
{
	const auto wonder = MakeBuiltWonder(k_TownId);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::PLAYER_TWO), 1.0f + k_Power);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::NEUTRAL), 1.0f);
	wonders::RemoveFromPlayer(wonder);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::PLAYER_TWO), 1.0f);
}

TEST_F(WondersTest, AWonderWithNoTownRaisesTheNeutralPlayersPowerForItsTribe)
{
	const auto wonder = MakeBuiltWonder(Abode::k_NoTown);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::NEUTRAL), 1.0f + k_Power);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::PLAYER_TWO), 1.0f);
	wonders::RemoveFromPlayer(wonder);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::NEUTRAL), 1.0f);
}

TEST_F(WondersTest, AWonderStillBeingBuiltRaisesNoPowerUntilItIsBuilt)
{
	const auto e = Reg().Create();
	auto& abode = Reg().Assign<Abode>(e);
	abode.townId = Abode::k_NoTown;
	abode.info = k_Wonder;
	abode.buildFlags = Abode::k_UnderConstruction;
	abode.percentBuilt = 0.0f;
	wonders::Create(e, k_Power);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::NEUTRAL), 1.0f);
	Reg().Get<Abode>(e).buildFlags = Abode::k_Built;
	Reg().Get<Abode>(e).percentBuilt = 1.0f;
	wonders::Built(e);
	EXPECT_FLOAT_EQ(MostPower(PlayerNames::NEUTRAL), 1.0f + k_Power);
}
