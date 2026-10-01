/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mods/BuiltinMods.h"

#include <array>
#include <charconv>
#include <memory>
#include <string_view>

#include "EngineConfig.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"

// Test aid, not in the original: the engine part is Worship/TestDispensers.cpp

namespace openblack::mods
{
namespace
{
/// The "level" choices: 0 base, 1 PU one, 2 PU two, 3 every level (one dispenser each)
constexpr std::array<std::string_view, 4> k_Levels = {"base", "pu1", "pu2", "all"};

class MiracleDispensersMod final: public Mod
{
public:
	MiracleDispensersMod()
	    : Mod({"test.miracle-dispensers", "Máquinas de milagros de prueba",
	           "No existe en el original: para probar los milagros. Al cargar una tierra en la que el jugador tiene "
	           "templo, pone junto a él un dispensador de milagros (el del desafío de Land 1) por cada milagro del "
	           "jugador: fuego, rayo, agua, comida, madera, curar, bosque, bandadas, escudos, teletransporte, tormenta "
	           "y explosión de rayo. Cada uno da otro orbe a los pocos segundos de cogerlo. Además pone una máquina vacía que "
	           "nunca da orbe y, si se quiere, una semilla de bola de fuego en la mano al empezar (para comparar la "
	           "transparencia del orbe, de la máquina y de la semilla)",
	           "Test"})
	{
		AddOption({"level", "Nivel (base, PU 1, PU 2, todos)", {"base", "pu1", "pu2", "all"}, 0, true});
		AddOption({"recharge", "Recarga del orbe", {"2s", "5s", "10s", "20s", "30s", "60s"}, 2, true});
		AddOption({"seed", "Bola de fuego en la mano al empezar", {"on", "off"}, 0, true});
	}

	void Apply() override
	{
		auto& config = Locator::config::value();
		config.testDispensers = IsEnabled();
		const auto& level = GetChoice("level");
		config.testDispensersLevel = 0;
		for (size_t i = 0; i < k_Levels.size(); ++i)
		{
			if (level == k_Levels.at(i))
			{
				config.testDispensersLevel = static_cast<int>(i);
			}
		}
		const auto& recharge = GetChoice("recharge");
		int seconds = 10;
		std::from_chars(recharge.data(), recharge.data() + recharge.size(), seconds); // "10s" -> 10
		config.testDispensersSeconds = static_cast<float>(seconds);
		config.testDispensersSeed = GetChoice("seed") == "on";
	}
};
} // namespace

void RegisterMiracleDispensersMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<MiracleDispensersMod>());
}

} // namespace openblack::mods
