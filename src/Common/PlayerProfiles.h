/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// The players who play on this computer, each with a profile of their own: the name they go by, the symbol they
/// picked, their creature's name and the file their creature is kept in between lands
namespace openblack::player_profiles
{

/// What the player sets in the options, kept with their profile. A profile that has never kept a setting has the
/// game's own: intelligent tooltips, the important texts shown, all but the least help, the creature's help on, reading
/// at middle speed, the newest text at the top, the right hand, no push scrolling and the land saved automatically
struct Settings
{
	/// None, minimum, intelligent or all
	int toolTips {2};
	/// No text, the important texts or all of them
	int storyText {1};
	/// No help up to all help, 0 to 4
	int helpLevel {3};
	bool creatureHelp {true};
	/// From 0 (slow) to 1 (fast)
	float readSpeed {0.5f};
	bool textFromBottom {false};
	bool leftHanded {false};
	bool pushScrolling {false};
	bool autoSave {true};

	bool operator==(const Settings&) const = default;
};

/// What the save game room's scroll counts for the player: the seconds they played in the games that ended, and how
/// many times they saved and loaded a game in the room
struct Statistics
{
	uint32_t secondsPlayed {0};
	uint32_t saves {0};
	uint32_t loads {0};

	bool operator==(const Statistics&) const = default;
};

/// The seconds played that the save game room shows: those kept with the profile, and those since the game started.
/// The count is in whole seconds and wraps as the game's 32-bit count does.
[[nodiscard]] constexpr uint32_t TimePlayed(uint32_t secondsKept, int64_t gameStarted, int64_t now)
{
	return secondsKept - static_cast<uint32_t>(gameStarted) + static_cast<uint32_t>(now);
}

struct Profile
{
	/// The name the player typed, as UTF-8
	std::string name;
	/// The file, in the creature-mind folder, the profile's creature is kept in
	std::string creatureFile;
	/// The creature's name, the player's own until it is changed
	std::string creatureName;
	/// The player's symbol, one of the sixteen
	int symbol {0};
	/// The options the player set
	Settings settings;
	/// The time played and the saves and loads, which the game keeps as the game ends
	Statistics statistics;

	bool operator==(const Profile&) const = default;
};

/// Every profile, in the order the Players page lists them (by name, ignoring the case of plain letters), and the one
/// being played, by its name
struct Profiles
{
	std::vector<Profile> profiles;
	std::string current;

