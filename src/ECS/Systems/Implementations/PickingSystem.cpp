/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PickingSystem.h"

#include <glm/common.hpp>
#include <glm/vector_relational.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/LandLine.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/HandLandPoint.h"
#include "ECS/HandMeshModel.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The island's cells as the line test reads them, none without an island
land_line::CellLookup Cells()
{
	if (!Locator::terrainSystem::has_value())
	{
		return [](int32_t /*x*/, int32_t /*z*/) -> std::optional<land_line::CellHeights> { return std::nullopt; };
	}
	return land_line::CellsOf(Locator::terrainSystem::value());
}

uint16_t CellsPerSide()
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : land_line::k_CellsPerSide;
}

float HeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// The land's height read at a point quantised as the hand's lookups are
float QuantisedHeightAt(glm::vec2 point)
{
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	return HeightAt({lookup(point.x), lookup(point.y)});
}
} // namespace

std::optional<glm::vec3> PickingSystem::LandAlong(glm::vec3 from, glm::vec3 to) const
{
	if (const auto hit = land_line::LandAlong(from, to, Cells(), CellsPerSide()))
	{
		return glm::vec3(hit->x, HeightAt(*hit), hit->y);
	}
	return std::nullopt;
}

std::optional<glm::vec3> PickingSystem::LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera) const
{
	if (const auto hit = land_line::LandOrSeaAlong(from, to, camera, Cells(), CellsPerSide()))
	{
		return glm::vec3(hit->x, HeightAt(*hit), hit->y);
	}
	return std::nullopt;
}

std::optional<glm::vec3> PickingSystem::LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint, bool withSea) const
{
	// the land's height read at the point quantised as the hand's lookups are, then the point kept within reach of the
	// map's middle
	const auto under = land_line::UnderPixel(camera, nearPoint, withSea, Cells(), QuantisedHeightAt, CellsPerSide());
	return under ? std::optional(land_line::KeptInReach(*under)) : std::nullopt;
}

std::optional<glm::vec3> PickingSystem::LandFrom2DPoint(glm::ivec2 pixel, bool shift, float depth) const
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto screenSize = Locator::windowing::value().GetSize();
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return std::nullopt;
	}
	const auto& camera = Locator::camera::value();
	const auto eye = camera.GetEye();
	if (glm::any(glm::isnan(eye)))
	{
		return std::nullopt;
	}
	// the pixel's point on the near plane the last camera update took, the line from the eye through it moved on or not
	const float nearClip = camera.GetPixelNearClip();
	const auto nearPoint = camera.PixelNearPoint(eye, screenSize, pixel, nearClip);
	const auto line = shift ? ecs::hand_land_point::ShiftedAlongView(eye, nearPoint, depth, nearClip)
	                        : ecs::hand_land_point::Line {.from = eye, .to = nearPoint};
	// the land, or the sea within reach of the eye, along it
	const auto hit = land_line::LandOrSeaAlong(line.from, line.to, eye, Cells(), CellsPerSide());
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(hit->x, QuantisedHeightAt(*hit), hit->y);
}

void PickingSystem::PickUnderCursor(const Frame& /*frame*/)
{
	// Nothing picks under the cursor this way yet: the hand still picks its object along its own ray in the same frame,
	// so the pick stays empty
}

std::optional<screen_pick::MeshHit> PickingSystem::FeelModel(entt::entity object, glm::vec3 origin, glm::vec3 direction) const
{
	// The hand's own test of the object's mesh where its pick finds it. That test gives the point, the normal turned
	// along the line and the distance; which triangle and where on it are not worked out, and stay 0
	const auto hit = ecs::hand_mesh_model::Feel(object, origin, direction);
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	return screen_pick::MeshHit {.point = hit->point, .normal = hit->normal, .distance = hit->distance};
}

PickingSystemInterface::Pick picking::Locked(const PickingSystemInterface::Pick& previous, std::optional<glm::vec3> land,
                                             float landDistance)
{
	auto pick = previous;
	if (land.has_value())
	{
		pick.land = land;
		pick.point = land;
		pick.distance = landDistance;
	}
	return pick;
}

void picking::CarryHover(const PickingSystemInterface::Pick& previous, PickingSystemInterface::Pick& next, float seconds)
{
	if (next.object == previous.hoverObject)
	{
		next.hoverObject = previous.hoverObject;
		next.hoverSeconds = previous.hoverSeconds + seconds;
	}
	else
	{
		next.hoverObject = next.object;
		next.hoverSeconds = 0.0f;
	}
}
