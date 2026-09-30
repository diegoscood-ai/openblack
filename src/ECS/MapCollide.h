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

/// The collide data of the fixed objects in the map cells, only as far as the map script's
/// GObjectInfo::IsOkToCreateAtPos 0x638C40 needs it (research dev\tmp_dis\mapa\flecos_isok.md). It is filled while the
/// land script runs: the original checks it only in CREATE_TREE, CREATE_NEW_TREE, CREATE_POT and CREATE_MOBILEOBJECT.
namespace openblack::ecs::map_collide
{
/// NewCollide::Obj: a circle in XZ (the height never counts) and, for long shapes, a row of child circles
struct Shape
{
	glm::vec2 centre;
	float radius;
	std::vector<glm::vec2> children;
	float childRadius;
	/// what made it (only for the OPENBLACK_LOG_ISOK log)
	std::string what;
};

/// NewCollide(LH3DObject) 0x829390 (+ Obj(point, a, b, angle) 0x82ADD0 and CreateList 0x828F40), from the mesh bbox;
/// false without the mesh
bool FromMesh(entt::id_type meshResource, glm::vec2 position, float yAngle, float scale, Shape& shape);
/// NewCollide::Obj::Collide 0x829140 of a circle without children against the shape
[[nodiscard]] bool Collide(glm::vec2 point, float radius, const Shape& shape);

/// A land is loaded (LOAD_LANDSCAPE): no objects
void Clear();
/// MultiMapFixed::InsertMapObject 0x52E650: the shape of the mesh in every cell whose circle (cell centre, 7.1) it
/// touches (NewCollideDescriptor 0x46AAD0)
void RegisterFixed(entt::id_type meshResource, glm::vec3 position, float yAngle, float scale, std::string_view what);
/// Tree::CreateCollideData 0x74C5F0: a 0.3 circle at the tree, only in its own cell (Object::InsertMapObject 0x636740)
void RegisterTree(glm::vec3 position);
/// GObjectInfo::IsOkToCreateAtPos 0x638C40: false if a 0.5 circle at the position (x, z) hits an object of its cell and
/// the cell has no water; true off the map. The command is only for the OPENBLACK_LOG_ISOK=1 log of the rejections.
[[nodiscard]] bool IsOkToCreateAtPos(glm::vec3 position, std::string_view command);
} // namespace openblack::ecs::map_collide
