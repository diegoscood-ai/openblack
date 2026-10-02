/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The engine switches mods may set: each one a field of EngineConfig, with its type, range and when it takes effect.
// The fields and the code that reads them are unchanged; this table only gives them a name. Adding a switch: add the
// field to EngineConfig (off = the original's behaviour), read it in the engine, and add a line here.

#include "Switches.h"

#include <glm/vec2.hpp>

#include "ECS/StaticGrounding.h"
#include "EngineConfig.h"
#include "Graphics/RendererInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::mods::switches
{
namespace
{
template <typename T>
void Field(std::string name, T EngineConfig::* field, When when, double min, double max, std::string description,
           std::function<void()> onChange = {})
{
	Switch value;
	value.name = std::move(name);
	if constexpr (std::is_same_v<T, bool>)
	{
		value.type = Type::Bool;
	}
	else if constexpr (std::is_integral_v<T>)
	{
		value.type = Type::Int;
	}
	else
	{
		value.type = Type::Float;
	}
	value.when = when;
	value.description = std::move(description);
	value.min = min;
	value.max = max;
	value.defaultValue = static_cast<double>(EngineConfig {}.*field);
	value.get = [field]() { return static_cast<double>(Locator::config::value().*field); };
	value.set = [field](double v) {
		if constexpr (std::is_same_v<T, bool>)
		{
			Locator::config::value().*field = v != 0.0;
		}
		else
		{
			Locator::config::value().*field = static_cast<T>(v);
		}
	};
	value.onChange = std::move(onChange);
	Register(std::move(value));
}
} // namespace

void RegisterEngineSwitches()
{
	using C = EngineConfig;

	// Graphics
	Field("graphics.msaa.samples", &C::msaa, When::Live, 0, 16,
	      "Multisample anti-aliasing of the back buffer: 0 (off, as the original), 2, 4, 8 or 16 samples",
	      []() {
		      // the back buffer is made again with the new sample count (before: MsaaMod::Apply)
		      if (Locator::rendererInterface::has_value() && Locator::windowing::has_value())
		      {
			      Locator::rendererInterface::value().Reset(glm::u16vec2(Locator::windowing::value().GetSize()));
		      }
	      });
	Field("graphics.mipmaps", &C::textureMipmaps, When::Restart, 0, 1,
	      "Mip levels and trilinear filtering for model, landscape and water textures");
	Field("graphics.anisotropic", &C::anisotropicFiltering, When::Restart, 0, 1,
	      "Anisotropic filtering of the textures (implies the mip levels)");
	Field("graphics.terrain.upscale", &C::terrainTexturesX2, When::MapLoad, 0, 1,
	      "Landscape material textures upscaled 2x (Lanczos-3) when the island loads");
	Field("graphics.terrain.repeat", &C::terrainTextureDensity, When::MapLoad, 1, 4,
	      "Landscape material textures repeated this many times per block (1 = the original)");
	Field("graphics.terrain.triplanar", &C::terrainTriplanar, When::MapLoad, 0, 1,
	      "Steep landscape faces take the materials from the side instead of stretching them");
	Field("graphics.smooth-smoke", &C::smoothSmokeAlpha, When::Restart, 0, 1,
	      "raw/smokea (smoke, clouds, mists...) keeps its 8-bit alpha instead of the original's ARGB4444 cut (16 levels)");
	Field("graphics.hd-tweaks.textures", &C::hdTweaksTextures, When::Live, 0, 1,
	      "Villager and animal textures replaced by the HD images of Mods/graphics.hd-tweaks");
	Field("graphics.hd-tweaks.smooth", &C::hdTweaksSmoothLevel, When::Live, 0, 3,
	      "Villager, animal and hand meshes as curved PN triangles split into level^2 (0 = off)");
	Field("graphics.hd-tweaks.lighting", &C::hdTweaksLighting, When::Live, 0, 1,
	      "0 the original's vertex lighting, 1 the same light per pixel on the smooth normals");
	Field("graphics.hd-tweaks.mip-bias", &C::hdTweaksMipBias, When::Live, -4, 0,
	      "Texture mip bias of villagers and animals (negative: sharper far away)");
	Field("graphics.hd-tweaks.high-detail", &C::hdTweaksHighDetail, When::Live, 0, 1,
	      "Villagers and animals with their high detail mesh instead of the standard one");

	// Water
	Field("water.living", &C::livingWater, When::Live, 0, 1,
	      "The sea reflects everything and the reflection ripples (the original mirrors only sky and land)");

	// World
	Field("world.ground-statics", &C::groundStaticObjects, When::Live, 0, 1,
	      "Floating rocks and other static objects lowered onto the landscape", []() {
		      // objects created later ground themselves; existing ones are moved now (before: GroundStaticsMod::Apply)
		      if (Locator::entitiesRegistry::has_value())
		      {
			      ecs::StaticGrounding::ApplyToAll(Locator::config::value().groundStaticObjects);
		      }
	      });
	Field("world.foliage.density", &C::foliageDensity, When::Live, 0, 8,
	      "Grass and flowers: plants per cell multiplier (0 = none)");
	Field("world.foliage.distance", &C::foliageDistance, When::Live, 50, 1000,
	      "Grass and flowers: distance they are drawn to, in metres");
	Field("world.foliage.fields", &C::foliageFields, When::Live, 0, 1,
	      "Crop fields drawn as growing plants instead of their mesh");
	Field("world.crops.without-farmers", &C::fieldsWithoutFarmers, When::Live, 0, 1,
	      "Fields sow themselves and are sown again once harvested");
	Field("world.crops.growth", &C::fieldGrowthMultiplier, When::Live, 1, 100, "Crop growth speed multiplier");

	// Game
	Field("game.skip-tutorial", &C::skipTutorialChoice, When::Restart, 0, 3,
	      "Answer to the original's skip-tutorial question: 0 play everything, 1 tutorial, 2 also creature training, "
	      "3 also the creature glade");
	Field("game.free-start", &C::skipIntroFreeStart, When::MapLoad, 0, 1,
	      "Not original: the land's opening task does not move the camera, lock the interface or play music");

	// Test
	Field("test.dispensers", &C::testDispensers, When::MapLoad, 0, 1,
	      "A miracle dispenser of each player miracle around the player's temple");
	Field("test.dispensers.level", &C::testDispensersLevel, When::MapLoad, 0, 3,
	      "Power-up level of the test dispensers: 0 base, 1, 2, 3 every level");
	Field("test.dispensers.seconds", &C::testDispensersSeconds, When::Live, 1, 600,
	      "Seconds a test dispenser takes to make a new orb");
	Field("test.dispensers.seed", &C::testDispensersSeed, When::Live, 0, 1,
	      "A fire seed in the player's hand once the test dispensers are placed");
}

} // namespace openblack::mods::switches
