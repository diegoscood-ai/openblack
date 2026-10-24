/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PlayerProfileSystem.h"

#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/PlayerCreature.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Resources/CreatureFileWriter.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The game's log, none in a test that made none
std::shared_ptr<spdlog::logger> GameLog()
{
	return spdlog::get("game");
}

std::span<const uint8_t> BytesOf(std::string_view text)
{
	return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}
} // namespace

PlayerProfileSystem::Machine PlayerProfileSystem::ThisMachine()
{
	return {.accountName =
	            []() {
		            for (const char* variable : {"USERNAME", "USER"})
		            {
			            // NOLINTNEXTLINE(concurrency-mt-unsafe): read on the game's thread
			            if (const char* name = std::getenv(variable); name != nullptr && *name != '\0')
			            {
				            return gui::ToUtf16(name);
			            }
		            }
		            return std::u16string {};
	            },
	        .ticks =
	            []() {
		            const auto now = std::chrono::steady_clock::now().time_since_epoch();
		            return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
	            }};
}

PlayerProfileSystem::PlayerProfileSystem()
    : PlayerProfileSystem(nullptr,
                          {.folder = std::nullopt,
                           .runProfile = player_profiles::RunProfile(gui::ToUtf16(player_profiles::k_UnnamedPlayer),
                                                                     k_DefaultProfileCreatureFile, true)},
                          ThisMachine())
{
}

PlayerProfileSystem::PlayerProfileSystem(filesystem::FileSystemInterface* fileSystem, player_profiles::Store store,
                                         Machine machine)
    : _fileSystem(fileSystem)
    , _store(std::move(store))
    , _machine(std::move(machine))
{
	if (!IsKept())
	{
		// A run of the checks plays its one profile, read from nowhere
		_profiles = {.profiles = {_store.runProfile}, .current = _store.runProfile.name};
		return;
	}
	const auto path = *_store.folder / k_ProfilesFile;
	try
	{
		// the player's own file: a data mod never replaces it
		const auto bytes = _fileSystem->ReadUserFile(path);
		if (!bytes.has_value())
		{
			return;
		}
		_profiles = player_profiles::Read(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
	}
	catch (const std::exception& e)
	{
		if (const auto log = GameLog(); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "The player profiles could not be read from {}: {}", path.generic_string(), e.what());
		}
	}
}

bool PlayerProfileSystem::IsKept() const
{
	return _fileSystem != nullptr && _store.folder.has_value();
}

bool PlayerProfileSystem::CurrentProfileHasCreature() const
{
	// the creature's mind is in the game's creature-mind folder, by the profile's file (player_creature)
	return ecs::player_creature::ProfileHasCreature();
}

std::optional<player_profiles::Profile> PlayerProfileSystem::GetCurrentProfile() const
{
	if (const auto* profile = player_profiles::Current(_profiles); profile != nullptr)
	{
		return *profile;
	}
	return std::nullopt;
}

std::optional<std::string> PlayerProfileSystem::CreatureFile() const
{
	if (const auto* profile = player_profiles::Current(_profiles); profile != nullptr)
	{
		return profile->creatureFile;
	}
	return std::nullopt;
}

player_profiles::NameCheck PlayerProfileSystem::CheckNewName(std::u16string_view name) const
{
	return player_profiles::CheckNewName(_profiles, name);
}

void PlayerProfileSystem::CreateProfile(std::u16string_view name, int symbol)
{
	const auto accountName = _machine.accountName ? _machine.accountName() : std::u16string {};
	const auto ticks = _machine.ticks ? _machine.ticks() : 0u;
	const auto& made = player_profiles::Add(_profiles, name, accountName, player_profiles::CreatureFileName(ticks), symbol);
	// The profile made is the one played from now on
	_profiles.current = made.name;
	if (const auto log = GameLog(); log != nullptr)
	{
		SPDLOG_LOGGER_INFO(log, "Made the player profile \"{}\", its creature to be kept in {}", made.name, made.creatureFile);
	}
	Save();
}

player_profiles::Profile* PlayerProfileSystem::CurrentProfile()
{
	const auto found = std::ranges::find(_profiles.profiles, _profiles.current, &player_profiles::Profile::name);
	return found != _profiles.profiles.end() ? &*found : nullptr;
}

bool PlayerProfileSystem::SelectProfile(std::u16string_view name)
{
	if (!player_profiles::Select(_profiles, name))
	{
		return false;
	}
	if (const auto log = GameLog(); log != nullptr)
	{
		SPDLOG_LOGGER_INFO(log, "Playing as the player profile \"{}\"", _profiles.current);
	}
	Save();
	return true;
}

