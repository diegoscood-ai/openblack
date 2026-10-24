/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player profiles: the pure rules of the list and its file, where a run keeps them, and the profile system on a file
// system in memory (a player's kept, a run of the checks' kept nowhere, a folder that cannot be written)

#include <cstdint>

#include <bit>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#include "Common/PlayerProfiles.h"
#include "support/FileSystemFakes.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/PlayerProfileSystem.h"

using namespace openblack;
using namespace openblack::player_profiles;
using openblack::ecs::systems::PlayerProfileSystem;
using openblack::test::MemoryFileSystem;
using openblack::test::ReadOnlyFileSystem;

namespace
{
/// A computer whose account is Matt and whose clock reads 0
PlayerProfileSystem::Machine FakeMachine()
{
	return {.accountName = []() { return std::u16string(u"Matt"); }, .ticks = []() { return 0u; }};
}

/// Where the tests keep a player's profiles, from the root of the file system in memory
const std::filesystem::path k_Folder = std::filesystem::path("/") / "work" / "Profiles";

Store StoreIn(const std::filesystem::path& folder)
{
	return {.folder = folder, .runProfile = {}};
}

std::string TextOf(const std::vector<uint8_t>& bytes)
{
	return {bytes.begin(), bytes.end()};
}

/// The game's log, made for the tests that write (the file writer reports there), its lines kept
std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> GameLog()
{
	auto sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(16);
	spdlog::drop("game");
	spdlog::register_logger(std::make_shared<spdlog::logger>("game", sink));
	return sink;
}
} // namespace

TEST(PlayerProfiles, ACreatureFileIsNamedAfterTheScrambledTime)
{
	// Each byte has the next of the C runtime's numbers from seed 0x913 added: 7624, 7051, 2652 and 32074, their low
	// bytes 0xc8, 0x8b, 0x5c and 0x4a
	EXPECT_EQ(ScrambleTicks(0), 0x4a5c8bc8u);
	EXPECT_EQ(ScrambleTicks(0x12345678), 0x5c90e140u);
	EXPECT_EQ(CreatureFileName(0), "C4a5c8bc8.erc");
	// Lower-case hexadecimal without leading zeros, as the installed game's own file C4a4f6e63.erc
	EXPECT_EQ(CreatureFileName(0xb6000000), "C5c8bc8.erc");
}

TEST(PlayerProfiles, ANewNameMustNotBeKeptAlreadyNorBeEmptyNorTooLong)
{
	Profiles profiles;
	Add(profiles, u"Matt", u"", "C1.erc", 0);
	EXPECT_EQ(CheckNewName(profiles, u"Ann"), NameCheck::Accepted);
	// Plain letters match whatever their case; anything else must be the same
	EXPECT_EQ(CheckNewName(profiles, u"mATT"), NameCheck::AlreadyExists);
	EXPECT_EQ(CheckNewName(profiles, u"Matt "), NameCheck::Accepted);
	EXPECT_FALSE(SameName(u"Émile", u"émile"));
	EXPECT_EQ(CheckNewName(profiles, u""), NameCheck::Invalid);
	EXPECT_EQ(CheckNewName(profiles, std::u16string(k_LongestName, u'a')), NameCheck::Accepted);
	EXPECT_EQ(CheckNewName(profiles, std::u16string(k_LongestName + 1, u'a')), NameCheck::Invalid);
	// Spaces alone make a name
	EXPECT_EQ(CheckNewName(profiles, u"  "), NameCheck::Accepted);
}

TEST(PlayerProfiles, TheFirstProfileMadeIsTheOnePlayed)
{
	Profiles profiles;
	EXPECT_EQ(Current(profiles), nullptr);
	const auto& made = Add(profiles, u"Matt", u"", "C1.erc", 3);
	EXPECT_EQ(made.creatureName, "Matt");
	EXPECT_EQ(made.symbol, 3);
	ASSERT_NE(Current(profiles), nullptr);
	EXPECT_EQ(Current(profiles)->creatureFile, "C1.erc");
	// Another is kept, but not played; a name already kept keeps its own file
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	EXPECT_EQ(Add(profiles, u"matt", u"", "C3.erc", 0).creatureFile, "C1.erc");
	EXPECT_EQ(profiles.profiles.size(), 2u);
	EXPECT_EQ(profiles.current, "Matt");
}

