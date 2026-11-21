/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The keys that move the world camera (Camera/CameraKeys.h): a key's step from the camera's whole milliseconds, the
// order and signs each key adds its step in, the modifiers that make the movement keys tilt, zoom or turn, and opposite
// keys cancelling

#include <cstdint>

#include <bit>
#include <chrono>

#include <gtest/gtest.h>

#include "Camera/CameraKeys.h"
#include "Camera/EdgeScroll.h"

using namespace openblack;
using namespace openblack::camera_keys;

namespace
{
/// A frame of 16 whole milliseconds, as the camera takes it: 16 x 0.001
const float k_Seconds = edge_scroll::FrameSeconds(16);
} // namespace

TEST(CameraKeys, AKeysStepIsFourHundredTimesTheCamerasSeconds)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_Seconds), 0x3C83126Fu);
	const auto steps = Step({.turnRight = true}, k_Seconds);
	EXPECT_EQ(std::bit_cast<uint32_t>(steps.turn), 0x40CCCCCDu);
	// A frame's part of a millisecond does not count
	using namespace std::chrono_literals;
	EXPECT_EQ(edge_scroll::FrameSeconds(std::chrono::duration_cast<std::chrono::milliseconds>(16666us).count()), k_Seconds);
}

TEST(CameraKeys, TheTurnAndTiltKeysTurnAndTilt)
{
	const float step = Step({.turnRight = true}, k_Seconds).turn;
	EXPECT_EQ(Step({.turnLeft = true}, k_Seconds).turn, -step);
	EXPECT_EQ(Step({.tiltUp = true}, k_Seconds).tilt, step);
	EXPECT_EQ(Step({.tiltDown = true}, k_Seconds).tilt, -step);
}

TEST(CameraKeys, OppositeKeysCancel)
{
	EXPECT_EQ(Step({.turnLeft = true, .turnRight = true}, k_Seconds).turn, 0.0f);
	EXPECT_EQ(Step({.tiltUp = true, .tiltDown = true}, k_Seconds).tilt, 0.0f);
	EXPECT_EQ(Step({.forwards = true, .backwards = true}, k_Seconds).forwards, 0.0f);
	EXPECT_EQ(Step({.left = true, .right = true}, k_Seconds).strafe, 0.0f);
	EXPECT_EQ(Step({.left = true, .right = true, .zoom = true}, k_Seconds).turn, 0.0f);
}

TEST(CameraKeys, TheMovementKeysMoveTheCamera)
{
	const float step = Step({.turnRight = true}, k_Seconds).turn;
	EXPECT_EQ(Step({.forwards = true}, k_Seconds).forwards, -step);
	EXPECT_EQ(Step({.backwards = true}, k_Seconds).forwards, step);
	EXPECT_EQ(Step({.left = true}, k_Seconds).strafe, -step);
	EXPECT_EQ(Step({.right = true}, k_Seconds).strafe, step);
}

TEST(CameraKeys, TheZoomKeyMakesThemZoomAndTurn)
{
	const float step = Step({.turnRight = true}, k_Seconds).turn;
	const auto in = Step({.forwards = true, .zoom = true}, k_Seconds);
	EXPECT_EQ(in.zoom, -step);
	EXPECT_EQ(in.forwards, 0.0f);
	EXPECT_EQ(Step({.backwards = true, .zoom = true}, k_Seconds).zoom, step);
	// Left turns the other way from the turn-left key, as the game adds it
	EXPECT_EQ(Step({.left = true, .zoom = true}, k_Seconds).turn, step);
	EXPECT_EQ(Step({.right = true, .zoom = true}, k_Seconds).turn, -step);
	EXPECT_EQ(Step({.left = true, .zoom = true}, k_Seconds).strafe, 0.0f);
}

TEST(CameraKeys, TheTurnModifierMakesThemTiltAndTurn)
{
	const float step = Step({.turnRight = true}, k_Seconds).turn;
	EXPECT_EQ(Step({.forwards = true, .turnModifier = true}, k_Seconds).tilt, -step);
	EXPECT_EQ(Step({.backwards = true, .turnModifier = true}, k_Seconds).tilt, step);
	EXPECT_EQ(Step({.left = true, .turnModifier = true}, k_Seconds).turn, step);
	EXPECT_EQ(Step({.right = true, .turnModifier = true}, k_Seconds).turn, -step);
}

TEST(CameraKeys, KeysOnOnePartAddUpInTheGamesOrder)
{
	// The turn-right key, then the left key with the modifier: two steps; then the tilt keys with the forwards key
	const float step = Step({.turnRight = true}, k_Seconds).turn;
	EXPECT_EQ(Step({.turnRight = true, .left = true, .turnModifier = true}, k_Seconds).turn, step + step);
	EXPECT_EQ(Step({.tiltDown = true, .forwards = true, .turnModifier = true}, k_Seconds).tilt, -step - step);
	EXPECT_EQ(std::bit_cast<uint32_t>(Step({.turnRight = true, .left = true, .turnModifier = true}, k_Seconds).turn + step),
	          0x4199999Au);
}
