/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// The players of the magic objects (teleport stones, fire balls) and of the effects: none unless one was given; a
// player's stones in their order. A fireball's info row, where the hand sees it, and a fire seed taking one in

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/HandPlaceInHand.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Magic/Objects/MagicTeleport.h"
#include "creature/CreatureSystemFakes.h"
#include "support/FireFakes.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
class MagicObjectPlayerTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(MagicObjectPlayerTest, ATeleportStoneHasItsSpellsPlayerOrNone)
{
	const auto none = Reg().Create();
	Reg().Assign<ecs::components::TeleportStone>(none);
	const auto owned = Reg().Create();
	Reg().Assign<ecs::components::TeleportStone>(owned).player = PlayerNames::PLAYER_TWO;
	const auto notAStone = Reg().Create();
	EXPECT_EQ(magic::teleport::PlayerOf(none), std::nullopt);
	EXPECT_EQ(magic::teleport::PlayerOf(owned), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(magic::teleport::PlayerOf(notAStone), std::nullopt);
}

// A seed picked up off the map is picked up at its own place: where the last miracle cast from it on the land was
// cast, else where its maker made it; drawing it over its spell moved only its model (its Transform here). A stone, or
// anything else, has no seed's place
TEST_F(MagicObjectPlayerTest, ASeedIsPickedUpAtItsOwnPlaceNotWhereItIsDrawn)
{
	const auto seed = Reg().Create();
	Reg().Assign<ecs::components::Transform>(seed).position = glm::vec3(1250.0f, 37.5f, 2210.0f);
	auto& component = Reg().Assign<ecs::components::SpellSeed>(seed);
	component.madeAt = map_coords::MapCoords {.x = 8192000, .z = 14417920, .altitude = 0.0f};
	EXPECT_EQ(ecs::hand_place_in_hand::SeedPickUpPlace(seed), glm::vec3(1250.0f, 0.0f, 2200.0f));
	component.castAt = glm::vec3(1250.0f, 4.0f, 2210.0f);
	EXPECT_EQ(ecs::hand_place_in_hand::SeedPickUpPlace(seed), glm::vec3(1250.0f, 4.0f, 2210.0f));
	const auto stone = Reg().Create();
	Reg().Assign<ecs::components::TeleportStone>(stone);
	EXPECT_EQ(ecs::hand_place_in_hand::SeedPickUpPlace(stone), std::nullopt);
	EXPECT_EQ(ecs::hand_place_in_hand::SeedPickUpPlace(Reg().Create()), std::nullopt);
}

TEST_F(MagicObjectPlayerTest, APlayersStonesAreListedNewestFirst)
{
	// each stone of a player gets the next serial; one gone leaves the others in their order, newest first, and a stone
	// with no player is in no list
	uint32_t serial = 0;
	const auto stone = [&serial](std::optional<PlayerNames> player) {
		const auto entity = Reg().Create();
		auto& component = Reg().Assign<ecs::components::TeleportStone>(entity);
		component.player = player;
		if (player.has_value())
		{
			component.serial = serial++;
		}
		return entity;
	};
	const auto first = stone(PlayerNames::PLAYER_ONE);
	const auto second = stone(PlayerNames::PLAYER_ONE);
	const auto unowned = stone(std::nullopt);
	const auto third = stone(PlayerNames::PLAYER_ONE);
	const auto otherPlayers = stone(PlayerNames::PLAYER_TWO);
	Reg().Destroy(second);
	const auto fourth = stone(PlayerNames::PLAYER_ONE);

	EXPECT_EQ(magic::teleport::StonesOf(PlayerNames::PLAYER_ONE), (std::vector {fourth, third, first}));
	EXPECT_EQ(magic::teleport::StonesOf(PlayerNames::PLAYER_TWO), std::vector {otherPlayers});
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		const auto stones = magic::teleport::StonesOf(static_cast<PlayerNames>(p));
		EXPECT_EQ(std::ranges::count(stones, unowned), 0);
	}
}

