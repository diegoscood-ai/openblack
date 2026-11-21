/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::events
{
/// The debug window's request to restart the scripts as a new game does, without loading a land: their state goes back
/// to its start and their whole program starts again (Game::ResetScripts, Game::RestartScriptProgram).
/// See docs/wiki/scripts/land-change-and-saving.md#what-a-land-change-keeps
struct ScriptsRestartRequested
{
};
} // namespace openblack::ecs::events
