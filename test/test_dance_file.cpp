/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <DanceFile.h>
#include <gtest/gtest.h>

#include "InfoConstants.h"

using namespace openblack;

namespace
{
void Put(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutAction(std::vector<uint8_t>& bytes, std::vector<uint32_t> groups, uint32_t type, uint32_t first, uint32_t second)
{
	Put(bytes, static_cast<uint32_t>(groups.size()));
	for (const auto group : groups)
	{
		Put(bytes, group);
	}
	Put(bytes, type);
	Put(bytes, first);
	Put(bytes, second);
	for (int i = 2; i < 14; ++i)
	{
		Put(bytes, 0);
	}
}

/// A dance of two groups named M and W, sharing their dancers half and half from its first key frame
std::vector<uint8_t> TwoGroupDance()
{
	std::vector<uint8_t> bytes;
	Put(bytes, 1); // version
	Put(bytes, 2); // key frames
	Put(bytes, std::bit_cast<uint32_t>(0.0f));
	Put(bytes, 1);
	Put(bytes, 2); // actions
	PutAction(bytes, {0}, 6, 0, 50);
	PutAction(bytes, {1}, 6, 0, 50);
	Put(bytes, std::bit_cast<uint32_t>(20.0f));
	Put(bytes, 0);
	Put(bytes, 1);
	PutAction(bytes, {1}, 16, 2, 0);
	Put(bytes, std::bit_cast<uint32_t>(0.0f)); // beat
	Put(bytes, 2);                             // groups
	Put(bytes, 14);
	Put(bytes, 0);
	Put(bytes, 1); // loops
	for (const char name : {'M', 'W'})
	{
		Put(bytes, 1);
		bytes.push_back(static_cast<uint8_t>(name));
	}
	return bytes;
}

/// A version 2 dance of one group whose three key frames, at 20, 0 and 20 beats, each have one action of its own type
std::vector<uint8_t> UnsortedDance()
{
	std::vector<uint8_t> bytes;
	Put(bytes, 2); // version
	Put(bytes, 3); // key frames
	for (const auto& [time, type] : {std::pair {20.0f, 1u}, std::pair {0.0f, 2u}, std::pair {20.0f, 3u}})
	{
		Put(bytes, std::bit_cast<uint32_t>(time));
		Put(bytes, 0);
		Put(bytes, 1);
		PutAction(bytes, {0, 2}, type, 0, 0);
	}
	Put(bytes, std::bit_cast<uint32_t>(10.0f)); // beat
	Put(bytes, 1);                              // groups
	Put(bytes, 30);
	Put(bytes, 1);
	Put(bytes, 2);                             // loops
	Put(bytes, std::bit_cast<uint32_t>(0.5f)); // angle
	Put(bytes, 4);                             // the name's length, with a null character inside it
	for (const char c : {'A', 'B', '\0', 'C'})
	{
		bytes.push_back(static_cast<uint8_t>(c));
	}
	return bytes;
}

} // namespace

TEST(DanceFile, ReadsTheKeyFramesSettingsAndGroupNames)
{
	dance::DanceFile file;
	ASSERT_EQ(file.Open(TwoGroupDance()), dance::DanceResult::Success);
	EXPECT_EQ(file.version, 1u);
	ASSERT_EQ(file.keyFrames.size(), 2u);
	EXPECT_FLOAT_EQ(file.keyFrames[1].time, 20.0f);
	EXPECT_EQ(file.keyFrames[0].actions[1].groups, std::vector<uint32_t> {1});
	EXPECT_EQ(file.keyFrames[0].actions[1].arguments[1], 50u);
	EXPECT_EQ(file.groupCount, 2u);
	EXPECT_EQ(file.loops, 1u);
	EXPECT_FALSE(file.angle.has_value());
	EXPECT_EQ(file.groupNames, (std::vector<std::string> {"M", "W"}));
}

TEST(DanceFile, AFileIsTakenAsItReadsWhateverItHolds)
{
	// Cut short, it keeps what it read before its end: both key frames, the settings and the first name
	auto bytes = TwoGroupDance();
	bytes.pop_back();
	dance::DanceFile cut;
	EXPECT_EQ(cut.Open(bytes), dance::DanceResult::ErrTruncated);
	EXPECT_EQ(cut.keyFrames.size(), 2u);
	EXPECT_EQ(cut.groupCount, 2u);
	EXPECT_EQ(cut.groupNames, std::vector<std::string> {"M"});
	// With more after its end, it is read whole and the rest left
	bytes = TwoGroupDance();
	bytes.push_back(0);
	dance::DanceFile longer;
	EXPECT_EQ(longer.Open(bytes), dance::DanceResult::ErrTrailingBytes);
	EXPECT_EQ(longer.keyFrames.size(), 2u);
	EXPECT_EQ(longer.groupNames, (std::vector<std::string> {"M", "W"}));
	// Read again, it has only the new file's key frames
	ASSERT_EQ(longer.Open(TwoGroupDance()), dance::DanceResult::Success);
	EXPECT_EQ(longer.keyFrames.size(), 2u);
}

TEST(DanceFile, KeyFramesGoInTheOrderOfTheirTimesTheLaterOfTwoAtOneTimeFirst)
{
	dance::DanceFile file;
	ASSERT_EQ(file.Open(UnsortedDance()), dance::DanceResult::Success);
	ASSERT_EQ(file.keyFrames.size(), 3u);
	EXPECT_FLOAT_EQ(file.keyFrames[0].time, 0.0f);
	EXPECT_EQ(file.keyFrames[0].actions[0].type, 2u);
	// Each goes before the first whose time is not less than its own
	EXPECT_EQ(file.keyFrames[1].actions[0].type, 3u);
	EXPECT_EQ(file.keyFrames[2].actions[0].type, 1u);
	// An action's groups stay in the file's order
	EXPECT_EQ(file.keyFrames[0].actions[0].groups, (std::vector<uint32_t> {0, 2}));
	EXPECT_FLOAT_EQ(file.beat, 10.0f);
	EXPECT_EQ(file.unknown44, 30u);
	EXPECT_EQ(file.unknown48, 1u);
	EXPECT_EQ(file.loops, 2u);
	ASSERT_TRUE(file.angle.has_value());
	EXPECT_FLOAT_EQ(*file.angle, 0.5f);
	// A name ends at its first null character
	EXPECT_EQ(file.groupNames, std::vector<std::string> {"AB"});
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it.
// Every file the dances' table names reads whole, its key frames' times in order, no two the same and no two in the
// same half second, so the file order and the game's order by time are the same for all of them. The one row whose
// file the game doesn't have (DancePrayInwards) makes a dance without a file.
TEST(DanceFile, TheGamesDancesRead)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream info(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(info.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(info)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto constants = std::make_unique<InfoConstants>();
	std::memcpy(constants.get(), data.data() + 0x2C, sizeof(InfoConstants));

	std::set<std::string> read;
	for (std::size_t row = 0; row < constants->dance.size(); ++row)
	{
		const auto& name = constants->dance.at(row).fileName;
		const std::string_view field(name.data(), name.size());
		// The names are under the game's folder, their parts apart by one or two backslashes
		auto path = std::filesystem::path(game);
		std::string part;
		for (const char c : field.substr(0, field.find('\0')))
		{
			if (c != '\\')
			{
				part.push_back(c);
				continue;
			}
			if (!part.empty())
			{
				path /= part == "scripts" ? std::string("Scripts") : part;
				part.clear();
			}
		}
		path /= part;
		if (row == static_cast<std::size_t>(DanceInfo::DancePrayInwards))
		{
			EXPECT_FALSE(std::filesystem::exists(path)) << path;
			continue;
		}
		std::ifstream file(path, std::ios::binary);
		ASSERT_TRUE(file.is_open()) << path;
		const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		dance::DanceFile dance;
		ASSERT_EQ(dance.Open(bytes), dance::DanceResult::Success) << path;
		for (std::size_t k = 1; k < dance.keyFrames.size(); ++k)
		{
			const auto before = dance.keyFrames[k - 1].time;
			const auto after = dance.keyFrames[k].time;
			EXPECT_LT(before, after) << path << " key frame " << k;
			EXPECT_NE(static_cast<int32_t>(before) / 5, static_cast<int32_t>(after) / 5) << path << " key frame " << k;
		}
		read.insert(path.filename().string());
	}
	// 20 files of their own: the citadels' dances share the worship ones
	EXPECT_EQ(read.size(), 20u);
}
