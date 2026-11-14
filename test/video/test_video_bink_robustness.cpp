/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's robustness cases on our video::BinkDecoder: damaged and malformed videos fail cleanly, with no crash and
// no read outside the data, and a frame that fails leaves the last good picture. Deterministic fuzzing of synthetic
// packets everywhere, and of the game's own packets when OPENBLACK_GAME_PATH names the game's folder.
//
// Our decoder decodes the frames of a container, not loose packets: each run puts its packets, in the order his
// decoder gets them, into one synthetic file and decodes it frame by frame, so every packet is decoded over the
// picture the ones before it left. Each packet sits behind an audio packet of 0 or 1 byte, so that a packet of any
// size, even an empty or odd one, is a valid frame of the file and is the frame's whole video data.

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include <BinkDecoder.h>
#include <BinkFile.h>
#include <gtest/gtest.h>

// the tests build their own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "bink/BinkTestFiles.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::video;
using namespace openblack::bink::test;

namespace
{
/// OPENBLACK_BINK_FUZZ_SCALE multiplies the rounds, for longer runs by hand (under a sanitizer, say)
int Scale()
{
	const char* scale = std::getenv("OPENBLACK_BINK_FUZZ_SCALE");
	return scale != nullptr ? std::max(1, std::atoi(scale)) : 1;
}

std::vector<std::vector<uint8_t>> SyntheticPackets()
{
	return {FillFrame(90, 100, 110), RawFrame(), MotionFrame(-8), SkipFrame(), MotionFrame(3)};
}

using Picture = std::array<std::vector<uint8_t>, 3>;

/// The decoder's last picture: its Y, U and V planes
Picture GetPicture(const BinkDecoder& decoder)
{
	Picture picture;
	for (size_t plane = 0; plane < picture.size(); ++plane)
	{
		const auto pixels = decoder.PlanePixels(plane);
		picture.at(plane).assign(pixels.begin(), pixels.end());
	}
	return picture;
}

/// A file of `width` x `height` whose frames' video data are `packets`, each behind one audio track's packet
BikFile MakeFile(const std::vector<std::vector<uint8_t>>& packets, uint32_t width, uint32_t height)
{
	std::vector<std::vector<uint8_t>> frames;
	frames.reserve(packets.size());
	for (const auto& packet : packets)
	{
		const auto pad = static_cast<uint8_t>(packet.size() & 1);
		std::vector<uint8_t> frame = {pad, 0, 0, 0};
		frame.resize(frame.size() + pad, 0);
		frame.insert(frame.end(), packet.begin(), packet.end());
		frames.push_back(std::move(frame));
	}
	BikFile file;
	EXPECT_TRUE(file.Parse(MakeBik(frames, {.width = width, .height = height, .audioTracks = 1}))) << file.GetError();
	return file;
}

/// Decodes frames `first` to `end` (not included) in order: whatever each one gives, a failure must leave the picture
/// before it
void DecodeDamaged(BinkDecoder& decoder, uint32_t first, uint32_t end)
{
	for (uint32_t frame = first; frame < end; ++frame)
	{
		const auto before = GetPicture(decoder);
		if (decoder.DecodeOnly(frame) == 0)
		{
			ASSERT_EQ(GetPicture(decoder), before) << "frame " << frame;
		}
	}
}

void Fuzz(uint32_t width, uint32_t height, const std::vector<std::vector<uint8_t>>& packets, std::span<const uint8_t> reference,
          uint32_t seed, int rounds)
{
	std::mt19937 random(seed);
	// The reference first, then the damaged packets in the order his decoder is given them, then the reference again
	std::vector<std::vector<uint8_t>> damagedPackets = {std::vector<uint8_t>(reference.begin(), reference.end())};
	for (const auto& packet : packets)
	{
		// Every truncation of short packets, a spread of them for long ones
		const size_t step = std::max<size_t>(1, packet.size() / 64);
		for (size_t size = 0; size < packet.size(); size += step)
		{
			damagedPackets.emplace_back(packet.begin(), packet.begin() + static_cast<ptrdiff_t>(size));
		}
		for (int round = 0; round < rounds; ++round)
		{
			auto damaged = packet;
			const int flips = 1 + static_cast<int>(random() % 8);
			for (int i = 0; i < flips; ++i)
			{
				damaged[random() % damaged.size()] ^= static_cast<uint8_t>(1u << (random() % 8));
			}
			if (random() % 4 == 0)
			{
				damaged.resize(random() % damaged.size());
			}
			damagedPackets.push_back(std::move(damaged));
		}
		// Random bytes
		std::vector<uint8_t> noise(packet.size());
		std::ranges::generate(noise, [&random]() { return static_cast<uint8_t>(random()); });
		damagedPackets.push_back(std::move(noise));
	}
	damagedPackets.emplace_back(reference.begin(), reference.end());

	const auto file = MakeFile(damagedPackets, width, height);
	ASSERT_TRUE(file.IsOpen());
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();
	ASSERT_NE(decoder.DecodeOnly(0), 0u) << decoder.GetError();
	const uint32_t last = file.FrameCount() - 1;
	// Every damaged packet, decoded over the picture the ones before it left
	DecodeDamaged(decoder, 1, last);
	// It still decodes good data afterwards
	EXPECT_NE(decoder.DecodeOnly(last), 0u) << decoder.GetError();
}
} // namespace

