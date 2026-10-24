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
#include <optional>
#include <span>
#include <vector>

#include <SaveGameInfo.h>

namespace openblack::ecs::systems
{

/// How a write of a save's files went: written; nothing written because the first file did not open; or failed, the
/// old files kept
enum class SaveWrite
{
	Written,
	NotOpened,
	Failed,
};

/// A file of a save, its path from the game's folder and its bytes
struct SaveFile
{
	std::filesystem::path path;
	std::vector<uint8_t> bytes;
};

/// What the save game room shows of a slot
struct SlotSummary
{
	uint32_t slot {0};
	/// The slot's SaveGame.inf, when it is there and whole
	std::optional<gamesave::SaveGameInfo> info;
	/// Whether the slot's world file is there
	bool hasWorld {false};
	/// The loads begun from the world file that did not end, the counter at its start
	uint32_t loadAttempts {0};
	/// Whether a load of the slot would begin: a world file that two loads did not end is refused
	bool loadable {false};
};

/// The saved games of the current profile: the slots' folders and files, the safe write of a save's files, and the
/// world file's count of the loads that did not end. With no profile kept (a run of the checks) there are no slots
/// and it reads and writes nothing
class SaveGameSystemInterface
{
public:
	virtual ~SaveGameSystemInterface() = default;

	/// The folder of the slot's world file for the land, from the game's folder; none with no profile kept
	[[nodiscard]] virtual std::optional<std::filesystem::path> SlotFolder(uint32_t slot, uint32_t land) const = 0;
	/// The slot's world file for the land, as a load reads it; none with no profile kept
	[[nodiscard]] virtual std::optional<std::filesystem::path> WorldFile(uint32_t slot, uint32_t land) const = 0;
	/// What the save game room shows of the slot
	[[nodiscard]] virtual SlotSummary Summary(uint32_t slot, uint32_t land) const = 0;
	/// Whether the autosave on exit can be loaded at the start: its world file is there and opens. Its counter and
	/// its revision are not looked at
	[[nodiscard]] virtual bool IsAutoSaveValid() const = 0;
	/// Writes a save's files as the game does: each to a temporary file first; when every one is written, each old file
	/// is removed and its temporary file takes its name. A file that fails keeps every old file
	virtual SaveWrite WriteFiles(std::span<const SaveFile> files) = 0;
	/// The start of a load of a world file: false when it does not open, or when two loads of it did not end (a file
	/// too short for its counter counts as many), else its counter goes up by one in the file
	virtual bool BeginLoad(const std::filesystem::path& worldFile) = 0;
	/// The end of a load that did not fail: the world file's counter goes back to 0
	virtual void EndLoad(const std::filesystem::path& worldFile) = 0;
};

} // namespace openblack::ecs::systems
