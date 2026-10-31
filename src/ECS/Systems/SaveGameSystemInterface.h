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
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <SaveGameInfo.h>

#include "ECS/SavedWorld/WorldWriter.h"

namespace openblack::gamesave
{
struct WorldRead;
}

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

/// What a save of a slot writes, made from the game first
struct SaveContents
{
	/// The slot's SaveGame.inf; none writes none (the world's save alone)
	std::optional<gamesave::SaveGameInfo> info;
	/// Whether the slot's pictures are written as a save of a slot writes them: the profile's map pictures copied
	/// from its Current folder, and the temporary picture put into the slot's cells of the large and small pages
	bool pictures {false};
	/// The world file whole, as the game's save leaves it (its first word, the loads' counter, 0)
	std::vector<uint8_t> world;
	/// The creatures' mind files, each by its creature's place in the game's list of creatures
	std::vector<std::pair<uint32_t, std::vector<uint8_t>>> minds;
	/// Writes the help record into the profile's own file, as the game's save does just before the script machine's file;
	/// none writes none
	std::function<void()> helpRecord;
	/// Writes the script machine's state into the file given (from the game's folder, as the service's paths are);
	/// false when it could not
	std::function<bool(const std::filesystem::path& file)> scriptState;
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
	/// Saves the game into the slot as the game's save of a slot does, in its order: the slot's folders made; with
	/// `pictures` the map pictures copied and the temporary picture put into the slot's cells; the info file; the world
	/// file into its temporary file (when that does not open nothing more is written), the creatures' mind files, the
	/// script machine's file into its temporary file; then, when nothing failed, the old world file removed and the
	/// temporary one given its name, then the same for the script machine's file. A failure keeps the old world files
	virtual SaveWrite Save(uint32_t slot, uint32_t land, const SaveContents& contents) = 0;
	/// The start of a load of a world file: false when it does not open, when two loads of it did not end (a file too
	/// short for its counter counts as many) or when its counter cannot be written (a read-only file), else its counter
	/// goes up by one in the file
	virtual bool BeginLoad(const std::filesystem::path& worldFile) = 0;
	/// The end of a load that did not fail: the world file's counter goes back to 0
	virtual void EndLoad(const std::filesystem::path& worldFile) = 0;

	/// Keeps the world file a load read and the objects it made of it, by their numbers in the file, for the next
	/// save: what openblack does not write yet is written from them
	virtual void KeepLoadedWorld(std::shared_ptr<const gamesave::WorldRead> world,
	                             ecs::saved_world_write::LoadedObjects objects) = 0;
	/// A land that was not loaded keeps nothing of the last load
	virtual void ForgetLoadedWorld() = 0;
	/// The world file the last load read, none after a land that was not loaded
	[[nodiscard]] virtual const gamesave::WorldRead* LoadedWorld() const = 0;
	[[nodiscard]] virtual const ecs::saved_world_write::LoadedObjects& LoadedWorldObjects() const = 0;

	/// The turn of the last autosave: 0 as the game starts, the turn of each autosave, and the loaded turn after a load.
	/// Another land keeps it
	[[nodiscard]] virtual uint32_t LastAutoSaveTurn() const = 0;
	virtual void SetLastAutoSaveTurn(uint32_t turn) = 0;
	/// The turn the last quick save was asked for, as the game keeps it and its save writes it; 0 as the game starts
	[[nodiscard]] virtual uint32_t LastQuickSaveTurn() const = 0;
	virtual void SetLastQuickSaveTurn(uint32_t turn) = 0;
};

} // namespace openblack::ecs::systems
