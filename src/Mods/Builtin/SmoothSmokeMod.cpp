/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mods/BuiltinMods.h"

#include "EngineConfig.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"

// The original keeps smokea.raw as ARGB4444 like every texture with the alpha flag (fn_00837400, a.raw 0x8375C1):
// 16 alpha levels, 228 -> 238/255 (Graphics/Argb4444.h, docs/bw1-notes/rendering.md "Texturas ARGB4444"). This mod
// keeps the file's 8 bits, for smoother smoke, clouds and mists.

namespace openblack::mods
{
namespace
{
class SmoothSmokeMod final: public Mod
{
public:
	SmoothSmokeMod()
	    : Mod({"graphics.smooth-smoke", "Smooth smoke alpha",
	           "Smoke, clouds and mists keep the 8-bit alpha of smokea.raw instead of the original's 16 levels",
	           "Graphics", true})
	{
	}

	void Apply() override { Locator::config::value().smoothSmokeAlpha = IsEnabled(); }
};
} // namespace

void RegisterSmoothSmokeMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<SmoothSmokeMod>());
}

} // namespace openblack::mods
