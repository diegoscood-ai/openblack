/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The rules of the prints that things give the land without being in the map's cells: where such a print lies, when the
/// land takes it and the order the land paints them in. No registry and no render dependencies.
/// docs/wiki/world/terrain.md#the-prints-the-16th-step
namespace openblack::ecs::land_footprints
{

/// A thing that shows only as a print of its model on the land, as the land's list of prints holds it
struct GroundPrintThing
{
	entt::id_type mesh {0};
	/// Where its print lies (GroundPrintPlacement)
	std::array<float, 12> placement {};
	/// The order in which the prints were made
	uint32_t made {0};
	/// Whether the land took its print (IsNewGroundPrint when it was made)
	bool printed {true};

	bool operator==(const GroundPrintThing&) const = default;
};

/// Where the print of a thing lies: at its place, with no turn and no scale. The right, up and forward axes are the
/// world's, then the place, whose x and z go through the map's fixed point first, as the game keeps them; y is kept.
[[nodiscard]] std::array<float, 12> GroundPrintPlacement(const glm::vec3& position);

/// Whether the land takes a new print of `mesh` at `placement`: not when one of the prints it took has the same model
/// and the same twelve numbers of its placement. A print left out stays out, also once the older one is gone.
[[nodiscard]] bool IsNewGroundPrint(std::span<const GroundPrintThing> prints, entt::id_type mesh,
                                    const std::array<float, 12>& placement);

/// The prints the land took, in the order that it paints them, as indices into `prints`: the newest first
[[nodiscard]] std::vector<size_t> GroundPrintOrder(std::span<const GroundPrintThing> prints);

/// The box of a print on the ground (min x, min z, max x, max z): the corners of its footprint's triangles through the
/// placement, x and z only; all zero for a footprint with no corners, as the game's record starts
[[nodiscard]] glm::vec4 GroundPrintBox(std::span<const glm::vec2> corners, const std::array<float, 12>& placement);

/// The block places (map x and z / 160) the land marks out of date for a new print, from its box: each side x 1/160
/// truncated toward zero, both ends in
[[nodiscard]] std::array<glm::ivec2, 2> GroundPrintBlocks(const glm::vec4& box);

} // namespace openblack::ecs::land_footprints
