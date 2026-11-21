/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Temple/TempleControlDraw.h"

using namespace openblack::temple::control_draw;

TEST(TempleControlDraw, ARoomsControlsAreTheSubMeshesItsControlsTakeInOrder)
{
	const auto controls = ControlSubMeshes(8, [](uint32_t index) { return index == 1 || index == 4 || index == 7; });
	EXPECT_EQ(controls, (std::vector<uint32_t> {1, 4, 7}));
	EXPECT_TRUE(ControlSubMeshes(3, [](uint32_t) { return false; }).empty());
	EXPECT_TRUE(ControlSubMeshes(0, [](uint32_t) { return true; }).empty());
}

TEST(TempleControlDraw, AControlIsDrawnInTheLightTimesItsWhiteAStepDownEachByte)
{
	// each byte times 255, then down by 8 bits: 255 becomes 254, 128 becomes 127, 0 stays
	const auto white = TimesWhite(glm::vec3(1.0f, 128.0f / 255.0f, 0.0f));
	EXPECT_FLOAT_EQ(white.x, 254.0f / 255.0f);
	EXPECT_FLOAT_EQ(white.y, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(white.z, 0.0f);
	EXPECT_FLOAT_EQ(TimesWhite(glm::vec3(1.0f / 255.0f)).x, 0.0f);
}

TEST(TempleControlDraw, AButtonHeldDownSinksAlongItsFramesZ)
{
	// a frame with its axes as they are: the sink is 0.3 back along its z axis, with that axis' length
	EXPECT_FLOAT_EQ(k_HeldButtonSink, 0.3f);
	const glm::mat4 identity(1.0f);
	EXPECT_EQ(HeldButtonOffset(identity), glm::vec3(0.0f, 0.0f, -0.3f));
	const glm::mat4 turned(glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
	                       glm::vec4(-2.0f, 0.0f, 0.0f, 0.0f), glm::vec4(5.0f, 6.0f, 7.0f, 1.0f));
	const auto offset = HeldButtonOffset(turned);
	EXPECT_FLOAT_EQ(offset.x, 0.6f);
	EXPECT_FLOAT_EQ(offset.y, 0.0f);
	EXPECT_FLOAT_EQ(offset.z, 0.0f);
}
