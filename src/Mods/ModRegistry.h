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

/// The mod library: every mod registers here once and gets a menu entry, a line in mods.cfg and a --mod switch.
///
/// mods.cfg (next to the executable) holds "<mod> = on|off" and "<mod>.<option> = <choice>" lines. Data mods are the
/// folders of <executable>/Mods: each has a mod.cfg ("name = ...", "description = ...") and replacement files in the
/// game's layout (Data/..., Scripts/...). Enabled data mods replace the game's files; with two of them replacing the
/// same file, the folder that comes later alphabetically wins.
class ModRegistry
{
public:
	ModRegistry();
	~ModRegistry();

	void Register(std::unique_ptr<Mod> mod);
	/// Registers a DataMod for each folder of modsDirectory
	void DiscoverDataMods(const std::filesystem::path& modsDirectory);

	/// Reads mods.cfg; later saves go to the same file
	void LoadSettings(const std::filesystem::path& settingsPath);
	void SaveSettings() const;

	/// Applies "<mod>", "<mod>=on|off" or "<mod>.<option>=<choice>" (the --mod switch). For this session only: the
	/// menu saves what it changes, the command line does not.
	/// @return an error message, empty on success
	std::string ApplyArgument(std::string_view argument);

	/// Applies every mod (start-up)
	void ApplyAll();

	/// Menu changes: apply the mod and save mods.cfg
	void SetEnabled(Mod& mod, bool enabled);
	void SetOption(Mod& mod, size_t optionIndex, size_t choice);

	/// Makes the enabled data mods replace the game's files
	void MountDataMods(filesystem::FileSystemInterface& fileSystem) const;

	[[nodiscard]] const std::vector<std::unique_ptr<Mod>>& GetMods() const noexcept { return _mods; }
	[[nodiscard]] Mod* Find(std::string_view id) const noexcept;
	[[nodiscard]] const std::filesystem::path& GetModsDirectory() const noexcept { return _modsDirectory; }

private:
	struct SavedState;

	std::vector<std::unique_ptr<Mod>> _mods;
	std::filesystem::path _settingsPath;
	std::filesystem::path _modsDirectory;
	/// What mods.cfg says, so that command line changes are not saved along with a menu change
	std::unique_ptr<SavedState> _saved;
};

} // namespace openblack::mods
