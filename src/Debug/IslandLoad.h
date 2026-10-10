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

namespace openblack::debug::gui
{

/// The debug bar's "Load Island": with the creature taken along, as the game's own land change, which saves the
/// player's creature to the profile before the new land's script loads it back; else only the new land is loaded.
/// `Game` is the game or a test's fake, with LoadMap and ChangeLand as the game has them
template <typename Game>
bool LoadIsland(Game& game, const std::filesystem::path& scriptPath, bool takeCreature)
{
	return takeCreature ? game.ChangeLand(scriptPath) : game.LoadMap(scriptPath);
}

} // namespace openblack::debug::gui
