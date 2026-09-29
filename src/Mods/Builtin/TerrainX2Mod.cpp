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

namespace openblack::mods
{
namespace
{
class TerrainX2Mod final: public Mod
{
public:
	TerrainX2Mod()
	    : Mod({"graphics.terrain-x2", "Sharper landscape textures",
	           "Each landscape and sea texture repeated 2, 3 or 4 times as often, optionally upscaled 2x with Lanczos-3 "
	           "when loaded; cliffs take the texture from the side instead of stretching it (triplanar)",
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

void RegisterTerrainX2Mod(ModRegistry& registry)
{
	registry.Register(std::make_unique<TerrainX2Mod>());
}

} // namespace openblack::mods
