/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ECS/Systems/SavePictureSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::filesystem
{
class FileSystemInterface;
}

namespace openblack::ecs::systems
{

/// The saved games' pictures in the current profile's folder, read and written through the file system service
class SavePictureSystem final: public SavePictureSystemInterface
{
public:
	/// The player's folder of the current profile, from the game's folder, or none when no profile is kept
	using PlayerFolder = std::function<std::optional<std::filesystem::path>()>;
	/// Asks the renderer for a picture of the frame on the screen: the name it comes back under, or none when the frame
	/// on the screen was not kept
	using AskShownFrame = std::function<std::optional<std::string>(graphics::ScreenPicturer picturer)>;

	/// Through the file system (which outlives this system; none reads and writes nothing)
	SavePictureSystem(filesystem::FileSystemInterface* fileSystem, PlayerFolder playerFolder, AskShownFrame askShownFrame);

	void TakePicture(std::optional<uint32_t> slot) override;
	[[nodiscard]] bool KeepsShownFrames() const override;
	[[nodiscard]] std::vector<std::string> TakeRequests() override;
	void Collect(const Screens& screens) override;
	[[nodiscard]] std::optional<std::vector<uint8_t>> ReadPictureFile(std::string_view name) const override;
	[[nodiscard]] uint32_t Changes() const override { return _changes; }
	[[nodiscard]] size_t Waiting() const override { return _waiting.size(); }

private:
	struct WaitingPicture
	{
		std::string name;
		/// The pictures' folder of the profile played as it was asked, which it goes into once back, as the game reads
		/// its screen at once
		std::filesystem::path folder;
		/// The slots whose cells take the picture once it is back
		std::vector<uint32_t> slots;
	};

	[[nodiscard]] std::optional<std::filesystem::path> PicturesFolder() const;
	[[nodiscard]] std::optional<std::vector<uint8_t>> Read(const std::filesystem::path& path) const;
	void WriteTemporaryPicture(const std::filesystem::path& folder, std::span<const uint8_t> picture);
	void PutSlotPictures(const std::filesystem::path& folder, uint32_t slot);

	filesystem::FileSystemInterface* _fileSystem;
	PlayerFolder _playerFolder;
	AskShownFrame _askShownFrame;
	std::vector<WaitingPicture> _waiting;
	std::vector<std::string> _requests;
	uint32_t _asked {0};
	uint32_t _changes {0};
};

} // namespace openblack::ecs::systems
