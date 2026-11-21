/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player's own symbol pictures (src/3D/PlayerOwnSymbols.h): which files are read, the greys their texels take, and
// the cells they go into, on bitmaps made here and a fake profile folder

#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/PlayerOwnSymbols.h"

using namespace openblack::player_own_symbols;

namespace
{
/// A bitmap file with a header of 40 bytes: its height negative for rows from the top down, its palette as blue, green,
/// red and a spare byte, then its texels as given
std::vector<uint8_t> File(int32_t width, int32_t height, uint16_t depth, uint32_t compression,
                          const std::vector<uint8_t>& palette, const std::vector<uint8_t>& texels)
{
	constexpr uint32_t k_Headers = 14 + 40;
	std::vector<uint8_t> bytes(k_Headers, 0);
	const auto word = [&bytes](size_t at, uint32_t value, size_t length) {
		for (size_t i = 0; i < length; ++i)
		{
			bytes.at(at + i) = static_cast<uint8_t>(value >> (8 * i));
		}
	};
	bytes.at(0) = 'B';
	bytes.at(1) = 'M';
	word(10, k_Headers + static_cast<uint32_t>(palette.size()), 4);
	word(14, 40, 4);
	word(18, static_cast<uint32_t>(width), 4);
	word(22, static_cast<uint32_t>(height), 4);
	word(26, 1, 2);
	word(28, depth, 2);
	word(30, compression, 4);
	word(46, static_cast<uint32_t>(palette.size() / 4), 4);
	bytes.insert(bytes.end(), palette.begin(), palette.end());
	bytes.insert(bytes.end(), texels.begin(), texels.end());
	word(2, static_cast<uint32_t>(bytes.size()), 4);
	return bytes;
}

/// The grey ramp palette, entry i the grey i
std::vector<uint8_t> GreyPalette()
{
	std::vector<uint8_t> palette;
	for (uint32_t i = 0; i < 256; ++i)
	{
		const auto v = static_cast<uint8_t>(i);
		palette.insert(palette.end(), {v, v, v, 0});
	}
	return palette;
}

/// The grey of texel (x, y) of the test pictures
uint8_t GreyAt(uint32_t x, uint32_t y)
{
	return static_cast<uint8_t>((x * 3) + (y * 5));
}

/// A 64 x 64 picture of 8 bits a texel, grey palette, its rows from the bottom up as bitmaps keep them
std::vector<uint8_t> GreyPicture()
{
	std::vector<uint8_t> texels;
	for (int32_t row = 63; row >= 0; --row)
	{
		for (uint32_t x = 0; x < 64; ++x)
		{
			texels.push_back(GreyAt(x, static_cast<uint32_t>(row)));
		}
	}
	return File(64, 64, 8, 0, GreyPalette(), texels);
}
} // namespace

TEST(PlayerOwnSymbols, TheFilesAreSymbolOneToSixteen)
{
	EXPECT_EQ(FileName(0), "Symbol1.bmp");
	EXPECT_EQ(FileName(15), "Symbol16.bmp");
}

TEST(PlayerOwnSymbols, ATexelOfOneGreyStepTakesTheStepsMiddleAndAColourHasNoKnownGrey)
{
	// the values the game's own reading gave on this machine's Windows
	EXPECT_EQ(GreyOf(0, 0, 0), std::optional<uint8_t> {4});
	EXPECT_EQ(GreyOf(200, 200, 200), std::optional<uint8_t> {204});
	EXPECT_EQ(GreyOf(255, 255, 255), std::optional<uint8_t> {252});
	EXPECT_EQ(GreyOf(83, 85, 87), std::optional<uint8_t> {84});
	EXPECT_FALSE(GreyOf(19, 44, 222).has_value());
	EXPECT_FALSE(GreyOf(80, 80, 88).has_value());
}

TEST(PlayerOwnSymbols, APictureIsItsTexelsGreysTopRowFirst)
{
	const auto picture = PictureOf(GreyPicture());
	ASSERT_TRUE(picture.has_value());
	for (uint32_t y = 0; y < 64; ++y)
	{
		for (uint32_t x = 0; x < 64; ++x)
		{
			ASSERT_EQ(picture->at((y * 64) + x), *GreyOf(GreyAt(x, y), GreyAt(x, y), GreyAt(x, y))) << x << ", " << y;
		}
	}
}

TEST(PlayerOwnSymbols, TheTexelsPastASmallerBitmapAreNought)
{
	// 50 x 30 texels of 24 bits, grey 0x80: 4554 bytes with the headers
	std::vector<uint8_t> texels;
	constexpr uint32_t k_Stride = ((50 * 3) + 3) & ~3u;
	for (uint32_t row = 0; row < 30; ++row)
	{
		std::vector<uint8_t> line(k_Stride, 0);
		std::fill_n(line.begin(), 50 * 3, uint8_t {0x80});
		texels.insert(texels.end(), line.begin(), line.end());
	}
	const auto picture = PictureOf(File(50, 30, 24, 0, {}, texels));
	ASSERT_TRUE(picture.has_value());
	EXPECT_EQ(picture->at((29 * 64) + 49), 0x84);
	EXPECT_EQ(picture->at((29 * 64) + 50), 0);
	EXPECT_EQ(picture->at((30 * 64) + 0), 0);
}