TEST(PlayerProfiles, AProfileMadeWithoutANameTakesTheAccountsName)
{
	Profiles profiles;
	EXPECT_EQ(Add(profiles, u"", u"Matt", "C1.erc", 0).name, "Matt");
	EXPECT_EQ(Add(profiles, u"", u"", "C2.erc", 0).name, k_UnnamedPlayer);
}

TEST(PlayerProfiles, ProfilesAreReadBackAsWritten)
{
	Profiles profiles;
	Add(profiles, u"Matt Smith", u"", "C4a4f6e63.erc", 5);
	Add(profiles, u"Émile", u"", "C2.erc", 15);
	EXPECT_EQ(Read(Write(profiles)), profiles);
	// Lines it doesn't understand are passed over, and Windows line ends are read too
	const auto read = Read("current\tAnn\r\nnonsense\nprofile\tAnn\tC2.erc\t7\tRex\r\nprofile\tBob\n");
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(read.profiles.front().creatureFile, "C2.erc");
	EXPECT_EQ(read.profiles.front().symbol, 7);
	EXPECT_EQ(read.profiles.front().creatureName, "Rex");
	EXPECT_EQ(read.current, "Ann");
}

TEST(PlayerProfiles, TheListIsInTheOrderOfTheNamesWhateverTheirCase)
{
	Profiles profiles;
	Add(profiles, u"matt", u"", "C1.erc", 0);
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	Add(profiles, u"bob", u"", "C3.erc", 0);
	Add(profiles, u"Ab", u"", "C4.erc", 0);
	ASSERT_EQ(profiles.profiles.size(), 4u);
	EXPECT_EQ(profiles.profiles[0].name, "Ab");
	EXPECT_EQ(profiles.profiles[1].name, "Ann");
	EXPECT_EQ(profiles.profiles[2].name, "bob");
	EXPECT_EQ(profiles.profiles[3].name, "matt");
	EXPECT_EQ(IndexOf(profiles, u"BOB"), 2u);
	EXPECT_FALSE(IndexOf(profiles, u"Zed").has_value());
	// Read back in the list's order, whatever the file's
	const auto read = Read("profile\tzed\tC1.erc\t0\tz\nprofile\tAl\tC2.erc\t0\ta\n");
	ASSERT_EQ(read.profiles.size(), 2u);
	EXPECT_EQ(read.profiles[0].name, "Al");
}

TEST(PlayerProfiles, PickingAndForgettingProfiles)
{
	Profiles profiles;
	Add(profiles, u"Matt", u"", "C1.erc", 0);
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	EXPECT_EQ(profiles.current, "Matt");
	EXPECT_TRUE(Select(profiles, u"ann"));
	EXPECT_EQ(profiles.current, "Ann");
	EXPECT_FALSE(Select(profiles, u"Zed"));
	EXPECT_EQ(profiles.current, "Ann");
	// Forgetting another leaves the one played; forgetting the one played leaves none
	EXPECT_TRUE(Remove(profiles, u"Matt"));
	EXPECT_EQ(profiles.current, "Ann");
	EXPECT_FALSE(Remove(profiles, u"Matt"));
	EXPECT_TRUE(Remove(profiles, u"ANN"));
	EXPECT_TRUE(profiles.current.empty());
	EXPECT_TRUE(profiles.profiles.empty());
}

