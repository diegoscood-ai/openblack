/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapCollide.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <unordered_map>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::map_collide;

namespace
{
/// NewCollideDescriptor 0x46AAD0: the circle of a map cell (0x40E33333)
constexpr float k_CellCircleRadius = 7.1f;
/// MapCell::CollideWithFixe 0x601D10: the circle at the tested position
constexpr float k_TestRadius = 0.5f;
/// Tree::CreateCollideData 0x74C5F0 (fn_00829590(position, 0.3))
constexpr float k_TreeRadius = 0.3f;
/// Obj(point, a, b, angle) 0x82ADD0 is used above this ratio of the half sizes
constexpr float k_LongRatio = 1.4f;

std::unordered_map<uint32_t, std::vector<Shape>> g_cells;

/// x' = x cos a - z sin a, z' = x sin a + z cos a (the sign checked on the maps saved by the game)
glm::vec2 Rotate(glm::vec2 v, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {v.x * c - v.y * s, v.x * s + v.y * c};
}

int CellsPerSide()
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : 512;
}

uint32_t Key(int cx, int cz)
{
	return static_cast<uint32_t>(cx) * 0x10000u + static_cast<uint32_t>(cz);
}

bool LogRejections()
{
	static const bool log = [] {
		const char* env = std::getenv("OPENBLACK_LOG_ISOK");
		return env != nullptr && env[0] != '\0' && env[0] != '0';
	}();
	return log;
}
} // namespace

bool openblack::ecs::map_collide::FromMesh(entt::id_type meshResource, glm::vec2 position, float yAngle, float scale,
                                           Shape& shape)
{
	if (!Locator::resources::has_value())
	{
		return false;
	}
	const auto mesh = Locator::resources::value().GetMeshes().Handle(meshResource);
	if (!mesh)
	{
		return false;
	}
	// LH3DMesh::ComputeBoundingBox 0x8081B0: centre and half size over every vertex of every submesh
	const auto& bb = mesh->GetBoundingBox();
	shape.centre = position + Rotate(scale * glm::xz(bb.Center()), yAngle);
	const float ex = std::max(1.0f, scale * bb.Size().x * 0.5f);
	const float ez = std::max(1.0f, scale * bb.Size().z * 0.5f);
	const float longHalf = std::max(ex, ez);
	const float shortHalf = std::min(ex, ez);
	shape.children.clear();
	shape.childRadius = 0.0f;
	if (longHalf / shortHalf <= k_LongRatio)
	{
		shape.radius = longHalf;
		return true;
	}
	// 0x82ADD0: the outer circle; CreateList 0x828F40: int(long / short) + 1 circles of the short half size in a row
	// along the long axis, turned with the object
	shape.radius = std::sqrt(ex * ex + ez * ez);
	shape.childRadius = shortHalf;
	const int count = static_cast<int>(longHalf / shortHalf) + 1;
	const float step = 2.0f * longHalf / static_cast<float>(count);
	for (int i = 0; i < count; ++i)
	{
		const float t = (static_cast<float>(i) + 0.5f) * step - longHalf;
		const glm::vec2 local = ex > ez ? glm::vec2(t, 0.0f) : glm::vec2(0.0f, t);
		shape.children.push_back(shape.centre + Rotate(local, yAngle));
	}
	return true;
}

bool openblack::ecs::map_collide::Collide(glm::vec2 point, float radius, const Shape& shape)
{
	const auto hits = [point, radius](glm::vec2 centre, float other) {
		const auto delta = point - centre;
		return glm::dot(delta, delta) <= (radius + other) * (radius + other);
	};
	if (!hits(shape.centre, shape.radius))
	{
		return false;
	}
	if (shape.children.empty())
	{
		return true;
	}
	return std::any_of(shape.children.begin(), shape.children.end(),
	                   [&](glm::vec2 child) { return hits(child, shape.childRadius); });
}

