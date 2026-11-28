/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's grid of dispensers laid out and cleared away (Debug/TestbedDispenserGrid.h), through a fake of the
// miracles that makes and removes plain dispenser entities

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>

#include <set>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Debug/TestbedDispenserGrid.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Locator.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using ecs::components::SpellDispenser;
using ecs::components::TestbedDispenser;

namespace
{

/// The miracles as the grid reaches them: a dispenser is a plain entity with its magic, and every dispenser made,
/// charged and removed is recorded
class GridMagic final: public test::InertMagicSystem
{
public:
	struct Made
	{
		entt::entity entity;
		glm::vec3 position;
		MagicType type;
		float yaw;
	};

	entt::entity CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<SpellDispenser>(entity, SpellDispenser {.magicType = type});
		made.push_back({.entity = entity, .position = position, .type = type, .yaw = yAngleRadians});
		return entity;
	}
	void ChargeDispenser(entt::entity dispenser) override { charged.push_back(dispenser); }
	bool Remove(entt::entity entity) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(entity) || !registry.AllOf<SpellDispenser>(entity))
		{
			return false;
		}
		removed.push_back(entity);
		registry.Destroy(entity);
		return true;
	}

	std::vector<Made> made;
	std::vector<entt::entity> charged;
	std::vector<entt::entity> removed;
};

class TestbedDispenserGridPlacing: public ::testing::Test
{
protected:
	TestbedDispenserGridPlacing()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		magic = &static_cast<GridMagic&>(Locator::magicSystem::emplace<GridMagic>());
	}

	GridMagic* magic {nullptr};

private:
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::magicSystem> _restoreMagic;
};

constexpr glm::vec2 k_Middle {2560.0f, 2560.0f};

} // namespace

TEST_F(TestbedDispenserGridPlacing, LaysOutOneChargedMarkedDispenserOfEveryMiracle)
{
	testbed_dispensers::PlaceGrid(k_Middle);

	const auto types = testbed_dispensers::GridMagicTypes();
	const auto offsets = testbed_dispensers::GridOffsets(types.size());
	ASSERT_EQ(magic->made.size(), types.size());
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < types.size(); ++i)
	{
		const auto& made = magic->made[i];
		EXPECT_EQ(made.type, types[i]);
		// From the middle of the map, the map's y as the world's z
		EXPECT_FLOAT_EQ(made.position.x, k_Middle.x + offsets[i].x);
		EXPECT_FLOAT_EQ(made.position.z, k_Middle.y + offsets[i].y);
		EXPECT_FLOAT_EQ(made.yaw, testbed_dispensers::k_GridYawRadians);
		EXPECT_TRUE(registry.AllOf<TestbedDispenser>(made.entity));
	}
	// Every one with its bubble there to take
	const std::set<entt::entity> charged(magic->charged.begin(), magic->charged.end());
	EXPECT_EQ(charged.size(), types.size());
	for (const auto& made : magic->made)
	{
		EXPECT_TRUE(charged.contains(made.entity));
	}
}

TEST_F(TestbedDispenserGridPlacing, ClearsAwayTheGridAndLeavesAScenariosOwnDispensers)
{
	testbed_dispensers::PlaceGrid(k_Middle);
	// A scenario's own dispenser, made after the grid, carries no mark
	const auto own = magic->CreateDispenser({k_Middle.x, 0.0f, k_Middle.y}, MagicType::Fireball, 0.0f);
	const auto gridCount = testbed_dispensers::GridMagicTypes().size();

	testbed_dispensers::RemoveGrid();

	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(magic->removed.size(), gridCount);
	EXPECT_EQ(registry.Size<TestbedDispenser>(), 0U);
	ASSERT_TRUE(registry.Valid(own));
	EXPECT_TRUE(registry.AllOf<SpellDispenser>(own));

	// Nothing left to clear the second time
	testbed_dispensers::RemoveGrid();
	EXPECT_EQ(magic->removed.size(), gridCount);
}

TEST_F(TestbedDispenserGridPlacing, PlacesNothingWithoutTheMiracles)
{
	Locator::magicSystem::reset();
	testbed_dispensers::PlaceGrid(k_Middle);
	testbed_dispensers::RemoveGrid();
	EXPECT_EQ(Locator::entitiesRegistry::value().Size<SpellDispenser>(), 0U);
}
