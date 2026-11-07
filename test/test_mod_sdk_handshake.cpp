/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ModSdkHandshake.h"

using namespace openblack::mod_sdk;

namespace
{
constexpr std::string_view k_Game = "aaaa";
} // namespace

TEST(ModSdkHandshake, TheSameSdkVersionActivatesTheMods)
{
	EXPECT_EQ(Check(true, true, "aaaa", k_Game), Handshake::Active);
	EXPECT_TRUE(WhyUnmodded(Handshake::Active).empty());
}

TEST(ModSdkHandshake, AnotherSdkVersionStartsUnmodded)
{
	EXPECT_EQ(Check(true, true, "bbbb", k_Game), Handshake::VersionMismatch);
	EXPECT_EQ(Check(true, true, "", k_Game), Handshake::VersionMismatch);
	EXPECT_FALSE(WhyUnmodded(Handshake::VersionMismatch).empty());
}

TEST(ModSdkHandshake, AMissingLibraryOrFunctionStartsUnmodded)
{
	EXPECT_EQ(Check(false, false, "", k_Game), Handshake::NoLibrary);
	EXPECT_EQ(Check(false, true, "aaaa", k_Game), Handshake::NoLibrary);
	EXPECT_EQ(Check(true, false, "aaaa", k_Game), Handshake::MissingFunctions);
	EXPECT_EQ(WhyUnmodded(Handshake::NoLibrary), "ModLoader: no library, no mods");
	EXPECT_FALSE(WhyUnmodded(Handshake::MissingFunctions).empty());
}
