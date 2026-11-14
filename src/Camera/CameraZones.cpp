/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraZones.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>

#include "Common/X87Precision.h"
#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::camera_zones;

namespace
{
/// A point this close to a corner, squared, is on the fence; a side shorter than this, squared, is skipped
constexpr float k_OnCorner = 1e-8f;
/// A line this near parallel to a side doesn't cross it, and a crossing this near the point is the point
constexpr float k_Parallel = 1e-4f;
/// The camera has hardly moved this frame when it is this near, squared, across the ground to where it started
constexpr float k_HardlyMoved = 1e-4f;
/// The search round the crossing turns by a 64th of a turn, up to 31 of them either way
constexpr float k_SearchStep = 0.0981747732f;
constexpr int k_SearchSteps = 32;
/// The original maps' cells a side, and their disc's middle and radius
constexpr float k_OriginalCellsPerSide = 512.0f;
constexpr float k_OriginalDiscMiddle = 2560.0f;
constexpr float k_OriginalDiscRadius = 5120.0f;
/// The height of the disc's middle, which the camera's heights are measured from and put back to as well
constexpr float k_DiscMiddleHeight = 0.0f;

/// Zero, or not a number, as a compare that is equal or unordered reads it
bool ZeroOrNotANumber(float value)
{
	return value == 0.0f || std::isnan(value);
}

float DistanceSquaredAcross(const glm::vec3& a, const glm::vec3& b)
{
	const float x = a.x - b.x;
	const float z = a.z - b.z;
	return x * x + z * z;
}
} // namespace

Zones camera_zones::AfterMapClear(Zones zones)
{
	Zones cleared;
	cleared.exclusions = std::move(zones.exclusions);
	std::erase_if(cleared.exclusions, [](const Exclusion& exclusion) { return exclusion.saved; });
	return cleared;
}

float camera_zones::HeldDomeSize(float value)
{
	return value > 0.0f ? std::min(value, k_DomeLimit) : 0.0f;
}

Exclusion camera_zones::MadeDome(const glm::vec3& position, float radius, float height)
{
	return AdjustedDome(position, radius, height == 0.0f ? radius : height);
}

Exclusion camera_zones::AdjustedDome(const glm::vec3& position, float radius, float height)
{
	return {.position = position,
	        .radius = HeldDomeSize(radius),
	        .height = HeldDomeSize(height),
	        .kind = Exclusion::Kind::Dome,
	        .saved = false};
}

camera_zones::DomeSize camera_zones::CreatureDomeSize(float bodyRadius, float size)
{
	return {.radius = bodyRadius * k_CreatureDomeScale, .height = k_CreatureDomeScale * (k_StandingHeightPerSize * size)};
}

camera_zones::DomeSize camera_zones::TempleDomeSize(float meshHalfHeight)
{
	return {.radius = k_TempleDomeRadius, .height = meshHalfHeight * 2.0f};
}

bool camera_zones::InsideExclusion(std::span<const Exclusion> exclusions, const glm::vec3& point)
{
	return std::ranges::any_of(exclusions, [&point](const Exclusion& exclusion) {
		const float dx = exclusion.position.x - point.x;
		const float dz = exclusion.position.z - point.z;
		const float reach = k_ExclusionMargin + exclusion.radius;
		// across the ground first, strictly within the radius (not a number is outside)
		if (!(dz * dz + dx * dx < reach * reach))
		{
			return false;
		}
		switch (exclusion.kind)
		{
		case Exclusion::Kind::Cylinder:
			return true;
		case Exclusion::Kind::Dome:
		{
			// the ellipsoid: across by the radius, up by the height, both with the margin; not a number is inside
			const float x = (point.x - exclusion.position.x) / reach;
			const float y = (point.y - exclusion.position.y) / (k_ExclusionMargin + exclusion.height);
			const float z = (point.z - exclusion.position.z) / reach;
			return !((z * z + y * y) + x * x >= 1.0f);
		}
		case Exclusion::Kind::Other:
			return false;
		}
		return false;
	});
}

std::vector<Exclusion> camera_zones::NearbyExclusions(std::span<const Exclusion> exclusions, const glm::vec3& originAtStart)
{
	constexpr float k_Reach = k_ExclusionMargin * 2.0f;
	std::vector<Exclusion> nearby;
	for (const auto& exclusion : exclusions)
	{
		const float dx = exclusion.position.x - originAtStart.x;
		const float dz = exclusion.position.z - originAtStart.z;
		const float reach = k_Reach + exclusion.radius;
		if (!(dz * dz + dx * dx < reach * reach))
		{
			continue;
		}
		if (exclusion.kind != Exclusion::Kind::Dome || originAtStart.y - exclusion.position.y < k_Reach + exclusion.height)
		{
			nearby.push_back(exclusion);
		}
	}
	return nearby;
}

