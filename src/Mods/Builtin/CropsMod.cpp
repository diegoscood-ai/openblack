/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mods/BuiltinMods.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <memory>
#include <ranges>

#include "EngineConfig.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"

namespace openblack::mods
{
namespace
{
/// The growth multipliers of the "speed" slider, its choices being "x1", "x2"...
constexpr std::array<float, 7> k_Speeds = {1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f};

class CropsMod final: public Mod
{
public:
	CropsMod()
	    : Mod({"world.crops", "Crops without farmers",
	           "In the original the town's farmers sow every crop of a field and harvest it; openblack has no villager "
	           "jobs yet, so its fields stay empty forever. This sows them (at the start they are ripe, as a land "
	           "starts) and sows them again once harvested, and can make them grow faster",
	           "World"})
	{
		AddOption({"speed", "Growth speed", {"x1", "x2", "x5", "x10", "x20", "x50", "x100"}, 0, true});
	}

	void Apply() override
	{
		auto& config = Locator::config::value();
		config.fieldsWithoutFarmers = IsEnabled();
		const auto& speed = GetChoice("speed");
		size_t index = 0;
		std::from_chars(speed.data() + 1, speed.data() + speed.size(), index); // "x10" -> 10
		const auto found = std::ranges::find(k_Speeds, static_cast<float>(index));
		config.fieldGrowthMultiplier = found != k_Speeds.end() ? *found : 1.0f;
	}
};
} // namespace

void RegisterCropsMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<CropsMod>());
}

} // namespace openblack::mods
