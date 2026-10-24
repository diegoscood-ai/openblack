/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <string>
#include <string_view>

#include "Common/PlayerProfiles.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::filesystem
{
class FileSystemInterface;
}

namespace openblack::ecs::systems
{

/// The profiles are kept in one text file (in place of the original's registry) in the store's folder, each profile's
/// own files in a folder of its own beside it, all read and written through the file system service. Without a folder
/// (a run of the checks) it plays the store's run profile and reads and writes nothing. A file that cannot be written
/// is reported and the profiles stay as they are in memory
class PlayerProfileSystem final: public PlayerProfileSystemInterface
{
public:
	/// What a new profile is made from: the name of the computer's account, and the system clock in milliseconds
	struct Machine
	{
		std::function<std::u16string()> accountName;
		std::function<uint32_t()> ticks;
	};

	/// The profiles' file in the store's folder
	static constexpr std::string_view k_ProfilesFile = "Profiles.txt";
	/// A profile's saved games, in its own folder
	static constexpr std::string_view k_SavedGamesFolder = "Saved Games";

	/// A run profile of the game's own (its name "Player", the default creature file), kept nowhere: for a locator a
	/// test builds without a game
	PlayerProfileSystem();
	/// The profiles of the store, read and written through the file system (which outlives this system; none keeps
	/// nothing whatever the store)
	PlayerProfileSystem(filesystem::FileSystemInterface* fileSystem, player_profiles::Store store,
	                    Machine machine = ThisMachine());

	[[nodiscard]] size_t GetProfileCount() const final { return _profiles.profiles.size(); }
	[[nodiscard]] bool CurrentProfileHasCreature() const final;
	[[nodiscard]] std::optional<player_profiles::Profile> GetCurrentProfile() const final;
	[[nodiscard]] std::optional<std::string> CreatureFile() const final;
	[[nodiscard]] player_profiles::NameCheck CheckNewName(std::u16string_view name) const final;
	void CreateProfile(std::u16string_view name, int symbol) final;
	[[nodiscard]] const player_profiles::Profiles& GetProfiles() const final { return _profiles; }
	bool SelectProfile(std::u16string_view name) final;
	bool DeleteProfile(std::u16string_view name) final;
	void EditCurrentProfile(std::u16string_view creatureName, int symbol) final;
	void SetCurrentSettings(const player_profiles::Settings& settings) final;
	void StartPlaying(int64_t now) final;
	void StopPlaying(int64_t now) final;
	[[nodiscard]] uint32_t TimePlayed(int64_t now) const final;

	[[nodiscard]] bool IsKept() const final;
	[[nodiscard]] std::optional<std::filesystem::path> ProfilesFolder() const final;
	[[nodiscard]] std::optional<std::filesystem::path> PlayerFolder(std::u16string_view name) const final;
	[[nodiscard]] std::optional<std::filesystem::path> CurrentPlayerFolder() const final;
	[[nodiscard]] std::optional<std::filesystem::path> SavedGamesFolder() const final;
	[[nodiscard]] std::optional<std::vector<uint8_t>> ReadPlayerFile(const std::filesystem::path& file) const final;
	bool WritePlayerFile(const std::filesystem::path& file, std::span<const uint8_t> bytes) final;

	/// The computer's own: its account's name, and the milliseconds since it started
	[[nodiscard]] static Machine ThisMachine();

private:
	void Save() const;
	[[nodiscard]] player_profiles::Profile* CurrentProfile();

	filesystem::FileSystemInterface* _fileSystem {nullptr};
	player_profiles::Store _store;
	Machine _machine;
	player_profiles::Profiles _profiles;
	/// When the game started, in seconds since 1970 of the system clock
	int64_t _gameStarted {0};
};

} // namespace openblack::ecs::systems
