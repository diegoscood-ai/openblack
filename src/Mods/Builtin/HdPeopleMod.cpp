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
class HdPeopleMod final: public Mod
{
public:
	HdPeopleMod()
	    : Mod({"graphics.hd-people", "HD villagers",
	           "Better looking villagers: their textures upscaled 4x (images in Mods/graphics.hd-people, made with its "
	           "tools) and rounder bodies (each triangle curved and split in 4 or 9)",
	           "Graphics", true})
	{
		AddOption({"textures", "Textures", {"hd", "original"}, 0});
		AddOption({"smooth", "Rounder shapes", {"off", "soft", "round"}, 2});
	}

	void Apply() override
	{
		auto& config = Locator::config::value();
		config.hdPeopleTextures = IsEnabled() && GetChoice("textures") == "hd";
		const auto& smooth = GetChoice("smooth");
		config.hdPeopleSmoothLevel = !IsEnabled() ? 0 : smooth == "round" ? 3 : smooth == "soft" ? 2 : 0;
	}
};
} // namespace

void RegisterHdPeopleMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<HdPeopleMod>());
}

} // namespace openblack::mods
