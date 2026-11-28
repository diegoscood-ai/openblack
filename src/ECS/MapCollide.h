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

#include <string>
#include <string_view>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The collide data of the objects in the map cells: ecs::map_cells builds each object's on its insert
/// (CollideDataOf), and IsOkToCreateAtPos reads the live cells (the original checks it in CREATE_TREE,
/// CREATE_NEW_TREE, CREATE_POT and CREATE_MOBILEOBJECT).
namespace openblack::ecs::map_collide
{
/// A circle in XZ (the height never counts) and, for long shapes, a row of child circles
struct Shape
{
	glm::vec2 centre;
	float radius;
	std::vector<glm::vec2> children;
	float childRadius;
	/// what made it (a debug name)
	std::string what;
	/// each child's own radius, when what made the shape gives every child its own (the citadel heart's and the
	/// worship site's); empty: every child has childRadius
	std::vector<float> childRadii;
};

/// One circle in XZ
struct Circle
{
	glm::vec2 centre;
	float radius;
};

/// The radius of the shape's child circle `child`: its own when the shape has one per child, else childRadius
[[nodiscard]] float ChildRadius(const Shape& shape, size_t child);
/// The citadel heart's own child circles, round its position (x, z) turned by its y angle: a 15.05 m circle at the
/// middle, then seven arms 2 pi / 7 apart from the y angle + 3.83, of 3.311 m circles: the first two arms six each (three
/// straight, then three turned towards each other), the other five four each
/// (docs/wiki/magic/temple-and-citadel.md#the-hearts-and-the-worship-sites-collide-shapes)
[[nodiscard]] std::vector<Circle> CitadelHeartCircles(glm::vec2 position, float yAngle);
/// The worship site's own child circles: ten points of the site's model put where its rotation, scale and position
/// take them, each of a radius 1.3 times the point's height (1.82, 2.6 or 3.9 m), and none at the middle
[[nodiscard]] std::vector<Circle> WorshipSiteCircles(const glm::mat3& rotation, glm::vec3 scale, glm::vec3 position);
/// The shape keeps its own circle and takes the given circles as its children, each with its own radius
void SetOwnChildren(Shape& shape, const std::vector<Circle>& circles);

/// The shape of an object from its mesh bbox (with a row of child circles for a long one); false without the mesh
bool FromMesh(entt::id_type meshResource, glm::vec2 position, float yAngle, float scale, Shape& shape);
/// A circle without children against the shape
[[nodiscard]] bool Collide(glm::vec2 point, float radius, const Shape& shape);

/// True when map_cells::CollideWithFixed has no bit 8 (no object of the position's cell fixed list has collide data a
/// 0.5 circle at the position (x, z) touches), else whether it is water; off the map that decides (true). The command
/// is only for the OPENBLACK_LOG_ISOK=1 log of the rejections.
[[nodiscard]] bool IsOkToCreateAtPos(glm::vec3 position, std::string_view command);
} // namespace openblack::ecs::map_collide
