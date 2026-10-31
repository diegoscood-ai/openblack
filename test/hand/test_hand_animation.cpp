/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's lean with the trailing cursor and the clips it shows (hand_animation),
// docs/wiki/hand/animation-and-placement.md and docs/wiki/project/progress/hand/hand_animations.md

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "3D/HandPoseBuffers.h"
#include "Hand/HandAnimationRules.h"
#include "Hand/HandPlaceRules.h"

using namespace openblack;
namespace ha = openblack::hand_animation;

TEST(HandAnimation, TheSidewaysLeanIsTheTrailingCursorsLagClampedToEightyPixels)
{
	const auto sideways = [](glm::vec2 cursor, glm::vec2 smooth) {
		return hand_place::LeanMotion(cursor, smooth, false).sideways;
	};
	// the smoothed x less the mouse x, in pixels
	EXPECT_EQ(sideways({100.0f, 0.0f}, {140.0f, 0.0f}), std::optional(40.0f));
	EXPECT_EQ(sideways({100.0f, 0.0f}, {60.0f, 0.0f}), std::optional(-40.0f));
	// clamped to 80 pixels either way
	EXPECT_EQ(sideways({0.0f, 0.0f}, {500.0f, 0.0f}), std::optional(80.0f));
	EXPECT_EQ(sideways({500.0f, 0.0f}, {0.0f, 0.0f}), std::optional(-80.0f));
	EXPECT_EQ(sideways({0.0f, 0.0f}, {80.0f, 0.0f}), std::optional(80.0f));
	EXPECT_EQ(sideways({0.0f, 0.0f}, {20.0f, 0.0f}), std::optional(20.0f));
}

TEST(HandAnimation, TheBackAndForthLeanIsTheVerticalLag)
{
	const auto forwards = [](glm::vec2 cursor, glm::vec2 smooth, bool gripping) {
		return hand_place::LeanMotion(cursor, smooth, gripping).forwards;
	};
	// the mouse y less the smoothed y, clamped the same way
	EXPECT_EQ(forwards({0.0f, 100.0f}, {0.0f, 60.0f}, false), std::optional(40.0f));
	EXPECT_EQ(forwards({0.0f, 60.0f}, {0.0f, 100.0f}, false), std::optional(-40.0f));
	EXPECT_EQ(forwards({0.0f, 900.0f}, {0.0f, 10.0f}, false), std::optional(80.0f));
	EXPECT_EQ(forwards({0.0f, 10.0f}, {0.0f, 900.0f}, false), std::optional(-80.0f));
	// gripping changes only the sideways lean
	EXPECT_EQ(forwards({0.0f, 100.0f}, {0.0f, 60.0f}, true), std::optional(40.0f));
}

TEST(HandAnimation, ALeanOfAtMostATenThousandthIsLeftOut)
{
	const auto lean = [](glm::vec2 cursor, glm::vec2 smooth) { return hand_place::LeanMotion(cursor, smooth, false); };
	// the sideways lag itself
	EXPECT_EQ(lean({0.0f, 0.0f}, {0.0001f, 0.0f}).sideways, std::nullopt);
	EXPECT_EQ(lean({0.0f, 0.0f}, {-0.0001f, 0.0f}).sideways, std::nullopt);
	EXPECT_EQ(lean({0.0f, 0.0f}, {0.0002f, 0.0f}).sideways, std::optional(0.0002f));
	// the back-and-forth lean is left out while the smoothed y itself is that small, whatever the lag
	EXPECT_EQ(lean({0.0f, 40.0f}, {0.0f, 0.0f}).forwards, std::nullopt);
	EXPECT_EQ(lean({0.0f, 40.0f}, {0.0f, -0.0001f}).forwards, std::nullopt);
	EXPECT_EQ(lean({0.0f, 40.0f}, {0.0f, 0.001f}).forwards, std::optional(40.0f - 0.001f));
	// and kept with a large smoothed y and no lag at all: a lag of 0 still leans, to the range's time at half its
	// length
	EXPECT_EQ(lean({0.0f, 300.0f}, {0.0f, 300.0f}).forwards, std::optional(0.0f));
}

