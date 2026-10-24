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
#include <string_view>

#include "ECS/Systems/SaveGameSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::filesystem
{
class FileSystemInterface;
}

namespace openblack::ecs::systems
{

/// The saved games in the current profile's folder, read and written through the file system service. The profile's
/// folder is asked for each time, so a profile chosen later is the one used; without one nothing is read or written
class SaveGameSystem final: public SaveGameSystemInterface
{
public:
	/// The ending a file being written has until every file of the save is written
	static constexpr std::string_view k_TemporaryEnding = ".tmp";
	/// The loads of a world file that did not end, after which it is not loaded again
	static constexpr uint32_t k_MostLoadAttempts = 2;
	/// What the counter of a world file too short to hold one stands for
	static constexpr uint32_t k_UnreadLoadAttempts = 1000;

	/// The player's folder of the current profile, from the game's folder, or none when no profile is kept
	using PlayerFolder = std::function<std::optional<std::filesystem::path>()>;

	/// Saves through the file system (which outlives this system; none reads and writes nothing)
	SaveGameSystem(filesystem::FileSystemInterface* fileSystem, PlayerFolder playerFolder);

	[[nodiscard]] std::optional<std::filesystem::path> SlotFolder(uint32_t slot, uint32_t land) const override;
	[[nodiscard]] std::optional<std::filesystem::path> WorldFile(uint32_t slot, uint32_t land) const override;
	[[nodiscard]] SlotSummary Summary(uint32_t slot, uint32_t land) const override;
	[[nodiscard]] bool IsAutoSaveValid() const override;
	SaveWrite WriteFiles(std::span<const SaveFile> files) override;
	bool BeginLoad(const std::filesystem::path& worldFile) override;
	void EndLoad(const std::filesystem::path& worldFile) override;

	/// The temporary file a file of a save is written to first
	[[nodiscard]] static std::filesystem::path TemporaryPath(const std::filesystem::path& path);

private:
	filesystem::FileSystemInterface* _fileSystem {nullptr};
	PlayerFolder _playerFolder;

	/// The file whole, or none when it is not there or cannot be read
	[[nodiscard]] std::optional<std::vector<uint8_t>> Read(const std::filesystem::path& path) const;
	/// The counter at the start of a world file's bytes
	[[nodiscard]] static uint32_t LoadAttempts(std::span<const uint8_t> bytes);
	/// Writes the counter into the world file's first four bytes
	bool WriteLoadAttempts(const std::filesystem::path& worldFile, std::vector<uint8_t> bytes, uint32_t attempts);
};

} // namespace openblack::ecs::systems
