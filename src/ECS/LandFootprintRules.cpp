/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandFootprintRules.h"

#include <algorithm>
#include <functional>

#include "3D/BlockTextureQueue.h"
#include "3D/MapCoords.h"

namespace openblack::ecs::land_footprints
{

std::array<float, 12> GroundPrintPlacement(const glm::vec3& position)
{
	// the world's right, up and forward axes, then the place
	std::array<float, 12> placement {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	placement[9] = map_coords::Quantise(position.x);
	placement[10] = position.y;
	placement[11] = map_coords::Quantise(position.z);
	return placement;
}

bool IsNewGroundPrint(std::span<const GroundPrintThing> prints, entt::id_type mesh, const std::array<float, 12>& placement)
{
	return std::ranges::none_of(prints, [&](const GroundPrintThing& print) {
		return print.printed && print.mesh == mesh && print.placement == placement;
	});
}

std::vector<size_t> GroundPrintOrder(std::span<const GroundPrintThing> prints)
{
	std::vector<size_t> order;
	for (size_t i = 0; i < prints.size(); ++i)
	{
		if (prints[i].printed)
		{
			order.push_back(i);
		}
	}
	// each new print goes in front of the ones before it
	std::ranges::stable_sort(order, std::greater {}, [&prints](size_t index) { return prints[index].made; });
	return order;
}

glm::vec4 GroundPrintBox(std::span<const glm::vec2> corners, const std::array<float, 12>& placement)
{
	if (corners.empty())
	{
		return glm::vec4(0.0f);
	}
	const auto at = [&placement](const glm::vec2& corner) {
		// the corner on the ground (x, 0, z) through the placement
		return glm::vec2(((corner.x * placement[0]) + (corner.y * placement[6])) + placement[9],
		                 ((corner.x * placement[2]) + (corner.y * placement[8])) + placement[11]);
	};
	const auto first = at(corners.front());
	glm::vec4 box {first.x, first.y, first.x, first.y};
	for (const auto& corner : corners.subspan(1))
	{
		const auto point = at(corner);
		box.x = std::min(box.x, point.x);
		box.y = std::min(box.y, point.y);
		box.z = std::max(box.z, point.x);
		box.w = std::max(box.w, point.y);
	}
	return box;
}

std::array<glm::ivec2, 2> GroundPrintBlocks(const glm::vec4& box)
{
	const auto place = [](float metres) { return static_cast<int32_t>(metres * block_texture::k_BlocksPerMetre); };
	return {glm::ivec2(place(box.x), place(box.y)), glm::ivec2(place(box.z), place(box.w))};
}

} // namespace openblack::ecs::land_footprints
