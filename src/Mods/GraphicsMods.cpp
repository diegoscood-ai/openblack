/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/vec2.hpp>

#include "BuiltinMods.h"
#include "EngineConfig.h"
#include "Graphics/RendererInterface.h"
#include "Locator.h"
#include "ModRegistry.h"
#include "Windowing/WindowingInterface.h"

// The original draws with bilinear filtering, no mip levels and no anti-aliasing (docs/bw1-notes/rendering.md).

namespace openblack::mods
{
namespace
{
class MsaaMod final: public Mod
{
public:
	MsaaMod()
	    : Mod({"graphics.msaa", "Anti-aliasing (MSAA)",
	           "Multisampled backbuffer; leaves and fences get smooth edges with alpha to coverage", "Graphics"})
	{
		AddOption({"samples", "Samples", {"2x", "4x", "8x", "16x"}, 1});
	}

	void Apply() override
	{
		const auto& samples = GetChoice("samples");
		Locator::config::value().msaa = IsEnabled() ? static_cast<uint8_t>(std::stoi(samples)) : 0;
		if (Locator::rendererInterface::has_value() && Locator::windowing::has_value())
		{
			Locator::rendererInterface::value().Reset(glm::u16vec2(Locator::windowing::value().GetSize()));
		}
	}
};

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

class AnisotropicMod final: public Mod
{
public:
	AnisotropicMod()
	    : Mod({"graphics.anisotropic", "Anisotropic filtering",
	           "Sharper textures at grazing angles (includes the mip levels)", "Graphics", true})
	{
	}

	void Apply() override { Locator::config::value().anisotropicFiltering = IsEnabled(); }
};

class TerrainX2Mod final: public Mod
{
public:
	TerrainX2Mod()
	    : Mod({"graphics.terrain-x2", "Landscape textures x2",
	           "Sharper landscape: each texture repeated twice per block (density), upscaled 2x with Lanczos-3 when the "
	           "island loads (upscale), or both",
	           "Graphics", true})
	{
		AddOption({"method", "Method", {"density", "upscale", "both"}, 0});
	}

	void Apply() override
	{
		const auto& method = GetChoice("method");
		auto& config = Locator::config::value();
		config.terrainTexturesX2 = IsEnabled() && method != "density";
		config.terrainTextureDensity = IsEnabled() && method != "upscale" ? 2.0f : 1.0f;
	}
};
} // namespace

void RegisterGraphicsMods(ModRegistry& registry)
{
	registry.Register(std::make_unique<MsaaMod>());
	registry.Register(std::make_unique<MipmapsMod>());
	registry.Register(std::make_unique<AnisotropicMod>());
	registry.Register(std::make_unique<TerrainX2Mod>());
}

} // namespace openblack::mods
