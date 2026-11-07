/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Graphics/SeaReflection.h"

using openblack::graphics::sea_reflection::ReflectedBones;
using openblack::graphics::sea_reflection::ReflectsVillager;

TEST(SeaReflection, VillagersOnTheLandAreNotReflected)
{
	EXPECT_FALSE(ReflectsVillager({}));
	EXPECT_FALSE(ReflectsVillager({.handReflected = true}));
}

TEST(SeaReflection, VillagersInThePhysicsAreReflected)
{
	EXPECT_TRUE(ReflectsVillager({.inPhysics = true}));
}

TEST(SeaReflection, HeldVillagersAreReflectedWithTheHand)
{
	EXPECT_TRUE(ReflectsVillager({.inHand = true, .handReflected = true}));
	EXPECT_FALSE(ReflectsVillager({.inHand = true, .handReflected = false}));
}

TEST(SeaReflection, AnObjectIsMirroredInThePoseItIsDrawnWith)
{
	const std::array<glm::mat4, 2> mesh {glm::mat4(1.0f), glm::mat4(1.0f)};
	const std::vector<glm::mat4> pose {glm::translate(glm::mat4(1.0f), {0.0f, 1.0f, 0.0f}),
	                                   glm::translate(glm::mat4(1.0f), {0.0f, 2.0f, 0.0f})};
	const auto bones = ReflectedBones(mesh, pose);
	EXPECT_EQ(bones.data(), pose.data());
	EXPECT_EQ(bones.size(), 2u);
}

TEST(SeaReflection, AnObjectWithoutAFittingPoseIsMirroredInItsMeshsBones)
{
	const std::array<glm::mat4, 2> mesh {glm::mat4(1.0f), glm::mat4(1.0f)};
	EXPECT_EQ(ReflectedBones(mesh, {}).data(), mesh.data());
	const std::vector<glm::mat4> other {glm::mat4(2.0f)};
	EXPECT_EQ(ReflectedBones(mesh, other).data(), mesh.data());
	EXPECT_EQ(ReflectedBones(mesh, other).size(), 2u);
}
