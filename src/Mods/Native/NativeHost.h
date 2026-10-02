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
#include <cstdint>

#include <string_view>

namespace openblack::mods
{
class ModRegistry;
}

/// Native mods: each active mod with mod.json "entry": {"native": ...} is a library (DLL / .so) loaded with
/// SDL_LoadObject and driven through the C API of components/modsdk/include/openblack/mod_api.h (the translation of
/// Mods/Api.h). The library's API major version is checked before any of its code runs.
namespace openblack::mods::native
{

/// Loads the libraries of the active mods, in load order (Game::Run, after the Lua scripts)
void Start(ModRegistry& registry);
/// ob_mod_unload of each, in reverse order, then every library closes
void Stop();

void OnTurn(uint32_t turn);
void OnFrame(float seconds);
void OnLandLoaded(std::string_view land);

/// How many native mods are loaded
[[nodiscard]] size_t Loaded();

} // namespace openblack::mods::native
