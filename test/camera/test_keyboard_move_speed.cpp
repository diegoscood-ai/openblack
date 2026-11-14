/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "Camera/KeyboardMoveSpeed.h"

using namespace openblack;

TEST(KeyboardMoveSpeed, DefaultLeavesTheGamesDistanceUntouched)
{
	// At the default the camera has to move exactly as far as the game moves it, to the bit
	for (const float distance : {0.0f, 0.4f, 6.666667f, -13.25f, 400.0f})
	{
		EXPECT_EQ(ScaleKeyboardMove(distance, k_KeyboardMoveSpeedDefault), distance);
	}
}

TEST(KeyboardMoveSpeed, ScalesTheDistance)
{
	EXPECT_FLOAT_EQ(ScaleKeyboardMove(6.0f, 2.0f), 12.0f);
	EXPECT_FLOAT_EQ(ScaleKeyboardMove(-6.0f, 0.5f), -3.0f);
}

TEST(KeyboardMoveSpeed, KeepsWithinTheRange)
{
	EXPECT_EQ(ClampKeyboardMoveSpeed(0.0f), k_KeyboardMoveSpeedMin);
	EXPECT_EQ(ClampKeyboardMoveSpeed(-3.0f), k_KeyboardMoveSpeedMin);
	EXPECT_EQ(ClampKeyboardMoveSpeed(100.0f), k_KeyboardMoveSpeedMax);
	EXPECT_EQ(ClampKeyboardMoveSpeed(std::numeric_limits<float>::infinity()), k_KeyboardMoveSpeedMax);
	EXPECT_EQ(ClampKeyboardMoveSpeed(std::numeric_limits<float>::quiet_NaN()), k_KeyboardMoveSpeedDefault);
	EXPECT_EQ(ClampKeyboardMoveSpeed(3.0f), 3.0f);
	// A speed out of range scales as the nearest one in range
	EXPECT_FLOAT_EQ(ScaleKeyboardMove(1.0f, 50.0f), k_KeyboardMoveSpeedMax);
}

TEST(KeyboardMoveSpeed, TwoStepsDoubleAndHalve)
{
	EXPECT_FLOAT_EQ(StepKeyboardMoveSpeed(1.0f, 2), 2.0f);
	EXPECT_FLOAT_EQ(StepKeyboardMoveSpeed(1.0f, -2), 0.5f);
	EXPECT_FLOAT_EQ(StepKeyboardMoveSpeed(1.0f, 1), 1.41421356f);
	EXPECT_FLOAT_EQ(StepKeyboardMoveSpeed(4.0f, 2), 8.0f);
}

TEST(KeyboardMoveSpeed, SteppingBackReturnsToExactlyTheDefault)
{
	auto speed = k_KeyboardMoveSpeedDefault;
	for (int i = 0; i < 3; ++i)
	{
		speed = StepKeyboardMoveSpeed(speed, 1);
	}
	for (int i = 0; i < 3; ++i)
	{
		speed = StepKeyboardMoveSpeed(speed, -1);
	}
	EXPECT_EQ(speed, k_KeyboardMoveSpeedDefault);
	// A speed set off the steps by the slider snaps onto the nearest step first
	EXPECT_FLOAT_EQ(StepKeyboardMoveSpeed(1.1f, 0), 1.0f);
	EXPECT_FLOAT_EQ(StepKeyboardMoveSpeed(1.1f, 2), 2.0f);
}

TEST(KeyboardMoveSpeed, SteppingStopsAtTheEnds)
{
	EXPECT_EQ(StepKeyboardMoveSpeed(k_KeyboardMoveSpeedMax, 1), k_KeyboardMoveSpeedMax);
	EXPECT_EQ(StepKeyboardMoveSpeed(k_KeyboardMoveSpeedMin, -1), k_KeyboardMoveSpeedMin);
	EXPECT_EQ(StepKeyboardMoveSpeed(k_KeyboardMoveSpeedDefault, 100), k_KeyboardMoveSpeedMax);
	EXPECT_EQ(StepKeyboardMoveSpeed(k_KeyboardMoveSpeedDefault, -100), k_KeyboardMoveSpeedMin);
}

TEST(KeyboardMove, GoesFurtherTheBiggerTheZoomsStep)
{
	// Looking down at 45 degrees along the heading 0, forwards: a thousandth of the step for each unit of input
	constexpr float k_Pitch = std::numbers::pi_v<float> / 4.0f;
	const auto low = KeyboardMoveOffset({-10.0f, 0.0f}, 60.0f, k_Pitch, 0.0f);
	const auto high = KeyboardMoveOffset({-10.0f, 0.0f}, 600.0f, k_Pitch, 0.0f);
	EXPECT_NEAR(low.z, -0.6f, 1e-5f);
	EXPECT_NEAR(high.z, -6.0f, 1e-4f);
	EXPECT_NEAR(low.x, 0.0f, 1e-6f);
	EXPECT_EQ(low.y, 0.0f);
}