TEST(PlayerProfiles, TheCreaturesNameIsThePlayersUntilOneIsGivenAndTheSymbolOneOfSixteen)
{
	Profile profile {
	    .name = "Matt", .creatureFile = "C1.erc", .creatureName = "Matt", .symbol = 0, .settings = {}, .statistics = {}};
	Edit(profile, u"Rex", 15);
	EXPECT_EQ(profile.creatureName, "Rex");
	EXPECT_EQ(profile.symbol, 15);
	Edit(profile, u"", 16);
	EXPECT_EQ(profile.creatureName, "Matt");
	EXPECT_EQ(profile.symbol, 0);
	Edit(profile, u"Rex", -1);
	EXPECT_EQ(profile.symbol, 0);
}

TEST(PlayerProfiles, SettingsAreTheGamesOwnUntilKeptAndAreReadBackAsWritten)
{
	// The game's own: intelligent tooltips, the important texts, help level 3, creature help, middle reading speed, the
	// newest text at the top, the right hand, no push scrolling, saving automatically
	constexpr Settings k_Default;
	EXPECT_EQ(k_Default.toolTips, 2);
	EXPECT_EQ(k_Default.storyText, 1);
	EXPECT_EQ(k_Default.helpLevel, 3);
	EXPECT_TRUE(k_Default.creatureHelp);
	EXPECT_FLOAT_EQ(k_Default.readSpeed, 0.5f);
	EXPECT_FALSE(k_Default.textFromBottom);
	EXPECT_FALSE(k_Default.leftHanded);
	EXPECT_FALSE(k_Default.pushScrolling);
	EXPECT_TRUE(k_Default.autoSave);

	Profiles profiles;
	Add(profiles, u"Matt", u"", "C1.erc", 0);
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	auto& matt = profiles.profiles.at(*IndexOf(profiles, u"Matt"));
	matt.settings = {.toolTips = 0,
	                 .storyText = 2,
	                 .helpLevel = 4,
	                 .creatureHelp = false,
	                 .readSpeed = 0.25f,
	                 .textFromBottom = true,
	                 .leftHanded = true,
	                 .pushScrolling = true,
	                 .autoSave = false};
	EXPECT_EQ(Read(Write(profiles)), profiles);
	// A file kept before the settings were has the game's own; a value not understood is passed over
	const auto read = Read("profile\tAnn\tC2.erc\t7\tRex\nsetting\tAnn\thelp_level\tlots\nsetting\tAnn\ttooltips\t3\n");
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(read.profiles.front().settings.helpLevel, 3);
	EXPECT_EQ(read.profiles.front().settings.toolTips, 3);
}

TEST(PlayerProfiles, TheReadSpeedIsKeptWithTheProfileAndIsAHalfWithoutOne)
{
	// A profile that has not kept a read speed reads at 0.5, the original's 0x3F000000
	const auto read = Read("profile\tAnn\tC2.erc\t7\tRex\n");
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(std::bit_cast<uint32_t>(read.profiles.front().settings.readSpeed), 0x3F000000u);
	// The original keeps the slider's float as a double and narrows it back: every speed comes back to the bit
	Profiles profiles;
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	for (const float speed : {0.0f, 0.1f, 1.0f / 3.0f, 0.7f, 1.0f})
	{
		profiles.profiles.front().settings.readSpeed = speed;
		const auto back = Read(Write(profiles));
		ASSERT_EQ(back.profiles.size(), 1u);
		EXPECT_EQ(std::bit_cast<uint32_t>(back.profiles.front().settings.readSpeed), std::bit_cast<uint32_t>(speed));
	}
}

TEST(PlayerProfiles, AProfilesFolderIsNamedByItsKey)
{
	// Plain letters and digits each after an underscore, anything else as four hexadecimal digits
	EXPECT_EQ(ProfileKey(u"Bob 2"), "_B_o_b0020_2");
	EXPECT_EQ(ProfileKey(u"J\u00e9r\u00f4me!"), "_J00e9_r00f4_m_e0021");
	EXPECT_EQ(ProfileKey(u""), "");
	// It stops once it reaches its longest, finishing the character it is on
	EXPECT_EQ(ProfileKey(std::u16string(200, u'a')).size(), k_LongestKey);
	EXPECT_EQ(ProfileKey(std::u16string(1, u'a') + std::u16string(100, u'.')).size(), 254u);
}

