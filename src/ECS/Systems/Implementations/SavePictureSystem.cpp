/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SavePictureSystem.h"

#include <algorithm>
#include <exception>
#include <iterator>
#include <tuple>
#include <utility>

#include <SavePictureMaker.h>
#include <SavePictures.h>
#include <SaveSlots.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"

using namespace openblack::ecs::systems;

namespace
{
std::optional<std::span<const uint8_t>> View(const std::optional<std::vector<uint8_t>>& bytes)
{
	return bytes.has_value() ? std::optional<std::span<const uint8_t>>(*bytes) : std::nullopt;
}

void Note(std::string_view text)
{
	if (const auto log = spdlog::get("game"); log != nullptr)
	{
		log->debug("Save pictures: {}", text);
	}
}
} // namespace

std::vector<uint8_t> openblack::ecs::systems::SaveScreenPicture(const graphics::ScreenView& screen)
{
	return gamesave::SampleScreen({.width = screen.width,
	                               .height = screen.height,
	                               .pitch = screen.pitch,
	                               .bottomUp = screen.yflip,
	                               .bgra = screen.bgra})
	    .value_or(std::vector<uint8_t> {});
}

SavePictureSystem::SavePictureSystem(filesystem::FileSystemInterface* fileSystem, PlayerFolder playerFolder,
                                     AskShownFrame askShownFrame)
    : _fileSystem(fileSystem)
    , _playerFolder(std::move(playerFolder))
    , _askShownFrame(std::move(askShownFrame))
{
}

std::optional<std::filesystem::path> SavePictureSystem::PicturesFolder() const
{
	const auto player = _playerFolder ? _playerFolder() : std::nullopt;
	if (_fileSystem == nullptr || !player.has_value())
	{
		return std::nullopt;
	}
	return *player / gamesave::k_SavedGamesFolder / gamesave::k_PicturesFolder;
}

std::optional<std::vector<uint8_t>> SavePictureSystem::Read(const std::filesystem::path& path) const
{
	try
	{
		return _fileSystem->ReadUserFile(path);
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}

void SavePictureSystem::TakePicture(std::optional<uint32_t> slot)
{
	const auto folder = PicturesFolder();
	if (!folder.has_value())
	{
		return;
	}
	// The picture is of the frame on the screen, as the game reads its screen at once; when that frame was not kept, of
	// the next frame drawn
	auto name = _askShownFrame ? _askShownFrame(&SaveScreenPicture) : std::nullopt;
	if (!name.has_value())
	{
		name = fmt::format("{}save-picture-{}", graphics::ScreenPictures::k_Prefix, _asked++);
		_requests.push_back(*name);
		Note("a picture asked while the frame on the screen is not kept: it is of the next frame drawn");
	}
	// Pictures asked of one frame are one picture, going into the folder of the first ask
	auto found = std::ranges::find(_waiting, *name, &WaitingPicture::name);
	if (found == _waiting.end())
	{
		_waiting.push_back({.name = *name, .folder = *folder});
		found = std::prev(_waiting.end());
	}
	if (slot.has_value())
	{
		found->slots.push_back(*slot);
	}
}

bool SavePictureSystem::KeepsShownFrames() const
{
	return PicturesFolder().has_value();
}

std::vector<std::string> SavePictureSystem::TakeRequests()
{
	return std::exchange(_requests, {});
}

void SavePictureSystem::Collect(const Screens& screens)
{
	while (!_waiting.empty())
	{
		const auto waiting = _waiting.front();
		const auto picture = screens.take ? screens.take(waiting.name) : std::nullopt;
		if (!picture.has_value())
		{
			// The pictures come back in the order they were asked for
			if (!screens.failed || !screens.failed(waiting.name))
			{
				return;
			}
			Note(fmt::format("the renderer gave up the picture {}", waiting.name));
		}
		_waiting.erase(_waiting.begin());
		if (picture.has_value() && !picture->empty())
		{
			WriteTemporaryPicture(waiting.folder, *picture);
		}
		for (const auto slot : waiting.slots)
		{
			PutSlotPictures(waiting.folder, slot);
		}
	}
}

void SavePictureSystem::WriteTemporaryPicture(const std::filesystem::path& folder, std::span<const uint8_t> picture)
{
	// The game reads the old file, puts the picture into it and writes it
	std::ignore = _fileSystem->MakeFolder(folder);
	const auto path = folder / gamesave::k_TemporaryPictureFileName;
	const auto old = Read(path);
	if (_fileSystem->WriteFile(path, gamesave::MakeTemporaryPicture(View(old), picture)) ==
	    filesystem::FileSystemInterface::WriteResult::Written)
	{
		++_changes;
		Note(fmt::format("the temporary picture written, {}", path.generic_string()));
	}
}

void SavePictureSystem::PutSlotPictures(const std::filesystem::path& folder, uint32_t slot)
{
	// The temporary picture into the slot's cells of its large and small files, as a save into the slot puts it
	std::ignore = _fileSystem->MakeFolder(folder);
	const auto temporary = Read(folder / gamesave::k_TemporaryPictureFileName);
	const auto largePath = folder / gamesave::LargePictureFileName(slot);
	const auto smallPath = folder / gamesave::SmallPictureFileName(slot);
	const auto large = Read(largePath);
	const auto small = Read(smallPath);
	const auto pages = gamesave::PutSlotPictures(View(temporary), View(large), View(small), slot);
	std::ignore = _fileSystem->WriteFile(largePath, pages.large);
	std::ignore = _fileSystem->WriteFile(smallPath, pages.small);
	++_changes;
	Note(fmt::format("slot {}'s pictures written", slot));
}

std::optional<std::vector<uint8_t>> SavePictureSystem::ReadPictureFile(std::string_view name) const
{
	const auto folder = PicturesFolder();
	if (!folder.has_value())
	{
		return std::nullopt;
	}
	return Read(*folder / name);
}
