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

#include <optional>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/TreeSystem.h"
#include "ECS/Trees.h"

using openblack::ecs::HeldBendSource;
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