TEST(KeyboardMove, TheForwardsPartIsDividedByThePitchsTangentKeptBetweenAFifthAndTwo)
{
	// Level, the tangent is taken as 0.2; steep, as 2; the move across is not divided
	EXPECT_NEAR(KeyboardMoveOffset({-10.0f, 0.0f}, 60.0f, 0.0f, 0.0f).z, -3.0f, 1e-5f);
	EXPECT_NEAR(KeyboardMoveOffset({-10.0f, 0.0f}, 60.0f, 1.5f, 0.0f).z, -0.3f, 1e-5f);
	EXPECT_NEAR(KeyboardMoveOffset({0.0f, 10.0f}, 60.0f, 0.0f, 0.0f).x, -0.6f, 1e-5f);
	EXPECT_NEAR(KeyboardMoveOffset({0.0f, 10.0f}, 60.0f, 1.5f, 0.0f).x, -0.6f, 1e-5f);
}

TEST(KeyboardMove, TurnsWithTheHeading)
{
	// A quarter turn: forwards goes along x, across along z
	constexpr float k_Quarter = std::numbers::pi_v<float> / 2.0f;
	const auto forwards = KeyboardMoveOffset({-10.0f, 0.0f}, 60.0f, 0.0f, k_Quarter);
	EXPECT_NEAR(forwards.x, -3.0f, 1e-5f);
	EXPECT_NEAR(forwards.z, 0.0f, 1e-5f);
	const auto across = KeyboardMoveOffset({0.0f, 10.0f}, 60.0f, 0.0f, k_Quarter);
	EXPECT_NEAR(across.x, 0.0f, 1e-5f);
	EXPECT_NEAR(across.z, 0.6f, 1e-5f);
}

TEST(KeyboardMoveSpeed, BothKeysOfATurnOrTiltPairHeldCancelOut)
{
	// The turn keys (left, right) and the tilt keys (down, up) each add their own 400 a second
	EXPECT_EQ(KeyPairInput(false, false), 0.0f);
	EXPECT_EQ(KeyPairInput(true, false), -1.0f);
	EXPECT_EQ(KeyPairInput(false, true), 1.0f);
	// Both held: the camera stays still, rather than going the first key's way
	EXPECT_EQ(KeyPairInput(true, true), 0.0f);
}

TEST(KeyboardMoveSpeed, AWatchedFightKeepsTheTurnModifierOnUnlessTheZoomKeyIsHeld)
{
	// the turn key, or a fight watched in its arena; the zoom key held turns it off either way
	EXPECT_FALSE(TurnModifierOn(false, false, false));
	EXPECT_TRUE(TurnModifierOn(false, false, true));
	EXPECT_TRUE(TurnModifierOn(false, true, false));
	EXPECT_FALSE(TurnModifierOn(true, true, false));
	EXPECT_FALSE(TurnModifierOn(true, false, true));
}

TEST(KeyboardMoveSpeed, TheModifiersAloneOfferTheirIconsUnlessTheLandIsGripped)
{
	constexpr uint32_t k_TiltAndTurn = openblack::camera_drag::tricon::k_Pitch | openblack::camera_drag::tricon::k_Rotate;
	constexpr uint32_t k_ZoomAndTurn = openblack::camera_drag::tricon::k_Zoom | openblack::camera_drag::tricon::k_Rotate;
	// zoom key held, fight watched, turn key held, land gripped
	EXPECT_EQ(ModifierIconHints(false, false, false, false), 0U);
	EXPECT_EQ(ModifierIconHints(true, false, false, false), k_ZoomAndTurn);
	EXPECT_EQ(ModifierIconHints(false, false, true, false), k_TiltAndTurn);
	EXPECT_EQ(ModifierIconHints(false, true, false, false), k_TiltAndTurn);
	// The zoom key wins over the turn modifier, the clear view's two keys included
	EXPECT_EQ(ModifierIconHints(true, true, true, false), k_ZoomAndTurn);
	EXPECT_EQ(ModifierIconHints(true, false, true, false), k_ZoomAndTurn);
	// None while the land is gripped
	EXPECT_EQ(ModifierIconHints(true, false, false, true), 0U);
	EXPECT_EQ(ModifierIconHints(false, true, true, true), 0U);
}