CylinderPush camera_zones::PushOutOfCylinders(std::span<const Exclusion> nearby, const glm::vec3& origin)
{
	CylinderPush result {.origin = origin};
	for (const auto& cylinder : nearby)
	{
		const float dx = cylinder.position.x - result.origin.x;
		const float dz = cylinder.position.z - result.origin.z;
		const float reach = k_ExclusionMargin + cylinder.radius;
		if (!(dz * dz + dx * dx < reach * reach) || cylinder.kind != Exclusion::Kind::Cylinder)
		{
			continue;
		}
		const float x = result.origin.x - cylinder.position.x;
		const float z = result.origin.z - cylinder.position.z;
		// at least 0.01 from the axis, a distance that is not a number taken as 0.01
		const float fromAxis = std::sqrt(z * z + x * x);
		const float scale = reach / (fromAxis > k_NearestToCylinderAxis ? fromAxis : k_NearestToCylinderAxis);
		result.origin.x = cylinder.position.x + scale * x;
		result.origin.z = cylinder.position.z + scale * z;
		++result.pushes;
	}
	return result;
}

namespace
{
/// The roof of a dome over a point across the ground, where the point is under it: strictly within the radius, a
/// distance that is not a number counting as within. The squares are added z then x, the radius squared kept as a float
std::optional<float> DomeRoof(const Exclusion& dome, glm::vec2 point)
{
	const float dx = dome.position.x - point.x;
	const float dz = dome.position.z - point.y;
	const float radiusSquared = dome.radius * dome.radius;
	if (dz * dz + dx * dx >= radiusSquared)
	{
		return std::nullopt;
	}
	const float x = point.x - dome.position.x;
	const float z = point.y - dome.position.z;
	return std::sqrt(1.0f - (z * z + x * x) / radiusSquared) * dome.height + dome.position.y;
}

/// A way made a unit long as the probe makes its ball's axes: the squares added x, z then y, and each part times one
/// over the length; left as it is when all three parts are 0 or not a number
glm::vec3 UnitXzy(const glm::vec3& v)
{
	if (ZeroOrNotANumber(v.x) && ZeroOrNotANumber(v.y) && ZeroOrNotANumber(v.z))
	{
		return v;
	}
	return game_math::x87::Scaled<game_math::x87::SumOrder::Xzy>(v);
}

/// The same with the squares added z, y then x, as a dome's normal is made
glm::vec3 UnitZyx(const glm::vec3& v)
{
	if (ZeroOrNotANumber(v.x) && ZeroOrNotANumber(v.y) && ZeroOrNotANumber(v.z))
	{
		return v;
	}
	return game_math::x87::Scaled<game_math::x87::SumOrder::Zyx>(v);
}

/// The points round each ring of the probe, from straight below the camera to straight above
constexpr std::array<int, 5> k_RingPoints {1, 4, 6, 4, 1};
} // namespace

float camera_zones::SurfaceHeight(const Ground& ground, std::span<const Exclusion> nearby, bool withDomes, glm::vec2 point)
{
	float height = ground.height(point);
	if (!withDomes)
	{
		return height;
	}
	for (const auto& dome : nearby)
	{
		if (dome.kind != Exclusion::Kind::Dome)
		{
			continue;
		}
		// the roof when the land is not above it (a roof that is not a number is taken too)
		if (const auto roof = DomeRoof(dome, point); roof.has_value() && !(height > *roof))
		{
			height = *roof;
		}
	}
	return height;
}