TEST(PlayerProfiles, AProfilesFolderIsNamedAsTheGamesOwnProfileFolders)
{
	// The folders the original made for its players, in the game's own Profiles folder
	EXPECT_EQ(ProfileKey(u"Jahovia"), "_J_a_h_o_v_i_a");
	EXPECT_EQ(ProfileKey(u"ff"), "_f_f");
	EXPECT_EQ(ProfileKey(u"sisi"), "_s_i_s_i");
	// Anything but a plain letter or digit is its four digits in lower case: the space, the underscore and the
	// characters past the plain ones too
	EXPECT_EQ(ProfileKey(u" "), "0020");
	EXPECT_EQ(ProfileKey(u"_"), "005f");
	EXPECT_EQ(ProfileKey(u"\u00c9"), "00c9");
	EXPECT_EQ(ProfileKey(u"\u4e2d"), "4e2d");
	// The key stops after the character that takes it to 252 or more
	EXPECT_EQ(ProfileKey(std::u16string(126, u'a')).size(), 252u);
	EXPECT_EQ(ProfileKey(std::u16string(127, u'a')).size(), 252u);
	EXPECT_EQ(ProfileKey(std::u16string(125, u'a') + u"..").size(), 254u);
}

TEST(PlayerProfiles, ARunOfTheChecksPlaysTheProfileTheCommandLineMakes)
{
	const auto profile = RunProfile(u"Tester", "C4ba71b36.erc", true);
	EXPECT_EQ(profile.name, "Tester");
	EXPECT_EQ(profile.creatureName, "Tester");
	EXPECT_EQ(profile.creatureFile, "C4ba71b36.erc");
	EXPECT_EQ(profile.symbol, k_RunProfileSymbol);
	EXPECT_EQ(profile.symbol, 15);
	// The game's own settings, but for the hand, which is as the command line draws it
	EXPECT_EQ(profile.settings, Settings {});
	EXPECT_TRUE(RunProfile(u"Tester", "", false).settings.leftHanded);
}

TEST(PlayerProfiles, APlayersProfilesAreKeptInTheWorkingFolderAndAChecksRunKeepsNone)
{
	const auto work = std::filesystem::path("/") / "work";
	const auto run = RunProfile(u"Tester", "C1.erc", true);
	// A person's, in the working directory's Profiles folder, never the game's
	const auto player = StoreFor({.profileFolder = std::nullopt, .workingFolder = work, .checkRun = false, .runProfile = run});
	ASSERT_TRUE(player.folder.has_value());
	EXPECT_EQ(player.folder->filename(), "Profiles");
	EXPECT_TRUE(player.folder->is_absolute());
	// A run of the checks keeps none and plays its run profile
	const auto check = StoreFor({.profileFolder = std::nullopt, .workingFolder = work, .checkRun = true, .runProfile = run});
	EXPECT_FALSE(check.folder.has_value());
	EXPECT_EQ(check.runProfile, run);
	// A folder given is kept as a player's, whatever the run
	const auto given =
	    StoreFor({.profileFolder = std::filesystem::path("clean"), .workingFolder = work, .checkRun = true, .runProfile = run});
	ASSERT_TRUE(given.folder.has_value());
	EXPECT_EQ(given.folder->filename(), "clean");
	EXPECT_TRUE(given.folder->is_absolute());
}

