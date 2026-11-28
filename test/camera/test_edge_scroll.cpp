/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Camera/EdgeScroll.h"

using namespace openblack;
using namespace openblack::edge_scroll;

namespace
{
/// A 1024 x 768 screen: its last pixel is 1023 across and 767 down, the band past the edges 127 wide
constexpr glm::ivec2 k_Screen {1024, 768};
constexpr int32_t k_Band = 127;
constexpr float k_Step = 30.0f;
constexpr float k_Seconds = 0.1f;
constexpr double k_Near = 1e-5;
} // namespace

TEST(EdgeScroll, ItNeedsThePushScrollingTheFeatureAndFullScreen)
{
	EXPECT_TRUE(Enabled(true, 0x1BF, false));
	EXPECT_FALSE(Enabled(false, 0x1BF, false));
	EXPECT_FALSE(Enabled(true, 0x1BF & ~k_Feature, false));
	EXPECT_FALSE(Enabled(true, 0x1BF, true));
}

TEST(EdgeScroll, TheStepIsHalfThreeTimesTheHeightKeptWithinItsLimits)
{
	EXPECT_FLOAT_EQ(Step(150.0f, 50.0f), 150.0f);
	EXPECT_FLOAT_EQ(Step(55.0f, 50.0f), 30.0f);
	EXPECT_FLOAT_EQ(Step(5000.0f, 0.0f), 1000.0f);
	// below the focus at most four times the least
	EXPECT_FLOAT_EQ(Step(0.0f, 50.0f), 75.0f);
	EXPECT_FLOAT_EQ(Step(0.0f, 500.0f), 120.0f);
	EXPECT_FLOAT_EQ(Step(0.0f, 10.0f), 30.0f);
}

TEST(EdgeScroll, TheCursorRunsOnPastAnEdgeOnlyInFullScreen)
{
	const auto inside = FollowPointer({-50, 900}, {500, 400}, {-3, 4}, k_Screen, false);
	EXPECT_EQ(inside.position, glm::ivec2(500, 400));
	EXPECT_TRUE(inside.insideX);
	EXPECT_TRUE(inside.insideY);
	const auto atLeft = FollowPointer({-50, 400}, {0, 400}, {-3, 4}, k_Screen, false);
	EXPECT_EQ(atLeft.position, glm::ivec2(-53, 400));
	EXPECT_FALSE(atLeft.insideX);
	EXPECT_TRUE(atLeft.insideY);
	const auto atBottom = FollowPointer({500, 790}, {500, 767}, {0, 6}, k_Screen, false);
	EXPECT_EQ(atBottom.position, glm::ivec2(500, 796));
	EXPECT_FALSE(atBottom.insideY);
	const auto windowed = FollowPointer({-50, 400}, {0, 400}, {-3, 4}, k_Screen, true);
	EXPECT_EQ(windowed.position, glm::ivec2(0, 400));
	EXPECT_TRUE(windowed.insideX);
}

TEST(EdgeScroll, APushAcrossPastTheBandAddsToTheSpeedAndTheTurn)
{
	// on the middle row it does not turn
	const auto middle = Push({}, {-k_Band, 400}, {0, 383}, {-10, 0}, k_Screen, k_Step, k_Seconds);
	EXPECT_NEAR(middle.x, -10.0 * 0.14 * 30.0 * 0.1, k_Near);
	EXPECT_FLOAT_EQ(middle.y, 0.0f);
	EXPECT_FLOAT_EQ(middle.z, 0.0f);
	// at the bottom row it turns by the cube of how far down, the same way above and below the middle
	const auto bottom = Push({}, {1023 + k_Band, 767}, {1023, 767}, {10, 0}, k_Screen, k_Step, k_Seconds);
	const double down = static_cast<double>(767 - 383) / 767.0;
	EXPECT_NEAR(bottom.z, 10.0 * down * down * down * 30.0 * 0.1, k_Near);
	const auto top = Push({}, {1023 + k_Band, 0}, {1023, 0}, {10, 0}, k_Screen, k_Step, k_Seconds);
	const double up = 383.0 / 767.0;
	EXPECT_NEAR(top.z, 10.0 * up * up * up * 30.0 * 0.1, k_Near);
}