bool PlayerProfileSystem::DeleteProfile(std::u16string_view name)
{
	if (!player_profiles::Remove(_profiles, name))
	{
		return false;
	}
	if (const auto log = GameLog(); log != nullptr)
	{
		SPDLOG_LOGGER_INFO(log, "Deleted the player profile \"{}\"", gui::ToUtf8(name));
	}
	Save();
	return true;
}

void PlayerProfileSystem::EditCurrentProfile(std::u16string_view creatureName, int symbol)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr)
	{
		return;
	}
	const auto before = *profile;
	player_profiles::Edit(*profile, creatureName, symbol);
	if (*profile != before)
	{
		Save();
	}
}

void PlayerProfileSystem::StartPlaying(int64_t now)
{
	_gameStarted = now;
}

void PlayerProfileSystem::StopPlaying(int64_t now)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr)
	{
		return;
	}
	profile->statistics.secondsPlayed = player_profiles::TimePlayed(profile->statistics.secondsPlayed, _gameStarted, now);
	_gameStarted = now;
	Save();
}

uint32_t PlayerProfileSystem::TimePlayed(int64_t now) const
{
	const auto profile = GetCurrentProfile();
	return player_profiles::TimePlayed(profile.has_value() ? profile->statistics.secondsPlayed : 0U, _gameStarted, now);
}

void PlayerProfileSystem::SetCurrentSettings(const player_profiles::Settings& settings)
{
	auto* profile = CurrentProfile();
	if (profile == nullptr || profile->settings == settings)
	{
		return;
	}
	profile->settings = settings;
	Save();
}

std::optional<std::filesystem::path> PlayerProfileSystem::ProfilesFolder() const
{
	if (!IsKept())
	{
		return std::nullopt;
	}
	return _store.folder;
}

std::optional<std::filesystem::path> PlayerProfileSystem::PlayerFolder(std::u16string_view name) const
{
	if (!IsKept() || name.empty())
	{
		return std::nullopt;
	}
	return *_store.folder / player_profiles::ProfileKey(name);
}

std::optional<std::filesystem::path> PlayerProfileSystem::CurrentPlayerFolder() const
{
	const auto* profile = player_profiles::Current(_profiles);
	if (profile == nullptr)
	{
		return std::nullopt;
	}
	return PlayerFolder(gui::ToUtf16(profile->name));
}

std::optional<std::filesystem::path> PlayerProfileSystem::SavedGamesFolder() const
{
	const auto folder = CurrentPlayerFolder();
	if (!folder.has_value())
	{
		return std::nullopt;
	}
	return *folder / k_SavedGamesFolder;
}

std::optional<std::vector<uint8_t>> PlayerProfileSystem::ReadPlayerFile(const std::filesystem::path& file) const
{
	const auto folder = CurrentPlayerFolder();
	if (!folder.has_value())
	{
		return std::nullopt;
	}
	const auto path = *folder / file;
	try
	{
		// the player's own file: a data mod never replaces it
		return _fileSystem->ReadUserFile(path);
	}
	catch (const std::exception& e)
	{
		if (const auto log = GameLog(); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "{} could not be read: {}", path.generic_string(), e.what());
		}
		return std::nullopt;
	}
}

bool PlayerProfileSystem::WritePlayerFile(const std::filesystem::path& file, std::span<const uint8_t> bytes)
{
	const auto folder = CurrentPlayerFolder();
	if (!folder.has_value())
	{
		return false;
	}
	if (!_fileSystem->MakeFolder(*folder))
	{
		if (const auto log = GameLog(); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "The player's folder {} could not be made", folder->generic_string());
		}
		return false;
	}
	return resources::WriteFile(*_fileSystem, *folder / file, bytes);
}

void PlayerProfileSystem::Save() const
{
	if (!IsKept())
	{
		return;
	}
	const auto path = *_store.folder / k_ProfilesFile;
	const auto text = player_profiles::Write(_profiles);
	// profiles that cannot be written stay as they are in memory, for this run only
	if (!_fileSystem->MakeFolder(*_store.folder) || !resources::WriteFile(*_fileSystem, path, BytesOf(text)))
	{
		if (const auto log = GameLog(); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "The player profiles could not be written to {}; they are kept for this run only",
			                   path.generic_string());
		}
	}
}
