/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// video::FfmpegDecoder (src/Video/FfmpegDecoder.h) and binkw32's YUV -> RGB (src/Video/BinkYuv.h). The tables are
// checked without data; the decoder against the golden frames of binkw32.dll 1.0w (dev\_scratch\asistente\video\golden,
// bink_oracle.exe: BinkCopyToBuffer flags 9, 555) by their CRC-32 (zlib's) when OPENBLACK_TEST_GAME_PATH (or
// OPENBLACK_TEST_BW_ROOT) is the game's folder; without it those tests skip. No golden file is in the repo.

#include <cctype>
#include <cstdint>
#include <cstdlib>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/Rgb16.h"
#include "Video/BikFile.h"
#include "Video/BinkYuv.h"
#include "Video/FfmpegDecoder.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
std::string Lower(std::string s)
{
	for (auto& c : s)
	{
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	for (const char* name : {"OPENBLACK_TEST_GAME_PATH", "OPENBLACK_TEST_BW_ROOT"})
	{
		const char* root = std::getenv(name);
		if (root != nullptr && *root != '\0' && std::filesystem::is_directory(root))
		{
			return std::filesystem::path(root);
		}
	}
	return std::nullopt;
}

// The original runs on a case-insensitive file system
std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = Lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (Lower(entry.path().filename().string()) == wanted)
			{
				found = entry.path();
				break;
			}
		}
		if (!found)
		{
			return std::nullopt;
		}
		current = *found;
	}
	return current;
}

/// zlib's CRC-32 (reflected 0xEDB88320, init and final xor 0xFFFFFFFF) of the 555 texels as little-endian bytes,
/// the layout of the golden frame_NNNN.bin
uint32_t Crc32(std::span<const uint16_t> texels)
{
	uint32_t crc = 0xFFFFFFFFu;
	const auto byte = [&crc](uint8_t b) {
		crc ^= b;
		for (int k = 0; k < 8; ++k)
		{
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
		}
	};
	for (const uint16_t t : texels)
	{
		byte(static_cast<uint8_t>(t));
		byte(static_cast<uint8_t>(t >> 8));
	}
	return ~crc;
}

/// What VideoPlayer::DecodeNextFrame does with the picture: BinkCopyToBuffer(flags 9) = rgb16::Quantize(Rgb555)
std::vector<uint16_t> To555(std::span<const uint8_t> rgba)
{
	std::vector<uint16_t> texels(rgba.size() / 4);
	graphics::rgb16::Quantize(graphics::rgb16::Format::Rgb555, rgba, texels);
	return texels;
}

struct Golden
{
	uint32_t frame;
	uint32_t crc; ///< zlib.crc32 of golden/<video>/frame_NNNN.bin
};
} // namespace

TEST(BinkYuv, Tables)
{
	// y' = max(0, 76309 * (Y - 16) >> 16): 16 -> 0, 22 -> 6, 23 -> 8, 235 -> 254, under 16 -> 0, over 235 unclamped
	EXPECT_EQ(bink_yuv::k_Luma[16], 0);
	EXPECT_EQ(bink_yuv::k_Luma[22], 6);
	EXPECT_EQ(bink_yuv::k_Luma[23], 8);
	EXPECT_EQ(bink_yuv::k_Luma[162], 169); // 146 * 255 / 219 is 170 exactly: 76309 is just under 255 / 219
	EXPECT_EQ(bink_yuv::k_Luma[235], 254);
	EXPECT_EQ(bink_yuv::k_Luma[10], 0);
	EXPECT_EQ(bink_yuv::k_Luma[255], 278);
	// The chroma tables round toward zero
	EXPECT_EQ(bink_yuv::k_BlueFromU[70], -117); // the classic 132201 would give -116
	EXPECT_EQ(bink_yuv::k_BlueFromU[71], -114);
	EXPECT_EQ(bink_yuv::k_BlueFromU[182], 108);
	EXPECT_EQ(bink_yuv::k_RedFromV[40], -140);
	EXPECT_EQ(bink_yuv::k_RedFromV[215], 138);
	EXPECT_EQ(bink_yuv::k_GreenFromU[128], 0);
	EXPECT_EQ(bink_yuv::k_GreenFromV[128], 0);
	EXPECT_EQ(bink_yuv::k_GreenFromV[129], 0);
	EXPECT_EQ(bink_yuv::k_GreenFromV[127], 0);
	// Grey stays grey; the clamp is on the sum
	const auto grey = bink_yuv::ToRgb(128, 128, 128);
	EXPECT_EQ(grey.r, 130);
	EXPECT_EQ(grey.g, 130);
	EXPECT_EQ(grey.b, 130);
	const auto dark = bink_yuv::ToRgb(12, 128, 124); // golden: G 3 with y' 0 (-53279 * -4 >> 16)
	EXPECT_EQ(dark.g, 3);
	EXPECT_EQ(dark.r, 0);
}

