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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Common/PlayerProfiles.h"

namespace openblack::ecs::systems
{

/// The players who play on this computer, each with a profile of their own. It is the one owner of the profiles, their
/// folders and their files: a player's are kept in the Profiles folder of the program's working directory; a run of the
/// checks plays one profile made from the command line and keeps nothing
class PlayerProfileSystemInterface
{
public:
	virtual ~PlayerProfileSystemInterface() = default;

	/// How many player profiles there are
	[[nodiscard]] virtual size_t GetProfileCount() const = 0;
	/// Whether the current profile's creature has been kept: its mind was saved as the player left a land
	[[nodiscard]] virtual bool CurrentProfileHasCreature() const = 0;
	/// The profile being played, if there is one
	[[nodiscard]] virtual std::optional<player_profiles::Profile> GetCurrentProfile() const = 0;
	/// The file the current profile's creature is kept in between lands, none without a profile
	[[nodiscard]] virtual std::optional<std::string> CreatureFile() const = 0;
	/// Whether a new profile may take the name
	[[nodiscard]] virtual player_profiles::NameCheck CheckNewName(std::u16string_view name) const = 0;
	/// Makes a profile of the name with the player's symbol, plays as it and keeps it. The name must have been accepted
	virtual void CreateProfile(std::u16string_view name, int symbol) = 0;
	/// Every profile, in the order the Players page lists them, and the one played
	[[nodiscard]] virtual const player_profiles::Profiles& GetProfiles() const = 0;
	/// Plays as the profile of the name from now on, and keeps that choice; false when no profile has the name
	virtual bool SelectProfile(std::u16string_view name) = 0;
	/// Forgets the profile of the name for good; false when no profile has the name. Forgetting the one played leaves
	/// none played
	virtual bool DeleteProfile(std::u16string_view name) = 0;
	/// Keeps the creature's name and the symbol the player gave on the Players page with the profile played
	virtual void EditCurrentProfile(std::u16string_view creatureName, int symbol) = 0;
	/// Keeps the options the player set with the profile played
	virtual void SetCurrentSettings(const player_profiles::Settings& settings) = 0;
	/// The game started at that time, in seconds since 1970 of the system clock (machine_clock::WallTime)
	virtual void StartPlaying(int64_t now) = 0;
	/// The game ends at that time: the seconds played since it started are added to the profile played, which is kept
	virtual void StopPlaying(int64_t now) = 0;
	/// The seconds the player played, those kept with the profile and those since the game started
	[[nodiscard]] virtual uint32_t TimePlayed(int64_t now) const = 0;
	/// The autosaves the profile played made on the land (the game's "CircleSlot(land)"): 0 before its first, or with no
	/// profile. A profile service that keeps none answers 0
	[[nodiscard]] virtual uint32_t CircleSlot([[maybe_unused]] uint32_t land) const { return 0; }
	/// Keeps the land's count of autosaves with the profile played, at once
	virtual void SetCircleSlot([[maybe_unused]] uint32_t land, [[maybe_unused]] uint32_t count) {}
	/// One more game saved in the save game room by the profile played, kept with its statistics as the game ends
	virtual void CountRoomSave() {}

	/// Whether the profiles are kept between runs: a player's are, a run of the checks' never
	[[nodiscard]] virtual bool IsKept() const = 0;
	/// The folder the profiles are kept in, none when they are not kept
	[[nodiscard]] virtual std::optional<std::filesystem::path> ProfilesFolder() const = 0;
	/// A profile's own folder, named by the key of its name (player_profiles::ProfileKey); not made until a file is
	/// written into it. None when the profiles are not kept
	[[nodiscard]] virtual std::optional<std::filesystem::path> PlayerFolder(std::u16string_view name) const = 0;
	/// The folder of the profile played, none without one or when the profiles are not kept
	[[nodiscard]] virtual std::optional<std::filesystem::path> CurrentPlayerFolder() const = 0;
	/// The folder the profile played keeps its saved games in, none when there is no player folder
	[[nodiscard]] virtual std::optional<std::filesystem::path> SavedGamesFolder() const = 0;
	/// A file of the profile played, read from its folder; none when it is not there or there is no player folder
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> ReadPlayerFile(const std::filesystem::path& file) const = 0;
	/// Writes a file into the folder of the profile played, made at the first write; false when there is no player
	/// folder or the file cannot be written
	virtual bool WritePlayerFile(const std::filesystem::path& file, std::span<const uint8_t> bytes) = 0;
};

} // namespace openblack::ecs::systems
