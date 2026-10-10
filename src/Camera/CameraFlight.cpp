/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraFlight.h"

#include <cmath>
#include <cstddef>

#include <array>
#include <tuple>

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ScriptCamera.h"

namespace openblack::camera_flight
{
namespace
{
constexpr float k_NearDistance = 50.0f;
constexpr float k_NearShare = 0.8f;
constexpr float k_FarDistance = 100.0f;
constexpr float k_FarShare = 0.1f;

constexpr size_t k_Headings = 32;
/// pi / 16 between two headings
constexpr float k_HeadingStep = 0.196349546f;
/// The samples at 3/8 .. 7/8 of the distance
constexpr int k_FirstSample = 3;
constexpr int k_EndSample = 8;
constexpr float k_SampleStep = 0.125f;
/// The heading kept scores 50 more, the opposite one 50 less (a double)
constexpr double k_AheadBonus = 50.0;
/// A sample at 0.3 of the distance is read and not used
constexpr float k_UnusedShare = 0.3f;
/// Below any score
constexpr float k_NoScore = -1e20f;

/// The new pitch: the old one x 0.2, the land's tilt x 0.5 x 0.2 (a double) and 3 pi / 25 (a double)
constexpr float k_OldPitchShare = 0.2f;
constexpr float k_LandPitchHalf = 0.5f;
constexpr double k_LandPitchShare = 0.2;
constexpr double k_PitchBase = 0.37699114390179034;
/// pi / 8 and pi / 3
constexpr float k_LowestPitch = 0.392699093f;
constexpr float k_HighestPitch = 1.04719758f;

/// The ArenaLookPoint's height, in radii
constexpr float k_ArenaLookHeight = 0.5f;

/// Where the land is read for a point: its x and z through the map's fixed point, as the hand's turn reads the land
[[nodiscard]] glm::vec2 MapPoint(glm::vec3 point)
{
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	return {lookup(point.x), lookup(point.z)};
}

/// The land's height under a point. (inferred) GetHeightAt stands for the original's altitude, as for the hand
[[nodiscard]] float LandAt(const LandIslandInterface& land, glm::vec3 point)
{
	return land.GetHeightAt(MapPoint(point));
}
} // namespace

float ShapeDistance(float distance)
{
	if (distance < k_NearDistance)
	{
		distance = (k_NearDistance - distance) * k_NearShare + distance;
	}
	if (distance > k_FarDistance)
	{
		distance = (k_FarDistance - distance) * k_FarShare + distance;
	}
	return distance;
}

float FindBestAngle(const LandIslandInterface& land, float heading, float distance, glm::vec3 point, float& pitch)
{
	// How far the point stands above the land along each heading, level from the point
	std::array<float, k_Headings> scores {};
	for (size_t i = 0; i < k_Headings; ++i)
	{
		const float angle = static_cast<float>(i) * k_HeadingStep + heading;
		for (int j = k_FirstSample; j < k_EndSample; ++j)
		{
			const float along = static_cast<float>(j) * distance * k_SampleStep;
			const auto sample = script_camera::PointFromDistanceHeadingAndPitch(point, along, angle, 0.0f);
			scores[i] = (point.y - LandAt(land, sample)) + scores[i];
		}
	}

	// Plus 50 x the cosine of the turn from the heading; the first highest wins
	const float unusedDistance = distance * k_UnusedShare;
	size_t best = 0;
	float bestScore = k_NoScore;
	for (size_t i = 0; i < k_Headings; ++i)
	{
		const float turn = static_cast<float>(i) * k_HeadingStep;
		scores[i] = static_cast<float>(std::cos(static_cast<double>(turn)) * k_AheadBonus) + scores[i];
		// The land at 0.3 of the distance is read and thrown away: no effect
		std::ignore =
		    LandAt(land, script_camera::PointFromDistanceHeadingAndPitch(point, unusedDistance, turn + heading, 0.0f));
		if (scores[i] > bestScore)
		{
			bestScore = scores[i];
			best = i;
		}
	}

	// The land's own tilt at the point: the pitch from the point up its normal
	// (inferred) GetNormalAt stands for the original's land normal at the map position
	const auto normal = land.GetNormalAt(MapPoint(point));
	float unusedHeading = 0.0f;
	float landPitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(point + normal, point, unusedHeading, landPitch);

	const float landShare = static_cast<float>(static_cast<double>(landPitch * k_LandPitchHalf) * k_LandPitchShare);
	const float sum = pitch * k_OldPitchShare + landShare;
	float newPitch = static_cast<float>(static_cast<double>(sum) + k_PitchBase);
	// Not above the lowest (or not a number) goes to it; then not below the highest goes to it
	if (!(newPitch > k_LowestPitch))
	{
		newPitch = k_LowestPitch;
	}
	else if (!(newPitch < k_HighestPitch))
	{
		newPitch = k_HighestPitch;
	}
	pitch = newPitch;

	return static_cast<float>(best) * k_HeadingStep + heading;
}

View ViewOfPoint(const LandIslandInterface& land, glm::vec3 from, glm::vec3 focus)
{
	float heading = 0.0f;
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(from, focus, heading, pitch);
	// Their distance in 3D, z first
	const float dx = focus.x - from.x;
	const float dy = focus.y - from.y;
	const float dz = focus.z - from.z;
	const float distance = ShapeDistance(std::sqrt((dz * dz + dy * dy) + dx * dx));
	heading = FindBestAngle(land, heading, distance, focus, pitch);
	return {.origin = script_camera::PointFromDistanceHeadingAndPitch(focus, distance, heading, pitch), .focus = focus};
}

glm::vec3 ArenaLookPoint(glm::vec3 centre, float radius)
{
	return {centre.x + radius, radius * k_ArenaLookHeight + centre.y, centre.z};
}

} // namespace openblack::camera_flight
