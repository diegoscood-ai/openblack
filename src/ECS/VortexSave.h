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
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

/// The land change of the vortexes (docs/wiki/world/vortex.md#the-land-script-round-trip-of-vortextxt). An In writes what it
/// swallows to "vortex.txt" in the save folder as land-script lines (each class saves itself); the Out of the next land reads
/// them back one per even turn and runs each through the land-script interpreter, which creates the object.
namespace openblack::ecs::vortex_save
{
/// The reader (read mode): the file's lines and how many were read, and whether the file's last line ends in a new line.
/// A last line with none after it still runs, but it ends the file at once: what it makes is not handed over
struct Reader
{
	std::vector<std::string> lines;
	size_t next {0};
	uint32_t reads {0};
	bool lastLineEnded {true};
};

/// The writer (write mode) of a way out of a land: the file, emptied as it is opened, how many lines went in, and what
/// has been written already (each thing once, as the original's save marks do)
struct Writer
{
	std::ofstream file;
	uint32_t lines {0};
	std::unordered_set<entt::entity> written;
};

/// The folder "vortex.txt" is kept in, under the game's own folder: openblack has no save games of its own, so every
/// land change through a vortex goes through this one folder (the original keeps it in the save folder of the current
/// save game)
inline constexpr std::string_view k_SaveFolderName = "SaveGame";
/// The save folder for a game folder: k_SaveFolderName inside it
[[nodiscard]] std::filesystem::path SaveFolderUnder(const std::filesystem::path& gamePath);

/// The save folder of the current game, set as each land starts (Game::LoadMap); none before the first land, when no
/// file is read: the Out's reader is empty, its first even turn makes nothing, then come its 30 villagers
void SetSaveFolder(std::optional<std::filesystem::path> folder);
[[nodiscard]] std::optional<std::filesystem::path> SaveFolder();
/// Whether an In may write the file, set with the folder: never in a run of the checks (EngineConfig::checkRun), which
/// read a game folder nothing may be written to, so such a run makes no attempt to write at all
void SetWritable(bool writable);
[[nodiscard]] bool Writable();

/// Opens "vortex.txt" in the save folder for reading. Always a reader: without a folder or a file it is empty, its
/// first ReadNext fails and the Out deletes it, as in the original
[[nodiscard]] std::shared_ptr<Reader> OpenReader();
/// The reader of a crossing file's text: a line ends at each new line (a carriage return before it is dropped); text
/// after the last new line is a last line with none after it, and nothing after it is no line at all
[[nodiscard]] Reader ReaderFrom(std::string_view text);

/// Opens "vortex.txt" in the save folder for writing, emptied, as a way out of a land is made. None, with no attempt
/// to write, without a folder or when the file may not be written (Writable); none, with a warning, when it cannot
/// be opened
[[nodiscard]] std::shared_ptr<Writer> OpenWriter();
/// One land-script line, ending in its new line, appended to the file at once
void Write(Writer& writer, std::string_view line);

/// Runs the next line as a land-script command at `at` (the Out's emission point): counts the read, offsets every
/// script position by `at`, no tribe override; returns the land script's last created object after the line (a line
/// that makes nothing hands over again what the line before made, as in the original), or nullopt at the end of the
/// file, which is also what a last line with no new line after it gives once it has run.
[[nodiscard]] std::optional<entt::entity> ReadNext(Reader& reader, const map_coords::MapCoords& at);
} // namespace openblack::ecs::vortex_save
