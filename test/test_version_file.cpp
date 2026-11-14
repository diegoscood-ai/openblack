/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Game/VersionFile.h"

using namespace openblack::version_file;

TEST(VersionFile, TheSwitchIsNamedAfterADashOrASlashInAnyCase)
{
	EXPECT_TRUE(HasSwitch("runblack.exe -VERSION", k_Switch));
	EXPECT_TRUE(HasSwitch("runblack.exe /version", k_Switch));
	EXPECT_TRUE(HasSwitch("runblack.exe -VersionPlease", k_Switch));
	EXPECT_FALSE(HasSwitch("runblack.exe VERSION", k_Switch));
	EXPECT_FALSE(HasSwitch("runblack.exe -VERSIO", k_Switch));
	EXPECT_FALSE(HasSwitch("runblack.exe -", k_Switch));
	// anywhere on the line, a path's slash too
	EXPECT_TRUE(HasSwitch("C:/Games/Version/runblack.exe", k_Switch));
	EXPECT_TRUE(HasSwitch("runblack.exe --version", k_Switch));
	EXPECT_TRUE(NamesSwitch("vErSiOn", k_Switch));
	EXPECT_FALSE(NamesSwitch("VERSI0N", k_Switch));
}

TEST(VersionFile, TheSwitchIsTakenOutOfTheOptionsLeft)
{
	const std::array<std::string_view, 5> arguments {"-g", "C:/Game", "-VERSION", "/version", "--version"};
	const auto kept = WithoutSwitch(arguments, k_Switch);
	EXPECT_EQ(kept, (std::vector<std::string_view> {"-g", "C:/Game", "--version"}));
	const std::array<std::string_view, 3> line {"openblack", "-g", "x"};
	EXPECT_EQ(CommandLine(line), "openblack -g x");
}

TEST(VersionFile, TheChecksumIsTheSumOfTheProgramsBytes)
{
	EXPECT_EQ(Checksum({}), 0u);
	const std::array<uint8_t, 4> bytes {0x4D, 0x5A, 0x90, 0xFF};
	EXPECT_EQ(Checksum(bytes), 0x4Du + 0x5Au + 0x90u + 0xFFu);
	// kept to 32 bits
	const std::vector<uint8_t> many(0x01010102, 0xFF);
	EXPECT_EQ(Checksum(many), static_cast<uint32_t>(0x01010102ull * 0xFFull));
}

TEST(VersionFile, TheFileHoldsTheNumberAndNothingElse)
{
	EXPECT_EQ(Text(0), "0");
	EXPECT_EQ(Text(123456789), "123456789");
	EXPECT_EQ(Text(k_ChecksumError), "4294967295");
}