TEST_F(MagicObjectPlayerTest, AFireBallCanBeCaughtUnlessItIsTheCatchersOwn)
{
	const auto none = Reg().Create();
	Reg().Assign<ecs::components::MagicFireBall>(none);
	const auto owned = Reg().Create();
	Reg().Assign<ecs::components::MagicFireBall>(owned).player = PlayerNames::PLAYER_ONE;
	const auto notABall = Reg().Create();
	EXPECT_TRUE(magic::fireball::ValidForPlaceInHand(none, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(magic::fireball::ValidForPlaceInHand(owned, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(magic::fireball::ValidForPlaceInHand(owned, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(magic::fireball::ValidForPlaceInHand(notABall, PlayerNames::PLAYER_ONE));
}

TEST(MagicFireBallRow, TheFirstPowerUpTakesRowOneAndEveryOtherBallRowZero)
{
	// the spell's power-up level: the plain fireball and a ball with no spell (-1), the first power-up (0), the second
	// (1), and a level no fireball has (2)
	EXPECT_EQ(magic::fireball::InfoRowFor(-1), 0);
	EXPECT_EQ(magic::fireball::InfoRowFor(0), 1);
	EXPECT_EQ(magic::fireball::InfoRowFor(1), 0);
	EXPECT_EQ(magic::fireball::InfoRowFor(2), 0);
}

TEST(MagicFireBallHandTarget, TheHandSeesADrawnBallBrighterThanThirty)
{
	const glm::vec3 drawn(12.5f, 7.0f, -3.25f);
	EXPECT_EQ(magic::fireball::HandTargetOf(true, 30, drawn), std::nullopt);
	EXPECT_EQ(magic::fireball::HandTargetOf(false, 255, drawn), std::nullopt);
	EXPECT_EQ(magic::fireball::HandTargetOf(true, 31, drawn), std::optional(drawn));
	EXPECT_EQ(magic::fireball::HandTargetOf(true, 255, drawn), std::optional(drawn));
}

TEST(MagicFireBallApply, AFireSeedTakesInAnyonesBallAndNoOtherSeedDoes)
{
	// the fireball's whole test: a FIRE seed, a magic fireball; no player is asked
	EXPECT_TRUE(magic::seed::ValidToApplyToMagicFireBall(SpellSeedType::Fire, true));
	EXPECT_FALSE(magic::seed::ValidToApplyToMagicFireBall(SpellSeedType::Storm, true));
	EXPECT_FALSE(magic::seed::ValidToApplyToMagicFireBall(SpellSeedType::Fire, false));
	EXPECT_FALSE(magic::seed::ValidToApplyToMagicFireBall(SpellSeedType::None, true));
}

namespace
{
/// A registry, the miracles' stores (with their fireball list), fires that burn nothing and a table of constants whose
/// three fireball rows have the catch factors 0.05, 0.25 and 0.5, each put back as it was when the test ends
class FireBallIntoSeedTest: public ::testing::Test
{
protected:
	static constexpr std::array<float, 3> k_Factors {0.05f, 0.25f, 0.5f};

	FireBallIntoSeedTest()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		Locator::fireSystem::emplace<test::FakeFires>();
		auto info = std::make_unique<InfoConstants>();
		for (size_t row = 0; row < k_Factors.size(); ++row)
		{
			info->magicFireBall.at(row).catchIncreaseFactor = k_Factors.at(row);
		}
		Locator::infoConstants::reset(info.release());
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A ball of that info row on the fireball list, as the rule makes it
	static entt::entity Ball(int row)
	{
		const auto ball = Reg().Create();
		Reg().Assign<ecs::components::MagicFireBall>(ball).infoRow = row;
		auto& balls = Locator::magicSystem::value().MagicObjects().FireBalls();
		balls.insert(balls.begin(), ball);
		return ball;
	}

private:
	// put back in reverse order: the tables, the fires, the miracles' service with its stores, then the registry
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::magicSystem> _restoreMagic;
	const test::RestoreService<Locator::fireSystem> _restoreFires;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

TEST_F(FireBallIntoSeedTest, TheSeedGrowsByItsBallsOwnRowAndTheBallGoes)
{
	for (int row = 0; row < 3; ++row)
	{
		const auto seed = Reg().Create();
		auto& component = Reg().Assign<ecs::components::SpellSeed>(seed);
		component.seedType = SpellSeedType::Fire;
		component.power = 2.0f;
		const auto ball = Ball(row);
		magic::fireball::DeleteAndPutIntoSpellSeed(ball, seed);
		EXPECT_FLOAT_EQ(Reg().Get<ecs::components::SpellSeed>(seed).power,
		                2.0f * (1.0f + k_Factors.at(static_cast<size_t>(row))))
		    << "row " << row;
		EXPECT_FALSE(Reg().Valid(ball));
		EXPECT_EQ(std::ranges::count(magic::fireball::All(), ball), 0);
	}
}

TEST_F(FireBallIntoSeedTest, TheSeedsNewPowerIsTheOriginalsToTheBit)
{
	// the original's own results, run natively: the ball's row's factor, the seed's power before, and its bits after
	struct Case
	{
		int row;
		float power;
		uint32_t after;
	};
	constexpr std::array<Case, 6> k_Cases {{
	    {0, 1.0f, 0x3F866666u},
	    {0, 2.0f, 0x40066666u},
	    {0, 0.7f, 0x3F3C28F5u},
	    {0, 1.3f, 0x3FAEB851u},
	    {1, 0.7f, 0x3F600000u},
	    {1, 2.0f, 0x40200000u},
	}};
	for (const auto& c : k_Cases)
	{
		const auto seed = Reg().Create();
		Reg().Assign<ecs::components::SpellSeed>(seed).power = c.power;
		magic::fireball::DeleteAndPutIntoSpellSeed(Ball(c.row), seed);
		EXPECT_EQ(std::bit_cast<uint32_t>(Reg().Get<ecs::components::SpellSeed>(seed).power), c.after)
		    << "row " << c.row << " power " << c.power;
	}
}

TEST_F(FireBallIntoSeedTest, AHandThatIsNotReadyForAnObjectCatchesNothing)
{
	// the fake hand is never ready for an object: the catch makes no seed and the ball stays, on the list
	const test::RestoreService<Locator::handSystem> restoreHand;
	Locator::handSystem::emplace<test::creature_fakes::FakeHand>();
	const auto ball = Ball(0);
	EXPECT_EQ(magic::fireball::Catch(ball, PlayerNames::PLAYER_ONE), entt::entity {entt::null});
	EXPECT_TRUE(Reg().Valid(ball));
	EXPECT_EQ(std::ranges::count(magic::fireball::All(), ball), 1);
}

TEST(EffectValuesPlayer, NoneUntilOneIsGiven)
{
	// an effect from its info alone has no player (a spell without one leaves it so); the copy keeps a given one
	auto values = ecs::effects::EffectValues::FromEffectInfo(GEffectInfo {});
	EXPECT_EQ(values.player, std::nullopt);
	values.player = PlayerNames::PLAYER_TWO;
	const auto copy = values;
	EXPECT_EQ(copy.player, PlayerNames::PLAYER_TWO);
}

TEST_F(MagicObjectPlayerTest, NoHandTakesAShieldOfEitherKind)
{
	// a shield is no class the hand's allowlist names, so neither kind ever goes into a hand, whoever's hand it is
	// (docs/wiki/project/progress/miracles/physical_shield.md, the hand row)
	using ecs::components::MagicShield;
	const auto shield = [](MagicShield::Kind kind) {
		const auto entity = Reg().Create();
		Reg().Assign<MagicShield>(entity).kind = kind;
		Reg().Assign<ecs::components::Transform>(entity, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	};
	const auto spiritual = shield(MagicShield::Kind::Spiritual);
	// the physical one is drawn, with its mesh, as it is made
	const auto physical = shield(MagicShield::Kind::Physical);
	Reg().Assign<ecs::components::Mesh>(physical, ecs::components::Mesh {.id = 1, .submeshId = 0, .bbSubmeshId = 0});
	for (const auto player : {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::NEUTRAL})
	{
		EXPECT_FALSE(ecs::hand_place_in_hand::ValidForPlaceInHand(spiritual, player));
		EXPECT_FALSE(ecs::hand_place_in_hand::ValidForPlaceInHand(physical, player));
	}
}
