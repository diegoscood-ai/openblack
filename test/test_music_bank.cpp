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
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/GAudio/BankTables.h"
#include "Audio/LH/MusicBank.h"

// Milestone A2 of dev\tmp_dis\audio\PLAN.md. Expected values: dev\tmp_dis\audio\music_sad_table.md (segments, group,
// flags, loops, volume, min/max/scale of the first segment) and music_types.md; markers and segment layout from the
// data (music.md §2.3.1, §3.1). Needs OPENBLACK_TEST_BW_ROOT (the folder with runblack.exe and Audio\); without it the
// tests skip.

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

struct Expected
{
	const char* path;
	size_t segments;
	int group;
	uint32_t flags;
	int loops;  // GetLoops(0): -1 with flag 0x40, else the 0 requested
	int volume; // 127 unless flag 0x20
	float minD; // GetDistanceMapping({-1, -1, -1}): -1 = not overridden
	float maxD;
	float scale;
};

// music_sad_table.md (Audio\Music) + the three verses (Dialogue, music_types.md 51..53)
const std::array<Expected, 80> k_Expected = {{
    {"Audio/Music/Intro/intro.sad", 177, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/Intro/trailer.sad", 97, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Aztc_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Aztc_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Aztc_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Celt_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Celt_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Celt_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Egpt_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Egpt_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Egpt_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Grek_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Grek_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Grek_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Indn_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Indn_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Indn_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Japn_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Japn_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Japn_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Tbtn_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Tbtn_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Tbtn_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/chant/aztc_chant.sad", 34, 6, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/aztc_chant_vox.sad", 34, 6, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/celt_chant.sad", 47, 7, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/celt_chant_vox.sad", 47, 7, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/egpt_chant.sad", 57, 8, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/egpt_chant_vox.sad", 57, 8, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/grek_chant.sad", 44, 9, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/grek_chant_vox.sad", 44, 9, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/indn_chant.sad", 37, 10, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/indn_chant_vox.sad", 37, 10, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/japn_chant.sad", 32, 13, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/japn_chant_vox.sad", 32, 13, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/nrse_chant.sad", 81, 11, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/nrse_chant_vox.sad", 81, 11, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/tbtn_chant.sad", 31, 12, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/tbtn_chant_vox.sad", 31, 12, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/citadel/citadel.sad", 273, 4, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/outro/Outro.sad", 180, 0, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Christmas.sad", 16, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Circus.sad", 27, 0, 0x60, -1, 60, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Circus3D.sad", 27, 0, 0x3E0, -1, 60, 30.0f, 120.0f, 3.0f},
    {"Audio/Music/script/CreatureBigFight.sad", 127, 0, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureChosen.sad", 11, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureEndSequence.sad", 55, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureFight.sad", 83, 5, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureGuide.sad", 142, 0, 0x60, -1, 70, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic01.sad", 29, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic02.sad", 28, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic03.sad", 30, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic04.sad", 28, 0, 0x20, 0, 80, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Failure.sad", 17, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Funeral.sad", 15, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Gregorian.sad", 35, 0, 0x60, -1, 40, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Gregorian3D.sad", 35, 0, 0x3E0, -1, 50, 60.0f, 120.0f, 4.0f},
    {"Audio/Music/script/GuardianStone.sad", 20, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Hermit.sad", 50, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/MissionariesBackground.sad", 37, 0, 0x3C0, -1, 127, 30.0f, 100.0f, 4.0f},
    {"Audio/Music/script/MissionariesSad.sad", 19, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Nemesis.sad", 115, 0, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/PiperTune_M.sad", 107, 0, 0x3C0, -1, 127, 15.0f, 100.0f, 4.0f},
    {"Audio/Music/script/Pipercave_M.sad", 107, 0, 0x3C0, -1, 127, 25.0f, 80.0f, 4.0f},
    {"Audio/Music/script/Script01.sad", 50, 0, 0x20, 0, 60, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Script02.sad", 51, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Script03.sad", 57, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Script04.sad", 51, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/SingingStonesA.sad", 80, 0, 0x3C0, -1, 127, 100.0f, 200.0f, 4.0f},
    {"Audio/Music/script/Sleg.sad", 31, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Twinkle.sad", 14, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/WhistleFuneral.sad", 7, 0, 0x3C0, -1, 127, 30.0f, 80.0f, 2.0f},
    {"Audio/Music/script/WhistleTwinkle.sad", 10, 0, 0x3C0, -1, 127, 30.0f, 80.0f, 2.0f},
    {"Audio/Music/script/khazar.sad", 63, 0, 0x60, -1, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Dialogue/MissionariesVerse1.sad", 36, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Dialogue/MissionariesVerse2.sad", 37, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Dialogue/MissionariesVerse3.sad", 37, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
}};

// MPEG audio frame walk (ISO/IEC 11172-3 / 13818-3 header). Every music segment is MPEG-2 (LSF) Layer II at 22050 Hz:
// frame bytes = 144 * bitrate / 22050 + padding, 1152 samples per frame.
constexpr std::array<int, 16> k_Mpeg2LayerIIKbps = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};

