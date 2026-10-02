/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// Restarting openblack from the Mods window (a mod that takes effect after a restart): the window asks for it, the
/// game loop ends as with Quit, and main() starts openblack again with the same command line once everything is shut
/// down.
namespace openblack::mods::restart
{

/// The Mods window's "Restart openblack now"
void Request();
[[nodiscard]] bool Requested();
/// Starts a new openblack with this process's command line (main(), after the game is destroyed). False if it could
/// not
bool Relaunch(int argc, char** argv);

} // namespace openblack::mods::restart
