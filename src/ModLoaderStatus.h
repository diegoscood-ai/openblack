/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace openblack
{

/// A mod of the mods folder, as the mod loader lists it
struct ModLoaderMod
{
	std::string folder; ///< its folder's name, which names it to the library
	std::string name;
	std::string version;
	std::string author;
	bool enabled {false};
};

/// What the mod loader library (src/ModLoader) reported at start-up. The service (Locator::modLoader) exists only when
/// the library was found and loaded; the debug menu shows it then, and the game's menu a Mods page when the mods
/// folder is read
struct ModLoaderStatus
{
	int version {0};    ///< the library's version
	std::string result; ///< its one line about the mods folder
	/// Whether the mods folder is read (its version.txt matches); the Mods page shows only then
	bool folderRead {false};
	/// The mods the library lists, by their folder names; none when the folder is not read
	std::vector<ModLoaderMod> mods;
	/// The switched-on mods' folders, in the library's order, that the file system looks in first (a mod's file
	/// replaces the game's of the same path); taken at start-up, so a switch counts from the next start
	std::vector<std::filesystem::path> modRoots;
	/// Keeps a mod's switch through the library, true when it did; it holds the library loaded while the service lives
	std::function<bool(const std::string& folder, bool on)> setEnabled;

	/// Switches the mod at `index` on or off; the list follows only when the library kept it
	bool SetEnabled(size_t index, bool on)
	{
		if (index >= mods.size() || !setEnabled || !setEnabled(mods[index].folder, on))
		{
			return false;
		}
		mods[index].enabled = on;
		return true;
	}
};

} // namespace openblack