GroundPush camera_zones::PushOffTheGround(const Ground& ground, std::span<const Exclusion> nearby, bool exclusionsOn,
                                          const glm::vec3& origin, const glm::vec3& focus)
{
	namespace x87 = game_math::x87;
	GroundPush result {.origin = origin, .focus = focus};
	// The ball's axes: back from what the camera looks at to the camera, to its side level with the ground, and up from
	// those two; each scaled by the margin
	const auto back = UnitXzy(origin - focus);
	const auto side = UnitXzy(glm::vec3(back.z - 0.0f, 0.0f, 0.0f - back.x));
	const auto up = UnitXzy(glm::vec3((side.z * back.y) - (side.y * back.z), (side.x * back.z) - (back.x * side.z),
	                                  (back.x * side.y) - (side.x * back.y)));
	const auto sideRow = k_ExclusionMargin * side;
	const auto upRow = k_ExclusionMargin * up;
	const auto backRow = k_ExclusionMargin * back;
	constexpr float k_MostPush = k_ExclusionMargin * 0.5f;
	for (int ring = -2; ring <= 2; ++ring)
	{
		// the ring's angle, its cosine and sine kept as floats
		const float ringAngle = static_cast<float>(ring) * glm::quarter_pi<float>();
		const float ringCos = x87::Store(x87::Cos(ringAngle));
		const float ringSin = x87::Store(x87::Sin(ringAngle));
		const int count = k_RingPoints.at(static_cast<size_t>(ring + 2));
		const auto counted = static_cast<float>(count);
		for (int i = 0; i < count; ++i)
		{
			// the point on the ball: its side and back parts each the angle's sine or cosine times the ring's cosine,
			// one rounding; x adds the up, back and side terms in that order, y and z the up, side and back terms, then
			// each the camera's place
			const float angle = (static_cast<float>(i) * glm::two_pi<float>()) / counted;
			const float alongSide = x87::Store(x87::Mul(x87::Sin(angle), ringCos));
			const float alongBack = x87::Store(x87::Mul(x87::Cos(angle), ringCos));
			const glm::vec3 point {
			    (((upRow.x * ringSin) + (backRow.x * alongBack)) + (alongSide * sideRow.x)) + result.origin.x,
			    (((upRow.y * ringSin) + (sideRow.y * alongSide)) + (backRow.y * alongBack)) + result.origin.y,
			    (((upRow.z * ringSin) + (sideRow.z * alongSide)) + (backRow.z * alongBack)) + result.origin.z};
			const glm::vec2 across {point.x, point.z};
			if (!(SurfaceHeight(ground, nearby, exclusionsOn, across) > point.y))
			{
				continue;
			}
			// the surface here and its normal: the land's, or a dome's roof where it is above the land, facing out from
			// the dome's middle
			float surface = ground.height(across);
			auto normal = ground.normal(across);
			bool onDome = false;
			for (const auto& dome : nearby)
			{
				if (dome.kind != Exclusion::Kind::Dome)
				{
					continue;
				}
				if (const auto roof = DomeRoof(dome, across); roof.has_value() && *roof > surface)
				{
					normal = UnitZyx(point - dome.position);
					surface = *roof;
					onDome = true;
				}
			}
			const float depth = x87::DotZyx(normal, glm::vec3(point.x, surface, point.z) - point);
			const auto push = normal * (depth > k_MostPush ? k_MostPush : depth);
			result.origin += push;
			result.pushed = true;
			++result.points;
			if (onDome)
			{
				++result.onDomes;
			}
			if (const float away = x87::DotZyx(push, back); away > 0.0f)
			{
				result.focus += push * away;
				result.focusMoved = true;
			}
		}
	}
	return result;
}

Crossing camera_zones::CrossFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& point,
                                  const glm::vec3& direction)
{
	Crossing crossing {.inside = true, .closest = point};
	if (!fenceOn || fence.size() <= 2)
	{
		return crossing;
	}
	uint32_t ahead = 0;
	float nearestAhead = 1e20f;
	float nearestBehind = 1e20f;
	std::optional<std::pair<glm::vec3, glm::vec3>> hitAhead;
	std::optional<std::pair<glm::vec3, glm::vec3>> hitBehind;
	const auto* previous = &fence.back();
	for (const auto& corner : fence)
	{
		const float sideX = previous->x - corner.x;
		const float sideZ = previous->z - corner.z;
		const float sideSquared = sideZ * sideZ + sideX * sideX;
		if (DistanceSquaredAcross(point, corner) < k_OnCorner)
		{
			return crossing;
		}
		if (k_OnCorner < sideSquared)
		{
			const float across = -sideX;
			const float facing = direction.x * sideZ + direction.z * across;
			if (k_Parallel < std::abs(facing))
			{
				// How far along the line it meets the side, and how far along the side from this corner
				const float along = (((corner.z * across + corner.x * sideZ) - point.x * sideZ) - point.z * across) / facing;
				const glm::vec3 hit {along * direction.x + point.x, direction.y * along + point.y,
				                     along * direction.z + point.z};
				const float onSide = ((hit.z - corner.z) * sideZ + (hit.x - corner.x) * sideX) / sideSquared;
				if (0.0f <= onSide && onSide < 1.0f)
				{
					if (std::abs(along) < k_Parallel)
					{
						return crossing;
					}
					const glm::vec3 normal {sideZ, 0.0f, across};
					if (along > 0.0f)
					{
						++ahead;
						if (along < nearestAhead)
						{
							nearestAhead = along;
							hitAhead = {hit, normal};
						}
					}
					else if (-along < nearestBehind)
					{
						nearestBehind = -along;
						hitBehind = {hit, normal};
					}
				}
			}
		}
		previous = &corner;
	}
	bool found = false;
	if (const auto& hit = hitAhead.has_value() ? hitAhead : hitBehind; hit.has_value())
	{
		crossing.closest = hit->first;
		crossing.normal = hit->second;
		found = true;
	}
	// A corner nearer than the crossing is nearer still
	float nearest = DistanceSquaredAcross(crossing.closest, point);
	for (const auto& corner : fence)
	{
		const float distance = DistanceSquaredAcross(corner, point);
		if (distance < nearest || !found)
		{
			crossing.closest = corner;
			found = true;
			nearest = distance;
		}
	}
	crossing.inside = (ahead & 1u) != 0u;
	return crossing;
}