TEST(PlayerOwnSymbols, FilesThatGiveNoPicture)
{
	// fewer than 4096 bytes: the game does not open it
	auto small = GreyPicture();
	small.resize(4095);
	EXPECT_EQ(PictureOf(small).error(), Refusal::TooSmall);
	// no bitmap
	EXPECT_EQ(PictureOf(std::vector<uint8_t>(5000, 7)).error(), Refusal::NotABitmap);
	// rows from the top down: Windows does not load it, and what the game does then is not established
	auto topDown = GreyPicture();
	topDown.at(22) = 0xC0;
	topDown.at(23) = 0xFF;
	topDown.at(24) = 0xFF;
	topDown.at(25) = 0xFF;
	EXPECT_EQ(PictureOf(topDown).error(), Refusal::NotDetermined);
	// a texel of colour
	auto colour = GreyPicture();
	const size_t entry = 14 + 40 + (GreyAt(0, 0) * 4u);
	colour.at(entry) = 0xFF;
	EXPECT_EQ(PictureOf(colour).error(), Refusal::NotDetermined);
}

TEST(PlayerOwnSymbols, RunsMustCoverEveryTexel)
{
	// 8-bit runs of two texels each, every row ended, then the end
	std::vector<uint8_t> runs;
	for (uint32_t row = 0; row < 64; ++row)
	{
		for (uint32_t x = 0; x < 64; x += 2)
		{
			runs.insert(runs.end(), {2, 0x40});
		}
		runs.insert(runs.end(), {0, 0});
	}
	runs.insert(runs.end(), {0, 1});
	const auto picture = PictureOf(File(64, 64, 8, 1, GreyPalette(), runs));
	ASSERT_TRUE(picture.has_value());
	EXPECT_EQ(picture->at(0), 0x44);
	// a jump over texels: what Windows leaves there was not established
	auto jump = runs;
	jump.insert(jump.begin() + 4, {0, 2, 2, 0});
	jump.erase(jump.begin() + 8, jump.begin() + 10);
	EXPECT_EQ(PictureOf(File(64, 64, 8, 1, GreyPalette(), jump)).error(), Refusal::NotDetermined);
}

TEST(PlayerOwnSymbols, APictureGoesIntoItsSymbolsCellInEveryChannel)
{
	Picture picture {};
	picture.fill(0x11);
	picture.at(0) = 0x99;
	std::vector<uint8_t> colour(256 * 256 * 3, 0);
	std::vector<uint8_t> alpha(256 * 256, 0);
	// symbol 5: the second cell of the second row
	Paste(colour, 3, 5, picture);
	Paste(alpha, 1, 5, picture);
	const size_t corner = (64 * 256) + 64;
	EXPECT_EQ(colour.at(corner * 3), 0x99);
	EXPECT_EQ(colour.at((corner * 3) + 2), 0x99);
	EXPECT_EQ(alpha.at(corner), 0x99);
	EXPECT_EQ(alpha.at(corner + 63), 0x11);
	EXPECT_EQ(alpha.at(corner + 64), 0);
	EXPECT_EQ(alpha.at(corner - 1), 0);
	EXPECT_EQ(alpha.at(((64 + 63) * 256) + 64), 0x11);
	EXPECT_EQ(alpha.at((128 * 256) + 64), 0);
}

TEST(PlayerOwnSymbols, OnlyTheProfileFolderPicturesThatReadAreTaken)
{
	// each picture taken is logged
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	// a fake profile folder: the first and the last pictures, and a third too small
	std::map<std::string, std::vector<uint8_t>> folder {
	    {"Symbol1.bmp", GreyPicture()},
	    {"Symbol3.bmp", std::vector<uint8_t>(100, 0)},
	    {"Symbol16.bmp", GreyPicture()},
	};
	const auto pictures = ReadPictures([&folder](const std::filesystem::path& file) -> std::optional<std::vector<uint8_t>> {
		const auto it = folder.find(file.string());
		return it != folder.end() ? std::optional {it->second} : std::nullopt;
	});
	EXPECT_TRUE(pictures.at(0).has_value());
	EXPECT_FALSE(pictures.at(1).has_value());
	EXPECT_FALSE(pictures.at(2).has_value());
	EXPECT_TRUE(pictures.at(15).has_value());
	// an empty folder: nothing
	const auto none = ReadPictures([](const std::filesystem::path&) { return std::optional<std::vector<uint8_t>> {}; });
	EXPECT_TRUE(std::ranges::none_of(none, [](const auto& p) { return p.has_value(); }));
}

TEST(PlayerOwnSymbols, AProfileReadLaterReplacesOnlyTheSymbolsItHasPicturesFor)
{
	Picture first {};
	first.fill(0x14);
	Picture second {};
	second.fill(0x24);
	Pictures kept;
	kept.at(0) = first;
	kept.at(1) = first;
	// the next profile has a picture for the second symbol only: the first keeps the earlier profile's
	Pictures read;
	read.at(1) = second;
	EXPECT_TRUE(Merge(kept, read));
	EXPECT_EQ(kept.at(0), std::optional {first});
	EXPECT_EQ(kept.at(1), std::optional {second});
	EXPECT_FALSE(kept.at(2).has_value());
	// read again: nothing changes
	EXPECT_FALSE(Merge(kept, read));
	EXPECT_FALSE(Merge(kept, Pictures {}));
}

TEST(PlayerOwnSymbols, OnlyTheChooseSymbolFilesTakeThePictures)
{
	EXPECT_EQ(ChannelsOfSheet("choosesymbol"), std::optional<uint8_t> {3});
	EXPECT_EQ(ChannelsOfSheet("choosesymbola"), std::optional<uint8_t> {1});
	EXPECT_FALSE(ChannelsOfSheet("originalchoosesymbol").has_value());
	EXPECT_FALSE(ChannelsOfSheet("playerssymbols").has_value());
}