TEST(BinkYuv, NearestChroma)
{
	// 4x2 picture: one U / V per 2x2 block
	const uint8_t y[8] = {128, 128, 128, 128, 128, 128, 128, 128};
	const uint8_t u[2] = {128, 70};
	const uint8_t v[2] = {128, 128};
	std::vector<uint8_t> rgba(4 * 2 * 4);
	bink_yuv::CopyToRgba8({y, u, v, 4, 2, 2, 4, 2}, rgba);
	for (int row = 0; row < 2; ++row)
	{
		for (int x = 0; x < 4; ++x)
		{
			const auto* p = &rgba[static_cast<size_t>((row * 4 + x) * 4)];
			EXPECT_EQ(p[2], x < 2 ? 130 : 13) << x << "," << row; // 130 - 117
			EXPECT_EQ(p[3], 0xFF);
		}
	}
}

TEST(FfmpegDecoder, HasTheBinkDecoder)
{
	EXPECT_TRUE(FfmpegDecoder::IsAvailable());
}

TEST(FfmpegDecoder, RefusesNoFilm)
{
	FfmpegDecoder decoder;
	const BikFile file;
	EXPECT_FALSE(decoder.Open(file));
	EXPECT_TRUE(decoder.DecodeNext(0).empty());
}

TEST(FfmpegDecoder, LogoMatchesBinkw32)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	const auto path = FindNoCase(*root, "Data/logo.bik");
	ASSERT_TRUE(path.has_value());
	BikFile file;
	ASSERT_TRUE(file.Open(*path)) << file.GetError();
	FfmpegDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();

	constexpr Golden k_Logo[] = {{0, 0xC6AE5801}, {1, 0xF94865E9}};
	for (const auto& golden : k_Logo)
	{
		const auto rgba = decoder.DecodeNext(golden.frame);
		ASSERT_EQ(rgba.size(), size_t {768} * 512 * 4) << decoder.GetError();
		EXPECT_EQ(Crc32(To555(rgba)), golden.crc) << "logo.bik frame " << golden.frame;
	}
	EXPECT_TRUE(decoder.DecodeNext(2).empty()); // past the last frame
	// BinkGoto back to frame 0 (logo.bik: both frames are key frames)
	EXPECT_EQ(Crc32(To555(decoder.DecodeNext(0))), 0xC6AE5801u);
}

TEST(FfmpegDecoder, TipsGotoMatchesBinkw32)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	const auto path = FindNoCase(*root, "Data/tips.bik");
	ASSERT_TRUE(path.has_value());
	BikFile file;
	ASSERT_TRUE(file.Open(*path)) << file.GetError();
	FfmpegDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();
	// MakeTipVideo's BinkGoto(tip + 1): straight to a frame, backwards and forwards
	constexpr Golden k_Tips[] = {{34, 0x5CBF3B81}, {20, 0x3BAF3B3A}, {34, 0x5CBF3B81}};
	for (const auto& golden : k_Tips)
	{
		EXPECT_EQ(Crc32(To555(decoder.DecodeNext(golden.frame))), golden.crc) << "tips.bik frame " << golden.frame;
	}
}

TEST(FfmpegDecoder, IntroInOrderMatchesBinkw32)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	const auto path = FindNoCase(*root, "Data/INTRO.bik");
	ASSERT_TRUE(path.has_value());
	BikFile file;
	ASSERT_TRUE(file.Open(*path)) << file.GetError();
	FfmpegDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();
	// Frames 0..10 in order, as the player asks for them: frame 10 is the 10th delta after the only key frame
	std::span<const uint8_t> rgba;
	for (uint32_t i = 0; i <= 10; ++i)
	{
		rgba = decoder.DecodeNext(i);
		ASSERT_FALSE(rgba.empty()) << i << ": " << decoder.GetError();
	}
	EXPECT_EQ(Crc32(To555(rgba)), 0xB4672079u);
}
