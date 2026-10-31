/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A sick creature's drops: twelve a turn from its head, thrown out at random a little in brownish tints under the land's
// light; each falls under gravity, lands and slides off at random, slowing, three seconds before it lands and as long
// again after, drawn solid in a frame of the sheet's fifth row picked anew each time.
// docs/wiki/creature/physiology.md#being-sick

#include <cmath>
#include <cstdint>

#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreaturePuke.h"
#include "Physics/TurnRules.h"

using namespace openblack;
namespace turn = openblack::physics::turn;

namespace
{
/// The numbers a scripted local stream gives, one after the other, and what it was asked for
struct Scripted
{
	std::vector<int32_t> values;
	std::vector<int32_t> asked;
	size_t next {0};
	int32_t operator()(int32_t n)
	{
		asked.push_back(n);
		return values.at(next++ % values.size());
	}
};
} // namespace

// Twelve drops from the mouth, each taking its speed across, down and across, then its red, green and blue, on the
// local stream, the speed a hundredth of 0.8 of the size a step less 0.4 of it across, and a fifth of the size
TEST(CreaturePuke, TwelveDropsATurnFromTheMouthEachThrownAtRandom)
{
	Scripted script {.values = {50, 10, 0, 99, 79, 29}};
	const creature_puke::LocalRandom random = [&script](int32_t n) { return script(n); };
	const glm::vec3 mouth {10.0f, 20.0f, 30.0f};
	const float size = 2.0f;
	const auto drops = creature_puke::DropsOf(mouth, size, 0xFFFFFFFFu, random);
	ASSERT_EQ(drops.size(), 12u);
	ASSERT_EQ(script.asked.size(), 12u * 6u);
	EXPECT_EQ((std::vector<int32_t>(script.asked.begin(), script.asked.begin() + 6)),
	          (std::vector<int32_t> {100, 100, 100, 100, 80, 30}));
	const auto& first = drops.front();
	EXPECT_EQ(first.position, mouth);
	EXPECT_EQ(first.velocity.x, 50.0f * (size * 0.008f) - size * 0.4f);
	EXPECT_EQ(first.velocity.y, -(10.0f * (size * 0.008f)));
	EXPECT_EQ(first.velocity.z, 0.0f * (size * 0.008f) - size * 0.4f);
	EXPECT_EQ(first.size, size * 0.2f);
	// under a white light the tint is (99 + 80, 79 + 60, 29 + 20)
	EXPECT_EQ(first.argb, 0xFFB38B31u);
}

// The tint is cut to whole numbers channel by channel, the light times the tint over 255; the alpha is the light's
TEST(CreaturePuke, TheTintIsTheLightTimesTheTintCut)
{
	EXPECT_EQ(creature_puke::DropColour(0x80808080u, 100, 60, 20), 0x80321E0Au);
	EXPECT_EQ(creature_puke::DropColour(0xFF000000u, 179, 139, 49), 0xFF000000u);
}

// A drop lives three seconds, keeps its size, and shows a frame of the sheet's fifth row
TEST(CreaturePuke, ADropLivesThreeSecondsAtItsSizeInTheFifthRow)
{
	EXPECT_EQ(turn::PuffLife(turn::PuffKind::Drop), 3.0f);
	EXPECT_FALSE(turn::IsAdditive(turn::PuffKind::Drop));
	EXPECT_EQ(turn::DustPuffSize(turn::PuffKind::Drop, 1.5f, 0.4f), 0.4f);
	EXPECT_EQ(turn::DropFrame(0), 32);
	EXPECT_EQ(turn::DropFrame(15), 39);
	EXPECT_EQ(turn::DropFrame(16), 32);
}

// Falling, gravity takes 9.81 a second off its upward speed, then it moves by the new speed
TEST(CreaturePuke, AFallingDropTakesGravityThenMoves)
{
	glm::vec3 position {0.0f, 10.0f, 0.0f};
	glm::vec3 velocity {1.0f, 0.0f, -2.0f};
	turn::FallDrop(position, velocity, 0.5f);
	EXPECT_EQ(velocity.y, 0.0f - 0.5f * 9.81f);
	EXPECT_EQ(position, glm::vec3(0.5f, 0.5f * velocity.y + 10.0f, -1.0f));
}

// It lands sliding off at (r % 100) times its size times 0.6 less thirty times its size on x and on z, with no upward
// speed
TEST(CreaturePuke, ALandedDropSlidesOffAtRandom)
{
	const float size = 0.5f;
	const auto velocity = turn::DropLandingVelocity(size, 150, 7);
	EXPECT_EQ(velocity.x, 50.0f * size * 0.599999964f - size * 30.0f);
	EXPECT_EQ(velocity.y, 0.0f);
	EXPECT_EQ(velocity.z, 7.0f * size * 0.599999964f - size * 30.0f);
}

// Sliding, its speed falls to a quarter each second, then it moves by it
TEST(CreaturePuke, ASlidingDropSlowsToAQuarterASecond)
{
	glm::vec3 position {0.0f, 0.0f, 0.0f};
	glm::vec3 velocity {8.0f, 0.0f, -4.0f};
	turn::SlideDrop(position, velocity, 1.0f);
	EXPECT_EQ(velocity, glm::vec3(2.0f, 0.0f, -1.0f));
	EXPECT_EQ(position, glm::vec3(2.0f, 0.0f, -1.0f));
	turn::SlideDrop(position, velocity, 0.5f);
	EXPECT_FLOAT_EQ(velocity.x, 1.0f);
}
