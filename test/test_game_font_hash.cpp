/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/GameFont.h"

#define LOCATOR_IMPLEMENTATIONS
#include "Resources/Resources.h"

using openblack::graphics::GameFont;

namespace
{
// A fake rand() stream: call k (from 0) returns values(k), and the calls are counted
struct FakeRand
{
	int32_t (*values)(int32_t call);
	int32_t calls {0};

	[[nodiscard]] GameFont::RandFn Fn()
	{
		return [this] { return values(calls++); };
	}
};

// The range the original draws over at call k: 256, 255, ..., 2
constexpr int32_t RangeAt(int32_t call)
{
	return 256 - call;
}
} // namespace

TEST(GameFontHashPermutation, AllZeroDrawsRotateTheFirst255)
{
	// Every entry swaps with entry 0: entry 0 ends as 254, entry k as k - 1 for k = 1..254, and 255 is never touched
	FakeRand rand {.values = [](int32_t) { return 0; }};
	const auto table = GameFont::MakeHashPermutation(rand.Fn());

	std::array<uint8_t, 256> expected {};
	expected[0] = 254;
	for (size_t k = 1; k < 255; ++k)
	{
		expected[k] = static_cast<uint8_t>(k - 1);
	}
	expected[255] = 255;
	EXPECT_EQ(table, expected);
}

TEST(GameFontHashPermutation, LastOfEachRangeSwapsOnlyTheEnds)
{
	// rand() % range = 255 - i: entries 0..127 swap with their mirrors, 128..254 swap them back, and the first swap
	// (0 with 255) is never undone. The multiple of the range checks the modulo
	FakeRand rand {.values = [](int32_t call) { return (RangeAt(call) * 7) + (RangeAt(call) - 1); }};
	const auto table = GameFont::MakeHashPermutation(rand.Fn());

	std::array<uint8_t, 256> expected {};
	for (size_t k = 0; k < expected.size(); ++k)
	{
		expected[k] = static_cast<uint8_t>(k);
	}
	expected[0] = 255;
	expected[255] = 0;
	EXPECT_EQ(table, expected);
}

TEST(GameFontHashPermutation, Draws255TimesOverRanges256DownTo2)
{
	// Returning the expected range gives 0 and returning it less one gives the range's top only when the stream is
	// reduced modulo exactly that range, so both runs matching their tables pins every range in order
	FakeRand atRange {.values = [](int32_t call) { return RangeAt(call); }};
	const auto zero = GameFont::MakeHashPermutation(atRange.Fn());
	EXPECT_EQ(atRange.calls, 255);
	FakeRand allZero {.values = [](int32_t) { return 0; }};
	EXPECT_EQ(zero, GameFont::MakeHashPermutation(allZero.Fn()));

	FakeRand belowRange {.values = [](int32_t call) { return RangeAt(call) - 1; }};
	const auto top = GameFont::MakeHashPermutation(belowRange.Fn());
	EXPECT_EQ(belowRange.calls, 255);
	EXPECT_EQ(top[0], 255);
	EXPECT_EQ(top[255], 0);
	for (size_t k = 1; k < 255; ++k)
	{
		EXPECT_EQ(top[k], k) << "entry " << k;
	}
}

TEST(GameFontHashPermutation, IsAPermutationOf0To255)
{
	// A stream like the C runtime's rand() (0..0x7FFF), from a fixed seed
	uint32_t seed = 12345;
	const auto table = GameFont::MakeHashPermutation([&seed] {
		seed = (seed * 0x343FDu) + 0x269EC3u;
		return static_cast<int32_t>((seed >> 16u) & 0x7FFFu);
	});

	std::vector<uint8_t> sorted(table.begin(), table.end());
	std::ranges::sort(sorted);
	for (size_t k = 0; k < sorted.size(); ++k)
	{
		EXPECT_EQ(sorted[k], k) << "value " << k;
	}
	EXPECT_FALSE(std::ranges::is_sorted(table));
}

TEST(GameFontHashPermutation, KeptWithTheFontCache)
{
	// The resources keep the table start-up stores, as it was made
	FakeRand rand {.values = [](int32_t call) { return RangeAt(call) - 1; }};
	const auto table = GameFont::MakeHashPermutation(rand.Fn());
	openblack::resources::Resources resources;
	resources.SetFontHashPermutation(table);
	EXPECT_EQ(resources.GetFontHashPermutation(), table);
}