camera_zones::FenceKeep camera_zones::PushInsideFence(std::span<const glm::vec3> fence, bool fenceOn,
                                                      const glm::vec3& originAtStart, const glm::vec3& origin,
                                                      const glm::vec3& focus, float searchRadius)
{
	if (!fenceOn)
	{
		return {.inclusionDistance = k_NoFenceDistance};
	}
	// Hardly moved, or a distance that is not a number: the line goes towards what it looks at
	const auto towards = !(DistanceSquaredAcross(originAtStart, origin) >= k_HardlyMoved) ? focus : originAtStart;
	const auto direction = towards - origin;
	const auto crossing = CrossFence(fence, fenceOn, origin, direction);
	if (crossing.inside)
	{
		const float dx = crossing.closest.x - origin.x;
		const float dz = crossing.closest.z - origin.z;
		return {.inclusionDistance = std::sqrt(dz * dz + dx * dx)};
	}
	// The way across the ground from the camera to the crossing, of length 1 unless every part of it is 0 (or not a
	// number)
	glm::vec3 way {crossing.closest.x - origin.x, 0.0f, crossing.closest.z - origin.z};
	if (!(ZeroOrNotANumber(way.x) && ZeroOrNotANumber(way.z)))
	{
		way *= 1.0f / std::sqrt((way.z * way.z + way.y * way.y) + way.x * way.x);
	}
	auto target = crossing.closest;
	for (int step = 0; step < k_SearchSteps; ++step)
	{
		for (const int turn : {-step, step})
		{
			// The angle is a single precision value; its sine and cosine are taken at full precision and rounded once as
			// the radius scales them
			const float angle = static_cast<float>(turn) * k_SearchStep;
			const auto sine = static_cast<float>(std::sin(static_cast<double>(angle)) * static_cast<double>(searchRadius));
			const auto cosine = static_cast<float>(std::cos(static_cast<double>(angle)) * static_cast<double>(searchRadius));
			const glm::vec3 candidate =
			    crossing.closest + glm::vec3(sine * -way.z + cosine * way.x, 0.0f, sine * way.x + cosine * way.z);
			if (CrossFence(fence, fenceOn, candidate, direction).inside)
			{
				return {.target = candidate, .inclusionDistance = 0.0f};
			}
		}
	}
	return {.target = target, .inclusionDistance = 0.0f};
}

MapDisc camera_zones::MapDiscFor(uint16_t cellsPerSide)
{
	const float scale = static_cast<float>(cellsPerSide) / k_OriginalCellsPerSide;
	return {.centre = glm::vec2(k_OriginalDiscMiddle * scale), .radius = k_OriginalDiscRadius * scale};
}

float camera_zones::AcrossFromMiddle(const MapDisc& disc, const glm::vec3& point)
{
	const float dx = point.x - disc.centre.x;
	const float dz = point.z - disc.centre.y;
	return std::sqrt(dz * dz + dx * dx);
}

bool camera_zones::InsideMapDisc(const MapDisc& disc, const glm::vec3& point)
{
	const float dx = point.x - disc.centre.x;
	const float dz = point.z - disc.centre.y;
	return dz * dz + dx * dx < disc.radius * disc.radius;
}

