/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's Bink 1 container cases on our video::BikFile: synthetic files for the layout and every check, and the
// game's five videos when OPENBLACK_GAME_PATH names the game's folder (those tests skip without it).

#include <cstddef>
#include <cstdint>

#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <BinkFile.h>
#include <gtest/gtest.h>

// the tests build their own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "TestTempFolder.h"
#include "bink/BinkTestFiles.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::video;
using namespace openblack::bink::test;

namespace
{
std::vector<std::vector<uint8_t>> Packets(std::initializer_list<size_t> sizes)
{
	std::vector<std::vector<uint8_t>> packets;
	uint8_t value = 1;
	for (const size_t size : sizes)
	{
		packets.emplace_back(size, value++);
	}
	return packets;
}

std::string ParseError(std::vector<uint8_t> data)
{
	BikFile file;
	EXPECT_FALSE(file.Parse(std::move(data)));
	return file.GetError();
}
} // namespace

TEST(VideoBinkFile, ReadsTheLayout)
{
	BikFile file;
	ASSERT_TRUE(file.Parse(MakeBik(Packets({8, 4, 6}), {.width = 640, .height = 360, .keyFrames = {0, 2}}))) << file.GetError();
	EXPECT_EQ(file.Revision(), 'i');
	EXPECT_EQ(file.FrameCount(), 3u);
	EXPECT_EQ(file.Width(), 640u);
	EXPECT_EQ(file.Height(), 360u);
	EXPECT_EQ(file.LargestFrameSize(), 8u);
	EXPECT_EQ(file.HeaderFileSize(), file.Size() - 8);
	EXPECT_TRUE(file.AudioTracks().empty());
	EXPECT_TRUE(file.IsKeyFrame(0));
	EXPECT_FALSE(file.IsKeyFrame(1));
	EXPECT_TRUE(file.IsKeyFrame(2));
	EXPECT_FALSE(file.IsKeyFrame(3));
	EXPECT_EQ(file.KeyFrameCount(), 2u);
	// His KeyFrameAtOrBefore is not checked: our container has no such query, the decoder finds the key frame itself
	const size_t tables = BikFile::k_HeaderSize + 4 * 4;
	EXPECT_EQ(file.FrameOffset(0), tables);
	EXPECT_EQ(file.FrameOffset(3), file.Size());
	EXPECT_EQ(file.VideoData(0).size(), 8u);
	EXPECT_EQ(file.VideoData(1).size(), 4u);
	EXPECT_EQ(file.VideoData(1)[0], 2);
	EXPECT_TRUE(file.VideoData(3).empty());
	EXPECT_TRUE(file.FrameData(3).empty());
}

TEST(VideoBinkFile, FrameRateIsRoundedDown)
{
	// 30000 / 1001 is 29.97 frames a second, played at 29
	BikFile file;
	ASSERT_TRUE(file.Parse(MakeBik(Packets({4}), {.fpsNumerator = 30000, .fpsDenominator = 1001}))) << file.GetError();
	EXPECT_EQ(file.Fps(), 29u);
}

TEST(VideoBinkFile, SplitsTheAudioPackets)
{
	// Two tracks: frame 0 has 4 bytes of the first and 2 of the second, then 6 bytes of video
	std::vector<uint8_t> packet = {4, 0, 0, 0, 0xA, 0xA, 0xA, 0xA, 2, 0, 0, 0, 0xB, 0xB, 1, 2, 3, 4, 5, 6};
	BikFile file;
	ASSERT_TRUE(file.Parse(MakeBik({packet}, {.audioTracks = 2}))) << file.GetError();
	ASSERT_EQ(file.AudioTracks().size(), 2u);
	EXPECT_EQ(file.AudioTracks()[1].sampleRate, 22051);
	EXPECT_EQ(file.AudioTracks()[1].id, 8u);
	EXPECT_EQ(file.AudioData(0, 0).size(), 4u);
	EXPECT_EQ(file.AudioData(0, 1).size(), 2u);
	EXPECT_EQ(file.AudioData(0, 1)[0], 0xB);
	EXPECT_TRUE(file.AudioData(0, 2).empty());
	ASSERT_EQ(file.VideoData(0).size(), 6u);
	EXPECT_EQ(file.VideoData(0)[0], 1);
}

