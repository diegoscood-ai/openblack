/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The saved games' pictures of the screen: asked of the frame on the screen (or of the next frame drawn), written into
// the temporary picture file once back, and put into the cells of the slots saved by themselves that wait for them
// (docs/wiki/engine/saved-games.md#the-pictures)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <SavePictureMaker.h>
#include <SavePictures.h>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/SavePictureSystem.h"
#include "support/FileSystemFakes.h"

using namespace openblack;
using openblack::ecs::systems::SavePictureSystem;

namespace
{
const std::filesystem::path k_Player = std::filesystem::path("Profiles") / "Player";
const std::filesystem::path k_Pictures = k_Player / "Saved Games" / "Pictures";

std::optional<std::filesystem::path> KeptProfile()
{
	return k_Player;
}

std::optional<std::filesystem::path> NoProfile()
{
	return std::nullopt;
}

/// A screen's picture of one colour
std::vector<uint8_t> ScreenPicture(uint8_t shade)
{
	return std::vector<uint8_t>(
	    static_cast<size_t>(gamesave::k_ScreenPictureWidth) * gamesave::k_ScreenPictureHeight * gamesave::k_TexelSize, shade);
}

/// The renderer: the shown frame's name it answers, and how many times it was asked
struct FakeRenderer
{
	std::optional<std::string> shownName;
	int asked {0};

	SavePictureSystem::AskShownFrame Ask()
	{
		return [this](const graphics::ScreenPicturer& /*picturer*/) {
			++asked;
			return shownName;
		};
	}
};

SavePictureSystem::Screens Back(const std::string& name, std::vector<uint8_t> picture)
{
	return {.take = [name, picture](const std::string& asked) -> std::optional<std::vector<uint8_t>> {
		        if (asked == name)
		        {
			        return picture;
		        }
		        return std::nullopt;
	        },
	        .failed = [](const std::string& /*name*/) { return false; }};
}
} // namespace

TEST(SavePictureSystem, WithNoProfileKeptNothingIsAskedNorKeptNorWritten)
{
	test::MemoryFileSystem files;
	FakeRenderer renderer;
	SavePictureSystem pictures(&files, NoProfile, renderer.Ask());
	EXPECT_FALSE(pictures.KeepsShownFrames());
	pictures.TakePicture(std::nullopt);
	pictures.TakePicture(19);
	EXPECT_EQ(renderer.asked, 0);
	EXPECT_TRUE(pictures.TakeRequests().empty());
	EXPECT_EQ(pictures.Waiting(), 0u);
	EXPECT_FALSE(pictures.ReadPictureFile("save_game_picture_lores_0.raw").has_value());
	EXPECT_EQ(files.writes, 0);
}

TEST(SavePictureSystem, ThePictureOfTheFrameOnTheScreenIsTheTemporaryPictureOnceBack)
{
	test::MemoryFileSystem files;
	FakeRenderer renderer {.shownName = "memory:shown-frame/7"};
	SavePictureSystem pictures(&files, KeptProfile, renderer.Ask());
	EXPECT_TRUE(pictures.KeepsShownFrames());
	// Going into the temple: no slot waits for it, and the frame on the screen needs no request of the next frame
	pictures.TakePicture(std::nullopt);
	EXPECT_EQ(renderer.asked, 1);
	EXPECT_TRUE(pictures.TakeRequests().empty());
	EXPECT_EQ(pictures.Waiting(), 1u);

	// Not back yet: nothing is written and it still waits
	pictures.Collect(Back("memory:other", ScreenPicture(0)));
	EXPECT_EQ(files.writes, 0);
	EXPECT_EQ(pictures.Waiting(), 1u);

	const auto screen = ScreenPicture(200);
	pictures.Collect(Back("memory:shown-frame/7", screen));
	EXPECT_EQ(pictures.Waiting(), 0u);
	const auto temporary = files.files.find(k_Pictures / "save_game_picture_temporary.raw");
	ASSERT_TRUE(temporary != files.files.end());
	// as the game makes it from no old file: the picture put into it, made smooth, its small picture beside
	EXPECT_EQ(temporary->second, gamesave::MakeTemporaryPicture(std::nullopt, screen));
	EXPECT_EQ(pictures.ReadPictureFile("save_game_picture_temporary.raw"), temporary->second);
	EXPECT_EQ(pictures.Changes(), 1u);
	// and no slot's cells were written
	EXPECT_FALSE(files.files.contains(k_Pictures / gamesave::LargePictureFileName(0)));
}

TEST(SavePictureSystem, ASaveByItselfPutsThePictureIntoItsSlotsCellsOnceBack)
{
	test::MemoryFileSystem files;
	FakeRenderer renderer;
	SavePictureSystem pictures(&files, KeptProfile, renderer.Ask());
	// The old temporary picture, of another screen, which the save copied at once
	const auto old = gamesave::MakeTemporaryPicture(std::nullopt, ScreenPicture(10));
	files.files[k_Pictures / "save_game_picture_temporary.raw"] = old;

	// The frame on the screen was not kept: the picture is asked of the next frame drawn
	pictures.TakePicture(19);
	const auto requests = pictures.TakeRequests();
	ASSERT_EQ(requests.size(), 1u);
	EXPECT_TRUE(graphics::ScreenPictures::InMemory(requests.front()));
	EXPECT_TRUE(pictures.TakeRequests().empty());

	const auto screen = ScreenPicture(90);
	pictures.Collect(Back(requests.front(), screen));
	const auto temporary = gamesave::MakeTemporaryPicture(old, screen);
	EXPECT_EQ(files.files.at(k_Pictures / "save_game_picture_temporary.raw"), temporary);
	// slot 19's cells: the bottom half of the tenth file of large pictures, a cell of the third file of small ones
	const auto expected = gamesave::PutSlotPictures(temporary, std::nullopt, std::nullopt, 19);
	EXPECT_EQ(files.files.at(k_Pictures / "save_game_picture_hires_9.raw"), expected.large);
	EXPECT_EQ(files.files.at(k_Pictures / "save_game_picture_lores_2.raw"), expected.small);
	EXPECT_EQ(pictures.Changes(), 2u);
}

