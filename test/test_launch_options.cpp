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

TEST(LaunchOptions, TheGamesOwnSwitchIsADashOrASlashThenItsWordInAnyCase)
{
	EXPECT_TRUE(HasGameSwitch("openblack.exe -PREINTROVIDEO", k_PreIntroVideoSwitch));
	EXPECT_TRUE(HasGameSwitch("openblack.exe /preintrovideo -g C:/Game", k_PreIntroVideoSwitch));
	// what follows the word does not matter, and the word may sit anywhere on the line
	EXPECT_TRUE(HasGameSwitch("openblack.exe -PreIntroVideoPlease", k_PreIntroVideoSwitch));
	EXPECT_TRUE(HasGameSwitch("a-preintrovideo", k_PreIntroVideoSwitch));
	// a word cut short, a word without its dash or slash, or another word is not it
	EXPECT_FALSE(HasGameSwitch("openblack.exe -PREINTROVIDE", k_PreIntroVideoSwitch));
	EXPECT_FALSE(HasGameSwitch("openblack.exe PREINTROVIDEO", k_PreIntroVideoSwitch));
	EXPECT_FALSE(HasGameSwitch("openblack.exe --pre-intro", k_PreIntroVideoSwitch));
	EXPECT_FALSE(HasGameSwitch("", k_PreIntroVideoSwitch));
}

TEST(LaunchOptions, AGameSwitchArgumentIsLeftOutOfTheNamedOptions)
{
	EXPECT_TRUE(IsGameSwitchArgument("-PREINTROVIDEO", k_PreIntroVideoSwitch));
	EXPECT_TRUE(IsGameSwitchArgument("/preintrovideo", k_PreIntroVideoSwitch));
	// the named options keep their own arguments, even one holding the word after a dash further on
	EXPECT_FALSE(IsGameSwitchArgument("--pre-intro", k_PreIntroVideoSwitch));
	EXPECT_FALSE(IsGameSwitchArgument("C:/Games/-preintrovideo", k_PreIntroVideoSwitch));
	EXPECT_FALSE(IsGameSwitchArgument("", k_PreIntroVideoSwitch));
}

TEST(LaunchOptions, NoLoadMusicIsOneOfTheGamesSwitches)
{
	EXPECT_TRUE(HasGameSwitch("openblack.exe -NOLOADMUSIC", k_NoLoadMusicSwitch));
	EXPECT_TRUE(HasGameSwitch("openblack.exe -g C:/Game /noloadmusic", k_NoLoadMusicSwitch));
	EXPECT_FALSE(HasGameSwitch("openblack.exe -NOLOADMUSI", k_NoLoadMusicSwitch));
	EXPECT_TRUE(IsGameSwitchArgument("-NoLoadMusic", k_NoLoadMusicSwitch));
	EXPECT_TRUE(IsGameSwitchArgument("/SETTINGS:my.cfg", k_SettingsSwitch));
	EXPECT_FALSE(IsGameSwitchArgument("my.cfg", k_SettingsSwitch));
}

TEST(LaunchOptions, TheSettingsFileIsWhatFollowsTheLastSettingsSwitchUpToASpace)
{
	EXPECT_EQ(SettingsFileNamed("openblack.exe -SETTINGS:mods.cfg -g C:/Game"), "mods.cfg");
	EXPECT_EQ(SettingsFileNamed("openblack.exe /settings:C:/cfg/mine.txt"), "C:/cfg/mine.txt");
	// the last one counts, and one with nothing after it names none
	EXPECT_EQ(SettingsFileNamed("openblack.exe -SETTINGS:a.txt -SETTINGS:b.txt"), "b.txt");
	EXPECT_EQ(SettingsFileNamed("openblack.exe -SETTINGS:a.txt -SETTINGS: -g C:/Game"), "");
	// without the colon, or without the switch, no file
	EXPECT_EQ(SettingsFileNamed("openblack.exe -SETTINGS a.txt"), "");
	EXPECT_EQ(SettingsFileNamed("openblack.exe"), "");
}
