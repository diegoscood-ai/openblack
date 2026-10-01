/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/MusicBank.h"
#include "Audio/MusicEngine.h"
#include "Audio/MusicStream.h"

// Milestone A4 of dev\tmp_dis\audio\PLAN.md: the continuous MP2 decoding of the music segments (the bank's decoder,
// 0x1000F740) and MusicStream under the engine. Expected values from the data (music.md §3.1-3.2, music_sad_table.md):
// align/good.sad has 430 segments of 21 frames = 24192 samples, the last one shorter, 471 s at 22050 Hz, stereo.
// Needs OPENBLACK_TEST_BW_ROOT; without it the tests skip. No OpenAL context here: MusicStream only decodes and queues.

using namespace openblack::audio;

namespace
{
std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

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

std::unique_ptr<MusicBank> Open(std::string_view relative)
{
	const auto root = GameRoot();
	if (!root)
	{
		return nullptr;
	}
	const auto path = FindNoCase(*root, relative);
	return path ? MusicBank::Register(*path) : nullptr;
}
} // namespace

TEST(MusicStream, DecodesGoodContinuously)
{
	auto bank = Open("audio/music/align/good.sad");
	if (!bank)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set or no align/good.sad";
	}
	ASSERT_EQ(bank->GetSegmentCount(), 430u);
	MusicSegmentDecoder decoder;
	std::vector<uint8_t> data;
	std::vector<int16_t> pcm;
	uint64_t total = 0;
	for (size_t i = 0; i < bank->GetSegmentCount(); ++i)
	{
		ASSERT_TRUE(bank->ReadSegment(i, data));
		pcm.clear();
		const auto frames = decoder.Decode(data, pcm);
		if (i + 1 < bank->GetSegmentCount())
		{
			ASSERT_EQ(frames, static_cast<size_t>(k_MusicSamplesPerSegment)) << "segment " << i;
		}
		else
		{
			EXPECT_GT(frames, 0u);
			EXPECT_LT(frames, static_cast<size_t>(k_MusicSamplesPerSegment));
			EXPECT_EQ(frames % 1152, 0u); // whole Layer II frames
		}
		ASSERT_EQ(pcm.size(), frames * 2) << "segment " << i;
		total += frames;
	}
	EXPECT_EQ(decoder.GetChannels(), 2);
	EXPECT_EQ(decoder.GetSampleRate(), 22050);
	EXPECT_GE(total, 429u * static_cast<uint64_t>(k_MusicSamplesPerSegment));
	// 429 x 24192 + the last segment (15865 bytes, music.md §6) = 19 frames of 1152: 10400256 samples, 471.67 s (the
	// 471.1 s of music_sad_table.md is an estimate from the bytes)
	EXPECT_EQ(total, 429u * static_cast<uint64_t>(k_MusicSamplesPerSegment) + 19u * 1152u);
	std::cout << "align/good.sad: " << total << " samples, " << static_cast<double>(total) / 22050.0 << " s\n";
}

TEST(MusicStream, DecodesMono64k)
{
	auto bank = Open("audio/music/script/gregorian3d.sad");
	if (!bank)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set or no script/Gregorian3D.sad";
	}
	MusicSegmentDecoder decoder;
	std::vector<uint8_t> data;
	std::vector<int16_t> pcm;
	ASSERT_TRUE(bank->ReadSegment(0, data));
	EXPECT_EQ(decoder.Decode(data, pcm), static_cast<size_t>(k_MusicSamplesPerSegment));
	EXPECT_EQ(decoder.GetChannels(), 1);
	EXPECT_EQ(pcm.size(), static_cast<size_t>(k_MusicSamplesPerSegment));
}

TEST(MusicStream, EngineQueuesDecodedChunks)
{
	auto good = Open("audio/music/align/good.sad");
	auto evil = Open("audio/music/align/evil.sad");
	if (!good || !evil)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set or no align banks";
	}
	MusicStream stream;
	ASSERT_FALSE(stream.IsAvailable()); // no OpenAL context in the tests
	MusicEngine engine(stream);
	engine.NoteBankRegistered(*good);
	engine.NoteBankRegistered(*evil);
	EXPECT_EQ(engine.GetTotalGroups(), 1u);

	MusicPlayOptions options;
	options.bank = good.get();
	options.volume = 80;
	options.fade = 1;
	const int a = engine.Play(options);
	ASSERT_EQ(a, 0);
	uint32_t now = 1000;
	ASSERT_TRUE(engine.Process(now));
	EXPECT_EQ(engine.GetQueued(a), k_MusicQueueDepth);
	EXPECT_EQ(stream.GetDecodedFrames(a), 4u * k_MusicSamplesPerSegment);

	// the change good -> evil (same group 1, sync): the same chunk, decoded by a fresh decoder
	options.bank = evil.get();
	options.sync = 1;
	const int b = engine.Play(options);
	ASSERT_EQ(b, 1);
	now += k_MusicPassSleepMs;
	ASSERT_TRUE(engine.Process(now));
	EXPECT_EQ(engine.GetCurrentChunk(b), engine.GetCurrentChunk(a));
	EXPECT_EQ(stream.GetDecodedFrames(b), 4u * k_MusicSamplesPerSegment);
	EXPECT_EQ(engine.GetMasterInfo(), b);

	engine.Stop(0);
	EXPECT_EQ(engine.GetQueued(a), 0);
	EXPECT_EQ(engine.GetQueued(b), 0);
	EXPECT_TRUE(stream.IsChannelDone(a));
}
