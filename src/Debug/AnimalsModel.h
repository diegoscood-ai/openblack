/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// What the Animals debug window works out before it shows an animal: which animal is nearest the hand, and how its
// species, states and heading read. Pure functions, tested with hand-made data.

namespace openblack::debug::animals
{

/// An animal on the land, as the window reads it
struct AnimalAt
{
	entt::entity entity {entt::null};
	glm::vec3 position {0.0f};
};

/// The animal nearest the point over the land (height left out), the first of the list on a tie; none without animals
[[nodiscard]] std::optional<entt::entity> Nearest(std::span<const AnimalAt> animals, glm::vec3 point);

/// How a species reads in the window
[[nodiscard]] std::string_view SpeciesName(AnimalInfo type);

/// A state as its number and the state table's name for it (the name ends at its first null), the number alone when
/// the name is empty
[[nodiscard]] std::string StateLabel(uint8_t state, std::span<const char> tableName);

/// A heading of 2048 steps a circle in degrees
[[nodiscard]] float HeadingDegrees(uint16_t angle);

} // namespace openblack::debug::animals