// Number of frames if the segment is exactly a run of MPEG-2 Layer II 22050 Hz frames, else -1
int CountFrames(const std::vector<uint8_t>& data)
{
	size_t pos = 0;
	int frames = 0;
	while (pos < data.size())
	{
		if (pos + 4 > data.size())
		{
			return -1;
		}
		const uint8_t* h = &data[pos];
		const bool sync = h[0] == 0xFF && (h[1] & 0xF0) == 0xF0;
		const bool mpeg2 = ((h[1] >> 3) & 1) == 0;
		const bool layer2 = ((h[1] >> 1) & 3) == 2;
		const bool rate22050 = ((h[2] >> 2) & 3) == 0;
		const int kbps = k_Mpeg2LayerIIKbps[h[2] >> 4];
		if (!sync || !mpeg2 || !layer2 || !rate22050 || kbps == 0)
		{
			return -1;
		}
		pos += static_cast<size_t>(144 * kbps * 1000 / 22050 + ((h[2] >> 1) & 1));
		++frames;
	}
	return pos == data.size() ? frames : -1;
}
} // namespace

TEST(MusicBank, MissingFile)
{
	EXPECT_EQ(MusicBank::Register("this/file/does/not/exist.sad"), nullptr);
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// MUSIC_TYPE_SCRIPT_WELCOME_DANCE: LHBankRegister fails, the bank stays null
	EXPECT_EQ(MusicBank::Register(*root / std::string(MusicBankFor(MusicType::ScriptWelcomeDance).path)), nullptr);
}

TEST(MusicBank, FirstSegmentFields)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	for (const auto& e : k_Expected)
	{
		const auto path = FindNoCase(*root, e.path);
		ASSERT_TRUE(path.has_value()) << e.path;
		const auto bank = MusicBank::Register(*path);
		ASSERT_NE(bank, nullptr) << e.path;
		EXPECT_TRUE(bank->IsMusic()) << e.path;
		EXPECT_EQ(bank->GetSegmentCount(), e.segments) << e.path;
		EXPECT_EQ(bank->GetGroupId(), e.group) << e.path;
		EXPECT_EQ(bank->GetFlags(), e.flags) << e.path;
		EXPECT_EQ(bank->GetSampleRate(), 22050u) << e.path;
		EXPECT_EQ(bank->GetVolume(), e.volume) << e.path;
		EXPECT_EQ(bank->GetLoops(0), e.loops) << e.path;
		if (!bank->HasFlag(MusicBankFlag::Loops))
		{
			EXPECT_EQ(bank->GetLoops(5), 5) << e.path;
		}
		const auto mapping = bank->GetDistanceMapping({-1.0f, -1.0f, -1.0f});
		EXPECT_FLOAT_EQ(mapping.minDistance, e.minD) << e.path;
		EXPECT_FLOAT_EQ(mapping.maxDistance, e.maxD) << e.path;
		EXPECT_FLOAT_EQ(mapping.scale, e.scale) << e.path;
		if (bank->HasFlag(MusicBankFlag::MaxDistance))
		{
			EXPECT_FLOAT_EQ(bank->GetMaxDistance(), e.maxD) << e.path;
			EXPECT_FLOAT_EQ(bank->GetMinDistance(), e.minD) << e.path;
		}
	}
}

