/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Mod.h"

namespace openblack::filesystem
{
class FileSystemInterface;
}

namespace openblack::mods
{

/// The mod library: every mod registers here once and gets a menu entry, a folder with its settings and a --mod switch.
///
/// Everything lives in <executable>/Mods, one folder per mod: Mods/<built-in mod id>/ for the built-in mods (their
/// files, e.g. Mods/world.foliage/foliage.cfg) and Mods/<name>/ for the data mods. Each folder has a settings.cfg with
/// "enabled = on|off" and "<option> = <choice>" lines. A data mod folder also has a mod.cfg ("name = ...",
/// "description = ...") and replacement files in the game's layout (Data/..., Scripts/...). Enabled data mods replace
/// the game's files; with two of them replacing the same file, the folder that comes later alphabetically wins.
/// A folder whose mod.cfg has "module_of = <mod id>" is a module of that mod instead: no replacement files, just more
/// files of the parent's own kind (GetModuleDirectories), shown under the parent in the menu.
class ModRegistry
{
public:
	ModRegistry();
	~ModRegistry();

	void Register(std::unique_ptr<Mod> mod);
	/// Registers a DataMod for each folder of modsDirectory
	void DiscoverDataMods(const std::filesystem::path& modsDirectory);

	/// Reads every mod's settings.cfg (call after DiscoverDataMods) and writes the missing ones, so every mod has its
	/// folder
	void LoadSettings();
	/// The old single file ("<mod> = on|off", "<mod>.<option> = <choice>"): split into the mods' settings.cfg files,
	/// then deleted
	void ImportLegacySettings(const std::filesystem::path& legacyPath);
	void SaveSettings(const Mod& mod) const;
	/// Folder of a mod: Mods/<id>/ for a built-in mod, its own folder for a data mod
	[[nodiscard]] std::filesystem::path GetModDirectory(const Mod& mod) const;

	/// Enabled, and so is the mod it is a module of (if any)
	[[nodiscard]] bool IsActive(const Mod& mod) const;
	/// Folders of the active modules of a mod: Mods/<name>/ folders whose mod.cfg says "module_of = <parentId>", in
	/// alphabetical order. The parent reads its own kind of files there (e.g. world.foliage a foliage.cfg).
	[[nodiscard]] std::vector<std::filesystem::path> GetModuleDirectories(std::string_view parentId) const;
	/// An active module: its folder and the choices of the options its mod.cfg declares ("option.<id> = ...")
	struct Module
	{
		std::filesystem::path directory;
		std::map<std::string, std::string> options;
	};
	/// The active modules of a mod, as GetModuleDirectories, with their options
	[[nodiscard]] std::vector<Module> GetModules(std::string_view parentId) const;

	/// Applies "<mod>", "<mod>=on|off" or "<mod>.<option>=<choice>" (the --mod switch). For this session only: the
	/// menu saves what it changes, the command line does not.
	/// @return an error message, empty on success
	std::string ApplyArgument(std::string_view argument);

	/// Applies every mod (start-up)
	void ApplyAll();

	/// Menu changes: apply the mod and save its settings.cfg
	void SetEnabled(Mod& mod, bool enabled);
	void SetOption(Mod& mod, size_t optionIndex, size_t choice);

	/// Makes the enabled data mods replace the game's files
	void MountDataMods(filesystem::FileSystemInterface& fileSystem) const;

	[[nodiscard]] const std::vector<std::unique_ptr<Mod>>& GetMods() const noexcept { return _mods; }
	[[nodiscard]] Mod* Find(std::string_view id) const noexcept;
	[[nodiscard]] const std::filesystem::path& GetModsDirectory() const noexcept { return _modsDirectory; }
	/// Files of a built-in mod: Mods/<its id>/ (never listed as a data mod)
	[[nodiscard]] std::filesystem::path GetModFilesDirectory(std::string_view id) const { return _modsDirectory / id; }

private:
	struct SavedState;

	std::vector<std::unique_ptr<Mod>> _mods;
	std::filesystem::path _modsDirectory;
	/// What the settings files say, so that command line changes are not saved along with a menu change
	std::unique_ptr<SavedState> _saved;
};

} // namespace openblack::mods
