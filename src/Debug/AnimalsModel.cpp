/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalsModel.h"

#include <algorithm>
#include <array>
#include <iterator>

#include <fmt/format.h>

using namespace openblack;
using namespace openblack::debug;
using namespace openblack::debug::animals;

namespace
{
constexpr std::array<std::string_view, static_cast<size_t>(AnimalInfo::_COUNT)> k_SpeciesNames {
    "lion",         "tiger",       "wolf",
    "leopard",      "sheep",       "goat",
    "tortoise",     "zebra",       "cow",
    "horse",        "pig",         "crow",
    "dove",         "swallow",     "pigeon",
    "seagull",      "bat",         "vulture",
    "citadel dove", "citadel bat", "spell dove",
    "spell bat",    "spell wolf",  "puzzle lion",
    "puzzle sheep", "puzzle wolf", "puzzle villager",
    "puzzle horse", "puzzle cow",  "puzzle tortoise",
    "puzzle pig",
};

/// Steps of a heading in a circle
constexpr float k_CircleSteps = 2048.0f;
} // namespace

std::optional<entt::entity> animals::Nearest(std::span<const AnimalAt> animals, glm::vec3 point)
{
	std::optional<entt::entity> nearest;
	float best = 0.0f;
	for (const auto& animal : animals)
	{
		const float dx = animal.position.x - point.x;
		const float dz = animal.position.z - point.z;
		const float distance = (dx * dx) + (dz * dz);
		if (!nearest || distance < best)
		{
			nearest = animal.entity;
			best = distance;
		}
	}
	return nearest;
}

std::string_view animals::SpeciesName(AnimalInfo type)
{
	const auto index = static_cast<int>(type);
	if (index < 0 || index >= static_cast<int>(k_SpeciesNames.size()))
	{
		return "unknown";
	}
	return k_SpeciesNames.at(static_cast<size_t>(index));
}

std::string animals::StateLabel(uint8_t state, std::span<const char> tableName)
{
	const auto end = std::ranges::find(tableName, '\0');
	const std::string_view name(tableName.data(), static_cast<size_t>(std::distance(tableName.begin(), end)));
	return name.empty() ? fmt::format("{}", state) : fmt::format("{} {}", state, name);
}

float animals::HeadingDegrees(uint16_t angle)
{
	return static_cast<float>(angle) * 360.0f / k_CircleSteps;
}
