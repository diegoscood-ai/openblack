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

#include <SavePictureMaker.h>
#include <SavePictures.h>
#include <SaveSlots.h>
#include <WorldReader.h>
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

std::vector<std::string> SaveGameSystem::MapPictureNames()
{
	std::vector<std::string> names;
	for (uint32_t page = 0; page < k_LargeMapPictures; ++page)
	{
		names.push_back("screenshots_hires_" + std::to_string(page) + "_map_0.raw");
	}
	for (uint32_t page = 0; page < k_SmallMapPictures; ++page)
	{
		names.push_back("screenshots_lores_" + std::to_string(page) + "_map_0.raw");
	}
	return names;
}

SaveWrite SaveGameSystem::Save(uint32_t slot, uint32_t land, const SaveContents& contents)
{
	const auto player = _playerFolder ? _playerFolder() : std::nullopt;
	const auto folder = SlotFolder(slot, land);
	if (!player.has_value() || !folder.has_value())
	{
		return SaveWrite::NotOpened;
	}
	const auto minds = *player / gamesave::MindFolder(slot, land);
	try
	{
		return SaveInOrder(slot, *player, *folder, minds, contents);
	}
	catch (const std::exception& error)
	{
		// A write that stops part of the way is a write that failed: the old world files stay, and the temporary files
		// as far as they were written, as the game leaves them
		if (const auto log = spdlog::get("game"); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "The game's save into slot {} stopped: {}", slot, error.what());
		}
		return SaveWrite::Failed;
	}
}

SaveWrite SaveGameSystem::SaveInOrder(uint32_t slot, const std::filesystem::path& player, const std::filesystem::path& folder,
                                      const std::filesystem::path& minds, const SaveContents& contents)
{
	using WriteResult = filesystem::FileSystemInterface::WriteResult;
	// The save's folders are made first, as the game makes them again before every save
	std::ignore = _fileSystem->MakeFolder(folder);
	std::ignore = _fileSystem->MakeFolder(minds);
	const auto world = folder / gamesave::VanillaWorldFileName(slot);
	const auto script = folder / gamesave::VanillaScriptFileName(slot);

	if (contents.pictures)
	{
		// The profile's map pictures, each copied from its Current folder when it is there; a copy that fails is not
		// looked at
		for (const auto& name : MapPictureNames())
		{
			if (auto bytes = Read(player / k_CurrentFolder / name); bytes.has_value())
			{
				std::ignore = _fileSystem->WriteFile(folder / name, *bytes);
			}
		}
		// The temporary picture into the slot's cells of its large and small pages
		const auto pictures = player / gamesave::k_SavedGamesFolder / gamesave::k_PicturesFolder;
		std::ignore = _fileSystem->MakeFolder(pictures);
		const auto temporary = Read(pictures / gamesave::k_TemporaryPictureFileName);
		const auto largePath = pictures / gamesave::LargePictureFileName(slot);
		const auto smallPath = pictures / gamesave::SmallPictureFileName(slot);
		const auto large = Read(largePath);
		const auto small = Read(smallPath);
		const auto span = [](const std::optional<std::vector<uint8_t>>& bytes) {
			return bytes.has_value() ? std::optional<std::span<const uint8_t>>(*bytes) : std::nullopt;
		};
		const auto pages = gamesave::PutSlotPictures(span(temporary), span(large), span(small), slot);
		std::ignore = _fileSystem->WriteFile(largePath, pages.large);
		std::ignore = _fileSystem->WriteFile(smallPath, pages.small);
	}
	// The info file, before the world's
	if (contents.info.has_value())
	{
		const auto info = folder / gamesave::k_InfoFileName;
		if (_fileSystem->WriteFile(info, gamesave::Write(*contents.info)) != WriteResult::Written)
		{
			Warn("write", info);
		}
	}

	// The world file's temporary file: the game opens it first, and saves nothing when it does not open
	const auto written = _fileSystem->WriteFile(TemporaryPath(world), contents.world);
	if (written == WriteResult::NotOpened)
	{
		Warn("write", TemporaryPath(world));
		return SaveWrite::NotOpened;
	}
	bool failed = written != WriteResult::Written;
	// Inside the game object's save: each creature's mind file, under its own name, then the script machine's file
	for (const auto& [index, bytes] : contents.minds)
	{
		const auto path = minds / gamesave::SavedCreatureFileName(index);
		if (_fileSystem->WriteFile(path, bytes) != WriteResult::Written)
		{
			Warn("write", path);
		}
	}
	if (contents.helpRecord)
	{
		contents.helpRecord();
	}
	if (contents.scriptState && !contents.scriptState(TemporaryPath(script)))
	{
		Warn("write", TemporaryPath(script));
		failed = true;
	}
	if (failed)
	{
		// The game keeps the old world files and leaves the temporary files where they are
		return SaveWrite::Failed;
	}
	// The old world file removed and the temporary one given its name, then the same for the script machine's; the
	// game looks at neither result, so a rename that fails leaves the save in its temporary file
	std::ignore = _fileSystem->Remove(world);
	std::ignore = _fileSystem->Rename(TemporaryPath(world), world);
	std::ignore = _fileSystem->Remove(script);
	std::ignore = _fileSystem->Rename(TemporaryPath(script), script);
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
	// The attempt is kept before the load, so a load that ends the game twice leaves the save refused. A file whose
	// counter cannot be written (a read-only one) is refused, as the game opens it for writing first
	return WriteLoadAttempts(worldFile, std::move(*bytes), attempts + 1);
}

void SaveGameSystem::EndLoad(const std::filesystem::path& worldFile)
{
	if (auto bytes = Read(worldFile); bytes.has_value())
	{
		std::ignore = WriteLoadAttempts(worldFile, std::move(*bytes), 0);
	}
}

void SaveGameSystem::KeepLoadedWorld(std::shared_ptr<const gamesave::WorldRead> world, saved_world_write::LoadedObjects objects)
{
	_loadedWorld = std::move(world);
	_loadedObjects = std::move(objects);
}

void SaveGameSystem::ForgetLoadedWorld()
{
	_loadedWorld.reset();
	_loadedObjects.clear();
}
