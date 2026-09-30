/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <gtest/gtest.h>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// GInfluenceInfo from info.dat: 0.4, 0.2, 0.2
GInfluenceInfo ShippedInfluenceInfo()
{
	return {0.4f, 0.2f, 0.2f};
}

class InfluenceTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		info->influence = ShippedInfluenceInfo();
		info->town.influence = 25.0f;
		info->town.storyInfluence = {25.0f, 25.0f, 25.0f, 50.0f, 25.0f};
		info->citadelHeart.influence = 125.0f;
		info->citadelHeart.storyInfluence = {750.0f, 450.0f, 250.0f, 450.0f, 450.0f};
		auto& house = info->abode.at(0);
		house.abodeNumber = AbodeNumber::A;
		house.tribeType = Tribe::NORSE;
		house.influence = 5.0f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	static entt::entity MakeTown(uint32_t id, const glm::vec3& position, PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<Town>(town, id).owner = owner;
		registry.Assign<Tribe>(town, Tribe::NORSE);
		registry.Assign<Transform>(town, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<TownInfluence>(town);
		registry.Context().towns.insert({id, town});
		return town;
	}
};
} // namespace

TEST(Influence, rangeGradient)
{
	// Influence::CalculateInfluenceOnRange 0x5CD560 with r = 100: 1 up to 40, 0.8 -> 0 up to 60, 0.2 -> 0 up to 100
	const auto info = ShippedInfluenceInfo();
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(0.0f, 100.0f, info), 1.0f);
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(40.0f, 100.0f, info), 1.0f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(40.001f, 100.0f, info), 0.8f, 1e-3f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(50.0f, 100.0f, info), 0.4f, 1e-5f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(60.0f, 100.0f, info), 0.0f, 1e-5f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(60.001f, 100.0f, info), 0.2f, 1e-4f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(80.0f, 100.0f, info), 0.1f, 1e-5f);
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(100.0f, 100.0f, info), 0.0f);
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(150.0f, 100.0f, info), 0.0f);
}

TEST_F(InfluenceTest, townRadiusAndPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	influence::SetLandNumber(4);
	influence::SetTownInfluenceMultiplier(0.8f);
	const auto town = MakeTown(0, glm::vec3(1000.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE);
	// a house of scale 2 with no villagers: 5 x 2 x 1 x (0 + 0 + 1) = 10
	const auto house = registry.Create();
	registry.Assign<Abode>(house, AbodeNumber::A, 0u, 0u, 0u);
	registry.Assign<Transform>(house, glm::vec3(1010.0f, 0.0f, 1000.0f), glm::mat3(1.0f), glm::vec3(2.0f));
	influence::ProcessTowns();
	// (story[3] = 50 + 10) x 0.8
	EXPECT_FLOAT_EQ(influence::TownRadius(town), 48.0f);
	// inside: the town gives its radius, clamped to 1; outside (strict d < r) and for another player: 0
	EXPECT_FLOAT_EQ(influence::CalculatePlayerRawInfluence(PlayerNames::PLAYER_ONE, glm::vec3(1040.0f, 0.0f, 1000.0f)), 1.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerRawInfluence(PlayerNames::PLAYER_ONE, glm::vec3(1048.0f, 0.0f, 1000.0f)), 0.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerRawInfluence(PlayerNames::PLAYER_TWO, glm::vec3(1000.0f, 0.0f, 1000.0f)), 0.0f);
	// the height does not count (GetDistanceInMetres is x,z)
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(1000.0f, 500.0f, 1040.0f)), 1.0f);
}

TEST_F(InfluenceTest, citadelStoryInfluence)
{
	auto& registry = Locator::entitiesRegistry::value();
	influence::SetLandNumber(1);
	const auto temple = registry.Create();
	registry.Assign<Temple>(temple, PlayerNames::PLAYER_ONE);
	registry.Assign<Transform>(temple, glm::vec3(2000.0f, 0.0f, 2000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FLOAT_EQ(influence::CitadelRadius(temple), 750.0f);
	// fixed with the heart: a later multiplier only scales it (Citadel::GetInfluence)
	influence::SetPlayerInfluenceMultiplier(0.5f);
	EXPECT_FLOAT_EQ(influence::CitadelRadius(temple), 375.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(2000.0f, 0.0f, 2374.0f)), 1.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(2000.0f, 0.0f, 2376.0f)), 0.0f);
}

TEST_F(InfluenceTest, ringsAndAntiRings)
{
	const glm::vec3 centre(500.0f, 0.0f, 500.0f);
	const auto ring = influence::CreateRing(centre, PlayerNames::PLAYER_ONE, 100.0f, false);
	EXPECT_NEAR(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, centre + glm::vec3(50.0f, 0.0f, 0.0f)), 0.4f,
	            1e-5f);
	// two rings add up and clamp to 1
	influence::CreateRing(centre + glm::vec3(20.0f, 0.0f, 0.0f), PlayerNames::PLAYER_ONE, 100.0f, false);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, centre + glm::vec3(50.0f, 0.0f, 0.0f)), 1.0f);
	// an anti ring of the player makes it 0 (a shield's ring for its enemies)
	influence::CreateRing(centre, PlayerNames::PLAYER_ONE, 10.0f, true);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, centre), 0.0f);
	EXPECT_TRUE(influence::IsInAntiInfluence(PlayerNames::PLAYER_ONE, centre));
	EXPECT_FALSE(influence::IsInAntiInfluence(PlayerNames::PLAYER_TWO, centre));
	EXPECT_FALSE(influence::IsInAntiInfluence(PlayerNames::PLAYER_ONE, centre + glm::vec3(11.0f, 0.0f, 0.0f)));
	influence::DeleteRing(ring);
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(ring));
}

TEST_F(InfluenceTest, ringFollowsItsObject)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	registry.Assign<Transform>(object, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto ring = influence::CreateRingOnObject(object, PlayerNames::PLAYER_ONE, 30.0f, false);
	ASSERT_TRUE(ring != entt::null);
	registry.Get<Transform>(object).position = glm::vec3(300.0f, 0.0f, 100.0f);
	influence::ProcessRings();
	EXPECT_FLOAT_EQ(registry.Get<InfluenceRing>(ring).position.x, 300.0f);
	registry.Destroy(object);
	influence::ProcessRings();
	EXPECT_FALSE(registry.Valid(ring));
}

TEST_F(InfluenceTest, everywhere)
{
	influence::SetInfluenceEverywhere(true);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_THREE, glm::vec3(0.0f)), 1.0f);
	influence::SetInfluenceEverywhere(false);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_THREE, glm::vec3(0.0f)), 0.0f);
}
