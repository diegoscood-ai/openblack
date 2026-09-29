/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::mods
{
class ModRegistry;

// The built-in mods, one file per category. A new mod: derive from Mod, fill its Info, apply its state in Apply()
// (usually an EngineConfig switch the engine reads), and register it in the matching function.
void RegisterGraphicsMods(ModRegistry& registry);
void RegisterWaterMods(ModRegistry& registry);
void RegisterWorldMods(ModRegistry& registry);

inline void RegisterBuiltinMods(ModRegistry& registry)
{
	RegisterGraphicsMods(registry);
	RegisterWaterMods(registry);
	RegisterWorldMods(registry);
}

} // namespace openblack::mods
