/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The domes the game puts over the creatures and the temples to keep the player's camera out: their sizes
// (camera_zones), and their making, moving and taking away on a registry of their own (ecs::camera_domes)

#define LOCATOR_IMPLEMENTATIONS

#include <limits>
#include <optional>

#include <entt/core/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraZones.h"
#include "Creature/CreatureMorph.h"
#include "ECS/CameraDomes.h"
#include "ECS/Components/CameraDome.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureBodyPlacement.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::camera_zones;
using namespace openblack::ecs::components;

TEST(CameraDomes, ADomesSizesAreHeldBetweenNothingAndTenThousand)
{
	EXPECT_EQ(HeldDomeSize(12.5f), 12.5f);
	EXPECT_EQ(HeldDomeSize(20000.0f), 10000.0f);
	EXPECT_EQ(HeldDomeSize(-3.0f), 0.0f);
	EXPECT_EQ(HeldDomeSize(0.0f), 0.0f);
	EXPECT_EQ(HeldDomeSize(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST(CameraDomes, ADomeMadeWithNoHeightIsAsHighAsItIsWideButAMovedOneKeepsNoHeight)
{
	const glm::vec3 at {10.0f, 2.0f, 30.0f};
	const auto made = MadeDome(at, 15.0f, 0.0f);
	EXPECT_EQ(made.kind, Exclusion::Kind::Dome);
	EXPECT_FALSE(made.saved);
	EXPECT_EQ(made.position, at);
	EXPECT_EQ(made.radius, 15.0f);
	EXPECT_EQ(made.height, 15.0f);
	const auto moved = AdjustedDome(at, 15.0f, 0.0f);
	EXPECT_EQ(moved.height, 0.0f);
	EXPECT_EQ(MadeDome(at, 15.0f, 40.0f).height, 40.0f);
}

TEST(CameraDomes, ACreaturesDomeIsItsBodysRadiusAndStandingHeightTimesOnePointThreeFive)
{
	const auto size = CreatureDomeSize(10.0f, 2.0f);
	EXPECT_EQ(size.radius, 10.0f * 1.35f);
	// 1.35 times its standing height, 15 times its size
	EXPECT_EQ(size.height, 1.35f * (15.0f * 2.0f));
}

TEST(CameraDomes, ATemplesDomeIsFifteenAcrossAndAsHighAsItsMesh)
{
	const auto size = TempleDomeSize(31.25f);
	EXPECT_EQ(size.radius, 15.0f);
	EXPECT_EQ(size.height, 62.5f);
}

namespace
{
entt::entity MakeCreature(ecs::Registry& registry, glm::vec3 at, float size)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& creature = registry.Assign<Creature>(entity);
	creature.size = size;
	registry.Assign<CreatureBodyMetrics>(entity, CreatureBodyMetrics {.restHeight = 60.0f, .reach = 8.0f});
	return entity;
}
} // namespace

TEST(CameraDomes, EachCreaturesTurnPutsItsDomeWhereItsBodyIsPlacedAtTheSizeItIsDrawnAt)
{
	ecs::Registry registry;
	const auto creature = MakeCreature(registry, {100.0f, 5.0f, 200.0f}, 1.5f);
	// a creature without its body's measures has no dome
	const auto unmeasured = registry.Create();
	registry.Assign<Transform>(unmeasured, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Creature>(unmeasured);

	ecs::camera_domes::PlaceCreatures(registry);
	ASSERT_TRUE(registry.AllOf<CameraDome>(creature));
	EXPECT_FALSE(registry.AllOf<CameraDome>(unmeasured));
	{
		const auto& dome = registry.Get<const CameraDome>(creature);
		EXPECT_EQ(dome.position, glm::vec3(100.0f, 5.0f, 200.0f));
		EXPECT_EQ(dome.radius, creature_morph::Radius(1.5f, 60.0f, 8.0f) * 1.35f);
		EXPECT_EQ(dome.height, 1.35f * (15.0f * 1.5f));
	}

	// The next turn it follows the body as last placed, and the size it is now drawn at (its pen's, here), held to its
	// limits
	glm::mat4 cells(1.0f);
	cells[3] = glm::vec4(120.0f, 6.0f, 210.0f, 1.0f);
	registry.Assign<CreatureBodyPlacement>(creature, CreatureBodyPlacement {.cells = cells, .scale = 1.0f});
	registry.Assign<CreatureDrawPose>(creature, CreatureDrawPose {.size = 9.0f});
	ecs::camera_domes::PlaceCreatures(registry);
	const auto& dome = registry.Get<const CameraDome>(creature);
	EXPECT_EQ(dome.position, glm::vec3(120.0f, 6.0f, 210.0f));
	EXPECT_EQ(dome.radius, creature_morph::Radius(4.0f, 60.0f, 8.0f) * 1.35f);
	EXPECT_EQ(dome.height, 1.35f * (15.0f * 4.0f));

	const auto exclusions = ecs::camera_domes::Exclusions(registry);
	ASSERT_EQ(exclusions.size(), 1u);
	EXPECT_EQ(exclusions[0].kind, Exclusion::Kind::Dome);
	EXPECT_FALSE(exclusions[0].saved);
	EXPECT_EQ(exclusions[0].radius, dome.radius);
}

TEST(CameraDomes, ATemplesDomeIsMadeOverItFromItsMeshAndGoesWithIt)
{
	test::RestoreService<Locator::meshBoxProvider> restore;
	auto& boxes = static_cast<test::FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>());
	constexpr entt::id_type k_Small = 1;
	constexpr entt::id_type k_Big = 2;
	boxes.meshBox = [](entt::id_type id) -> std::optional<AxisAlignedBoundingBox> {
		if (id == k_Small)
		{
			return AxisAlignedBoundingBox {{-30.0f, 0.0f, -30.0f}, {30.0f, 40.0f, 30.0f}};
		}
		if (id == k_Big)
		{
			return AxisAlignedBoundingBox {{-30.0f, -10.0f, -30.0f}, {30.0f, 70.0f, 30.0f}};
		}
		return std::nullopt;
	};

	ecs::Registry registry;
	const auto heart = registry.Create();
	registry.Assign<Transform>(heart, glm::vec3(500.0f, 12.0f, 700.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(heart, k_Small, static_cast<int8_t>(0), static_cast<int8_t>(0));
	ecs::camera_domes::PlaceTemple(registry, heart);
	ASSERT_TRUE(registry.AllOf<CameraDome>(heart));
	EXPECT_EQ(registry.Get<const CameraDome>(heart).position, glm::vec3(500.0f, 12.0f, 700.0f));
	EXPECT_EQ(registry.Get<const CameraDome>(heart).radius, 15.0f);
	EXPECT_EQ(registry.Get<const CameraDome>(heart).height, 40.0f);

	// a new mesh as its outside changes sizes it again
	registry.Get<Mesh>(heart).id = k_Big;
	ecs::camera_domes::PlaceTemple(registry, heart);
	EXPECT_EQ(registry.Get<const CameraDome>(heart).height, 80.0f);

	ecs::camera_domes::RemoveTemple(registry, heart);
	EXPECT_FALSE(registry.AllOf<CameraDome>(heart));
	EXPECT_TRUE(ecs::camera_domes::Exclusions(registry).empty());
}
