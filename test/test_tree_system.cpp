/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// TreeSystem: the trees' shared state. AnyBent tells the bend pass whether the last pass left a tree bent; it starts
// true, so the first pass always runs. And what the player's hand gives the bend pass: what it holds, while it is drawn.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/TreeSystem.h"
#include "ECS/Trees.h"

using openblack::ecs::HeldBendSource;
using openblack::ecs::MarkingBendSource;
using openblack::ecs::TreeBendSource;
using openblack::ecs::systems::TreeSystem;

TEST(TreeSystem, TheFirstBendPassAlwaysRuns)
{
	const TreeSystem trees;
	EXPECT_TRUE(trees.AnyBent());
}

TEST(TreeSystem, AnyBentIsWhatTheLastPassLeft)
{
	TreeSystem trees;
	trees.SetAnyBent(false);
	EXPECT_FALSE(trees.AnyBent());
	trees.SetAnyBent(true);
	EXPECT_TRUE(trees.AnyBent());
}

TEST(TreeBends, WhatTheDrawnHandHoldsBendsTrees)
{
	constexpr auto k_Rock = static_cast<entt::entity>(7);
	static_assert(HeldBendSource(k_Rock, false) == k_Rock);
	EXPECT_EQ(HeldBendSource(k_Rock, false), k_Rock);
	EXPECT_EQ(HeldBendSource(std::nullopt, false), std::nullopt);
}

TEST(TreeBends, AHandPutAwayBendsNoTreesWithWhatItHolds)
{
	// Put away for a script's cinema bars, the rock still in it bends nothing until the hand comes back
	constexpr auto k_Rock = static_cast<entt::entity>(7);
	EXPECT_EQ(HeldBendSource(k_Rock, true), std::nullopt);
	EXPECT_EQ(HeldBendSource(std::nullopt, true), std::nullopt);
}

// A tree is marked by the last source whose 3 x 3 cells of 10 m hold it: the frame gives the player's creature after
// the physics objects in flight, so the creature wins a tree they both reach (docs/wiki/world/trees.md#drawing)
TEST(TreeBends, TheLastSourceRoundATreeMarksItSoTheCreatureWinsOverTheThingsInFlight)
{
	constexpr auto k_Tree = static_cast<entt::entity>(3);
	constexpr auto k_Rock = static_cast<entt::entity>(7);
	constexpr auto k_Creature = static_cast<entt::entity>(9);
	const glm::vec2 tree(105.0f, 205.0f); // cell (10, 20)
	const std::array<TreeBendSource, 2> both {
	    TreeBendSource {.entity = k_Rock, .position = glm::vec3(112.0f, 0.0f, 198.0f), .radius = 1.0f},    // (11, 19)
	    TreeBendSource {.entity = k_Creature, .position = glm::vec3(96.0f, 0.0f, 214.0f), .radius = 8.0f}, // (9, 21)
	};
	EXPECT_EQ(MarkingBendSource(k_Tree, tree, both), 1u);
	// two cells away the creature marks nothing, and the rock does
	const std::array<TreeBendSource, 2> apart {
	    both[0], TreeBendSource {.entity = k_Creature, .position = glm::vec3(125.0f, 0.0f, 205.0f), .radius = 8.0f}, // (12, 20)
	};
	EXPECT_EQ(MarkingBendSource(k_Tree, tree, apart), 0u);
	// a tree is never its own source, and with no source near nothing marks it
	const std::array<TreeBendSource, 1> itself {
	    TreeBendSource {.entity = k_Tree, .position = glm::vec3(105.0f, 0.0f, 205.0f), .radius = 1.0f}};
	EXPECT_EQ(MarkingBendSource(k_Tree, tree, itself), std::nullopt);
	EXPECT_EQ(MarkingBendSource(k_Tree, tree, {}), std::nullopt);
}

TEST(TreeDraw, ATreeCarriedByAParticleIsDrawnOutOfTheMapWithNoSway)
{
	openblack::ecs::Registry registry;
	namespace c = openblack::ecs::components;
	const auto standing = registry.Create();
	EXPECT_TRUE(openblack::ecs::TreeDrawnInTheMap(registry, standing));
	// drawn out of the map at the matrix it is given: no sway or bend overwrites its up axis
	const auto carried = registry.Create();
	registry.Assign<c::CarriedByTornado>(carried);
	EXPECT_FALSE(openblack::ecs::TreeDrawnInTheMap(registry, carried));
	const auto held = registry.Create();
	registry.Assign<c::HandDrawPose>(held);
	EXPECT_FALSE(openblack::ecs::TreeDrawnInTheMap(registry, held));
	const auto flying = registry.Create();
	registry.Assign<c::PhysicsDrawPose>(flying);
	EXPECT_FALSE(openblack::ecs::TreeDrawnInTheMap(registry, flying));
}
