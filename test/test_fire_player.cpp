/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player a fire made on an object is credited to (ecs::fire::traits::PlayerOf): the object's own player, as its
// class answers it in the original, over a registry of its own.

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class FirePlayer: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static std::optional<PlayerNames> Is(PlayerNames player) { return player; }
};
} // namespace

TEST_F(FirePlayer, AFireIsCreditedToTheObjectsOwnPlayer)
{
	using ecs::fire::traits::PlayerOf;
	// a villager: its town's owner, none without a town
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, 1u).owner = PlayerNames::PLAYER_TWO;
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager).town = town;
	EXPECT_EQ(PlayerOf(villager), Is(PlayerNames::PLAYER_TWO));
	const auto loose = Reg().Create();
	Reg().Assign<Villager>(loose).town = entt::null;
	EXPECT_EQ(PlayerOf(loose), std::nullopt);
	// a creature: its owner
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature).owner = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(PlayerOf(creature), Is(PlayerNames::PLAYER_ONE));
	// a rock: the player it was made for
	const auto rock = Reg().Create();
	Reg().Assign<MobileStatic>(rock,
	                           MobileStatic {.type = static_cast<MobileStaticInfo>(0), .owner = PlayerNames::PLAYER_THREE});
	EXPECT_EQ(PlayerOf(rock), Is(PlayerNames::PLAYER_THREE));
	// anything else: the neutral player, not none
	EXPECT_EQ(PlayerOf(Reg().Create()), Is(PlayerNames::NEUTRAL));
}
