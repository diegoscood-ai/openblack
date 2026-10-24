/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DefaultFileSystem.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <memory>
#include <system_error>

#include <spdlog/spdlog.h>

#include "FileStream.h"
#include "ModSearchRoots.h"
#include "fmt/format.h"

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
// clang-format on
#endif

using namespace openblack::filesystem;

// todo: exceptions need to be replaced with real exceptions

std::filesystem::path DefaultFileSystem::FindPath(const std::filesystem::path& path) const
{
	return FindPathIn(path, true);
}

std::filesystem::path DefaultFileSystem::FindPathIn(const std::filesystem::path& path, bool withMods) const
{
	if (path.empty())
	{
		throw std::invalid_argument("empty path");
	}

	// a switched-on data mod's file of the same path below the game's folder comes first
	if (withMods && !_modRoots.empty())
	{
		if (const auto relative = mod_roots::GameRelative(path, _gamePath); relative.has_value())
		{
			const auto exists = [](const std::filesystem::path& candidate) {
				std::error_code error;
				return std::filesystem::is_regular_file(candidate, error);
			};
			if (auto found = mod_roots::FirstInRoots(_modRoots, *relative, exists); found.has_value())
			{
				if (const auto logger = spdlog::get("game"); logger != nullptr)
				{
					SPDLOG_LOGGER_INFO(logger, "{} from a mod: {}", relative->generic_string(), found->generic_string());
				}
				return *found;
			}
		}
	}

	// try absolute first
	if (path.is_absolute())
	{
		if (std::filesystem::exists(path))
		{
			return path;
		}
	}
	else
	{
		// try relative to current directory
		if (std::filesystem::exists(path))
		{
			return path;
		}

		// try relative to game directory
		if (std::filesystem::exists(_gamePath / path))
		{
			return _gamePath / path;
		}

		// try relative to additional paths
		for (const auto& p : _additionalPaths)
		{
			if (std::filesystem::exists(p / path))
			{
				return p / path;
			}
		}
	}

	throw std::runtime_error("File " + path.string() + " not found");
}

bool DefaultFileSystem::IsPathValid(const std::filesystem::path& path)
{
	if (path.empty())
	{
		return false;
	}

	if (!std::filesystem::exists(path))
	{
		return false;
	}

	return true;
}

std::unique_ptr<Stream> DefaultFileSystem::Open(const std::filesystem::path& path, Stream::Mode mode)
{
	// a file that is written may not be there yet: then it is made where the path says, relative to the game folder
	if (mode != Stream::Mode::Read)
	{
		std::filesystem::path own;
		try
		{
			own = path.empty() ? path : FindPathIn(path, false);
		}
		catch (const std::exception&)
		{
			own = path.is_absolute() ? path : _gamePath / path;
		}
		return std::make_unique<FileStream>(own, mode);
	}
	return std::make_unique<FileStream>(FindPath(path), mode);
}

bool DefaultFileSystem::Exists(const std::filesystem::path& path) const
{
	try
	{
		[[maybe_unused]] auto realPath = FindPath(path);
		return true;
	}
	catch (std::exception&)
	{
		return false;
	}
}

std::vector<uint8_t> DefaultFileSystem::ReadAll(const std::filesystem::path& path)
{
	auto file = Open(path, Stream::Mode::Read);
	const std::size_t size = file->Size();

	std::vector<uint8_t> data(size);
	file->Read(data.data(), size);

	return data;
}

std::optional<std::vector<uint8_t>> DefaultFileSystem::ReadUserFile(const std::filesystem::path& path)
{
	// found as any file is, but never in a data mod's folders
	std::filesystem::path found;
	try
	{
		found = FindPathIn(path, false);
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
	FileStream file(found, Stream::Mode::Read);
	std::vector<uint8_t> data(file.Size());
	file.Read(data.data(), data.size());
	return data;
}

void DefaultFileSystem::Iterate(const std::filesystem::path& path, bool recursive,
                                const std::function<void(const std::filesystem::path&)>& function) const
{
	const auto fixedPath = FindPath(path);
	if (recursive)
	{
		for (const auto& f : std::filesystem::recursive_directory_iterator {fixedPath})
		{
			function(f);
		}
	}
	else
	{
		for (const auto& f : std::filesystem::directory_iterator {fixedPath})
		{
			function(f);
		}
	}
}

bool DefaultFileSystem::MakeFolder(const std::filesystem::path& path)
{
	// a folder given from the game's folder is made there, as a file written is
	const auto folder = path.is_absolute() ? path : _gamePath / path;
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	return !error && std::filesystem::is_directory(folder, error);
}

bool DefaultFileSystem::Rename(const std::filesystem::path& from, const std::filesystem::path& to)
{
	// both given from the game's folder, as a file written is
	const auto own = [this](const std::filesystem::path& path) { return path.is_absolute() ? path : _gamePath / path; };
	std::error_code error;
	std::filesystem::rename(own(from), own(to), error);
	return !error;
}

bool DefaultFileSystem::Remove(const std::filesystem::path& path)
{
	const auto own = path.is_absolute() ? path : _gamePath / path;
	std::error_code error;
	return std::filesystem::remove(own, error) && !error;
}

void DefaultFileSystem::SetGamePath(const std::filesystem::path& path)
{
	_gamePath = path;

#if defined(unix) || defined(__unix__) || defined(__unix)
	if (_gamePath.string().size() >= 2 && _gamePath.string().c_str()[0] == '~' && _gamePath.string().c_str()[1] == '/')
	{
		_gamePath = std::getenv("HOME") + _gamePath.string().substr(1);
	}
#endif

	if (_gamePath.empty())
	{
#ifdef _WIN32
		DWORD dataLen = 0;
		LSTATUS status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir",
		                             RRF_RT_REG_SZ, nullptr, nullptr, &dataLen);
		if (status == ERROR_SUCCESS)
		{
			std::vector<char> data(dataLen);
			status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir", RRF_RT_REG_SZ,
			                     nullptr, data.data(), &dataLen);

			_gamePath = std::filesystem::path(data.data());
		}
		else
		{
			throw std::runtime_error(fmt::format("Failed to find the GameDir registry value, game not installed."));
		}
#endif // _WIN32
	}

	if (!_gamePath.empty() && !Exists(_gamePath))
	{
		throw std::runtime_error(fmt::format("GamePath does not exist: '{}'", _gamePath.generic_string()));
	}
}
std::unique_ptr<std::istream> DefaultFileSystem::GetData(const std::filesystem::path& path)
{
	return std::make_unique<std::ifstream>(FindPath(path), std::ios::binary);
}
