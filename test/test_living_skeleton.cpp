/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/ECS/LivingSkeleton.h: the skeleton mark in a living thing's status bits, which SET_SKELETON writes and
// IS_SKELETON and the hunters read. Synthetic status words.

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/LivingSkeleton.h"

using openblack::ecs::living_skeleton::HasSkeleton;
using openblack::ecs::living_skeleton::k_SkeletonBit;
using openblack::ecs::living_skeleton::WithSkeleton;

TEST(LivingSkeleton, theMarkIsTheSeventhBit)
{
	EXPECT_EQ(k_SkeletonBit, 0x40);
	EXPECT_TRUE(HasSkeleton(0x40));
	EXPECT_FALSE(HasSkeleton(0xBF));
}

TEST(LivingSkeleton, settingTheMarkKeepsEveryOtherBit)
{
	// dying (bit 0), the landing's land type (bits 4-5) and downed by a predator (0x80) stay as they were
	EXPECT_EQ(WithSkeleton(0x0081, true), 0x00C1);
	EXPECT_EQ(WithSkeleton(0x00F1, false), 0x00B1);
	EXPECT_EQ(WithSkeleton(0xFFFF, false), 0xFFBF);
	EXPECT_EQ(WithSkeleton(0x0000, false), 0x0000);
}

TEST(LivingSkeleton, settingItTwiceChangesNothing)
{
	EXPECT_EQ(WithSkeleton(WithSkeleton(0x0030, true), true), 0x0070);
	EXPECT_EQ(WithSkeleton(WithSkeleton(0x0070, false), false), 0x0030);
}
