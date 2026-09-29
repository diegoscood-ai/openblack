/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuiltinMods.h"
#include "ECS/Registry.h"
#include "ECS/StaticGrounding.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "ModRegistry.h"

namespace openblack::mods
{
namespace
{
class GroundStaticsMod final: public Mod
{
public:
	GroundStaticsMod()
	    : Mod({"world.ground-statics", "Ground floating static objects",
	           "Lowers floating rocks and other static objects onto the landscape (meshes of modded packs float)", "World"})
	{
	}

	void Apply() override
	{
		auto& config = Locator::config::value();
		const bool changed = config.groundStaticObjects != IsEnabled();
		config.groundStaticObjects = IsEnabled();
		// objects created later ground themselves (MobileStaticArchetype); existing ones are moved now
		if (changed && Locator::entitiesRegistry::has_value())
		{
			ecs::StaticGrounding::ApplyToAll(IsEnabled());
		}
	}
};

class FoliageMod final: public Mod
{
public:
	FoliageMod()
	    : Mod({"world.foliage", "Grass and flowers",
	           "Grass, flowers, rushes and bushes over the landscape, chosen by the ground's material, altitude and slope "
	           "(rules and images in ModAssets/Foliage)",
	           "World"})
	{
		AddOption({"density", "Density", {"low", "medium", "high", "very high"}, 1});
		AddOption({"distance", "Draw distance", {"near", "medium", "far"}, 1});
	}

	void Apply() override
	{
		auto& config = Locator::config::value();
		const auto& density = GetChoice("density");
		const float amount = density == "low" ? 0.5f : density == "high" ? 2.0f : density == "very high" ? 4.0f : 1.0f;
		config.foliageDensity = IsEnabled() ? amount : 0.0f;
		const auto& distance = GetChoice("distance");
		config.foliageDistance = distance == "near" ? 120.0f : distance == "far" ? 320.0f : 200.0f;
	}
};
} // namespace

void RegisterWorldMods(ModRegistry& registry)
{
	registry.Register(std::make_unique<GroundStaticsMod>());
	registry.Register(std::make_unique<FoliageMod>());
}

} // namespace openblack::mods
