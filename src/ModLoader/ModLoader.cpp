/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModLoader.h"

#include <charconv>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ModManifest.h"
#include "generated/ModSdk/ModSdkVersion.h"

namespace
{
constexpr std::string_view k_Whitespace = " \t\r\n";

/// The integer a version.txt holds, with whitespace around it at most; nothing when it holds anything else
std::optional<int> ParseVersion(std::string_view text)
{
	const auto first = text.find_first_not_of(k_Whitespace);
	if (first == std::string_view::npos)
	{
		return std::nullopt;
	}
	text = text.substr(first, text.find_last_not_of(k_Whitespace) - first + 1);
	int version = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), version);
	if (error != std::errc {} || end != text.data() + text.size())
	{
		return std::nullopt;
	}
	return version;
}

ModLoaderResult Check(const char* modsFolder, std::string& line)
{
	if (modsFolder == nullptr)
	{
		line = "no mods folder given: no mods read";
		return ModLoaderResult::NoVersionFile;
	}
	const auto path = std::filesystem::path(modsFolder) / "version.txt";
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		line = "version.txt missing: no mods read";
		return ModLoaderResult::NoVersionFile;
	}
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		line = "version.txt cannot be read: no mods read";
		return ModLoaderResult::UnreadableVersion;
	}
	const std::string text {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	const auto version = ParseVersion(text);
	if (!version)
	{
		line = "version.txt does not hold a version number: no mods read";
		return ModLoaderResult::UnreadableVersion;
	}
	if (*version != k_ModLoaderVersion)
	{
		line = "version.txt is version " + std::to_string(*version) + ", the loader is version " +
		       std::to_string(k_ModLoaderVersion) + ": no mods read";
		return ModLoaderResult::VersionMismatch;
	}
	// a switched-on mod's files are found by the game's file system (ModLoader_EnabledModFolders)
	line = "version.txt matches version " + std::to_string(k_ModLoaderVersion);
	return ModLoaderResult::Matched;
}

/// Whether the mods folder is read: its version.txt matches
bool IsRead(const char* modsFolder)
{
	std::string line;
	return Check(modsFolder, line) == ModLoaderResult::Matched;
}

/// A file's whole text, empty when it is missing or cannot be read
std::string ReadText(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return {};
	}
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// A mod's manifest, when the folder holds one of the format read
std::optional<mod_loader::Manifest> ManifestOf(const std::filesystem::path& modFolder)
{
	const auto path = modFolder / mod_loader::k_ManifestFile;
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		return std::nullopt;
	}
	return mod_loader::ParseManifest(ReadText(path));
}

/// The mods of the folder, by their folder names' order
std::vector<std::pair<std::string, mod_loader::Manifest>> ModsOf(const std::filesystem::path& folder)
{
	std::vector<std::pair<std::string, mod_loader::Manifest>> mods;
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(folder, error))
	{
		std::error_code isFolder;
		if (!entry.is_directory(isFolder))
		{
			continue;
		}
		auto name = entry.path().filename().string();
		if (!mod_loader::IsModFolderName(name))
		{
			continue;
		}
		if (auto manifest = ManifestOf(entry.path()); manifest.has_value())
		{
			mods.emplace_back(std::move(name), std::move(*manifest));
		}
	}
	std::ranges::sort(mods, {}, &std::pair<std::string, mod_loader::Manifest>::first);
	return mods;
}
} // namespace

const char* ModLoader_SdkVersion() noexcept
{
	return k_ModSdkVersion;
}

int ModLoader_Version() noexcept
{
	return k_ModLoaderVersion;
}

int ModLoader_LoadMods(const char* modsFolder, ModLoaderLogFn log, void* context) noexcept
{
	std::string line;
	auto result = ModLoaderResult::UnreadableVersion;
	try
	{
		result = Check(modsFolder, line);
	}
	catch (const std::exception& e)
	{
		line = std::string("version.txt cannot be read (") + e.what() + "): no mods read";
	}
	if (log != nullptr)
	{
		log(line.c_str(), context);
	}
	return static_cast<int>(result);
}

int ModLoader_ListMods(const char* modsFolder, ModLoaderModFn each, void* context) noexcept
{
	try
	{
		if (!IsRead(modsFolder))
		{
			return -1;
		}
		const std::filesystem::path folder(modsFolder);
		const auto enabled = mod_loader::ParseEnabled(ReadText(folder / mod_loader::k_EnabledFile));
		const auto mods = ModsOf(folder);
		if (each != nullptr)
		{
			for (const auto& [name, manifest] : mods)
			{
				const ModLoaderModInfo info {.folder = name.c_str(),
				                             .name = manifest.name.c_str(),
				                             .version = manifest.version.c_str(),
				                             .author = manifest.author.c_str(),
				                             .enabled = enabled.contains(name) ? 1 : 0};
				each(&info, context);
			}
		}
		return static_cast<int>(mods.size());
	}
	catch (const std::exception&)
	{
		return -1;
	}
}

int ModLoader_SetModEnabled(const char* modsFolder, const char* modFolder, int enabled) noexcept
{
	try
	{
		if (modFolder == nullptr || !mod_loader::IsModFolderName(modFolder) || !IsRead(modsFolder))
		{
			return static_cast<int>(ModLoaderSetResult::NoSuchMod);
		}
		const std::filesystem::path folder(modsFolder);
		if (!ManifestOf(folder / modFolder).has_value())
		{
			return static_cast<int>(ModLoaderSetResult::NoSuchMod);
		}
		const auto path = folder / mod_loader::k_EnabledFile;
		auto list = mod_loader::ParseEnabled(ReadText(path));
		const bool changed = enabled != 0 ? list.emplace(modFolder).second : list.erase(modFolder) > 0;
		if (!changed)
		{
			return static_cast<int>(ModLoaderSetResult::Kept);
		}
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file << mod_loader::WriteEnabled(list);
		file.close();
		return static_cast<int>(file ? ModLoaderSetResult::Kept : ModLoaderSetResult::CannotWrite);
	}
	catch (const std::exception&)
	{
		return static_cast<int>(ModLoaderSetResult::CannotWrite);
	}
}

int ModLoader_EnabledModFolders(const char* modsFolder, ModLoaderFolderFn each, void* context) noexcept
{
	try
	{
		if (!IsRead(modsFolder))
		{
			return -1;
		}
		const std::filesystem::path folder(modsFolder);
		int given = 0;
		for (const auto& name : mod_loader::ParseEnabled(ReadText(folder / mod_loader::k_EnabledFile)))
		{
			std::error_code error;
			if (!std::filesystem::is_directory(folder / name, error) || !ManifestOf(folder / name).has_value())
			{
				continue;
			}
			if (each != nullptr)
			{
				each(name.c_str(), context);
			}
			++given;
		}
		return given;
	}
	catch (const std::exception&)
	{
		return -1;
	}
}
