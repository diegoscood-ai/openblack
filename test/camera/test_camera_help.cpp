/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The camera help's state and its system: the features the scripts allow, the auto-pitch, and the one copy the slot
// keeps

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "Camera/CameraHelp.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"

using namespace openblack;
using openblack::camera_help::CameraHelp;
using openblack::camera_help::Feature;

TEST(CameraHelp, StartsWithTheOriginalsValues)
{
	const CameraHelp help;
	EXPECT_EQ(help.features, 0x1BF);
	EXPECT_FLOAT_EQ(help.autoPitch, 0.523599f);
	EXPECT_FLOAT_EQ(help.autoPitchHeight, 75.0f);
	EXPECT_TRUE(help.IsFeatureEnabled(Feature::JustZoom));
	EXPECT_FALSE(help.IsFeatureEnabled(Feature::AutoPitch));
	EXPECT_TRUE(help.IsFeatureEnabled(Feature::DoubleClickFly));
}

TEST(CameraHelp, EnableReplacesOnlyTheMaskedBits)
{
	CameraHelp help;
	help.Enable(0x08, -1);
	EXPECT_EQ(help.features, 0x08);
	help.Enable(0x02, 0x03);
	EXPECT_EQ(help.features, 0x0A);
	help.Enable(0x40, 0);
	EXPECT_EQ(help.features, 0x4A);
}

TEST(CameraHelp, AutoPitchSetsItsBitAndKeepsItsParameters)
{
	CameraHelp help;
	help.Enable(0x08, -1);
	help.SetAutoPitch(1.0f, 2.0f, true);
	EXPECT_EQ(help.features, 0x48);
	EXPECT_TRUE(help.IsFeatureEnabled(Feature::AutoPitch));
	help.SetAutoPitch(3.0f, 4.0f, false);
	EXPECT_EQ(help.features, 0x08);
	EXPECT_FLOAT_EQ(help.autoPitch, 3.0f);
	EXPECT_FLOAT_EQ(help.autoPitchHeight, 4.0f);
}

TEST(CameraHelp, ResetGivesBackEveryStartValue)
{
	CameraHelp help;
	help.Enable(0, -1);
	help.SetAutoPitch(1.0f, 2.0f, true);
	help.Reset();
	EXPECT_EQ(help.features, 0x1BF);
	EXPECT_FLOAT_EQ(help.autoPitch, 0.523599f);
	EXPECT_FLOAT_EQ(help.autoPitchHeight, 75.0f);
}

TEST(CameraHelp, TheSelfTiltingCameraTiltsAFifthOfTheWayAtMostTheFramesSeconds)
{
	using openblack::camera_help::AutoPitchInput;
	// Within a hundredth (after the fifth: 0.04 x 0.2 = 0.008), nothing
	EXPECT_FALSE(AutoPitchInput(0.5f, 0.46f, 0.02f).has_value());
	// A fifth of 0.06 is 0.012, past a hundredth and under the frame's 0.02 seconds
	ASSERT_TRUE(AutoPitchInput(0.5f, 0.44f, 0.02f).has_value());
	EXPECT_NEAR(*AutoPitchInput(0.5f, 0.44f, 0.02f), 0.012f * -150.0f, 1e-4f);
	// A fifth of 0.3 is 0.06, more than the frame's 0.02 seconds
	ASSERT_TRUE(AutoPitchInput(0.5f, 0.2f, 0.02f).has_value());
	EXPECT_NEAR(*AutoPitchInput(0.5f, 0.2f, 0.02f), 0.02f * -150.0f, 1e-4f);
	EXPECT_NEAR(*AutoPitchInput(0.2f, 0.5f, 0.02f), -0.02f * -150.0f, 1e-4f);
	// A slow frame leaves the fifth
	EXPECT_NEAR(*AutoPitchInput(0.5f, 0.2f, 0.1f), 0.06f * -150.0f, 1e-4f);
}

TEST(CameraHelpSystem, KeepsOneCameraHelp)
{
	ecs::systems::CameraHelpSystem system;
	EXPECT_EQ(system.Get().features, 0x1BF);
	system.Get().Enable(0x01, -1);
	const auto& constSystem = system;
	EXPECT_EQ(&constSystem.Get(), &system.Get());
	EXPECT_EQ(constSystem.Get().features, 0x01);
}
