/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' "get target from A to B distance D angle T": the point past B along the direction from A to B, turned
// about the vertical axis, pinned to the original's pushed bits for the same arguments (the exe oracle, the native run
// with its pops fed and its push captured).
// docs/wiki/scripts/script-commands.md#get_target_relative_pos-136

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <string_view>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptTargetRelative.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace target_relative = script::target_relative;

namespace
{
struct Case
{
	std::array<uint32_t, 3> from;
	std::array<uint32_t, 3> to;
	uint32_t distance;
	uint32_t angle;
	std::array<uint32_t, 3> answer;
};

// The original's pushed point for each set of arguments, every value as its float bits
constexpr std::array<Case, 16> k_Cases {{
    // the quest scripts' own shapes: 10 or 50 past the second point, straight on
    {{0x447A2000, 0x41440000, 0x44FA1800},
     {0x447C8000, 0x41580000, 0x44FAA000},
     0x41200000,
     0x00000000,
     {0x447EC409, 0x416B1481, 0x44FB21BF}},
    {{0x451C4000, 0x41F00000, 0x44E10000},
     {0x451B0000, 0x41F80000, 0x44E07000},
     0x42480000,
     0x00000000,
     {0x4517F470, 0x4205BE98, 0x44DF1133}},
    // turned: a positive angle turns +z towards -x
    {{0x44BB8000, 0x00000000, 0x44BB8000},
     {0x44C80000, 0x00000000, 0x44BB8000},
     0x42F00000,
     0x42140000,
     {0x44D3FAC3, 0x00000000, 0x44C486F8}},
    {{0x44BB8000, 0x00000000, 0x44BB8000},
     {0x44BB8000, 0x00000000, 0x44C80000},
     0x42F00000,
     0x42B40000,
     {0x44AC8000, 0x00000000, 0x44C80000}},
    {{0x44BB8000, 0x00000000, 0x44BB8000},
     {0x44BB8000, 0x00000000, 0x44C80000},
     0x42F00000,
     0xC2B40000,
     {0x44CA8000, 0x00000000, 0x44C80000}},
    {{0x453B8000, 0x40A00000, 0x453B8000},
     {0x453CC000, 0x40C00000, 0x453A4000},
     0x3F000000,
     0x00000000,
     {0x453CC5A7, 0x40C090BA, 0x453A3A59}},
    {{0x453B8000, 0x40A00000, 0x453B8000},
     {0x453CC000, 0x40C00000, 0x453A4000},
     0x41C80000,
     0x43340000,
     {0x453BA555, 0x40DC4441, 0x453B5AAB}},
    {{0x453B8000, 0x40A00000, 0x453B8000},
     {0x453CC000, 0x40C00000, 0x453A4000},
     0x41C80000,
     0x43B38000,
     {0x453DD5B1, 0x40DC4441, 0x45392072}},
    {{0x453B8000, 0x40A00000, 0x453B8000},
     {0x453CC000, 0x40C00000, 0x453A4000},
     0x41C80000,
     0x44346000,
     {0x453DE1F8, 0x40DC4441, 0x45392CD4}},
    {{0x453B8000, 0x40A00000, 0x453B8000},
     {0x453CC000, 0x40C00000, 0x453A4000},
     0xC1700000,
     0x42340000,
     {0x453BD026, 0x40AF0A40, 0x453A4000}},
    {{0x3F800000, 0x40000000, 0x40400000},
     {0x40800000, 0xC0C00000, 0x41100000},
     0x40E00000,
     0x41200000,
     {0x40A908B5, 0xC135D23C, 0x4154FA00}},
    // two points a thousandth apart, the direction made one long from a tiny difference
    {{0x42F6E979, 0x429DCCCD, 0x4423948B},
     {0x42F6E9FC, 0x429DCCCD, 0x4423949C},
     0x447A0000,
     0x3F800000,
     {0x4449219C, 0x429DCCCD, 0x44AD5158}},
    {{0x3DCCCCCD, 0x3E4CCCCD, 0x3E99999A},
     {0x3ECCCCCD, 0x3F000000, 0x3F19999A},
     0x40533333,
     0x42F6CCCD,
     {0xC00F526C, 0x4019EFB7, 0x3F92263E}},
    // straight up: the height is not turned
    {{0x44FA0000, 0x42480000, 0x44FA0000},
     {0x44FA0000, 0x42B40000, 0x44FA0000},
     0x41200000,
     0x41F00000,
     {0x44FA0000, 0x42C80000, 0x44FA0000}},
    // the same point twice: no direction, the second point itself
    {{0x447A0000, 0x41200000, 0x447A0000},
     {0x447A0000, 0x41200000, 0x447A0000},
     0x41200000,
     0x42340000,
     {0x447A0000, 0x41200000, 0x447A0000}},
    // a large angle, reduced by the processor's own pi
    {{0x41200000, 0x00000000, 0x41200000},
     {0x41A00000, 0x00000000, 0x41F00000},
     0x42C80000,
     0x47C35000,
     {0x42E7B29E, 0x00000000, 0x3FBE52D0}},
}};

glm::vec3 Point(const std::array<uint32_t, 3>& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}
} // namespace