TEST(MusicBank, GroupsOfTheTypeTable)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// music_types.md: the alignment music is group 1, citadel 4, CreatureFight 5, the chants 6..13 in pairs, rest 0
	int maxGroup = -1;
	for (size_t i = 1; i < k_MusicBanks.size(); ++i)
	{
		const auto type = static_cast<MusicType>(i);
		const auto path = FindNoCase(*root, k_MusicBanks[i].path);
		if (type == MusicType::ScriptWelcomeDance)
		{
			EXPECT_FALSE(path.has_value());
			continue;
		}
		ASSERT_TRUE(path.has_value()) << k_MusicBanks[i].path;
		const auto bank = MusicBank::Register(*path);
		ASSERT_NE(bank, nullptr) << k_MusicBanks[i].path;
		int expected = 0;
		if (i >= 1 && i <= 27)
		{
			expected = 1;
		}
		else if (i >= 44 && i <= 46)
		{
			expected = 4;
		}
		else if (type == MusicType::CreatureFight)
		{
			expected = 5;
		}
		else if (i >= 28 && i <= 43)
		{
			// celt 7, aztc 6, japn 13, indn 10, egpt 8, grek 9, nrse 11, tbtn 12 (normal and _vox share it)
			constexpr std::array<int, 8> k_ChantGroups = {7, 6, 13, 10, 8, 9, 11, 12};
			expected = k_ChantGroups[(i - 28) / 2];
		}
		EXPECT_EQ(bank->GetGroupId(), expected) << k_MusicBanks[i].name;
		maxGroup = std::max(maxGroup, bank->GetGroupId());
	}
	// LHMusicGetTotalGroups (sys+0x40, 0x100027B5) = 13: GAudio+0x18 gets 13 positions
	EXPECT_EQ(maxGroup, 13);
}

TEST(MusicBank, SegmentsAreContiguousFrameAlignedChunks)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	size_t total = 0;
	std::vector<uint8_t> data;
	for (const auto& e : k_Expected)
	{
		const auto bank = MusicBank::Register(*FindNoCase(*root, e.path));
		ASSERT_NE(bank, nullptr) << e.path;
		uint32_t expectedOffset = 0;
		const auto& segments = bank->GetSegments();
		for (size_t i = 0; i < segments.size(); ++i)
		{
			EXPECT_EQ(segments[i].offset, expectedOffset) << e.path << " segment " << i;
			expectedOffset = segments[i].offset + segments[i].size;
			ASSERT_TRUE(bank->ReadSegment(i, data)) << e.path << " segment " << i;
			ASSERT_EQ(data.size(), segments[i].size);
			const int frames = CountFrames(data);
			// 21 frames = 24192 samples (k_MusicSamplesPerSegment); only the last segment may be shorter
			if (i + 1 < segments.size())
			{
				EXPECT_EQ(frames * 1152, k_MusicSamplesPerSegment) << e.path << " segment " << i;
			}
			else
			{
				EXPECT_GE(frames, 1) << e.path;
				EXPECT_LE(frames * 1152, k_MusicSamplesPerSegment) << e.path;
			}
		}
		EXPECT_EQ(expectedOffset, bank->GetWaveDataSize()) << e.path;
		EXPECT_FALSE(bank->ReadSegment(segments.size(), data));
		total += segments.size();
	}
	EXPECT_EQ(total, 13457u);
}

TEST(MusicBank, GoodSadLastSegment)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// music.md §6: align/good.sad = 429 x 24192 samples + a shorter last segment of 15865 bytes
	const auto bank = MusicBank::Register(*FindNoCase(*root, MusicBankFor(MusicType::GenericGood).path));
	ASSERT_NE(bank, nullptr);
	ASSERT_EQ(bank->GetSegmentCount(), 430u);
	EXPECT_EQ(bank->GetSegments().front().size, 17535u);
	EXPECT_EQ(bank->GetSegments().back().size, 15865u);
	std::vector<uint8_t> data;
	ASSERT_TRUE(bank->ReadSegment(429, data));
	EXPECT_EQ(CountFrames(data), 19);
}

