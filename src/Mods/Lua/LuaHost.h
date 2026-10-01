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

#include <string>
#include <string_view>

namespace openblack::mods
{
class ModRegistry;
}

/// Lua mods (Lua 5.4 through sol2): each active mod with mod.json "entry": {"lua": "scripts/main.lua"} runs its script
/// in its own sandbox (no io, os, package or debug; its own `require` that loads scripts/<name>.lua of the mod). The
/// script gets the table `ob` (the translation of Mods/Api.h, documented in mod-library.md, "Lua"): ob.log, ob.mod,
/// ob.switch, ob.on (events), ob.interfaces, ob.enums, ob.game. A script error is logged and never stops the game;
/// after 10 errors the mod's event functions are dropped.
namespace openblack::mods::lua
{

/// Runs the scripts of the active mods, in load order (Game::Run, before the first land loads)
void Start(ModRegistry& registry);
/// Every event function goes and the Lua state closes (Game shutdown)
void Stop();

void OnTurn(uint32_t turn);
void OnFrame(float seconds);
void OnLandLoaded(std::string_view land);

/// How many mod scripts are running
[[nodiscard]] size_t Running();

/// Runs a chunk of Lua in a mod's sandbox (tests); returns the error, empty on success
std::string RunForTest(std::string_view modId, std::string_view code);

} // namespace openblack::mods::lua
