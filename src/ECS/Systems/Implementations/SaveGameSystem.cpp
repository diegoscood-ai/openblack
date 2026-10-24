/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SaveGameSystem.h"

#include <cstring>

#include <exception>
#include <tuple>
#include <utility>

#include <SaveSlots.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"

using namespace openblack::ecs::systems;

namespace
{
void Warn(std::string_view what, const std::filesystem::path& path)
{
	if (const auto log = spdlog::get("game"); log != nullptr)
	{
		SPDLOG_LOGGER_WARN(log, "The game could not {} {}", what, path.generic_string());
	}
}
} // namespace

SaveGameSystem::SaveGameSystem(filesystem::FileSystemInterface* fileSystem, PlayerFolder playerFolder)
    : _fileSystem(fileSystem)
    , _playerFolder(std::move(playerFolder))
{
}

std::filesystem::path SaveGameSystem::TemporaryPath(const std::filesystem::path& path)
{
	auto temporary = path;
	temporary += k_TemporaryEnding;
	return temporary;
}

std::optional<std::filesystem::path> SaveGameSystem::SlotFolder(uint32_t slot, uint32_t land) const
{
	const auto folder = _playerFolder ? _playerFolder() : std::nullopt;
	if (_fileSystem == nullptr || !folder.has_value() || !gamesave::IsSlot(slot))
	{
		return std::nullopt;
	}
	return *folder / gamesave::SlotFolder(slot, land);
}

std::optional<std::filesystem::path> SaveGameSystem::WorldFile(uint32_t slot, uint32_t land) const
{
	const auto folder = _playerFolder ? _playerFolder() : std::nullopt;
	if (_fileSystem == nullptr || !folder.has_value() || !gamesave::IsSlot(slot))
	{
		return std::nullopt;
	}
	return *folder / gamesave::LoadSlotFolder(slot, land) / gamesave::VanillaWorldFileName(slot);
}

std::optional<std::vector<uint8_t>> SaveGameSystem::Read(const std::filesystem::path& path) const
{
	if (_fileSystem == nullptr)
	{
		return std::nullopt;
	}
	try
	{
		return _fileSystem->ReadUserFile(path);
	}
	catch (const std::exception&)
	{
		Warn("read", path);
		return std::nullopt;
	}
}

uint32_t SaveGameSystem::LoadAttempts(std::span<const uint8_t> bytes)
{
	uint32_t attempts = k_UnreadLoadAttempts;
	if (bytes.size() >= sizeof(attempts))
	{
		std::memcpy(&attempts, bytes.data(), sizeof(attempts));
	}
	return attempts;
}

SlotSummary SaveGameSystem::Summary(uint32_t slot, uint32_t land) const
{
	SlotSummary summary {.slot = slot};
	const auto folder = SlotFolder(slot, land);
	const auto world = WorldFile(slot, land);
	if (!folder.has_value() || !world.has_value())
	{
		return summary;
	}
	if (const auto info = Read(*folder / gamesave::k_InfoFileName); info.has_value())
	{
		summary.info = gamesave::ReadSaveGameInfo(*info);
	}
	if (const auto bytes = Read(*world); bytes.has_value())
	{
		summary.hasWorld = true;
		summary.loadAttempts = LoadAttempts(*bytes);
		summary.loadable = summary.loadAttempts < k_MostLoadAttempts;
	}
	return summary;
}

bool SaveGameSystem::IsAutoSaveValid() const
{
	// the autosave on exit has no folder for each land
	const auto world = WorldFile(gamesave::k_ExitSlot, 0);
	return world.has_value() && Read(*world).has_value();
}

SaveWrite SaveGameSystem::WriteFiles(std::span<const SaveFile> files)
{
	using WriteResult = filesystem::FileSystemInterface::WriteResult;
	if (_fileSystem == nullptr || files.empty())
	{
		return SaveWrite::NotOpened;
	}
	for (size_t i = 0; i < files.size(); ++i)
	{
		const auto result = _fileSystem->WriteFile(TemporaryPath(files[i].path), files[i].bytes);
		if (result == WriteResult::Written)
		{
			continue;
		}
		Warn("write", TemporaryPath(files[i].path));
		// Only the first file not opening means that the game wrote nothing
		return i == 0 && result == WriteResult::NotOpened ? SaveWrite::NotOpened : SaveWrite::Failed;
	}
	// The game removes the old file first and then gives the temporary file its name, and looks at neither result
	for (const auto& file : files)
	{
		std::ignore = _fileSystem->Remove(file.path);
		std::ignore = _fileSystem->Rename(TemporaryPath(file.path), file.path);
	}
	return SaveWrite::Written;
}

bool SaveGameSystem::WriteLoadAttempts(const std::filesystem::path& worldFile, std::vector<uint8_t> bytes, uint32_t attempts)
{
	if (bytes.size() < sizeof(attempts))
	{
		bytes.resize(sizeof(attempts));
	}
	std::memcpy(bytes.data(), &attempts, sizeof(attempts));
	if (_fileSystem->WriteFile(worldFile, bytes) != filesystem::FileSystemInterface::WriteResult::Written)
	{
		Warn("write", worldFile);
		return false;
	}
	return true;
}

bool SaveGameSystem::BeginLoad(const std::filesystem::path& worldFile)
{
	auto bytes = Read(worldFile);
	if (!bytes.has_value())
	{
		return false;
	}
	const auto attempts = LoadAttempts(*bytes);
	if (attempts >= k_MostLoadAttempts)
	{
		return false;
	}
	// The attempt is kept before the load, so a load that ends the game twice leaves the save refused
	std::ignore = WriteLoadAttempts(worldFile, std::move(*bytes), attempts + 1);
	return true;
}

void SaveGameSystem::EndLoad(const std::filesystem::path& worldFile)
{
	if (auto bytes = Read(worldFile); bytes.has_value())
	{
		std::ignore = WriteLoadAttempts(worldFile, std::move(*bytes), 0);
	}
}