TEST(MusicBank, Markers)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto load = [&root](MusicType type) { return MusicBank::Register(*FindNoCase(*root, MusicBankFor(type).path)); };

	// MissionariesVerse1: segment 1 says "!20063=P!39=L1"; the list puts L1 (39) before P (20063)
	{
		const auto bank = load(MusicType::ScriptMissionariesVerse1);
		ASSERT_NE(bank, nullptr);
		const auto markers = bank->ParseMarkers();
		ASSERT_EQ(markers.size(), 55u);
		EXPECT_EQ(markers[0].chunk, 1);
		EXPECT_EQ(markers[0].sample, 39);
		EXPECT_EQ(markers[0].label, "L1");
		EXPECT_EQ(markers[1].chunk, 1);
		EXPECT_EQ(markers[1].sample, 20063);
		EXPECT_EQ(markers[1].label, "P");
		EXPECT_EQ(markers[2].chunk, 2);
		EXPECT_EQ(markers[2].sample, 719);
		EXPECT_EQ(markers.back().chunk, 35);
		EXPECT_EQ(markers.back().sample, 3775);
		EXPECT_EQ(markers.back().label, "P");
		// LAST_MUSIC_LINE 1..12 (TheMissionaries.txt): the lines L1..L12, the last one in chunk 32
		std::vector<int> lines;
		for (const auto& m : markers)
		{
			ASSERT_FALSE(m.label.empty());
			if (m.label[0] == 'L')
			{
				lines.push_back(std::atoi(m.label.c_str() + 1));
				if (lines.back() == 12)
				{
					EXPECT_EQ(m.chunk, 32);
					EXPECT_EQ(m.sample, 9023);
				}
			}
			else
			{
				EXPECT_EQ(m.label, "P");
			}
		}
		const std::vector<int> expectedLines = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
		EXPECT_EQ(lines, expectedLines);
	}
	{
		const auto v2 = load(MusicType::ScriptMissionariesVerse2);
		const auto v3 = load(MusicType::ScriptMissionariesVerse3);
		ASSERT_NE(v2, nullptr);
		ASSERT_NE(v3, nullptr);
		EXPECT_EQ(v2->ParseMarkers().size(), 73u);
		EXPECT_EQ(v3->ParseMarkers().size(), 58u);
	}
	// Other music with a stray marker text: the outro's last segment and two "Record Take 001"
	{
		const auto bank = load(MusicType::Outro);
		ASSERT_NE(bank, nullptr);
		const auto markers = bank->ParseMarkers();
		ASSERT_EQ(markers.size(), 1u);
		EXPECT_EQ(markers[0].chunk, 180);
		EXPECT_EQ(markers[0].sample, 12679);
		EXPECT_EQ(markers[0].label, "Marker 4,343,047");
	}
	{
		const auto bank = load(MusicType::ScriptMissionariesBackground);
		ASSERT_NE(bank, nullptr);
		const auto markers = bank->ParseMarkers();
		ASSERT_EQ(markers.size(), 1u);
		EXPECT_EQ(markers[0].chunk, 1);
		EXPECT_EQ(markers[0].sample, 0);
		EXPECT_EQ(markers[0].label, "Record Take 001");
	}
	{
		const auto bank = load(MusicType::GenericGood);
		ASSERT_NE(bank, nullptr);
		EXPECT_TRUE(bank->ParseMarkers().empty());
	}
}

TEST(MusicBank, EffectBankIsNotMusic)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto bank = MusicBank::Register(*FindNoCase(*root, SfxBankPath(SfxBank::InGame)));
	ASSERT_NE(bank, nullptr);
	EXPECT_FALSE(bank->IsMusic());
	// 0x10002F4A / 0x10002F05 / 0x10002F25
	EXPECT_EQ(bank->GetGroupId(), -1);
	EXPECT_FLOAT_EQ(bank->GetMinDistance(), -1.0f);
	EXPECT_FLOAT_EQ(bank->GetMaxDistance(), -1.0f);
	EXPECT_EQ(bank->GetSegmentCount(), 210u);
}
