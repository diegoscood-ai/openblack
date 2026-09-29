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
} // namespace

void RegisterWorldMods(ModRegistry& registry)
{
	registry.Register(std::make_unique<GroundStaticsMod>());
}

} // namespace openblack::mods
