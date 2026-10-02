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
#include <optional>
#include <set>
#include <string>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/GAudio/BankTables.h"

// Milestone A1 of dev\tmp_dis\audio\PLAN.md: the tables 0x9CB3F8 and 0x9C9748 of runblack.exe W120 (dumped with
// dev\tmp_dis\audio\audit_banktable.py; music_types.md) and the music flag of LHFileSegmentBankInfo read by PackFile.
// The installation tests need OPENBLACK_TEST_BW_ROOT (the folder with runblack.exe and Audio\); without it they skip.

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

// The original runs on a case-insensitive file system: resolve each component without case
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
} // namespace

TEST(AudioTables, SfxBankPaths)
{
	// 0x9CB3F8: 11 pointers, the first null
	ASSERT_EQ(k_SfxBankPaths.size(), 11u);
	EXPECT_TRUE(k_SfxBankPaths[0].empty());
	EXPECT_EQ(SfxBankPath(SfxBank::InGame), "audio/sfx/game/ingame.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Editor), "audio/sfx/game/editor.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Spells), "audio/sfx/game/spells.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Creature), "audio/sfx/creature/creature.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::ScriptSfx), "audio/sfx/script/scriptsfx.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::HelpSprites), "audio/dialogue/HelpSprites.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Villagers), "audio/dialogue/Villagers.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::VillagersBanter), "audio/dialogue/VillagersBanter.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::SpellDialogue), "audio/dialogue/SpellDialogue.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Guidance), "audio/dialogue/Guidance.sad");
}

TEST(AudioTables, MusicBanks)
{
	// 0x9C9748 .. 0x9C99F0 with stride 8 = 85 entries (loop 0x426E8E..0x426F1F)
	ASSERT_EQ(k_MusicBanks.size(), 85u);
	EXPECT_TRUE(MusicBankFor(MusicType::None).path.empty());
	EXPECT_EQ(MusicBankFor(MusicType::None).name, "MUSIC_TYPE_NONE");
	for (size_t i = 1; i < k_MusicBanks.size(); ++i)
	{
		EXPECT_FALSE(k_MusicBanks[i].path.empty()) << i;
		EXPECT_TRUE(k_MusicBanks[i].name.starts_with("MUSIC_TYPE_")) << i;
	}
	EXPECT_EQ(MusicBankFor(MusicType::ScriptCreatureEndSequence).name, "MUSIC_TYPE_SCRIPT_CREATURE_END_SEQUENCE");
	EXPECT_EQ(MusicBankFor(MusicType::GenericGood).path, "audio/music/align/good.sad");
	EXPECT_EQ(MusicBankFor(MusicType::ScriptIntro).path, "audio/music/intro/intro.sad");
	EXPECT_EQ(MusicBankFor(MusicType::Outro).path, "audio/music/outro/outro.sad");

	// Norse town music is the Celtic one (same string pointers)
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownEvil).path, MusicBankFor(MusicType::CelticTownEvil).path);
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownNeutral).path, MusicBankFor(MusicType::CelticTownNeutral).path);
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownGood).path, MusicBankFor(MusicType::CelticTownGood).path);
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownGood).name, "MUSIC_TYPE_NORSE_TOWN_GOOD");
	// one citadel.sad for the 3 alignments
	EXPECT_EQ(MusicBankFor(MusicType::CitadelEvil).path, "audio/music/citadel/citadel.sad");
	EXPECT_EQ(MusicBankFor(MusicType::CitadelNeutral).path, MusicBankFor(MusicType::CitadelEvil).path);
	EXPECT_EQ(MusicBankFor(MusicType::CitadelGood).path, MusicBankFor(MusicType::CitadelEvil).path);
	// the missionaries' verses live in Dialogue
	EXPECT_EQ(MusicBankFor(MusicType::ScriptMissionariesVerse1).path, "audio/dialogue/MissionariesVerse1.sad");
	EXPECT_EQ(MusicBankFor(MusicType::ScriptMissionariesVerse3).path, "audio/dialogue/MissionariesVerse3.sad");
	EXPECT_EQ(MusicBankFor(MusicType::ScriptWelcomeDance).path, "audio/music/script/FollowUsWelcome.sad");

	// 79 different paths: 84 non-null minus the 3 Norse and the 2 extra citadel entries
	std::set<std::string> distinct;
	for (const auto& entry : k_MusicBanks)
	{
		if (!entry.path.empty())
		{
			distinct.insert(Lower(std::string(entry.path)));
		}
	}
	EXPECT_EQ(distinct.size(), 84u - 3u - 2u);
}

TEST(AudioTables, PathsExistInInstallation)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	for (size_t i = 1; i < k_SfxBankPaths.size(); ++i)
	{
		EXPECT_TRUE(FindNoCase(*root, k_SfxBankPaths[i]).has_value()) << k_SfxBankPaths[i];
	}
	for (size_t i = 1; i < k_MusicBanks.size(); ++i)
	{
		const bool exists = FindNoCase(*root, k_MusicBanks[i].path).has_value();
		// WELCOME_DANCE is the only one without a file
		EXPECT_EQ(exists, static_cast<MusicType>(i) != MusicType::ScriptWelcomeDance) << k_MusicBanks[i].path;
	}
	// MissionariesSad.sad is on disk but not in the table
	EXPECT_TRUE(FindNoCase(*root, "audio/music/script/MissionariesSad.sad").has_value());
}

TEST(AudioTables, BankInfoMusicFlag)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto audio = FindNoCase(*root, "audio");
	ASSERT_TRUE(audio.has_value());

	int music = 0;
	int other = 0;
	bool trailer = false;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(*audio))
	{
		if (!entry.is_regular_file() || Lower(entry.path().extension().string()) != ".sad")
		{
			continue;
		}
		const auto relative = Lower(std::filesystem::relative(entry.path(), *audio).generic_string());
		const bool expectMusic = relative.starts_with("music/") || relative.starts_with("dialogue/missionariesverse");

		openblack::pack::PackFile pack;
		ASSERT_EQ(pack.Open(entry.path()), openblack::pack::PackResult::Success) << relative;
		EXPECT_EQ(pack.IsAudioMusicBank(), expectMusic) << relative;
		EXPECT_EQ(pack.GetAudioBankInfo().isMusic, expectMusic ? 1u : 0u) << relative;
		(expectMusic ? music : other)++;
		trailer = trailer || relative == "music/intro/trailer.sad";
		if (relative == "sfx/atmos/ocean.sad")
		{
			// the only bank with the first two u32 set (engine.md §1.5)
			EXPECT_EQ(pack.GetAudioBankInfo().unknown0, 7u);
			EXPECT_EQ(pack.GetAudioBankInfo().unknown1, 6u);
		}
	}
	EXPECT_TRUE(trailer);
	// 77 in Music\ + MissionariesVerse1..3; 33 effect, atmos, creature and dialogue banks
	EXPECT_EQ(music, 80);
	EXPECT_EQ(other, 33);
}
