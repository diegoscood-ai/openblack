/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuiltinMods.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "ModRegistry.h"

namespace openblack::mods
{
namespace
{
/// The original mirrors only the sky and the land under the sea and never moves the reflection (rendering.md, "Mar")
class LivingWaterMod final: public Mod
{
public:
	LivingWaterMod()
	    : Mod({"water.living", "Living water", "The sea reflects everything, the reflection ripples with slow waves and the sea surface drifts (no per-row shimmer)",
	           "Water"})
	{
	}

	void Apply() override { Locator::config::value().livingWater = IsEnabled(); }
};
} // namespace

void RegisterWaterMods(ModRegistry& registry)
{
	registry.Register(std::make_unique<LivingWaterMod>());
}

} // namespace openblack::mods