TEST(VideoBinkFile, RejectsBadFiles)
{
	const auto good = MakeBik(Packets({4, 4}));
	ASSERT_TRUE(BikFile().Parse(good));

	EXPECT_NE(ParseError({}).find("shorter"), std::string::npos);
	EXPECT_NE(ParseError(std::vector<uint8_t>(good.begin(), good.begin() + 43)).find("shorter"), std::string::npos);

	auto bink2 = good;
	bink2[0] = 'K';
	bink2[1] = 'B';
	bink2[2] = '2';
	EXPECT_NE(ParseError(bink2).find("signature"), std::string::npos);

	auto noFrames = good;
	PutU32(noFrames, 8, 0);
	EXPECT_NE(ParseError(noFrames).find("no frames"), std::string::npos);

	auto noPicture = good;
	PutU32(noPicture, 24, 0);
	EXPECT_NE(ParseError(noPicture).find("no picture"), std::string::npos);

	auto noRate = good;
	PutU32(noRate, 32, 0);
	EXPECT_NE(ParseError(noRate).find("frame rate"), std::string::npos);

	// Counts far too big for the file must not wrap around
	auto hugeFrames = good;
	PutU32(hugeFrames, 8, 0xFFFFFFFF);
	EXPECT_NE(ParseError(hugeFrames).find("past the"), std::string::npos);
	auto hugeTracks = good;
	PutU32(hugeTracks, 40, 0xFFFFFFFF);
	EXPECT_NE(ParseError(hugeTracks).find("past the"), std::string::npos);

	auto insideTables = good;
	PutU32(insideTables, BikFile::k_HeaderSize, 8);
	EXPECT_NE(ParseError(insideTables).find("inside the tables"), std::string::npos);

	auto backwards = good;
	PutU32(backwards, BikFile::k_HeaderSize + 4, static_cast<uint32_t>(BikFile::k_HeaderSize + 12));
	EXPECT_NE(ParseError(backwards).find("is not before"), std::string::npos);

	auto shortTable = good;
	PutU32(shortTable, BikFile::k_HeaderSize + 8, static_cast<uint32_t>(good.size() - 2));
	EXPECT_NE(ParseError(shortTable).find("ends at"), std::string::npos);

	// An audio size reaching past its packet
	std::vector<uint8_t> packet = {200, 0, 0, 0, 1, 2};
	EXPECT_NE(ParseError(MakeBik({packet}, {.audioTracks = 1})).find("past the packet"), std::string::npos);
	std::vector<uint8_t> tooSmall = {1, 2};
	EXPECT_NE(ParseError(MakeBik({tooSmall}, {.audioTracks = 1})).find("no room"), std::string::npos);
}

TEST(VideoBinkFile, EveryTruncationIsRejected)
{
	const auto good = MakeBik(Packets({6, 4, 8}), {.audioTracks = 1});
	for (size_t size = 0; size < good.size(); ++size)
	{
		BikFile file;
		EXPECT_FALSE(file.Parse(std::vector<uint8_t>(good.begin(), good.begin() + static_cast<ptrdiff_t>(size)))) << size;
	}
}

TEST(VideoBinkFile, OpenReadsWhatParseReads)
{
	// Our container opens its files through the file system, which the test gives it
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const auto data = MakeBik(Packets({4, 6}));
	const auto path = openblack::test::UniqueTempPath("openblack_test_bink_file.bik");
	std::filesystem::create_directories(path.parent_path());
	{
		std::ofstream stream(path, std::ios::binary);
		stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
	}
	BikFile opened;
	const bool openedOk = opened.Open(path);
	ASSERT_TRUE(openedOk) << opened.GetError();
	EXPECT_EQ(opened.FrameCount(), 2u);
	EXPECT_EQ(opened.VideoData(1).size(), 6u);
	// Our opened file reads each packet from its stream when asked, so it is closed before the file goes
	opened.Close();
	std::filesystem::remove(path);

	BikFile missing;
	EXPECT_FALSE(missing.Open(path));
	EXPECT_NE(missing.GetError().find("cannot open"), std::string::npos);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
// No synthetic twin: the measured layout of the game's five films; the same reads are pinned on built files by
// ReadsTheLayout
TEST(VideoBinkFile, TheGamesVideos)
{
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const auto game = GamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	struct Expected
	{
		std::string_view path;
		uint32_t frameCount;
		uint32_t width;
		uint32_t height;
		uint32_t fps;
		uint32_t keyFrames;
	};
	constexpr std::array<Expected, 5> k_Expected = {{
	    {"Data/logo.bik", 2, 768, 512, 15, 2},
	    {"Data/tips.bik", 35, 768, 512, 15, 35},
	    {"Data/INTRO.bik", 1601, 640, 360, 24, 1},
	    {"Data/Spells/fall/fall.bik", 1200, 640, 360, 24, 1},
	    {"Data/pre_intro.bik", 2515, 768, 512, 25, 1},
	}};
	for (const auto& video : k_Expected)
	{
		const auto path = *game / video.path;
		if (!std::filesystem::exists(path))
		{
			GTEST_SKIP() << path << " not found";
		}
		BikFile file;
		ASSERT_TRUE(file.Open(path)) << path << ": " << file.GetError();
		EXPECT_EQ(file.Revision(), 'i');
		EXPECT_EQ(file.FrameCount(), video.frameCount);
		EXPECT_EQ(file.Width(), video.width);
		EXPECT_EQ(file.Height(), video.height);
		EXPECT_EQ(file.Fps(), video.fps);
		EXPECT_EQ(file.KeyFrameCount(), video.keyFrames);
		EXPECT_TRUE(file.AudioTracks().empty());
		EXPECT_EQ(file.VideoFlags() & BikFile::k_VideoFlagAlpha, 0u);
	}
}