TEST(SavePictureSystem, PicturesAskedOfOneFrameAreOnePictureForEverySlotWaiting)
{
	test::MemoryFileSystem files;
	FakeRenderer renderer {.shownName = "memory:shown-frame/3"};
	SavePictureSystem pictures(&files, KeptProfile, renderer.Ask());
	pictures.TakePicture(14);
	pictures.TakePicture(20);
	EXPECT_EQ(pictures.Waiting(), 1u);
	pictures.Collect(Back("memory:shown-frame/3", ScreenPicture(60)));
	EXPECT_TRUE(files.files.contains(k_Pictures / "save_game_picture_hires_7.raw"));
	EXPECT_TRUE(files.files.contains(k_Pictures / "save_game_picture_hires_10.raw"));
	EXPECT_TRUE(files.files.contains(k_Pictures / "save_game_picture_lores_1.raw"));
	EXPECT_TRUE(files.files.contains(k_Pictures / "save_game_picture_lores_2.raw"));
}

TEST(SavePictureSystem, APictureGoesToTheProfilePlayedAsItWasAskedForThoughAnotherIsPlayedOnceBack)
{
	test::MemoryFileSystem files;
	FakeRenderer renderer {.shownName = "memory:shown-frame/5"};
	// the save on leaving the game into the profile the Players page was opened with: that one played for the save,
	// the one picked played again before the picture is back
	std::filesystem::path played = std::filesystem::path("Profiles") / "Ann";
	SavePictureSystem pictures(
	    &files, [&played]() -> std::optional<std::filesystem::path> { return played; }, renderer.Ask());
	pictures.TakePicture(20);
	played = std::filesystem::path("Profiles") / "Bea";
	pictures.Collect(Back("memory:shown-frame/5", ScreenPicture(70)));
	const auto annPictures = std::filesystem::path("Profiles") / "Ann" / "Saved Games" / "Pictures";
	const auto beaPictures = std::filesystem::path("Profiles") / "Bea" / "Saved Games" / "Pictures";
	EXPECT_TRUE(files.files.contains(annPictures / "save_game_picture_temporary.raw"));
	EXPECT_TRUE(files.files.contains(annPictures / "save_game_picture_hires_10.raw"));
	EXPECT_TRUE(files.files.contains(annPictures / "save_game_picture_lores_2.raw"));
	EXPECT_FALSE(files.files.contains(beaPictures / "save_game_picture_temporary.raw"));
	EXPECT_FALSE(files.files.contains(beaPictures / "save_game_picture_hires_10.raw"));
}

TEST(SavePictureSystem, APictureGivenUpLeavesTheTemporaryPictureAndStillFillsTheSlotsCells)
{
	test::MemoryFileSystem files;
	FakeRenderer renderer;
	SavePictureSystem pictures(&files, KeptProfile, renderer.Ask());
	const auto old = gamesave::MakeTemporaryPicture(std::nullopt, ScreenPicture(30));
	files.files[k_Pictures / "save_game_picture_temporary.raw"] = old;
	pictures.TakePicture(2);
	pictures.TakePicture(std::nullopt);
	const auto requests = pictures.TakeRequests();
	ASSERT_EQ(requests.size(), 2u);
	pictures.Collect({.take = [](const std::string& /*name*/) { return std::optional<std::vector<uint8_t>> {}; },
	                  .failed = [&requests](const std::string& name) { return name == requests.front(); }});
	// The first was given up, so slot 2 takes the old picture; the second still waits
	EXPECT_EQ(files.files.at(k_Pictures / "save_game_picture_temporary.raw"), old);
	EXPECT_EQ(files.files.at(k_Pictures / "save_game_picture_hires_1.raw"),
	          gamesave::PutSlotPictures(old, std::nullopt, std::nullopt, 2).large);
	EXPECT_EQ(pictures.Waiting(), 1u);
}

TEST(SavePictureSystem, TheScreensPictureIsTheGamesSampleOfTheWholeScreen)
{
	constexpr uint32_t k_Width = 640;
	constexpr uint32_t k_Height = 480;
	std::vector<uint8_t> bgra(static_cast<size_t>(k_Width) * k_Height * 4);
	for (size_t i = 0; i < bgra.size(); ++i)
	{
		bgra[i] = static_cast<uint8_t>((i * 7) % 251);
	}
	const graphics::ScreenView screen {.width = k_Width, .height = k_Height, .pitch = k_Width * 4, .yflip = true, .bgra = bgra};
	const auto expected =
	    gamesave::SampleScreen({.width = k_Width, .height = k_Height, .pitch = k_Width * 4, .bottomUp = true, .bgra = bgra});
	ASSERT_TRUE(expected.has_value());
	EXPECT_EQ(ecs::systems::SaveScreenPicture(screen), *expected);
	EXPECT_TRUE(ecs::systems::SaveScreenPicture({}).empty());
}