TEST(EdgeScroll, APushUpOrDownAddsLessAndTurnsByTheColumn)
{
	const auto pushed = Push({}, {1023, 767 + k_Band}, {1023, 767}, {0, 5}, k_Screen, k_Step, k_Seconds);
	const double across = static_cast<double>(1023 - 511) / 1023.0;
	const double step = 30.0 * static_cast<double>(0.87f);
	EXPECT_FLOAT_EQ(pushed.x, 0.0f);
	EXPECT_NEAR(pushed.y, 5.0 * 0.14 * step * 0.1, k_Near);
	EXPECT_NEAR(pushed.z, 5.0 * across * across * across * step * 0.1, k_Near);
}

TEST(EdgeScroll, NoPushInsideTheBandInwardsOrWithoutMovement)
{
	const glm::vec3 speed {1.0f, 2.0f, 3.0f};
	EXPECT_EQ(Push(speed, {-k_Band + 1, 400}, {0, 400}, {-10, 0}, k_Screen, k_Step, k_Seconds), speed);
	EXPECT_EQ(Push(speed, {-k_Band, 400}, {0, 400}, {10, 0}, k_Screen, k_Step, k_Seconds), speed);
	EXPECT_EQ(Push(speed, {-k_Band, 400}, {0, 400}, {0, 0}, k_Screen, k_Step, k_Seconds), speed);
}

TEST(EdgeScroll, TheSpeedDiesAwayFastInsideAndSlowlyAtAnEdge)
{
	const auto inside = Decay({1.0f, 1.0f, 1.0f}, true, true, k_Seconds);
	EXPECT_NEAR(inside.x, std::exp(-0.7), k_Near);
	EXPECT_NEAR(inside.z, std::exp(-0.7), k_Near);
	const auto atEdge = Decay({1.0f, 1.0f, 1.0f}, false, true, k_Seconds);
	EXPECT_NEAR(atEdge.y, std::exp(-0.02), k_Near);
	EXPECT_NEAR(atEdge.z, std::exp(-0.22), k_Near);
	// below a hundredth squared it stops
	EXPECT_EQ(Decay({0.05f, 0.05f, 0.05f}, false, false, k_Seconds), glm::vec3(0.0f));
	EXPECT_TRUE(Stopped({0.05f, 0.05f, 0.05f}));
	EXPECT_FALSE(Stopped({0.1f, 0.0f, 0.0f}));
}

TEST(EdgeScroll, TheMoveIsTheSpeedOverTheLandAndTheTurnItsShareOfTheWidth)
{
	const auto level = MoveOverLand({10.0f, 5.0f, 0.0f}, k_Seconds, glm::quarter_pi<float>(), 0.0f);
	EXPECT_NEAR(level.x, -0.16, k_Near);
	EXPECT_FLOAT_EQ(level.y, 0.0f);
	EXPECT_NEAR(level.z, 0.08, k_Near);
	// a flat pitch divides the forwards part by the least slope
	const auto flat = MoveOverLand({0.0f, 5.0f, 0.0f}, k_Seconds, 0.0f, 0.0f);
	EXPECT_NEAR(flat.z, 0.08 / static_cast<double>(0.2f), k_Near);
	EXPECT_NEAR(Turn({0.0f, 0.0f, 100.0f}, k_Seconds, 1024), 10.0 * glm::pi<double>() / 1024.0, k_Near);
}

TEST(EdgeScroll, TheDyingAwayIsTheGamesOwnPowerOfE)
{
	// the original's own results, for whole milliseconds as its camera's frame gives them (the oracle, through the
	// same FPU steps in the camera's smoother)
	struct Pin
	{
		float rate;
		int64_t ms;
		uint32_t bits;
	};
	constexpr std::array<Pin, 9> k_Pins {{{-7.0f, 16, 0x3F64E016u},
	                                      {-7.0f, 37, 0x3F459631u},
	                                      {-7.0f, 52, 0x3F31E464u},
	                                      {-7.0f, 97, 0x3F01D2DFu},
	                                      {-0.2f, 218, 0x3F751406u},
	                                      {-0.2f, 258, 0x3F73201Bu},
	                                      {-2.2f, 26, 0x3F71C489u},
	                                      {-2.2f, 96, 0x3F4F42BDu},
	                                      {-2.2f, 104, 0x3F4BA513u}}};
	for (const auto& pin : k_Pins)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(DecayFactor(pin.rate, FrameSeconds(pin.ms))), pin.bits)
		    << pin.rate << " over " << pin.ms << " ms";
	}
	EXPECT_EQ(DecayFactor(-7.0f, 0.0f), 1.0f);
}
