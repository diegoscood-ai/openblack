/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <string>
#include <vector>

#include <EDTFile.h>
#include <gtest/gtest.h>

namespace
{
/// A fake segment file: the "LiOnHeAd" magic, then each segment's 32-byte name, u32 size and data
std::vector<uint8_t> SegmentFile(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& segments)
{
	std::vector<uint8_t> file = {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	for (const auto& [name, data] : segments)
	{
		std::vector<uint8_t> header(36, 0);
		std::memcpy(header.data(), name.data(), name.size());
		const auto size = static_cast<uint32_t>(data.size());
		std::memcpy(header.data() + 32, &size, 4);
		file.insert(file.end(), header.begin(), header.end());
		file.insert(file.end(), data.begin(), data.end());
	}
	return file;
}
} // namespace

TEST(CameraSegments, FindsASegmentByName)
{
	std::vector<uint8_t> camera(32, 0);
	camera[0] = 1;
	const auto file = SegmentFile({{"EDITOR", {1, 2, 3}}, {"Cam1", camera}});
	openblack::edt::EDTFile edits;
	ASSERT_EQ(edits.Open(file), openblack::edt::EDTResult::Success);
	ASSERT_EQ(edits.GetCameras().count(1), 1u);
	EXPECT_EQ(edits.GetCameras().count(2), 0u);
	EXPECT_TRUE(edits.GetTracks().empty());
}

TEST(CameraSegments, NotASegmentFile)
{
	const std::vector<uint8_t> file = {'n', 'o', 't', ' ', 'a', ' ', 'f', 'i', 'l', 'e'};
	openblack::edt::EDTFile edits;
	EXPECT_EQ(edits.Open(file), openblack::edt::EDTResult::ErrNotABlockFile);
}

TEST(CameraSegments, AShortSegmentIsNotATrack)
{
	const auto file = SegmentFile({{"Track7", {0, 0, 0, 0, 0x24, 0}}});
	openblack::edt::EDTFile edits;
	EXPECT_EQ(edits.Open(file), openblack::edt::EDTResult::ErrWayMalformed);
}