TEST(HandAnimation, GrippingTheLandTheSidewaysLeanGoesTheOtherWay)
{
	const auto sideways = [](glm::vec2 cursor, glm::vec2 smooth) {
		return hand_place::LeanMotion(cursor, smooth, true).sideways;
	};
	EXPECT_EQ(sideways({100.0f, 0.0f}, {140.0f, 0.0f}), std::optional(-40.0f));
	EXPECT_EQ(sideways({100.0f, 0.0f}, {60.0f, 0.0f}), std::optional(40.0f));
	EXPECT_EQ(sideways({500.0f, 0.0f}, {0.0f, 0.0f}), std::optional(80.0f));
}

TEST(HandAnimation, TheNormalStatesClockRunsOnThroughItsPosesAndTheFingersTakeTheirOwnTime)
{
	// the clock is the state's: a change of pose keeps its time, wrapped at the new pose's length
	openblack::HandStateClock clock;
	clock.Enter();
	EXPECT_EQ(ha::NormalPoseTime(clock.Advance(200, 266), ha::k_Wiggle, 266, false), 200u);
	EXPECT_EQ(ha::NormalPoseTime(clock.Advance(200, 533), ha::k_Stroke, 533, false), 400u);
	EXPECT_EQ(ha::NormalPoseTime(clock.Advance(200, 533), ha::k_Stroke, 533, false), 67u);
	// the fingers over the leash's rope: at their start, or at half their length while the leash is pulled
	EXPECT_EQ(ha::NormalPoseTime(clock.Advance(16, 526), ha::k_HoldFingers, 526, false), 0u);
	EXPECT_EQ(ha::NormalPoseTime(clock.Advance(16, 526), ha::k_HoldFingers, 526, true), 263u);
}

TEST(HandAnimation, OverAnObjectItStrokesUnlessItCanPickItUp)
{
	EXPECT_EQ(ha::OverObject(9), ha::k_CanPickUp);
	EXPECT_EQ(ha::OverObject(1), ha::k_Stroke);
	EXPECT_EQ(ha::OverObject(3), ha::k_Stroke);
	EXPECT_EQ(ha::OverObject(0x1D), ha::k_Stroke);
	EXPECT_EQ(ha::k_CanPickUp, std::string_view("Ccan_pickup"));
	EXPECT_EQ(ha::k_Stroke, std::string_view("Cstroke"));
}

TEST(HandAnimation, WithNothingUnderItTheHandWigglesOrShowsTheCamerasPoseInItsNormalStateOnly)
{
	EXPECT_EQ(ha::k_Wiggle, std::string_view("Cwiggle"));
	EXPECT_TRUE(ha::ShowsNavigationPose(false, 1, false, false, false));
	// dragging the land, over a screen object or a spell icon it does not
	EXPECT_FALSE(ha::ShowsNavigationPose(true, 1, false, false, false));
	EXPECT_FALSE(ha::ShowsNavigationPose(false, 1, true, false, false));
	EXPECT_FALSE(ha::ShowsNavigationPose(false, 1, false, true, false));
	// nor on the tied leash's rope, or pulling it
	EXPECT_FALSE(ha::ShowsNavigationPose(false, 1, false, false, true));
}

// On the tied leash's rope, or pulling it, the hand's fingers close over the rope instead of the wiggle
TEST(HandAnimation, OnTheTiedLeashsRopeTheHandsFingersCloseOverIt)
{
	EXPECT_EQ(ha::EntryClip(true), ha::k_HoldFingers);
	EXPECT_EQ(ha::EntryClip(false), ha::k_Wiggle);
	EXPECT_EQ(ha::k_HoldFingers, std::string_view("Chold_fingers"));
}

TEST(HandAnimation, InTheTempleTheIdleHandKeepsTheWiggle)
{
	// the temple's state (0xA) shows none of the camera's poses, so the wiggle stays
	EXPECT_FALSE(ha::ShowsNavigationPose(false, 0xA, false, false, false));
	// nor the other states
	for (const int32_t state : {0, 3, 4, 5, 7, 8, 9})
	{
		EXPECT_FALSE(ha::ShowsNavigationPose(false, state, false, false, false)) << state;
	}
}

namespace
{
/// The clips the hand's animations have, and the ones asked for, in order
struct Clips
{
	std::vector<std::string_view> have;
	std::vector<std::string_view> asked;

	[[nodiscard]] auto Has()
	{
		return [this](std::string_view name) {
			asked.push_back(name);
			return std::ranges::find(have, name) != have.end();
		};
	}
};
} // namespace
