/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The collide data of the objects in the map cells (NewCollide; research dev\tmp_dis\mapa\flecos_isok.md): ecs::map_cells
/// builds each object's on its insert (CollideDataOf), and GObjectInfo::IsOkToCreateAtPos 0x638C40 reads the live
/// cells (the original checks it in CREATE_TREE, CREATE_NEW_TREE, CREATE_POT and CREATE_MOBILEOBJECT).
namespace openblack::ecs::map_collide
{
/// NewCollide::Obj: a circle in XZ (the height never counts) and, for long shapes, a row of child circles
struct Shape
{
	glm::vec2 centre;
	float radius;
	std::vector<glm::vec2> children;
	float childRadius;
	/// what made it (a debug name)
	std::string what;
};

/// NewCollide(LH3DObject) 0x829390 (+ Obj(point, a, b, angle) 0x82ADD0 and CreateList 0x828F40), from the mesh bbox;
/// false without the mesh
bool FromMesh(entt::id_type meshResource, glm::vec2 position, float yAngle, float scale, Shape& shape);
/// NewCollide::Obj::Collide 0x829140 of a circle without children against the shape
[[nodiscard]] bool Collide(glm::vec2 point, float radius, const Shape& shape);

/// GObjectInfo::IsOkToCreateAtPos 0x638C40: true when map_cells::CollideWithFixed (0x604FE0) has no bit 8 (no object
/// of the position's cell fixed list has collide data a 0.5 circle at the position (x, z) touches), else
/// MapCoords::IsWater 0x6035B0; off the map IsWater decides (true). The command is only for the OPENBLACK_LOG_ISOK=1
/// log of the rejections.
[[nodiscard]] bool IsOkToCreateAtPos(glm::vec3 position, std::string_view command);
} // namespace openblack::ecs::map_collide
