/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingCircles.h"

#include <cstdint>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/WallHugBookkeeping.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// A cell index wraps at 16 bits, as the game counts it
constexpr int32_t k_CellIndexMask = 0xFFFF;

/// A mobile static whose model is a fence's
bool IsFence(const Registry& registry, entt::entity thing)
{
	const auto* mobileStatic = registry.TryGet<const MobileStatic>(thing);
	if (mobileStatic == nullptr || mobileStatic->type == MobileStaticInfo::None || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(mobileStatic->type));
	return wall_hug::IsFenceModel(info.meshId);
}
} // namespace

std::vector<wall_hug::BlockingCircle> thing_circles::CirclesOf(const map_collide::Shape& shape, bool fence)
{
	std::vector<wall_hug::BlockingCircle> circles;
	if (shape.children.empty())
	{
		circles.push_back({.centre = shape.centre, .radius = shape.radius, .landscape = false, .landscapeOrFence = fence});
		return circles;
	}
	circles.reserve(shape.children.size());
	for (const auto& child : shape.children)
	{
		circles.push_back({.centre = child, .radius = shape.childRadius, .landscape = false, .landscapeOrFence = fence});
	}
	return circles;
}

std::vector<wall_hug::BlockingCircle> thing_circles::CirclesOf(const Registry& registry, entt::entity thing)
{
	// A field is walked over, whatever its collide data
	if (!registry.Valid(thing) || registry.AnyOf<Field>(thing))
	{
		return {};
	}
	const auto* shape = map_cells::CollideDataOf(thing);
	if (shape == nullptr)
	{
		return {};
	}
	return CirclesOf(*shape, IsFence(registry, thing));
}

thing_circles::CellSources thing_circles::MapSources()
{
	return {
	    .forEachFixed = [](glm::ivec2 cell, const std::function<bool(entt::entity)>& fn) { map_cells::ForEachFixed(cell, fn); },
	    .circlesOf = [](entt::entity thing) { return CirclesOf(Locator::entitiesRegistry::value(), thing); },
	    // off the game map, or a cell with no land cell or with the water bit
	    .blocked =
	        [](glm::ivec2 cell) {
		        if (!Locator::terrainSystem::has_value())
		        {
			        return false; // (openblack, guard) the unit tests without an island
		        }
		        const auto& island = Locator::terrainSystem::value();
		        return !sea_cells::InBounds(island, cell) || sea_cells::IsWater(island, cell);
	        },
	};
}

std::vector<thing_circles::CellCircle> thing_circles::CirclesInCell(glm::ivec2 cell, const CellSources& sources)
{
	std::vector<CellCircle> circles;
	sources.forEachFixed(cell, [&circles, &sources](entt::entity thing) {
		const auto own = sources.circlesOf(thing);
		for (size_t i = 0; i < own.size(); ++i)
		{
			circles.push_back({.circle = own[i], .owner = thing, .index = static_cast<uint16_t>(i)});
		}
		return true;
	});
	if (!sources.blocked)
	{
		return circles;
	}
	// the cell and its eight neighbours in the shared order, each index in 16 bits
	for (const auto& offset : wall_hug::k_LandscapeCellOrder)
	{
		const glm::ivec2 next {(cell.x + offset.x) & k_CellIndexMask, (cell.y + offset.y) & k_CellIndexMask};
		if (!sources.blocked(next))
		{
			continue;
		}
		circles.push_back({.circle = {.centre = wall_hug::LandscapeCircleCentre(next),
		                              .radius = wall_hug::k_LandscapeBlockerRadius,
		                              .landscape = true,
		                              .landscapeOrFence = true},
		                   .owner = entt::null,
		                   .index = 0,
		                   .cell = next});
	}
	return circles;
}

std::vector<thing_circles::CellCircle> thing_circles::CirclesInCell(glm::ivec2 cell)
{
	return CirclesInCell(cell, MapSources());
}