TEST(PlayerProfileSystem, AFirstStartHasNoProfileUntilOneIsMade)
{
	const auto log = GameLog();
	MemoryFileSystem files;
	const auto store = StoreIn(k_Folder);
	PlayerProfileSystem system(&files, store, FakeMachine());
	EXPECT_TRUE(system.IsKept());
	EXPECT_EQ(system.GetProfileCount(), 0u);
	EXPECT_FALSE(system.CreatureFile().has_value());
	EXPECT_FALSE(system.CurrentPlayerFolder().has_value());
	// Nothing is written until a profile is made
	EXPECT_EQ(files.writes, 0);

	system.CreateProfile(u"Ann", 4);
	EXPECT_EQ(system.GetProfileCount(), 1u);
	EXPECT_EQ(system.CreatureFile(), "C4a5c8bc8.erc");
	ASSERT_TRUE(system.GetCurrentProfile().has_value());
	EXPECT_EQ(system.GetCurrentProfile()->symbol, 4);
	EXPECT_EQ(system.CheckNewName(u"ann"), NameCheck::AlreadyExists);
	EXPECT_TRUE(files.folders.contains(k_Folder));
	ASSERT_TRUE(files.files.contains(k_Folder / PlayerProfileSystem::k_ProfilesFile));

	// A later game reads the profile back
	PlayerProfileSystem later(&files, store, FakeMachine());
	EXPECT_EQ(later.GetProfileCount(), 1u);
	EXPECT_EQ(later.CreatureFile(), "C4a5c8bc8.erc");
}

TEST(PlayerProfileSystem, PickingDeletingAndEditingProfilesIsKept)
{
	const auto log = GameLog();
	MemoryFileSystem files;
	const auto store = StoreIn(k_Folder);
	PlayerProfileSystem system(&files, store, FakeMachine());
	system.CreateProfile(u"Matt", 0);
	system.CreateProfile(u"Ann", 0);
	// The profile made is the one played
	EXPECT_EQ(system.GetProfiles().current, "Ann");
	EXPECT_TRUE(system.SelectProfile(u"matt"));
	system.EditCurrentProfile(u"Rex", 9);
	auto settings = system.GetCurrentProfile()->settings;
	settings.helpLevel = 0;
	system.SetCurrentSettings(settings);
	EXPECT_TRUE(system.DeleteProfile(u"Ann"));
	EXPECT_FALSE(system.DeleteProfile(u"Ann"));

	PlayerProfileSystem later(&files, store, FakeMachine());
	ASSERT_EQ(later.GetProfileCount(), 1u);
	const auto profile = later.GetCurrentProfile();
	ASSERT_TRUE(profile.has_value());
	EXPECT_EQ(profile->name, "Matt");
	EXPECT_EQ(profile->creatureName, "Rex");
	EXPECT_EQ(profile->symbol, 9);
	EXPECT_EQ(profile->settings.helpLevel, 0);
}

TEST(PlayerProfiles, TheTimePlayedIsTheTimeKeptAndTheTimeSinceTheGameStarted)
{
	EXPECT_EQ(TimePlayed(100, 1000, 1060), 160u);
	// The count wraps as a 32-bit count does
	EXPECT_EQ(TimePlayed(0xFFFFFFF0u, 0, 0x20), 0x10u);
}

TEST(PlayerProfiles, TheStatisticsAreKeptWithTheProfile)
{
	Profiles profiles;
	Add(profiles, u"Ann", u"", "C1.erc", 0);
	profiles.profiles.front().statistics = {.secondsPlayed = 3725, .saves = 4, .loads = 2};
	const auto read = Read(Write(profiles));
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(read.profiles.front().statistics, (Statistics {.secondsPlayed = 3725, .saves = 4, .loads = 2}));
	// A line that cannot be read leaves the counts at zero
	const auto bad = Read("profile\tAnn\tC1.erc\t0\tAnn\nstatistics\tAnn\t5\tx\t1\n");
	ASSERT_EQ(bad.profiles.size(), 1u);
	EXPECT_EQ(bad.profiles.front().statistics, Statistics {});
}

TEST(PlayerProfileSystem, TheTimePlayedIsKeptAsTheGameEnds)
{
	const auto log = GameLog();
	MemoryFileSystem files;
	const auto store = StoreIn(k_Folder);
	PlayerProfileSystem system(&files, store, FakeMachine());
	system.CreateProfile(u"Ann", 0);
	system.StartPlaying(1000);
	EXPECT_EQ(system.TimePlayed(1030), 30u);
	system.StopPlaying(1100);
	PlayerProfileSystem later(&files, store, FakeMachine());
	ASSERT_TRUE(later.GetCurrentProfile().has_value());
	EXPECT_EQ(later.GetCurrentProfile()->statistics, (Statistics {.secondsPlayed = 100, .saves = 0, .loads = 0}));
	// The next game counts on from what was kept
	later.StartPlaying(5000);
	EXPECT_EQ(later.TimePlayed(5010), 110u);
}