	bool operator==(const Profiles&) const = default;
};

/// The players' symbols
constexpr int k_SymbolCount = 16;
/// The most characters a name typed into the new player's box can have
constexpr size_t k_NameBoxLength = 29;
/// A name longer than this is turned down
constexpr size_t k_LongestName = 64;
/// The name a profile made without one takes when the computer's account has no name either
constexpr std::string_view k_UnnamedPlayer = "Player";

/// What becomes of a name given for a new profile
enum class NameCheck : uint8_t
{
	Accepted,
	/// A profile of that name is already kept
	AlreadyExists,
	/// Empty, or too long
	Invalid,
};

/// A new profile's creature file is named after the time on the system's clock in milliseconds, scrambled so the names
/// look unlike each other: each of its four bytes, lowest first, has the C runtime's next random number added to it,
/// the random numbers always starting from the same seed
[[nodiscard]] uint32_t ScrambleTicks(uint32_t ticks);
/// The creature file of a profile made at that time: "C", the scrambled time in lower-case hexadecimal, ".erc"
[[nodiscard]] std::string CreatureFileName(uint32_t ticks);

/// Two names are one profile's when they are the same but for the case of their plain (ASCII) letters: profiles were
/// kept by name in a store that ignores the case of those letters
[[nodiscard]] bool SameName(std::u16string_view a, std::u16string_view b);
/// Whether a new profile may take the name: the check for a name already kept comes first, then its length
[[nodiscard]] NameCheck CheckNewName(const Profiles& profiles, std::u16string_view name);

/// The name a profile's own folder goes by: each plain (ASCII) letter or digit as an underscore and itself, any other
/// character as its four lower-case hexadecimal digits, ending once the name made reaches k_LongestKey characters
[[nodiscard]] std::string ProfileKey(std::u16string_view name);
/// A profile's folder name stops growing once it is this long, finishing the character it is on
constexpr size_t k_LongestKey = 252;

/// The profile being played, if there is one by its name
[[nodiscard]] const Profile* Current(const Profiles& profiles);
/// The profile of a name, if one is kept
[[nodiscard]] const Profile* Find(const Profiles& profiles, std::u16string_view name);
/// Makes a profile: an empty name is the account's (or k_UnnamedPlayer); a name already kept is used as it is, else a
/// new profile is made with the creature file, the player's name for its creature and the symbol. The first profile
/// becomes the one played. The profile of that name
const Profile& Add(Profiles& profiles, std::u16string_view name, std::u16string_view accountName, std::string_view creatureFile,
                   int symbol);

/// The place of the profile of a name in the list, none when it isn't kept
[[nodiscard]] std::optional<size_t> IndexOf(const Profiles& profiles, std::u16string_view name);
/// Plays as the profile of the name, if one is kept; whether it is
bool Select(Profiles& profiles, std::u16string_view name);
/// Forgets the profile of the name, if one is kept; whether it was. Forgetting the one played leaves none played
bool Remove(Profiles& profiles, std::u16string_view name);

/// The player's edits on the Players page kept with their profile: the creature's name typed (an empty one is the
/// player's own name) and the symbol picked (one past the sixteen is the first)
void Edit(Profile& profile, std::u16string_view creatureName, int symbol);

/// The profiles as they are kept on disk, UTF-8: one line naming the current profile, then a line for each profile with
/// its name, its creature file, its symbol and its creature's name, then a line for each of its settings: the profile's
/// name, the setting's and its value; all separated by tabs (no name can hold a tab)
[[nodiscard]] std::string Write(const Profiles& profiles);
/// The profiles read back, in the list's order; lines that aren't understood are passed over, and settings not kept
/// are the game's own
[[nodiscard]] Profiles Read(std::string_view text);

/// Where the profiles are kept for a run
struct Store
{
	/// The folder the profiles' file and each profile's own folder are kept in; none for a run of the checks, which plays
	/// `runProfile` and keeps nothing
	std::optional<std::filesystem::path> folder;
	/// The one profile a run of the checks plays, made from the command line, never read nor written
	Profile runProfile;
};

/// How the game was started, as far as it decides where the profiles are kept
struct RunOptions
{
	/// A folder given for the profiles (--profile-folder)
	std::optional<std::filesystem::path> profileFolder;
	/// The program's working directory as it started, whose Profiles folder keeps a player's profiles
	std::filesystem::path workingFolder;
	/// A run of the checks rather than a person playing (EngineConfig::checkRun)
	bool checkRun {false};
	/// The profile such a run plays (RunProfile)
	Profile runProfile;
};

/// The folder, in the program's working directory, that a player's profiles are kept in
constexpr std::string_view k_ProfilesFolder = "Profiles";
/// The symbol a run of the checks plays with, the Players page's first pick as it has always been
constexpr int k_RunProfileSymbol = 15;

/// The profile a run of the checks plays: the name it greets the player by, the creature file given on the command
/// line, the symbol it has always had, and the game's own settings but for the hand, which is as the command line
/// draws it. Its creature is called by the player's name
[[nodiscard]] Profile RunProfile(std::u16string_view name, std::string_view creatureFile, bool rightHandedHand);

/// A folder given keeps the profiles there, as a player's are, whatever started the game, so that a first start can be
/// had on a clean folder; a run of the checks plays its run profile, kept nowhere, so that it reads and writes nothing;
/// a player's are kept in the Profiles folder of the program's working directory, never in the game's folder
[[nodiscard]] Store StoreFor(const RunOptions& options);

} // namespace openblack::player_profiles
