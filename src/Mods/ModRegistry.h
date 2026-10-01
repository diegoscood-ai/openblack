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

/// The mod library (docs/bw1-notes/mod-library.md). Everything lives in <executable>/Mods:
///
///   Mods/<mod id>/mod.json            a loose mod: its manifest, icon.png, settings.cfg and files
///   Mods/<pack id>/modpack.json       a modpack, with its mods inside: Mods/<pack id>/<mod id>/mod.json
///   Mods/load_order.cfg               the user's load order (one id per line; dependencies always come first)
///
/// The mods that come with openblack have their mod.json built in (assets/mods/<id>/mod.json); a mod.json on disk with
/// the same id replaces it. Older folders are still read: a mod.cfg with "module_of = <mod id>" is a module of that
/// mod, any other folder a data mod ("data.<folder>": replacement files in the game's layout).
///
/// Each mod's settings.cfg has "enabled = on|off" and "<option> = <choice>" lines. A mod is active when it is enabled,
/// nothing blocks it (a missing or switched-off dependency, an incompatible mod, an API it was not written for) and,
/// for a module, its parent is active. Active mods set engine switches (Switches.h); with two setting the same one,
/// the later in load order wins.
class ModRegistry
{
public:
	ModRegistry();
	~ModRegistry();

	/// Adds a mod (tests, and the ones Discover makes)
	void Register(std::unique_ptr<Mod> mod);
	/// Registers the built-in mods (their manifests compiled into openblack), then every folder of modsDirectory
	void Discover(const std::filesystem::path& modsDirectory);

	/// Reads every mod's settings.cfg and Mods/load_order.cfg (call after Discover) and writes the missing settings
	/// files, so every mod has its folder
	void LoadSettings();
	/// The old single file ("<mod> = on|off", "<mod>.<option> = <choice>"): split into the mods' settings.cfg files,
	/// then deleted
	void ImportLegacySettings(const std::filesystem::path& legacyPath);
	void SaveSettings(const Mod& mod) const;
	/// Folder of a mod (where its files and settings.cfg are)
	[[nodiscard]] std::filesystem::path GetModDirectory(const Mod& mod) const;

	/// Enabled, not blocked, and so is the mod it is a module of (if any)
	[[nodiscard]] bool IsActive(const Mod& mod) const;
	/// Folders of the active modules of a mod, in load order. The parent reads its own kind of files there (e.g.
	/// world.foliage a foliage.cfg).
	[[nodiscard]] std::vector<std::filesystem::path> GetModuleDirectories(std::string_view parentId) const;
	/// An active module: its folder and the choices of its options
	struct Module
	{
		std::filesystem::path directory;
		std::map<std::string, std::string> options;
	};
	/// The active modules of a mod, as GetModuleDirectories, with their options
	[[nodiscard]] std::vector<Module> GetModules(std::string_view parentId) const;

	/// Applies "<mod>", "<mod>=on|off" or "<mod>.<option>=<choice>" (the --mod switch). For this session only: the
	/// window saves what it changes, the command line does not.
	/// @return an error message, empty on success
	std::string ApplyArgument(std::string_view argument);

	/// Resolves dependencies, sets the engine switches and applies every mod (start-up, and after any change)
	void ApplyAll();

	/// Window changes: apply and save settings.cfg
	void SetEnabled(Mod& mod, bool enabled);
	void SetOption(Mod& mod, size_t optionIndex, size_t choice);

	/// A switch set by a mod's Lua script or native library while it runs (it counts only while the mod is active, in
	/// its place of the load order). Returns false if there is no such switch
	bool SetRuntimeSwitch(Mod& mod, std::string_view name, double value);
	/// Sets the engine switches again from the active mods (after a runtime switch changed)
	void RefreshSwitches();

	/// Modpacks
	[[nodiscard]] const std::vector<Modpack>& GetModpacks() const noexcept { return _packs; }
	[[nodiscard]] const Modpack* FindModpack(std::string_view id) const noexcept;
	/// Every mod of the pack is enabled
	[[nodiscard]] bool IsModpackEnabled(const Modpack& pack) const;
	/// Turns every mod of the pack on or off
	void SetModpackEnabled(const Modpack& pack, bool enabled);

	/// Load order: dependencies first, then the user's order (Mods/load_order.cfg), then by id
	[[nodiscard]] const std::vector<Mod*>& GetLoadOrder() const noexcept { return _order; }
	/// Moves a mod up (delta < 0) or down in the user's order, as far as its dependencies allow, and saves it
	void MoveInLoadOrder(const Mod& mod, int delta);

	/// Folders of Mods/ whose manifest could not be read: folder and what was wrong (the window shows them)
	struct Broken
	{
		std::filesystem::path folder;
		std::vector<std::string> errors;
	};
	[[nodiscard]] const std::vector<Broken>& GetBroken() const noexcept { return _broken; }

	/// Makes the active data mods replace the game's files, in load order (a later mod wins)
	void MountDataMods(filesystem::FileSystemInterface& fileSystem) const;

	[[nodiscard]] const std::vector<std::unique_ptr<Mod>>& GetMods() const noexcept { return _mods; }
	[[nodiscard]] Mod* Find(std::string_view id) const noexcept;
	[[nodiscard]] const std::filesystem::path& GetModsDirectory() const noexcept { return _modsDirectory; }
	/// Files of a mod: its folder, or Mods/<id>/ if there is no such mod
	[[nodiscard]] std::filesystem::path GetModFilesDirectory(std::string_view id) const;

private:
	struct SavedState;

	void DiscoverFolder(const std::filesystem::path& folder, std::string_view pack,
	                    std::vector<std::pair<std::filesystem::path, std::map<std::string, std::string>>>& modules);
	void Resolve();
	void SortLoadOrder();
	void ApplySwitches();
	void SaveLoadOrder() const;

	std::vector<std::unique_ptr<Mod>> _mods;
	std::vector<Modpack> _packs;
	std::vector<Broken> _broken;
	std::vector<Mod*> _order;
	std::vector<std::string> _userOrder; ///< Mods/load_order.cfg
	std::filesystem::path _modsDirectory;
	/// What the settings files say, so that command line changes are not saved along with a window change
	std::unique_ptr<SavedState> _saved;
};

} // namespace openblack::mods