TEST(PlayerProfileSystem, ARunOfTheChecksPlaysItsRunProfileAndKeepsNothing)
{
	MemoryFileSystem files;
	const auto run = RunProfile(u"Tester", "C4ba71b36.erc", true);
	PlayerProfileSystem system(&files, {.folder = std::nullopt, .runProfile = run}, FakeMachine());
	EXPECT_FALSE(system.IsKept());
	ASSERT_EQ(system.GetProfileCount(), 1u);
	EXPECT_EQ(system.GetCurrentProfile(), run);
	EXPECT_EQ(system.CreatureFile(), "C4ba71b36.erc");
	EXPECT_FALSE(system.ProfilesFolder().has_value());
	EXPECT_FALSE(system.CurrentPlayerFolder().has_value());
	EXPECT_FALSE(system.SavedGamesFolder().has_value());
	EXPECT_FALSE(system.ReadPlayerFile("helpstats.dat").has_value());
	EXPECT_FALSE(system.WritePlayerFile("helpstats.dat", std::vector<uint8_t> {1, 2}));
	// Its changes stay in memory
	system.CreateProfile(u"Ann", 0);
	EXPECT_EQ(system.GetProfileCount(), 2u);
	system.SetCurrentSettings({.toolTips = 0});
	EXPECT_EQ(files.writes, 0);
	EXPECT_TRUE(files.files.empty());
	EXPECT_TRUE(files.folders.empty());

	// Without a file system nothing is kept either, whatever the store
	const PlayerProfileSystem none(nullptr, StoreIn(k_Folder), FakeMachine());
	EXPECT_FALSE(none.IsKept());
	EXPECT_EQ(none.GetProfileCount(), 1u);
}

TEST(PlayerProfileSystem, AFolderThatCannotBeWrittenKeepsTheProfilesInMemoryAndSaysSo)
{
	const auto log = GameLog();
	ReadOnlyFileSystem files;
	PlayerProfileSystem system(&files, StoreIn(k_Folder), FakeMachine());
	system.CreateProfile(u"Ann", 0);
	EXPECT_EQ(system.GetProfileCount(), 1u);
	EXPECT_EQ(system.GetProfiles().current, "Ann");
	const auto lines = log->last_formatted();
	ASSERT_FALSE(lines.empty());
	EXPECT_NE(lines.back().find("kept for this run only"), std::string::npos);
}

TEST(PlayerProfileSystem, EachProfileKeepsItsFilesInAFolderOfItsOwn)
{
	const auto log = GameLog();
	MemoryFileSystem files;
	PlayerProfileSystem system(&files, StoreIn(k_Folder), FakeMachine());
	system.CreateProfile(u"Bob 2", 0);
	const auto folder = k_Folder / "_B_o_b0020_2";
	EXPECT_EQ(system.ProfilesFolder(), k_Folder);
	EXPECT_EQ(system.PlayerFolder(u"Bob 2"), folder);
	EXPECT_EQ(system.CurrentPlayerFolder(), folder);
	EXPECT_EQ(system.SavedGamesFolder(), folder / "Saved Games");
	// Not made until a file is written into it
	EXPECT_FALSE(files.folders.contains(folder));
	EXPECT_FALSE(system.ReadPlayerFile("helpstats.dat").has_value());
	EXPECT_TRUE(system.WritePlayerFile("helpstats.dat", std::vector<uint8_t> {'h', 'i'}));
	EXPECT_TRUE(files.folders.contains(folder));
	const auto read = system.ReadPlayerFile("helpstats.dat");
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(TextOf(*read), "hi");
}