void openblack::ecs::map_collide::Clear()
{
	g_cells.clear();
}

void openblack::ecs::map_collide::RegisterFixed(entt::id_type meshResource, glm::vec3 position, float yAngle, float scale,
                                                std::string_view what)
{
	Shape shape;
	if (!FromMesh(meshResource, glm::xz(position), yAngle, scale, shape))
	{
		return;
	}
	shape.what = what;
	const int side = CellsPerSide();
	const float reach = shape.radius + k_CellCircleRadius;
	const int x0 = std::max(0, static_cast<int>(std::floor((shape.centre.x - reach) / LandIslandInterface::k_CellSize)));
	const int x1 = std::min(side - 1, static_cast<int>(std::floor((shape.centre.x + reach) / LandIslandInterface::k_CellSize)));
	const int z0 = std::max(0, static_cast<int>(std::floor((shape.centre.y - reach) / LandIslandInterface::k_CellSize)));
	const int z1 = std::min(side - 1, static_cast<int>(std::floor((shape.centre.y + reach) / LandIslandInterface::k_CellSize)));
	for (int cx = x0; cx <= x1; ++cx)
	{
		for (int cz = z0; cz <= z1; ++cz)
		{
			const glm::vec2 cellCentre = (glm::vec2(cx, cz) + 0.5f) * LandIslandInterface::k_CellSize;
			if (Collide(cellCentre, k_CellCircleRadius, shape))
			{
				g_cells[Key(cx, cz)].push_back(shape);
			}
		}
	}
}

void openblack::ecs::map_collide::RegisterTree(glm::vec3 position)
{
	const int cx = static_cast<int>(std::floor(position.x / LandIslandInterface::k_CellSize));
	const int cz = static_cast<int>(std::floor(position.z / LandIslandInterface::k_CellSize));
	const int side = CellsPerSide();
	if (cx < 0 || cz < 0 || cx >= side || cz >= side)
	{
		return;
	}
	g_cells[Key(cx, cz)].push_back(Shape {glm::xz(position), k_TreeRadius, {}, 0.0f, "tree"});
}

bool openblack::ecs::map_collide::IsOkToCreateAtPos(glm::vec3 position, std::string_view command)
{
	// MapCoords::CollideCollideWithFixe 0x604FE0: off the map every bit is set, and then IsWater 0x6035B0 has no land
	// cell and says yes: true
	const int cx = static_cast<int>(std::floor(position.x / LandIslandInterface::k_CellSize));
	const int cz = static_cast<int>(std::floor(position.z / LandIslandInterface::k_CellSize));
	const int side = CellsPerSide();
	if (cx < 0 || cz < 0 || cx >= side || cz >= side)
	{
		return true;
	}
	const auto found = g_cells.find(Key(cx, cz));
	if (found == g_cells.end())
	{
		return true;
	}
	const auto point = glm::xz(position);
	const auto blocker = std::find_if(found->second.begin(), found->second.end(),
	                                  [point](const Shape& shape) { return Collide(point, k_TestRadius, shape); });
	if (blocker == found->second.end())
	{
		return true;
	}
	// IsWater 0x6035B0: bit 0x10 of the land cell (hasWater); a cell of an empty block (LH3DIsland::GetCell gives none)
	// counts as water
	if (Locator::terrainSystem::has_value())
	{
		const auto& terrain = Locator::terrainSystem::value();
		const auto& cell = terrain.GetCell(glm::u16vec2(cx, cz));
		// out of range coordinates give the island's empty cell
		const auto& empty = terrain.GetCell(glm::u16vec2(0xFFFF, 0xFFFF));
		if (&cell == &empty || cell.properties.hasWater != 0)
		{
			return true;
		}
	}
	if (LogRejections())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "isok: {} at ({:.2f}, {:.2f}) rejected by {}", command, position.x,
		                   position.z, blocker->what);
	}
	return false;
}
