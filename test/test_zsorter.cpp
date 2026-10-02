/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The single queue of blended things (src/Graphics/ZSorter.h) against LH3DZSorter: far to near, stable on equal keys,
// the new entry dropped when full, one drain a frame, the key and the rain's packed user data.

#include <cmath>

#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ZSorter.h"

using namespace openblack::graphics;

namespace
{
std::vector<int> Drained(zsorter::Queue<int>& queue)
{
	std::vector<int> order;
	for (const auto& entry : queue.Drain())
	{
		order.push_back(*entry.item);
	}
	return order;
}
} // namespace

TEST(ZSorter, farToNearAndStable)
{
	// NewZObject 0x83F36A..0x83F376: before the first strictly smaller key, so equal keys keep their arrival order
	zsorter::Queue<int> queue;
	queue.Begin();
	queue.Submit(0, 10.0f);
	queue.Submit(1, 30.0f);
	queue.Submit(2, 10.0f);
	queue.Submit(3, 20.0f);
	queue.Submit(4, 30.0f);
	queue.Submit(5, 0.0f); // the key-0 callers (0x5E66B4, 0x68AB5C) are drawn last
	queue.Submit(6, 10.0f);
	EXPECT_EQ(Drained(queue), (std::vector<int> {1, 4, 3, 0, 2, 6, 5}));
}

TEST(ZSorter, drainedOnceAFrame)
{
	// fn_0082F280 sets [0xECA610]; FinishFrame drains only while it is 0, StartFrame clears it
	zsorter::Queue<int> queue;
	queue.Begin();
	queue.Submit(7, 1.0f);
	EXPECT_EQ(Drained(queue).size(), 1u);
	EXPECT_TRUE(Drained(queue).empty());
	queue.Begin();
	EXPECT_TRUE(queue.Empty());
	EXPECT_TRUE(Drained(queue).empty());
}

TEST(ZSorter, fullDropsTheNewOne)
{
	// 0x83F315 / 0x83F31C: with 0x800 entries the new one is lost, even if it is the farthest
	zsorter::Queue<int> queue;
	queue.Begin();
	for (int i = 0; i < static_cast<int>(zsorter::k_Capacity); ++i)
	{
		EXPECT_TRUE(queue.Submit(i, 1.0f));
	}
	EXPECT_FALSE(queue.Submit(-1, 1000.0f));
	EXPECT_EQ(queue.Size(), zsorter::k_Capacity);
	EXPECT_EQ(queue.Dropped(), 1u);
	const auto order = Drained(queue);
	EXPECT_EQ(order.front(), 0);
	EXPECT_EQ(order.back(), static_cast<int>(zsorter::k_Capacity) - 1);
}

TEST(ZSorter, userDataAndNaN)
{
	zsorter::Queue<int> queue;
	queue.Begin();
	queue.Submit(0, 5.0f, 0x12345u);
	// fcomp with a NaN sets C0 ("smaller"): a new NaN goes before the 5 (cur 5 against NaN is unordered), and then the
	// new 1 goes before the NaN (cur NaN against 1 is unordered too), ahead of the far 5
	queue.Submit(1, std::numeric_limits<float>::quiet_NaN());
	queue.Submit(2, 1.0f);
	const auto entries = queue.Drain();
	ASSERT_EQ(entries.size(), 3u);
	EXPECT_EQ(*entries[0].item, 2);
	EXPECT_EQ(*entries[1].item, 1);
	EXPECT_EQ(*entries[2].item, 0);
	EXPECT_EQ(entries[2].user, 0x12345u);
	EXPECT_EQ(entries[2].key, 5.0f);
}

TEST(ZSorter, key)
{
	// GetValueForZSorter: the squared distance, not the distance
	const glm::vec3 camera(1.0f, 2.0f, 3.0f);
	EXPECT_FLOAT_EQ(zsorter::Key(glm::vec3(4.0f, 6.0f, 15.0f), camera), 9.0f + 16.0f + 144.0f);
	EXPECT_FLOAT_EQ(zsorter::Key(glm::vec3(4.0f, 6.0f, 15.0f), camera, zsorter::SumOrder::XZY), 169.0f);
	// the two orders round differently in float: x^2 = 1, y^2 ~ 1e-6, z^2 = 2^24 (one ulp there is 2).
	// (1 + 1e-6) + 2^24 is just over 2^24 + 1 and rounds up; (1 + 2^24) is a tie that rounds to even, then + 1e-6 is lost
	const glm::vec3 point(1.0f, 1e-3f, 4096.0f);
	EXPECT_EQ(zsorter::Key(point, glm::vec3(0.0f)), 16777218.0f);
	EXPECT_EQ(zsorter::Key(point, glm::vec3(0.0f), zsorter::SumOrder::XZY), 16777216.0f);
}

TEST(ZSorter, rainUser)
{
	// fn_008341B0: alpha x 65536 + 256 trunc(z / 80) + trunc(x / 80), read back by 0x833F80
	const auto user = zsorter::PackRainUser(1680.0f, 2400.0f, 0x58);
	EXPECT_EQ(user, 0x58u * 65536u + 30u * 256u + 21u);
	const auto unpacked = zsorter::UnpackRainUser(user);
	EXPECT_EQ(unpacked.tileX, 21);
	EXPECT_EQ(unpacked.tileZ, 30);
	EXPECT_EQ(unpacked.alpha, 0x58);
	// ftol truncates: 159.9 / 80 is tile 1
	EXPECT_EQ(zsorter::UnpackRainUser(zsorter::PackRainUser(159.9f, 0.0f, 0)).tileX, 1);
}
