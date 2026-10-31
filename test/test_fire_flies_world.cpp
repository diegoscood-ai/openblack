/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fireflies of the testbed scene sky.fireflies_evening, over its row of trees at its hour, played turn by turn in a
// small world of the game's own systems and data: each is one small glowing sprite, cell 37 of the game's third sprite
// sheet at a size of 0.3, with no model, sound, light or body of its own, and it follows nothing, not even the camera.
// Every expected value is the original's (docs/wiki/world/day-and-night.md#night-lights,
// docs/wiki/project/progress/nature/fireflies.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/FireFlies.h"
#include "ECS/MapCells.h"
#include "support/SkyFrameGuard.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Sprite;
using ecs::components::Transform;

namespace
{
constexpr std::string_view k_Scene = "sky.fireflies_evening";
/// Where the scene's middle is put
const glm::vec2 k_Middle {1005.0f, 1205.0f};

class FireFliesWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		ecs::ClearFireFlies();
		if (!GameData())
		{
			// the trees are forest trees, their models boxes
			for (auto& tree : Info().tree)
			{
				tree.type = ObjectType::ForestTree;
				tree.normal = MeshId::TreeBeech;
			}
		}
		// the scene's trees, and its hour, standing still
		for (const auto& object : Scene(k_Scene).objects)
		{
			if (const auto* tree = std::get_if<TreeInfo>(&object.type))
			{
				const auto at = k_Middle + object.offset;
				LoadMesh(Info().tree.at(static_cast<size_t>(*tree)).normal, glm::vec3(-1.0f, 0.0f, -1.0f),
				         glm::vec3(1.0f, 8.0f, 1.0f));
				static_cast<void>(
				    ecs::archetypes::TreeArchetype::Create(0, glm::vec3(at.x, k_Ground, at.y), *tree, false, 0.0f, 1.0f, 1.0f));
			}
		}
		_clock.Reset();
		_clock.SetCycle(DayNightClock::k_DefaultDuration, 0.5f, 0.0f);
		_clock.SetScriptTime(Scene(k_Scene).environment.hour);
	}
	void TearDown() override
	{
		ecs::ClearFireFlies();
		WorldHarness::TearDown();
	}

	void BeforePhysics() override { ecs::ProcessFireFliesTurn(_clock); }

	/// The fireflies' sprites, once some are out
	std::vector<entt::entity> FireFlies()
	{
		std::vector<entt::entity> found;
		for (int turn = 0; turn < 100 && found.empty(); ++turn)
		{
			Turn();
			Registry().Each<const Sprite>([&found](entt::entity sprite, const Sprite& /*sprite*/) { found.push_back(sprite); });
		}
		return found;
	}

	/// Where every firefly is drawn, with the camera there
	static std::vector<glm::vec3> DrawnWith(const std::vector<entt::entity>& flies, glm::vec3 camera)
	{
		ecs::UpdateFireFlies(0.0f, camera);
		std::vector<glm::vec3> at;
		for (const auto fly : flies)
		{
			at.push_back(Registry().Get<const Transform>(fly).position);
		}
		return at;
	}

private:
	DayNightClock _clock;
	test::SkyFrameGuard _sky;
};
} // namespace

// A firefly is one small glowing sprite: cell 37 of the game's third sprite sheet of 8 by 8, at a size of 0.3, added to
// what is behind it
TEST_P(FireFliesWorld, AFireflyIsOneSmallGlowingSpriteOfTheThirdSheet)
{
	const auto flies = FireFlies();
	ASSERT_FALSE(flies.empty());
	for (const auto fly : flies)
	{
		const auto& sprite = Registry().Get<const Sprite>(fly);
		EXPECT_EQ(sprite.uvMin, graphics::frame_anim::SpriteCellUv(37, 8)[0]);
		EXPECT_TRUE(sprite.additive);
		EXPECT_EQ(Registry().Get<const Transform>(fly).scale, glm::vec3(0.3f));
	}
}

// It has no model, no sound, no light and no part in the physics: nothing can hit it or push it
TEST_P(FireFliesWorld, AFireflyHasNoModelAndNoPartInThePhysics)
{
	const auto flies = FireFlies();
	ASSERT_FALSE(flies.empty());
	for (const auto fly : flies)
	{
		EXPECT_FALSE((Registry().AnyOf<ecs::components::Mesh, ecs::components::InPhysics>(fly)));
		EXPECT_EQ(Dynamics().Find(fly), nullptr);
		EXPECT_FALSE(Dynamics().InteractsWithPhysicsObjects(fly));
		EXPECT_FALSE(ecs::map_cells::IsObjectInMap(fly));
	}
}

// It follows nothing, not even the camera: where it is drawn does not change with where the camera is
TEST_P(FireFliesWorld, AFireflyFollowsNothingNotEvenTheCamera)
{
	const auto flies = FireFlies();
	ASSERT_FALSE(flies.empty());
	const auto near = DrawnWith(flies, glm::vec3(k_Middle.x, k_Ground + 10.0f, k_Middle.y));
	const auto far = DrawnWith(flies, glm::vec3(k_Middle.x + 500.0f, k_Ground + 200.0f, k_Middle.y - 300.0f));
	EXPECT_EQ(near, far);
}

INSTANTIATE_TEST_SUITE_P(Data, FireFliesWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
