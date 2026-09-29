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
} // namespace

void RegisterAnisotropicMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<AnisotropicMod>());
}

} // namespace openblack::mods
