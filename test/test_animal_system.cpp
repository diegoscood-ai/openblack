/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The questions the animal service answers about one animal or flock, from the animal's components. The system is
// made here and asked directly; the locator only holds the fake registry and info constants it reads.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::AnimalSystem;

namespace
{
class AnimalSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		info->animal.at(static_cast<size_t>(AnimalInfo::Sheep)).playerCanPickUp = 1;
		info->animal.at(static_cast<size_t>(AnimalInfo::Lion)).playerCanPickUp = 0;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeAnimal(AnimalInfo type)
	{
		auto& registry = Reg();
		const auto entity = registry.Create();
		registry.Assign<Animal>(entity, type, 0u);
		registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	AnimalSystem _system;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

TEST_F(AnimalSystemTest, BatsVulturesBigCatsAndWolvesFrightenCreatures)
{
	// the species that frighten a creature, as the original's classes answer; every other one does not, the puzzle lion
	// and wolf among them
	constexpr std::array k_Frightening {AnimalInfo::Lion, AnimalInfo::Tiger,   AnimalInfo::Wolf,     AnimalInfo::Leopard,
	                                    AnimalInfo::Bat,  AnimalInfo::Vulture, AnimalInfo::SpellBat, AnimalInfo::SpellWolf};
	for (int t = 0; t < static_cast<int>(AnimalInfo::_COUNT); ++t)
	{
		const auto type = static_cast<AnimalInfo>(t);
		EXPECT_EQ(_system.IsFrighteningToCreature(MakeAnimal(type)),
		          std::ranges::find(k_Frightening, type) != k_Frightening.end())
		    << t;
	}
	// Anything that is not an animal frightens no creature here
	EXPECT_FALSE(_system.IsFrighteningToCreature(Reg().Create()));
}

TEST_F(AnimalSystemTest, TheHandPicksUpWhatTheSpeciesAllows)
{
	EXPECT_TRUE(_system.CanPlayerPickUp(MakeAnimal(AnimalInfo::Sheep)));
	EXPECT_FALSE(_system.CanPlayerPickUp(MakeAnimal(AnimalInfo::Lion)));
	// Not an animal: no
	EXPECT_FALSE(_system.CanPlayerPickUp(Reg().Create()));
}

TEST_F(AnimalSystemTest, TheLeaderIsTheFirstMemberStillThere)
{
	auto& registry = Reg();
	const auto flock = registry.Create();
	const auto leader = MakeAnimal(AnimalInfo::Sheep);
	const auto follower = MakeAnimal(AnimalInfo::Sheep);
	registry.Assign<Flock>(flock).members = {leader, follower};

	EXPECT_EQ(_system.LeaderOf(flock), leader);
	EXPECT_EQ(_system.MembersOf(flock), (std::vector<entt::entity> {leader, follower}));

	// An empty flock and a thing that is no flock have neither
	const auto empty = registry.Create();
	registry.Assign<Flock>(empty);
	EXPECT_EQ(_system.LeaderOf(empty), entt::entity {entt::null});
	EXPECT_TRUE(_system.MembersOf(empty).empty());
	EXPECT_EQ(_system.LeaderOf(leader), entt::entity {entt::null});
	EXPECT_TRUE(_system.MembersOf(leader).empty());
}

TEST_F(AnimalSystemTest, TheGoalIsTheBrainsMoveGoal)
{
	const auto animal = MakeAnimal(AnimalInfo::Dove);
	// No brain yet: nothing, and asking makes none
	EXPECT_EQ(_system.GoalOf(animal), glm::vec2(0.0f));
	EXPECT_EQ(_system.GoalHeightOf(animal), 0.0f);
	EXPECT_FALSE(Reg().AllOf<AnimalBrain>(animal));

	auto& brain = Reg().Assign<AnimalBrain>(animal);
	brain.goal = {12.5f, -3.0f};
	brain.goalAltitude = 20.0f;
	EXPECT_EQ(_system.GoalOf(animal), glm::vec2(12.5f, -3.0f));
	EXPECT_EQ(_system.GoalHeightOf(animal), 20.0f);
}

TEST_F(AnimalSystemTest, ScaleAndRadius)
{
	const auto animal = MakeAnimal(AnimalInfo::Sheep);
	_system.SetScale(animal, 0.75f);
	EXPECT_EQ(Reg().Get<Transform>(animal).scale, glm::vec3(0.75f));
	// Without a loaded model there is no footprint
	EXPECT_EQ(_system.RadiusOf(animal), 0.0f);
}
