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

// The built-in mods, one file each in Mods/Builtin/. A new mod: a new file there with a class derived from Mod (its
// Info, options, and Apply(), which usually sets an EngineConfig switch the engine reads) and a Register<Name>
// function, declared and called here. Its files in the game folder go in Mods/<its id>/ (next to its settings.cfg).
void RegisterMsaaMod(ModRegistry& registry);
void RegisterMipmapsMod(ModRegistry& registry);
void RegisterAnisotropicMod(ModRegistry& registry);
void RegisterTerrainX2Mod(ModRegistry& registry);
void RegisterLivingWaterMod(ModRegistry& registry);
void RegisterGroundStaticsMod(ModRegistry& registry);
void RegisterFoliageMod(ModRegistry& registry);
void RegisterCropsMod(ModRegistry& registry);
void RegisterHdTweaksMod(ModRegistry& registry);
void RegisterMiracleDispensersMod(ModRegistry& registry);
void RegisterSkipIntroMod(ModRegistry& registry);
void RegisterSmoothSmokeMod(ModRegistry& registry);

inline void RegisterBuiltinMods(ModRegistry& registry)
{
	RegisterMsaaMod(registry);
	RegisterMipmapsMod(registry);
	RegisterAnisotropicMod(registry);
	RegisterTerrainX2Mod(registry);
	RegisterLivingWaterMod(registry);
	RegisterGroundStaticsMod(registry);
	RegisterFoliageMod(registry);
	RegisterCropsMod(registry);
	RegisterHdTweaksMod(registry);
	RegisterMiracleDispensersMod(registry);
	RegisterSkipIntroMod(registry);
	RegisterSmoothSmokeMod(registry);
}

} // namespace openblack::mods
