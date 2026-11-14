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
#include <array>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Locator.h"
#include "MapCells.h"
#include "Resources/ResourcesInterface.h"
#include "SeaCells.h"

using namespace openblack;
using namespace openblack::ecs::map_collide;

namespace
{
/// A long shape (with child circles) is used above this ratio of the half sizes
constexpr float k_LongRatio = 1.4f;

/// x' = x cos a - z sin a, z' = x sin a + z cos a (the sign checked on the maps saved by the game)
glm::vec2 Rotate(glm::vec2 v, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {v.x * c - v.y * s, v.x * s + v.y * c};
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
	// the mesh's bounding box: centre and half size over every vertex of every submesh
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
	// the outer circle; then int(long / short) + 1 circles of the short half size in a row
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
	for (size_t i = 0; i < shape.children.size(); ++i)
	{
		if (hits(shape.children[i], ChildRadius(shape, i)))
		{
			return true;
		}
	}
	return false;
}

float openblack::ecs::map_collide::ChildRadius(const Shape& shape, size_t child)
{
	return child < shape.childRadii.size() ? shape.childRadii[child] : shape.childRadius;
}

std::vector<Circle> openblack::ecs::map_collide::CitadelHeartCircles(glm::vec2 position, float yAngle)
{
	constexpr float k_Size = 21.5f;
	constexpr float k_MiddleRadius = 15.05f;                      ///< the size x 0.7
	constexpr float k_ArmOffset = 3.83f;                          ///< the first arm's angle from the heart's y angle
	constexpr float k_ArmsApart = 0.8975979f;                     ///< 2 pi / 7, as a float
	constexpr std::array<float, 3> k_Turns {0.06f, 0.16f, 0.27f}; ///< the turns of the last three circles of arms 0 and 1
	constexpr std::array<float, 2> k_TurnedSteps {0.1f, 0.07f};   ///< the size shares between the turned circles
	const float armRadius = 1.4f * (k_Size * 0.11f);
	std::vector<Circle> circles;
	circles.reserve(33);
	circles.push_back({.centre = position, .radius = k_MiddleRadius});
	float angle = yAngle + k_ArmOffset;
	for (int arm = 0; arm < 7; ++arm)
	{
		// the sine and cosine of the arm, kept as floats
		const float s = static_cast<float>(std::sin(static_cast<double>(angle)));
		const float c = static_cast<float>(std::cos(static_cast<double>(angle)));
		float dist = k_Size * 0.74f;
		const int count = arm < 2 ? 6 : 4;
		for (int k = 0; k < count; ++k)
		{
			if (k < 3 || arm >= 2)
			{
				circles.push_back({.centre = {dist * c + position.x, dist * s + position.y}, .radius = armRadius});
				dist = dist + k_Size * 0.2f;
				continue;
			}
			// the last three of arms 0 and 1 turned towards each other: the cosine and sine of the turned angle are not
			// rounded before they are multiplied
			const float turn = k_Turns.at(static_cast<size_t>(k - 3));
			const float a = (arm == 0 ? turn : -turn) + angle;
			const auto x = static_cast<float>(std::cos(static_cast<double>(a)) * static_cast<double>(dist));
			const auto z = static_cast<float>(std::sin(static_cast<double>(a)) * static_cast<double>(dist));
			circles.push_back({.centre = {x + position.x, z + position.y}, .radius = armRadius});
			if (k < 5)
			{
				dist = dist + k_Size * k_TurnedSteps.at(static_cast<size_t>(k - 3));
			}
		}
		angle = angle + k_ArmsApart;
	}
	return circles;
}

std::vector<Circle> openblack::ecs::map_collide::WorshipSiteCircles(const glm::mat3& rotation, glm::vec3 scale,
                                                                    glm::vec3 position)
{
	// the points of the site's model {x, height, z}; the radius is 1.3 x the height
	constexpr std::array<glm::vec3, 10> k_Points {{
	    {2.448f, 1.4f, -34.171f},
	    {3.348f, 1.4f, -36.551f},
	    {24.71f, 1.4f, -24.133f},
	    {26.359f, 1.4f, -25.951f},
	    {3.93f, 2.0f, -39.084f},
	    {27.452f, 2.0f, -28.503f},
	    {7.405f, 3.0f, -40.707f},
	    {12.317f, 3.0f, -39.988f},
	    {22.537f, 3.0f, -35.073f},
	    {26.905f, 3.0f, -33.063f},
	}};
	constexpr float k_RadiusPerHeight = 1.3f;
	// the site's matrix: each axis of the rotation scaled by its scale
	const glm::vec3 xAxis = rotation[0] * scale.x;
	const glm::vec3 yAxis = rotation[1] * scale.y;
	const glm::vec3 zAxis = rotation[2] * scale.z;
	std::vector<Circle> circles;
	circles.reserve(k_Points.size());
	for (const auto& p : k_Points)
	{
		const float x = xAxis.x * p.x + yAxis.x * p.y + zAxis.x * p.z + position.x;
		const float z = xAxis.z * p.x + yAxis.z * p.y + zAxis.z * p.z + position.z;
		circles.push_back({.centre = {x, z}, .radius = k_RadiusPerHeight * p.y});
	}
	return circles;
}

void openblack::ecs::map_collide::SetOwnChildren(Shape& shape, const std::vector<Circle>& circles)
{
	shape.children.clear();
	shape.childRadii.clear();
	shape.childRadius = 0.0f;
	shape.children.reserve(circles.size());
	shape.childRadii.reserve(circles.size());
	for (const auto& circle : circles)
	{
		shape.children.push_back(circle.centre);
		shape.childRadii.push_back(circle.radius);
	}
}

bool openblack::ecs::map_collide::IsOkToCreateAtPos(glm::vec3 position, std::string_view command)
{
	// the collision with fixed objects on the live map cells (ecs::map_cells: everything in the map now, not only
	// what the land script made), bit 8 clear -> yes. Off the map it is all bits set, which has the bit. MapCoords
	// x, z only (FromMetres: the altitude is not read)
	const auto collide = map_cells::CollideWithFixed(map_coords::FromMetres(glm::vec2(position.x, position.z)));
	if ((collide & sea_cells::k_CollideFixed) == 0)
	{
		return true;
	}
	// set: whether it is water (ecs::sea_cells, the cell of the 16.16 MapCoords): bit 0x10 of the land cell
	// (hasWater); no landscape cell (off the map, or an empty block) counts as water
	if (sea_cells::IsWater(position))
	{
		return true;
	}
	if (LogRejections())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "isok: {} at ({:.2f}, {:.2f}) rejected (collide 0x{:X})", command,
		                   position.x, position.z, collide);
	}
	return false;
}
