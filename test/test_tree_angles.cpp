/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A tree keeps its angle within a turn of zero on its own side, and sways in the wind slot of that angle, rounded by
// adding a half and cutting towards zero, as the original makes a tree.
// docs/wiki/world/trees.md#drawing

#include <gtest/gtest.h>

#include "ECS/TreeAngles.h"

using namespace openblack::ecs::tree_angles;

// An angle of more than a turn either way loses its whole turns, counted towards zero; one within a turn is kept
TEST(TreeAngles, AnAngleKeepsWithinATurnOnItsOwnSide)
{
	EXPECT_EQ(StoredYAngle(7.0f), 7.0f - 6.28318548f);
	EXPECT_EQ(StoredYAngle(-7.0f), -7.0f + 6.28318548f);
	EXPECT_EQ(StoredYAngle(13.0f), 13.0f - 2.0f * 6.28318548f);
	EXPECT_EQ(StoredYAngle(-0.3f), -0.3f);
	EXPECT_EQ(StoredYAngle(3.0f), 3.0f);
	EXPECT_EQ(StoredYAngle(0.0f), 0.0f);
}

// The slot is the angle in sixteenths of a turn plus a half, cut towards zero: a small negative angle is slot 0, not
// slot 15, and a larger one wraps round to the high slots
TEST(TreeAngles, TheWindSlotIsCutTowardsZero)
{
	EXPECT_EQ(WindSlotOf(0.0f), 0);
	EXPECT_EQ(WindSlotOf(-0.3f), 0);
	EXPECT_EQ(WindSlotOf(0.3f), 1);
	EXPECT_EQ(WindSlotOf(3.14159274f), 8);
	EXPECT_EQ(WindSlotOf(StoredYAngle(7.0f)), 2);
	// -0.7168: -1.825 + 0.5 = -1.325, cut to -1, the low four bits 15
	EXPECT_EQ(WindSlotOf(StoredYAngle(-7.0f)), 15);
	EXPECT_EQ(WindSlotOf(-1.0f), 14);
}
