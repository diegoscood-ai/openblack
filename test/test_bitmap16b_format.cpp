/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The 16-bit images (.16b, and the sky's .555), read as the game reads them: a header of four 32-bit words, the width
// the second and the height the third, then the 16-bit texels row by row. A file written in the test reads back texel
// by texel; the game's sky pictures are 256 by 256 and its temple pictures read whole.
// docs/wiki/project/progress/engine/asset_formats.md

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "Common/Bitmap16B.h"

using namespace openblack;

namespace
{
[[nodiscard]] std::vector<uint8_t> FileBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

// A file of a 3 by 2 image reads back its size and its texels in order
TEST(Bitmap16BFormat, AFileWrittenInTheTestReadsBackItsSizeAndTexels)
{
	constexpr std::array<uint32_t, 4> k_Header {0x12345678u, 3, 2, 0};
	constexpr std::array<uint16_t, 6> k_Texels {0x7FFF, 0x001F, 0x03E0, 0x7C00, 0x0000, 0x1234};
	std::vector<uint8_t> bytes(sizeof(k_Header) + sizeof(k_Texels));
	std::memcpy(bytes.data(), k_Header.data(), sizeof(k_Header));
	std::memcpy(bytes.data() + sizeof(k_Header), k_Texels.data(), sizeof(k_Texels));
	const Bitmap16B image(bytes);
	EXPECT_EQ(image.Width(), 3u);
	EXPECT_EQ(image.Height(), 2u);
	ASSERT_EQ(image.Size(), sizeof(k_Texels));
	for (size_t i = 0; i < k_Texels.size(); ++i)
	{
		EXPECT_EQ(image.Data()[i], k_Texels.at(i)) << i;
	}
}

// The game's nine sky pictures, three alignments by three times of day, are 256 by 256 and fill their files; its
// temple pictures fill theirs
TEST(Bitmap16BFormat, realDataTheSkyPicturesAre256Square)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto data = std::filesystem::path(game) / "Data";
	for (const auto* name :
	     {"Sky_Ntrl_Day.555", "Sky_Ntrl_Dusk.555", "Sky_Ntrl_Night.555", "sky_evil_day.555", "sky_evil_dusk.555",
	      "sky_evil_night.555", "sky_good_day.555", "sky_good_dusk.555", "sky_good_night.555"})
	{
		const auto bytes = FileBytes(data / "WeatherSystem" / name);
		ASSERT_FALSE(bytes.empty()) << name;
		const Bitmap16B image(bytes);
		EXPECT_EQ(image.Width(), 256u) << name;
		EXPECT_EQ(image.Height(), 256u) << name;
		EXPECT_EQ(image.Size() + 16u, bytes.size()) << name;
	}
	size_t temple = 0;
	for (const auto& entry : std::filesystem::directory_iterator(data / "Citadel" / "OutsideMeshes"))
	{
		if (entry.path().extension() != ".16b")
		{
			continue;
		}
		const auto bytes = FileBytes(entry.path());
		const Bitmap16B image(bytes);
		EXPECT_GT(image.Width() * image.Height(), 0u) << entry.path();
		EXPECT_EQ(image.Size() + 16u, bytes.size()) << entry.path();
		++temple;
	}
	EXPECT_GT(temple, 0u);
}