float camera_zones::FrameMoveLimit(float focusDistance, float seconds)
{
	// The least taken only when it is above the speed's distance, which is kept when the two are equal (or unordered)
	const float fromSpeed = k_FrameMovePerFocusDistance * focusDistance * seconds;
	return k_MinFrameMove > fromSpeed ? k_MinFrameMove : fromSpeed;
}

std::optional<glm::vec3> camera_zones::LimitFrameMove(const glm::vec3& start, const glm::vec3& origin, float limit)
{
	// The game adds the squares from z to x, and makes the move a unit before it scales it
	const auto move = origin - start;
	if (!(game_math::x87::DotZyx(move, move) > limit * limit))
	{
		return std::nullopt;
	}
	return start + game_math::x87::Scaled<game_math::x87::SumOrder::Zyx>(move) * limit;
}

MapDiscKeep camera_zones::KeepInsideMapDisc(const MapDisc& disc, const glm::vec3& origin, const glm::vec3& focus)
{
	// Both points are taken from the middle and put back afterwards, each step rounded to single precision
	const auto fromMiddle = [&disc](const glm::vec3& point) {
		return glm::vec3(point.x - disc.centre.x, point.y - k_DiscMiddleHeight, point.z - disc.centre.y);
	};
	const auto backFromMiddle = [&disc](const glm::vec3& point) {
		return glm::vec3(disc.centre.x + point.x, k_DiscMiddleHeight + point.y, disc.centre.y + point.z);
	};
	auto originFromMiddle = fromMiddle(origin);
	const auto focusFromMiddle = fromMiddle(focus);
	// Measured across the ground, z first; only x and z are brought in (equal or not a number stays)
	const float distance2 = originFromMiddle.z * originFromMiddle.z + originFromMiddle.x * originFromMiddle.x;
	const bool beyond = distance2 > disc.radius * disc.radius;
	if (beyond)
	{
		const float shrink = disc.radius / std::sqrt(distance2);
		originFromMiddle.x = shrink * originFromMiddle.x;
		originFromMiddle.z = shrink * originFromMiddle.z;
	}
	return {.origin = backFromMiddle(originFromMiddle), .focus = backFromMiddle(focusFromMiddle), .broughtBack = beyond};
}

HeightBounds camera_zones::HeightBoundsAt(float ground, float floatingHeight, const Zones& zones)
{
	const float highest = zones.useMaxAltitude ? zones.maxAltitude : k_NoHeightLimit;
	const float aboveLand = zones.useHeightAboveLand ? zones.heightAboveLand : k_NoHeightLimit;
	const float overLand = ground + aboveLand;
	// The land's limit unless the highest is below it; a comparison that fails takes the land's
	return {.lowest = ground + floatingHeight, .highest = highest < overLand ? highest : overLand};
}

HeightKeep camera_zones::KeepBetweenHeights(const HeightBounds& bounds, bool exclusionsOn, const glm::vec3& origin,
                                            const glm::vec3& focus, const glm::vec3& turningPoint)
{
	HeightKeep kept {.origin = origin, .focus = focus, .turningPoint = turningPoint, .raised = false, .slid = false};
	// Not at or above the lowest, a height that is not a number included
	if (!(kept.origin.y >= bounds.lowest))
	{
		kept.origin.y = bounds.lowest;
		kept.raised = true;
	}
	if (exclusionsOn && kept.origin.y > bounds.highest)
	{
		const float over = kept.origin.y - bounds.highest;
		auto line = kept.focus - kept.origin;
		if (std::abs(static_cast<double>(line.y)) > k_SlideLeastRise)
		{
			line *= -(over / line.y);
			kept.origin += line;
			kept.focus += line;
			kept.turningPoint += line;
		}
		kept.slid = true;
	}
	return kept;
}

float camera_zones::DistanceZYX(const glm::vec3& a, const glm::vec3& b)
{
	return game_math::x87::Distance<game_math::x87::SumOrder::Zyx>(a, b);
}

glm::vec3 camera_zones::FocusAlongView(const glm::vec3& origin, const glm::vec3& focus, float distance)
{
	// The view made a unit long as the game makes it, the squares added z, y then x and multiplied by one over the root;
	// a view whose parts are all 0 (a part that is not a number counting as 0) is left as it is
	auto view = focus - origin;
	const auto counts = [](float part) { return part < 0.0f || part > 0.0f; };
	const bool none = !counts(view.x) && !counts(view.y) && !counts(view.z);
	if (!none)
	{
		const float scale = 1.0f / std::sqrt((view.z * view.z + view.y * view.y) + view.x * view.x);
		view *= scale;
	}
	return view * distance + origin;
}
