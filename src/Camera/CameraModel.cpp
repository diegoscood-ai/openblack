/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraModel.h"

#include <cassert>
#include <cmath>

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "DefaultWorldCameraModel.h"

using namespace openblack;

CameraModel::~CameraModel() = default;

std::unique_ptr<CameraModel> CameraModel::CreateModel(CameraModel::Model model)
{
	switch (model)
	{
	case CameraModel::Model::DefaultWorld:
		return std::make_unique<DefaultWorldCameraModel>();
	default:
		assert(false);
		return nullptr;
	}
}

CameraModel::FlightPath CameraModel::CharterFlight(const LandIslandInterface& land, glm::vec3 destinationOrigin,
                                                   glm::vec3 destinationFocus, glm::vec3 currentOrigin, float rise)
{
	// Halfway: the two origins added, then halved
	auto via = (destinationOrigin + currentOrigin) * 0.5f;
	// Raised by their distance across the land times the rise
	const float dx = currentOrigin.x - destinationOrigin.x;
	const float dz = currentOrigin.z - destinationOrigin.z;
	via.y = std::sqrt(dz * dz + dx * dx) * rise + via.y;
	// At least 10 above the land under it, read as the hand reads the land: the metres through the map's fixed point
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	// (inferred) GetHeightAt stands for the original's altitude, as for the hand
	const float lowest = land.GetHeightAt(glm::vec2(lookup(via.x), lookup(via.z))) + 10.0f;
	if (lowest > via.y)
	{
		via.y = lowest;
	}

	return {destinationOrigin, destinationFocus, std::make_optional<glm::vec3>(via)};
}
