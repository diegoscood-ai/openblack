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

#include <string_view>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "Audio/ClipSounds.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
namespace clip_sounds = openblack::audio::clip_sounds;

namespace
{
std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}
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
