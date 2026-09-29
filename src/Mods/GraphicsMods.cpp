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
	    : Mod({"graphics.terrain-x2", "Sharper landscape textures",
	           "Each landscape texture repeated 2, 3 or 4 times per block, optionally upscaled 2x with Lanczos-3 when the "
	           "island loads; cliffs take the texture from the side instead of stretching it (triplanar)",
	           "Graphics", true})
	{
		AddOption({"repeat", "Repeats per block", {"x1", "x2", "x3", "x4"}, 1});
		AddOption({"upscale", "Lanczos 2x upscale", {"off", "on"}, 0});
		AddOption({"cliffs", "Cliffs", {"triplanar", "stretched"}, 0});
	}

	void Apply() override
	{
		auto& config = Locator::config::value();
		const auto& repeat = GetChoice("repeat");
		config.terrainTextureDensity = IsEnabled() && repeat.size() == 2 ? static_cast<float>(repeat[1] - '0') : 1.0f;
		config.terrainTexturesX2 = IsEnabled() && GetChoice("upscale") == "on";
		config.terrainTriplanar = IsEnabled() && GetChoice("cliffs") == "triplanar";
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