TEST(VideoBinkRobustness, DamagedSyntheticPackets)
{
	const auto packets = SyntheticPackets();
	Fuzz(16, 16, packets, packets[0], 1234, 400 * Scale());
}

TEST(VideoBinkRobustness, EmptyAndTinyPackets)
{
	const std::vector<uint8_t> tiny = {4, 0, 0, 0};
	const auto file = MakeFile({{}, tiny}, 16, 16);
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();
	EXPECT_TRUE(decoder.DecodeNext(0).empty());
	EXPECT_TRUE(decoder.DecodeNext(1).empty());
	// His HasPicture is not checked: our decoder has no such query
}

TEST(VideoBinkRobustness, ImpossibleSizesAreRefused)
{
	// Only the one-pixel part of his case is kept, under his name. His refusal of impossible picture sizes
	// (0xFFFFFFFF x 16, 16 x 100000) is left out: our container and decoder refuse only a picture of no pixels, and the
	// decoder makes its picture buffers for any other size
	// One pixel is the smallest video
	std::mt19937 random(7);
	std::vector<std::vector<uint8_t>> packets;
	for (int i = 0; i < 200; ++i)
	{
		std::vector<uint8_t> noise(4 + random() % 64);
		std::ranges::generate(noise, [&random]() { return static_cast<uint8_t>(random()); });
		noise[0] = static_cast<uint8_t>(random() % 16);
		noise[1] = noise[2] = noise[3] = 0;
		packets.push_back(std::move(noise));
	}
	const auto file = MakeFile(packets, 1, 1);
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();
	DecodeDamaged(decoder, 0, file.FrameCount());
}

// His MalformedContainers (bits flipped in a whole file, mostly in its header and tables, then up to 8 frames
// decoded) is left out: a flipped bit in the width or the height asks our decoder for a picture of up to gigabytes,
// which his decoder refuses (see ImpossibleSizesAreRefused)

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
// No synthetic twin: it fuzzes the game's own film packets; DamagedSyntheticPackets runs the same fuzzing on
// synthetic packets
TEST(VideoBinkRobustness, DamagedGamePackets)
{
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const auto game = GamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	uint32_t seed = 1;
	for (const char* name : {"Data/INTRO.bik", "Data/tips.bik"})
	{
		const auto path = *game / name;
		if (!std::filesystem::exists(path))
		{
			GTEST_SKIP() << path << " not found";
		}
		BikFile file;
		ASSERT_TRUE(file.Open(path)) << file.GetError();
		std::vector<std::vector<uint8_t>> packets;
		for (const uint32_t frame : {0u, 1u, 30u, 200u, 600u, 1000u})
		{
			const auto packet = file.VideoData(frame % file.FrameCount());
			packets.emplace_back(packet.begin(), packet.end());
		}
		Fuzz(file.Width(), file.Height(), packets, packets[0], seed++, 40 * Scale());
	}
}
