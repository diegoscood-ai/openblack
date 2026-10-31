/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashPull.h"

#include <cmath>

#include <algorithm>

#include "3D/MapCoords.h"

using namespace openblack;

glm::vec3 creature_leash::TiedPointOf(const map_coords::MapCoords& place, float landHeight)
{
	return {map_coords::ToMetres(place.x), landHeight + place.altitude, map_coords::ToMetres(place.z)};
}

creature_leash::RopeScreenPoint creature_leash::ProjectRopePoint(const affine::AffineMatrix& worldToClipping, float nearClip,
                                                                 glm::ivec2 screen, const glm::vec3& point)
{
	// X, Y and Z, the depth, each summed x, z, y as the game's projection does
	const auto clip = affine::ToClipForScreenTest(worldToClipping, point);
	if (clip.z < nearClip)
	{
		return {.pixel = glm::ivec2(0), .depth = clip.z, .projected = false};
	}
	const float inverse = 1.0f / clip.z;
	const float halfWidth = static_cast<float>(screen.x) * 0.5f;
	const float halfHeight = static_cast<float>(screen.y) * 0.5f;
	const float y = (1.0f - inverse * clip.y) * halfHeight;
	const float x = (inverse * clip.x + 1.0f) * halfWidth;
	return {.pixel = {map_coords::FtoL(x), map_coords::FtoL(y)}, .depth = clip.z, .projected = true};
}

float creature_leash::RopeHitDepth(const affine::AffineMatrix& worldToCamera, const glm::vec3& from, const glm::vec3& to,
                                   float along)
{
	// the point along the stretch, then its depth summed z, y, x
	const glm::vec3 point {(to.x - from.x) * along + from.x, (to.y - from.y) * along + from.y,
	                       (to.z - from.z) * along + from.z};
	const auto& m = worldToCamera.m;
	return ((point.z * m[8] + point.y * m[5]) + point.x * m[2]) + m[11];
}

float creature_leash::PullLength(const glm::vec3& hand, const glm::vec3& tied, float radius2D)
{
	// single precision, in the game's order: the squares of the depth and the height first, then the width's
	const float dx = hand.x - tied.x;
	const float dy = hand.y - tied.y;
	const float dz = hand.z - tied.z;
	const float sum = (dz * dz + dy * dy) + dx * dx;
	const float distance = std::sqrt(sum);
	const float least = radius2D + radius2D;
	// the distance unless twice the radius is at least as long (an unordered pair keeps the distance)
	return least >= distance ? least : distance;
}

std::optional<float> creature_leash::RopeSegmentHit(glm::ivec2 mouse, glm::ivec2 a, int32_t aWidth, glm::ivec2 b,
                                                    int32_t bWidth)
{
	// the box round the stretch, the wider end's width out
	const int32_t width = std::max(aWidth, bWidth);
	if (mouse.x < std::min(a.x, b.x) - width || mouse.x > std::max(a.x, b.x) + width)
	{
		return std::nullopt;
	}
	if (mouse.y < std::min(a.y, b.y) - width || mouse.y > std::max(a.y, b.y) + width)
	{
		return std::nullopt;
	}
	// within either end's round
	const auto withinEnd = [&mouse](glm::ivec2 end, int32_t endWidth) {
		const auto d = mouse - end;
		return mouse.x >= end.x - endWidth && mouse.x <= end.x + endWidth && mouse.y >= end.y - endWidth &&
		       mouse.y <= end.y + endWidth && d.y * d.y + d.x * d.x < endWidth * endWidth;
	};
	if (withinEnd(a, aWidth))
	{
		return 0.0f;
	}
	if (withinEnd(b, bWidth))
	{
		return 1.0f;
	}
	// along the stretch: the cosine between it and the line to the mouse, and the mouse's distance from the stretch's
	// line against the width there, all in single precision in the game's order
	const auto toMouse = mouse - a;
	const auto along = b - a;
	const float mouseLength = std::sqrt(static_cast<float>(toMouse.x * toMouse.x + toMouse.y * toMouse.y));
	const float stretchLength = std::sqrt(static_cast<float>(along.x * along.x + along.y * along.y));
	const float lengths = stretchLength * mouseLength;
	if (lengths < 0.1f)
	{
		return std::nullopt;
	}
	const float t = static_cast<float>(toMouse.x * along.x + toMouse.y * along.y) / lengths;
	if (t < 0.0f || t > 1.0f)
	{
		return std::nullopt;
	}
	const float widthThere = static_cast<float>(bWidth - aWidth) * t + static_cast<float>(aWidth);
	const float across = mouseLength * mouseLength - t * t * stretchLength * stretchLength;
	if (widthThere * widthThere < across)
	{
		return std::nullopt;
	}
	return t;
}

std::optional<creature_leash::RopeHit> creature_leash::PickRope(glm::ivec2 mouse, std::span<const RopeScreenPoint> points,
                                                                uint16_t screenWidth, float halfWidth)
{
	if (points.size() < 2)
	{
		return std::nullopt;
	}
	const float drawnWidth = static_cast<float>(screenWidth) * halfWidth * k_RopePickWidthScale;
	const auto widthAt = [drawnWidth](float depth) {
		return std::max(map_coords::FtoL(drawnWidth / depth), k_RopePickLeastWidth);
	};
	for (size_t i = 1; i < points.size(); ++i)
	{
		const auto& from = points[i - 1];
		const auto& to = points[i];
		// both on the screen and both further in front of the camera than the nearest a point may be (an unordered depth
		// never is)
		if (!from.projected || !to.projected || !(from.depth > k_RopePickNearest) || !(to.depth > k_RopePickNearest))
		{
			continue;
		}
		if (const auto along = RopeSegmentHit(mouse, from.pixel, widthAt(from.depth), to.pixel, widthAt(to.depth)))
		{
			return RopeHit {.segment = i - 1, .along = *along};
		}
	}
	return std::nullopt;
}
