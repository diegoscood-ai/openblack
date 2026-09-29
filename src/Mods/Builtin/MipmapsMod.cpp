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

// The original draws with bilinear filtering and no mip levels (docs/bw1-notes/rendering.md).

namespace openblack::mods
{
namespace
{
class MipmapsMod final: public Mod
{
public:
	MipmapsMod()
	    : Mod({"graphics.mipmaps", "Mipmaps (trilinear filtering)",
	           "Mip levels for model, landscape and water textures: no shimmering in the distance", "Graphics", true})
	{
	}

	void Apply() override { Locator::config::value().textureMipmaps = IsEnabled(); }
};
} // namespace

void RegisterMipmapsMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<MipmapsMod>());
}

} // namespace openblack::mods
