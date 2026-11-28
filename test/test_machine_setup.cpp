/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "Common/MachineSetup.h"
#include "support/FileSystemFakes.h"

// The settings kept for this computer (Setup.txt): written and read back, the volumes taken as the audio takes them,
// and kept in and read from the profiles' folder of a memory file system; none without a folder.

using namespace openblack;

TEST(MachineSetup, TheVolumesAreReadBackAsWritten)
{
	const auto text = machine_setup::Write(machine_setup::FromVolumes(90, 41));
	EXPECT_EQ(text, "AudioSampleMasterVolume\t90\nAudioMusicMasterVolume\t41\n");
	const auto setup = machine_setup::Read(text);
	EXPECT_EQ(setup.sampleMainVolume, 90u);
	EXPECT_EQ(setup.musicMainVolume, 41u);
	// nothing kept, nothing written
	EXPECT_EQ(machine_setup::Write({}), "");
}

TEST(MachineSetup, LinesNotUnderstoodArePassedOver)
{
	const auto setup = machine_setup::Read(
	    "Something\t5\r\nAudioMusicMasterVolume\tloud\nAudioSampleMasterVolume\t-3\nAudioMusicMasterVolume 9\n"
	    "AudioSampleMasterVolume\t12\r\n");
	EXPECT_EQ(setup.sampleMainVolume, 12u);
	EXPECT_FALSE(setup.musicMainVolume.has_value());
}

TEST(MachineSetup, TheVolumesAreTakenAsTheAudioTakesThem)
{
	uint32_t sample = 127;
	uint32_t music = 127;
	// none kept: the game's own stay
	machine_setup::ApplyVolumes({}, sample, music);
	EXPECT_EQ(sample, 127u);
	EXPECT_EQ(music, 127u);
	machine_setup::ApplyVolumes({.sampleMainVolume = 0u, .musicMainVolume = 64u}, sample, music);
	EXPECT_EQ(sample, 0u);
	EXPECT_EQ(music, 64u);
	// the effects' volume ignores more than 127 and stays; the music's takes any
	machine_setup::ApplyVolumes({.sampleMainVolume = 128u, .musicMainVolume = 200u}, sample, music);
	EXPECT_EQ(sample, 0u);
	EXPECT_EQ(music, 200u);
}

TEST(MachineSetup, KeptInTheProfilesFolderAndNowhereWithoutOne)
{
	test::MemoryFileSystem files;
	const std::filesystem::path folder("Profiles");
	EXPECT_TRUE(machine_setup::Save(files, folder, machine_setup::FromVolumes(30, 100)));
	EXPECT_TRUE(files.folders.contains(folder));
	ASSERT_TRUE(files.files.contains(folder / "Setup.txt"));
	const auto setup = machine_setup::Load(files, folder);
	EXPECT_EQ(setup.sampleMainVolume, 30u);
	EXPECT_EQ(setup.musicMainVolume, 100u);
	// a run of the checks has no folder: it reads and writes nothing
	const auto writes = files.writes;
	EXPECT_FALSE(machine_setup::Save(files, std::nullopt, machine_setup::FromVolumes(1, 2)));
	EXPECT_EQ(files.writes, writes);
	EXPECT_FALSE(machine_setup::Load(files, std::nullopt).sampleMainVolume.has_value());
	// no file yet: nothing kept
	EXPECT_FALSE(machine_setup::Load(files, std::filesystem::path("Elsewhere")).musicMainVolume.has_value());
	// a folder that cannot be made keeps nothing
	files.unmakable.insert(std::filesystem::path("Locked"));
	EXPECT_FALSE(machine_setup::Save(files, std::filesystem::path("Locked"), machine_setup::FromVolumes(1, 2)));
	EXPECT_FALSE(files.files.contains(std::filesystem::path("Locked") / "Setup.txt"));
}

// The detail level the options chose is kept for the computer as the original's "detailidx", with the highest level
// it manages ("maxdetailidx"), and a start takes the kept level when it is one of the game's; a setting kept over the
// others leaves them as they were (docs/wiki/rendering/visible-blocks-and-lod.md)
TEST(MachineSetup, TheDetailLevelKeptIsTakenAtTheNextStartAndAMergeKeepsTheOthers)
{
	const auto read = machine_setup::Read(machine_setup::Write({.detailLevel = 3, .maxDetailLevel = 5}));
	EXPECT_EQ(read.detailLevel, 3u);
	EXPECT_EQ(read.maxDetailLevel, 5u);
	EXPECT_EQ(machine_setup::Read("detailidx\t2\n").detailLevel, 2u);
	EXPECT_EQ(machine_setup::StartDetailLevel({.detailLevel = 0}), std::optional<uint8_t>(0));
	EXPECT_EQ(machine_setup::StartDetailLevel({.detailLevel = 6}), std::optional<uint8_t>(6));
	// none kept, or no level of the game's: the game starts at its own
	EXPECT_FALSE(machine_setup::StartDetailLevel({}).has_value());
	EXPECT_FALSE(machine_setup::StartDetailLevel({.detailLevel = 7}).has_value());

	test::MemoryFileSystem files;
	const std::filesystem::path folder("Profiles");
	EXPECT_TRUE(machine_setup::Save(files, folder, machine_setup::FromVolumes(30, 100)));
	EXPECT_TRUE(machine_setup::Merge(files, folder, {.detailLevel = 2}));
	EXPECT_TRUE(machine_setup::Merge(files, folder, machine_setup::FromVolumes(40, 90)));
	const auto kept = machine_setup::Load(files, folder);
	EXPECT_EQ(kept.sampleMainVolume, 40u);
	EXPECT_EQ(kept.musicMainVolume, 90u);
	EXPECT_EQ(kept.detailLevel, 2u);
	// a run of the checks has no folder: nothing is merged
	EXPECT_FALSE(machine_setup::Merge(files, std::nullopt, {.detailLevel = 1}));
}