TEST(ScriptTargetRelative, GivesTheOriginalsPointForTheSameArguments)
{
	for (const auto& c : k_Cases)
	{
		const auto answer = target_relative::Position(Point(c.from), Point(c.to), std::bit_cast<float>(c.distance),
		                                              std::bit_cast<float>(c.angle));
		EXPECT_EQ(std::bit_cast<uint32_t>(answer.x), c.answer[0]) << std::bit_cast<float>(c.angle);
		EXPECT_EQ(std::bit_cast<uint32_t>(answer.y), c.answer[1]) << std::bit_cast<float>(c.angle);
		EXPECT_EQ(std::bit_cast<uint32_t>(answer.z), c.answer[2]) << std::bit_cast<float>(c.angle);
	}
}

TEST(ScriptTargetRelative, ANotANumberPartCountsAsNoDirection)
{
	// The direction is made one long only when one of its parts is an ordered value other than nought. A difference
	// that is not a number, with the other two nought, leaves it as it is, so the height stays the second point's
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	const auto alone = target_relative::Position({nan, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}, 5.0f, 0.0f);
	EXPECT_TRUE(std::isnan(alone.x));
	EXPECT_EQ(alone.y, 2.0f);
	EXPECT_TRUE(std::isnan(alone.z));
	// With another part that is something, the not-a-number spreads to the whole direction
	const auto spread = target_relative::Position({nan, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}, 5.0f, 0.0f);
	EXPECT_TRUE(std::isnan(spread.x));
	EXPECT_TRUE(std::isnan(spread.y));
	EXPECT_TRUE(std::isnan(spread.z));
}

TEST(ScriptTargetRelative, TheNativeTakesTwoPointsADistanceAndAnAngleAndGivesAPoint)
{
	// The script pushes the first point, the second, the distance and then the angle; the native gives the point as
	// three coordinates, x first
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	test::RestoreService<Locator::vm> restoreVm;
	Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	int found = 0;
	for (const auto& native : api.GetFunctionsTable())
	{
		if (std::string_view(native.name) != "GET_TARGET_RELATIVE_POS")
		{
			continue;
		}
		++found;
		EXPECT_EQ(native.stackIn, 8);
		EXPECT_EQ(native.stackOut, 3);
		const auto& c = k_Cases[2];
		auto& vm = Locator::vm::value();
		// a value left below shows that the native took its eight and left three
		vm.Pushf(-5.0f);
		for (const auto bits : c.from)
		{
			vm.Pushv(std::bit_cast<float>(bits));
		}
		for (const auto bits : c.to)
		{
			vm.Pushv(std::bit_cast<float>(bits));
		}
		vm.Pushf(std::bit_cast<float>(c.distance));
		vm.Pushf(std::bit_cast<float>(c.angle));
		native.impl();
		const auto z = vm.Popf();
		const auto y = vm.Popf();
		const auto x = vm.Popf();
		EXPECT_EQ(std::bit_cast<uint32_t>(x), c.answer[0]);
		EXPECT_EQ(std::bit_cast<uint32_t>(y), c.answer[1]);
		EXPECT_EQ(std::bit_cast<uint32_t>(z), c.answer[2]);
		EXPECT_EQ(vm.Popf(), -5.0f);
	}
	EXPECT_EQ(found, 1);
}
