/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The wind sway the trees and the ripe fields share, before its speeds are first drawn (no random numbers)

#include <chrono>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/VegetationSystem.h"

using namespace openblack;

TEST(VegetationSystem, NoLeanBeforeTheFirstUpdate)
{
	const ecs::systems::VegetationSystem vegetation;
	for (uint8_t slot = 0; slot < ecs::systems::VegetationInterface::k_SwayCount; ++slot)
	{
		EXPECT_EQ(vegetation.GetLean(slot), 0.0f);
	}
}

TEST(VegetationSystem, StillSwaysLeanTheirFullWayUntilTheSpeedsAreDrawn)
{
	ecs::systems::VegetationSystem vegetation;
	// The speeds are 0 for the first two seconds: every sway stays at its start, leaning back by 3% of the height
	vegetation.Update(std::chrono::duration<float, std::milli>(1000.0f));
	EXPECT_FLOAT_EQ(vegetation.GetLean(0), -0.03f);
	// The slots wrap at 16
	EXPECT_EQ(vegetation.GetLean(16), vegetation.GetLean(0));
}

TEST(VegetationSystem, AFieldLeansFurtherThanATree)
{
	ecs::systems::VegetationSystem vegetation;
	vegetation.Update(std::chrono::duration<float, std::milli>(1000.0f));
	glm::mat4 model(2.0f);
	model[1].x = 5.0f;
	model[1].w = 7.0f;
	const auto swaying = vegetation.GetFieldMatrix(model, 4.0f, 3);
	EXPECT_EQ(swaying[1].x, 0.0f);
	EXPECT_EQ(swaying[1].z, 4.0f * 1.75f * vegetation.GetLean(3));
	// The rest of the matrix is kept, the second column's w too
	EXPECT_EQ(swaying[1].y, 2.0f);
	EXPECT_EQ(swaying[1].w, 7.0f);
	EXPECT_EQ(swaying[0], model[0]);
	EXPECT_EQ(swaying[3], model[3]);
}
