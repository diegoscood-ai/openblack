/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the command line chooses: the modern graphics interface drawn through (Vulkan unless another is named, and none
// of DirectX 7) and the window's mode (windowed, full screen or borderless), by their exact names.
// docs/wiki/project/progress/engine/platforms_and_performance.md, rendering/display_and_frame_rate.md

#include <optional>

#include <gtest/gtest.h>

#include "LaunchOptions.h"

using namespace openblack;
using namespace openblack::launch_options;

TEST(LaunchOptions, TheGameDrawsThroughVulkanUnlessTheCommandLineNamesAnotherInterface)
{
#ifndef _APPLE_
	EXPECT_EQ(DefaultBackend(), GraphicsBackend::Vulkan);
#endif
	EXPECT_EQ(BackendNamed("Vulkan"), GraphicsBackend::Vulkan);
	EXPECT_EQ(BackendNamed("Direct3D12"), GraphicsBackend::Direct3D12);
	EXPECT_EQ(BackendNamed("Metal"), GraphicsBackend::Metal);
	EXPECT_EQ(BackendNamed("Noop"), GraphicsBackend::Noop);
	// the names are exact, and the old interface is not one of them
	EXPECT_EQ(BackendNamed("vulkan"), std::nullopt);
	EXPECT_EQ(BackendNamed("Direct3D7"), std::nullopt);
	EXPECT_EQ(BackendNamed(""), std::nullopt);
}

TEST(LaunchOptions, TheWindowIsWindowedFullScreenOrBorderlessByItsName)
{
	EXPECT_EQ(DisplayModeNamed("windowed"), windowing::DisplayMode::Windowed);
	EXPECT_EQ(DisplayModeNamed("fullscreen"), windowing::DisplayMode::Fullscreen);
	EXPECT_EQ(DisplayModeNamed("borderless"), windowing::DisplayMode::Borderless);
	EXPECT_EQ(DisplayModeNamed("Fullscreen"), std::nullopt);
	EXPECT_EQ(DisplayModeNamed("full screen"), std::nullopt);
}
