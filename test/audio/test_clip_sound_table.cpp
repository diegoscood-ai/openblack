/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The sounds placed on the clips' frames (Audio/ClipSounds) and their cache (ClipSoundsManager), from made-up bytes

#include <cstdint>
#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "Resources/ResourcesInterface.h"
#include "Sound/ClipSounds.h"

using namespace openblack;
namespace clip_sounds = openblack::audio::clip_sounds;

namespace
{
std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}

/// The game's own Data/SmallSounds.SAS read whole, nullopt without the game's folder (OPENBLACK_TEST_BW_ROOT)
std::optional<clip_sounds::ClipSoundTable> GameTable()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0')
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(root) / "Data" / "SmallSounds.SAS", std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	return clip_sounds::Parse(bytes);
}

/// A clip's sounds as (time, sound, action), in the file's order; empty when the table has no such clip
std::vector<std::tuple<int32_t, int32_t, int32_t>> EventsOf(const clip_sounds::ClipSoundTable& table, std::string_view name)
{
	std::vector<std::tuple<int32_t, int32_t, int32_t>> events;
	for (const auto& clip : table.clips)
	{
		if (clip.name == name)
		{
			for (const auto& event : clip.events)
			{
				events.emplace_back(event.time, event.soundId, event.action);
			}
		}
	}
	return events;
}

using Events = std::vector<std::tuple<int32_t, int32_t, int32_t>>;
} // namespace

TEST(ClipSoundTable, EachClipHasItsGroupAndItsSoundsInTheFilesOrder)
{
	const auto table = clip_sounds::Parse(Bytes("1\nWalk 2\n100 7 0 1\n300 8 0 0\nSaw 0\n50 9 1 0\nEND\n"));
	ASSERT_EQ(table.clips.size(), 2u);
	const auto& walk = table.clips.at(0);
	EXPECT_EQ(walk.name, "Walk");
	EXPECT_EQ(walk.group, 2);
	ASSERT_EQ(walk.events.size(), 2u);
	EXPECT_EQ(walk.events.at(0).time, 100);
	EXPECT_EQ(walk.events.at(0).soundId, 7);
	EXPECT_EQ(walk.events.at(1).time, 300);
	const auto& saw = table.clips.at(1);
	EXPECT_EQ(saw.name, "Saw");
	ASSERT_EQ(saw.events.size(), 1u);
	EXPECT_EQ(saw.events.at(0).action, 1);
}

TEST(ClipSoundTable, WithoutGroupsEveryClipIsGroupZero)
{
	const auto table = clip_sounds::Parse(Bytes("0\nRun\n10 3 0 0\nEND\n"));
	ASSERT_EQ(table.clips.size(), 1u);
	EXPECT_EQ(table.clips.at(0).group, 0);
	ASSERT_EQ(table.clips.at(0).events.size(), 1u);
	EXPECT_EQ(table.clips.at(0).events.at(0).soundId, 3);
}

TEST(ClipSoundTable, AShortLineEndsTheReading)
{
	const auto table = clip_sounds::Parse(Bytes("0\nRun\n10 3\nEND\nJump\n1 1 0 0\n"));
	ASSERT_EQ(table.clips.size(), 1u);
	EXPECT_TRUE(table.clips.at(0).events.empty());
	EXPECT_TRUE(clip_sounds::Parse({}).clips.empty());
}

TEST(ClipSoundTable, TheResourcesKeepTheTableTheFileGives)
{
	resources::ClipSoundsManager tables;
	tables.Load(clip_sounds::k_TableId.value(), resources::ClipSoundsLoader::FromBufferTag {},
	            Bytes("0\nRun\n10 3 0 0\nEND\n"));
	ASSERT_TRUE(tables.Contains(clip_sounds::k_TableId.value()));
	const auto& table = *tables.Handle(clip_sounds::k_TableId.value());
	ASSERT_EQ(table.clips.size(), 1u);
	EXPECT_EQ(table.clips.at(0).name, "Run");
}

// ---- the villagers' work clips in the game's own file -------------------------------------------------------------------
// Each clip's sounds as the file places them: (time in ms, sound, 0 play or 1 stop), the sound being the last column of
// the editor bank's key

TEST(ClipSoundTable, ChoppingATreePlaysTwoSoundsTwiceEach)
{
	const auto table = GameTable();
	if (!table)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// one sound at about 1.5 s and 3.6 s, the other at 2.3 s and 4.4 s
	EXPECT_EQ(EventsOf(*table, "M_P_Chopping_Tree"), (Events {{1494, 28, 0}, {3573, 28, 0}, {2274, 29, 0}, {4405, 29, 0}}));
}

TEST(ClipSoundTable, SawingAlternatesTheTwoStrokesEachStoppingTheOther)
{
	const auto table = GameTable();
	if (!table)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// one stroke's sound is stopped and 10 ms later the other's starts, at two points of the clip
	EXPECT_EQ(EventsOf(*table, "M_P_Saw_Wood"), (Events {{750, 30, 1}, {760, 31, 0}, {323, 31, 1}, {333, 30, 0}}));
}

TEST(ClipSoundTable, HammeringAndTheSledgehammerPlayTheSameOneSound)
{
	const auto table = GameTable();
	if (!table)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	EXPECT_EQ(EventsOf(*table, "M_P_Hammering"), (Events {{661, 53, 0}}));
	EXPECT_EQ(EventsOf(*table, "M_P_Sledgehammer"), (Events {{1397, 53, 0}}));
}

TEST(ClipSoundTable, FishingPlaysOneSoundPerClip)
{
	const auto table = GameTable();
	if (!table)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	EXPECT_EQ(EventsOf(*table, "M_P_Fisherman"), (Events {{1645, 66, 0}}));
}
