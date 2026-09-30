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

// The original's villagers share 256x256 atlases, four villagers each (docs/bw1-notes/mods.md, "Aldeanos").

namespace openblack::mods
{
namespace
{
class HdTweaksMod final: public Mod
{
public:
	HdTweaksMod()
	    : Mod({"graphics.hd-tweaks", "HD-Tweaks",
	           "Better looking villagers, animals and hand: villager and animal textures upscaled 4x (images in "
	           "Mods/graphics.hd-tweaks, made with its tools), rounder villagers, animals and hand (each triangle curved "
	           "and split in 4 or 9), the original's light per pixel on those smooth shapes, textures kept sharp far "
	           "away, and the high detail meshes of villagers and animals (the original only draws the standard ones)",
	           "Graphics", false})
	{
		AddOption({"textures", "Textures", {"hd", "original"}, 0});
		AddOption({"smooth", "Rounder shapes", {"off", "soft", "round"}, 2});
		AddOption({"light", "Lighting", {"smooth", "original"}, 0});
		AddOption({"sharp", "Sharp far away", {"on", "off"}, 0});
		AddOption({"detail", "Villager and animal meshes", {"high", "original"}, 0});
	}

	void Apply() override
	{
		auto& config = Locator::config::value();
		config.hdTweaksTextures = IsEnabled() && GetChoice("textures") == "hd";
		const auto& smooth = GetChoice("smooth");
		config.hdTweaksSmoothLevel = !IsEnabled() ? 0 : smooth == "round" ? 3 : smooth == "soft" ? 2 : 0;
		const auto& light = GetChoice("light");
		config.hdTweaksLighting = IsEnabled() && light == "smooth" ? 1 : 0;
		config.hdTweaksMipBias = IsEnabled() && GetChoice("sharp") == "on" ? -1.0f : 0.0f;
		config.hdTweaksHighDetail = IsEnabled() && GetChoice("detail") == "high";
	}
};
} // namespace

void RegisterHdTweaksMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<HdTweaksMod>());
}

} // namespace openblack::mods
