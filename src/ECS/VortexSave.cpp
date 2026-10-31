/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexSave.h"

#include <iterator>
#include <system_error>

#include <spdlog/spdlog.h>

#include "ECS/Systems/ScriptStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct VortexSaveState
{
	std::optional<std::filesystem::path> saveFolder {};
	bool writable {false};
};

VortexSaveState& VortexSaveData()
{
	return openblack::Locator::scriptState::value().Get<VortexSaveState>();
}
} // namespace

std::filesystem::path vortex_save::SaveFolderUnder(const std::filesystem::path& gamePath)
{
	return gamePath / k_SaveFolderName;
}

void vortex_save::SetSaveFolder(std::optional<std::filesystem::path> folder)
{
	VortexSaveData().saveFolder = std::move(folder);
}

std::optional<std::filesystem::path> vortex_save::SaveFolder()
{
	return VortexSaveData().saveFolder;
}

void vortex_save::SetWritable(bool writable)
{
	VortexSaveData().writable = writable;
}

bool vortex_save::Writable()
{
	return VortexSaveData().writable;
}

std::shared_ptr<vortex_save::Reader> vortex_save::OpenReader()
{
	auto& state = VortexSaveData();
	// the Out always has a reader: without a file its first line fails, the Out deletes it that even turn and makes
	// no villager then; the next even turn starts the 30. Before the first land there is no folder
	auto reader = std::make_shared<Reader>();
	if (!state.saveFolder.has_value())
	{
		return reader;
	}
	// "vortex.txt" in the save folder, the player's own file, read whole and cut into its lines
	const auto bytes = Locator::filesystem::has_value()
	                       ? Locator::filesystem::value().ReadUserFile(*state.saveFolder / "vortex.txt")
	                       : std::nullopt;
	if (!bytes.has_value())
	{
		return reader;
	}
	*reader = ReaderFrom(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
	return reader;
}

vortex_save::Reader vortex_save::ReaderFrom(std::string_view text)
{
	// The original reads the file a character at a time up to each new line: a line with no new line after it is still
	// read, but the reader then says the file has ended; an empty end of the file is no line. (pending) the
	// interpreter's own handling of a carriage return and of comments: here one text line is one command
	Reader reader;
	while (!text.empty())
	{
		const auto end = text.find('\n');
		auto line = text.substr(0, end);
		if (!line.empty() && line.back() == '\r')
		{
			line.remove_suffix(1);
		}
		reader.lines.emplace_back(line);
		if (end == std::string_view::npos)
		{
			reader.lastLineEnded = false;
			break;
		}
		text.remove_prefix(end + 1);
	}
	return reader;
}

std::shared_ptr<vortex_save::Writer> vortex_save::OpenWriter()
{
	const auto& state = VortexSaveData();
	if (!state.saveFolder.has_value() || !state.writable)
	{
		return nullptr;
	}
	std::error_code error;
	std::filesystem::create_directories(*state.saveFolder, error);
	auto writer = std::make_shared<Writer>();
	// emptied by every new way out of a land
	writer->file.open(*state.saveFolder / "vortex.txt", std::ios::out | std::ios::trunc | std::ios::binary);
	if (!writer->file.is_open())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "VortexSave: could not write {}",
		                   (*state.saveFolder / "vortex.txt").generic_string());
		return nullptr;
	}
	return writer;
}

void vortex_save::Write(Writer& writer, std::string_view line)
{
	writer.file.write(line.data(), static_cast<std::streamsize>(line.size()));
	writer.file.flush();
	writer.lines += 1;
}

std::optional<entt::entity> vortex_save::ReadNext(Reader& reader, const map_coords::MapCoords& at)
{
	reader.reads += 1;
	if (reader.next >= reader.lines.size())
	{
		return std::nullopt; // the end of the file: no object
	}
	const auto& line = reader.lines[reader.next++];
	// a last line with no new line after it runs, then the file has ended: nothing is handed over
	const bool endsTheFile = reader.next == reader.lines.size() && !reader.lastLineEnded;
	// the position offset and no tribe override. The original also sets the scripts' player to the local player, but
	// no command of the game ever reads it, so it is not kept
	lhscriptx::Script::SetPositionOffset(at);
	lhscriptx::Script::SetTribeOverride(std::nullopt);
	// The last created object is not cleared before the line, as in the original: a line that makes nothing (a pot of
	// nothing, a thing on top of another, a miracle globe, which never sets it) hands over again what the line before
	// made
	// (openblack) the original leaves the position offset pointing at the caller's coordinates until the next loader
	// sets it; here it is cleared after the line, even when the interpreter throws, so that a console line or another
	// script does not get the vortex's offset
	struct OffsetGuard
	{
		~OffsetGuard() { lhscriptx::Script::SetPositionOffset(std::nullopt); }
	} guard;
	try
	{
		lhscriptx::Script script;
		script.Load(line);
	}
	catch (const std::exception& e)
	{
		// (openblack) a line the interpreter cannot take is skipped, as the map loader does
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "VortexSave: line {} not read ({})", reader.next, e.what());
		if (endsTheFile)
		{
			return std::nullopt;
		}
		return entt::entity {entt::null};
	}
	if (endsTheFile)
	{
		return std::nullopt;
	}
	return lhscriptx::Script::LastCreated();
}
