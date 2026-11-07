/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bgfx/defines.h>
#include <gtest/gtest.h>

#include "Graphics/ResetFlags.h"

using openblack::graphics::ResetFlagsWithMultisampling;
using openblack::graphics::ResetFlagsWithVsync;

TEST(ResetFlags, StartupFlagsAreTheVsyncBitAlone)
{
	EXPECT_EQ(ResetFlagsWithVsync(BGFX_RESET_NONE, true), static_cast<uint32_t>(BGFX_RESET_VSYNC));
	EXPECT_EQ(ResetFlagsWithVsync(BGFX_RESET_NONE, false), static_cast<uint32_t>(BGFX_RESET_NONE));
}

TEST(ResetFlags, SwitchingKeepsTheOtherFlags)
{
	constexpr uint32_t k_Others = BGFX_RESET_MSAA_X4 | BGFX_RESET_HIDPI;
	EXPECT_EQ(ResetFlagsWithVsync(k_Others, true), k_Others | BGFX_RESET_VSYNC);
	EXPECT_EQ(ResetFlagsWithVsync(k_Others | BGFX_RESET_VSYNC, false), k_Others);
	// switching to what it already is changes nothing
	EXPECT_EQ(ResetFlagsWithVsync(k_Others | BGFX_RESET_VSYNC, true), k_Others | BGFX_RESET_VSYNC);
	EXPECT_EQ(ResetFlagsWithVsync(k_Others, false), k_Others);
}

TEST(ResetFlags, MultisamplingIsFourSamplesAndKeepsTheOtherFlags)
{
	// the start-up flags have none, as the original draws
	EXPECT_EQ(ResetFlagsWithMultisampling(BGFX_RESET_VSYNC, false), static_cast<uint32_t>(BGFX_RESET_VSYNC));
	EXPECT_EQ(ResetFlagsWithMultisampling(BGFX_RESET_VSYNC, true), BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4);
	// switched off, any sample count goes
	EXPECT_EQ(ResetFlagsWithMultisampling(BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X16, false),
	          static_cast<uint32_t>(BGFX_RESET_VSYNC));
	// switched on over another count, it is four
	EXPECT_EQ(ResetFlagsWithMultisampling(BGFX_RESET_MSAA_X8, true), static_cast<uint32_t>(BGFX_RESET_MSAA_X4));
}
