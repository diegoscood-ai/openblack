/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenPick.h"

#include <cmath>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include "Common/X87Vector.h"

namespace openblack::screen_pick
{
namespace
{
namespace x87 = game_math::x87;

// The view's sides a corner is beyond, as bits: in front of the near plane, past the right, left, top and bottom edges
constexpr uint8_t k_BeyondBottom = 0x2;
constexpr uint8_t k_BeyondTop = 0x4;
constexpr uint8_t k_BeyondLeft = 0x8;
constexpr uint8_t k_BeyondRight = 0x10;
constexpr uint8_t k_BeforeNear = 0x20;
/// The sides in the order the clipper cuts by them
constexpr std::array<uint8_t, 5> k_Sides = {k_BeforeNear, k_BeyondRight, k_BeyondLeft, k_BeyondTop, k_BeyondBottom};
/// The room a cut triangle's corners are given at first: its own and two more for each side
constexpr size_t k_MostCutCorners = 3 + 2 * k_Sides.size();

[[nodiscard]] uint8_t SidesBeyond(const View& view, const ClipCorner& corner)
{
	uint8_t sides = corner.w < view.near ? k_BeforeNear : 0;
	if (corner.x > corner.w)
	{
		sides |= k_BeyondRight;
	}
	else if (corner.x < -corner.w)
	{
		sides |= k_BeyondLeft;
	}
	if (corner.y > corner.w)
	{
		sides |= k_BeyondTop;
	}
	else if (corner.y < -corner.w)
	{
		sides |= k_BeyondBottom;
	}
	return sides;
}

/// How far inside a side of the view a corner is, below 0 beyond it
[[nodiscard]] float Inside(const View& view, const ClipCorner& corner, uint8_t side)
{
	switch (side)
	{
	case k_BeforeNear:
		return corner.w - view.near;
	case k_BeyondRight:
		return corner.w - corner.x;
	case k_BeyondLeft:
		return corner.w + corner.x;
	case k_BeyondTop:
		return corner.w - corner.y;
	default:
		return corner.w + corner.y;
	}
}

/// The corner on the screen, kept on the screen's pixels: a skinned corner's height times one over its depth, then by
/// half the screen's height; an unskinned one's times the product of the two
[[nodiscard]] ScreenCorner Project(const View& view, const ClipCorner& corner, bool skinned = true)
{
	const glm::vec2 half = view.resolution * 0.5f;
	const float overDepth = 1.0f / corner.w;
	const float x = std::clamp((overDepth * corner.x + 1.0f) * half.x, 0.0f, view.resolution.x - 1.0f);
	const float down = skinned ? corner.y * overDepth * half.y : corner.y * (overDepth * half.y);
	const float y = std::clamp(half.y - down, 0.0f, view.resolution.y - 1.0f);
	return {.x = x, .y = y, .depth = view.near * overDepth, .uv = corner.uv};
}

/// A corner of a triangle being cut: where it is in clip space, the sides it is beyond still, and where it lands on
/// the screen once it is beyond none
struct CutCorner
{
	ClipCorner clip;
	uint8_t beyond;
	ScreenCorner screen;
};

/// The sides past a side a corner made by cutting at it is beyond, each tested on its own, in the clipper's order
[[nodiscard]] uint8_t SidesBeyondAfter(uint8_t side, const ClipCorner& corner)
{
	uint8_t sides = 0;
	switch (side)
	{
	case k_BeforeNear:
		if (corner.x > corner.w)
		{
			sides = k_BeyondRight;
		}
		[[fallthrough]];
	case k_BeyondRight:
		if (-corner.w > corner.x)
		{
			sides |= k_BeyondLeft;
		}
		[[fallthrough]];
	case k_BeyondLeft:
		if (corner.y > corner.w)
		{
			sides |= k_BeyondTop;
		}
		[[fallthrough]];
	case k_BeyondTop:
		if (-corner.w > corner.y)
		{
			sides |= k_BeyondBottom;
		}
		break;
	default:
		break;
	}
	return sides;
}

/// A corner on the screen taken back to clip space, as the clipper does with a corner inside every side: from its
/// pixel kept on the screen and its depth
[[nodiscard]] ClipCorner Unproject(const View& view, const ScreenCorner& corner)
{
	const glm::vec2 half = view.resolution * 0.5f;
	const glm::vec2 overHalf {1.0f / half.x, 1.0f / half.y};
	const float w = view.near / corner.depth;
	return {.x = (corner.x - half.x) * overHalf.x * w, .y = (half.y - corner.y) * overHalf.y * w, .w = w, .uv = corner.uv};
}

/// The corner where the edge from a corner inside a side to one beyond it crosses the side, put on the screen when it
/// is beyond no side left, each step a float in the game's order
[[nodiscard]] CutCorner Intersect(const View& view, uint8_t side, const CutCorner& inside, const CutCorner& outside)
{
	const auto in = inside.beyond == 0 ? Unproject(view, inside.screen) : inside.clip;
	const auto& out = outside.clip;
	const float inDistance = Inside(view, in, side);
	const float outDistance = Inside(view, out, side);
	const float t = inDistance / (inDistance - outDistance);
	const float before = 1.0f - t;
	CutCorner cut {.clip = {.x = out.x * t + in.x * before,
	                        .y = in.y * before + out.y * t,
	                        .w = in.w * before + out.w * t,
	                        .uv = in.uv * before + out.uv * t},
	               .beyond = 0,
	               .screen = {}};
	cut.beyond = SidesBeyondAfter(side, cut.clip);
	if (cut.beyond == 0)
	{
		const glm::vec2 half = view.resolution * 0.5f;
		const float overDepth = 1.0f / cut.clip.w;
		const float x = std::clamp((overDepth * cut.clip.x + 1.0f) * half.x, 0.0f, view.resolution.x - 1.0f);
		const float scale = overDepth * half.y;
		const float y = std::clamp(half.y - scale * cut.clip.y, 0.0f, view.resolution.y - 1.0f);
		cut.screen = {.x = x, .y = y, .depth = overDepth * view.near, .uv = cut.clip.uv};
	}
	return cut;
}

/// Whether a triangle faces the camera on the screen
[[nodiscard]] bool FacesCamera(const ScreenCorner& a, const ScreenCorner& b, const ScreenCorner& c)
{
	return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y) > 0.0f;
}

/// Whether the cursor is in a triangle on the screen, on its edges included, whichever way round its corners go
[[nodiscard]] bool Covers(const ScreenCorner& a, const ScreenCorner& b, const ScreenCorner& c, glm::vec2 cursor)
{
	const glm::vec2 d0(a.x - cursor.x, a.y - cursor.y);
	const glm::vec2 d1(b.x - cursor.x, b.y - cursor.y);
	const glm::vec2 d2(c.x - cursor.x, c.y - cursor.y);
	const float first = d1.y * d0.x - d1.x * d0.y;
	const float second = d2.y * d1.x - d2.x * d1.y;
	const float third = d0.y * d2.x - d0.x * d2.y;
	if (first <= 0.0f)
	{
		return second <= 0.0f && third <= 0.0f;
	}
	return second >= 0.0f && third >= 0.0f;
}

/// The depth (near over the view depth) and texture coordinates of a triangle at the cursor: across the scan line
/// through the cursor, between its long edge and whichever short edge the line crosses
struct AtCursor
{
	float depth;
	glm::vec2 uv;
};
[[nodiscard]] AtCursor Interpolate(const ScreenCorner& first, const ScreenCorner& second, const ScreenCorner& third,
                                   glm::vec2 cursor)
{
	// The corners from the top of the screen down
	const ScreenCorner* low = &second;
	const ScreenCorner* middle = &first;
	const ScreenCorner* high = &third;
	if (second.y <= first.y)
	{
		if (third.y <= first.y)
		{
			low = &third;
			high = &first;
			middle = &second;
			if (second.y < third.y)
			{
				low = &second;
				middle = &third;
			}
		}
	}
	else
	{
		low = &first;
		middle = &second;
		high = &third;
		if (third.y <= second.y)
		{
			low = &third;
			high = &second;
			middle = &first;
			if (first.y < third.y)
			{
				low = &first;
				middle = &third;
			}
		}
	}

	const float alongLong = (low->y - cursor.y) / ((low->y - cursor.y) - (high->y - cursor.y));
	const float beforeLong = 1.0f - alongLong;
	const float depthLong = alongLong * high->depth + beforeLong * low->depth;
	const float xLong = alongLong * high->x + beforeLong * low->x;
	const auto uvLong = (alongLong * high->uv * high->depth + low->uv * low->depth * beforeLong) / depthLong;

	const ScreenCorner* start = middle;
	const ScreenCorner* end = high;
	if (cursor.y < middle->y)
	{
		start = low;
		end = middle;
	}
	const float alongShort = (start->y - cursor.y) / ((start->y - cursor.y) - (end->y - cursor.y));
	const float beforeShort = 1.0f - alongShort;
	const float depthShort = alongShort * end->depth + beforeShort * start->depth;
	const float xShort = beforeShort * start->x + alongShort * end->x;
	const auto uvShort = (end->uv * end->depth * alongShort + start->uv * start->depth * beforeShort) / depthShort;

	const float across = (xShort - cursor.x) / ((xShort - cursor.x) - (xLong - cursor.x));
	const float rest = 1.0f - across;
	const float depth = depthLong * across + depthShort * rest;
	const auto uv = (uvLong * depthLong * across + uvShort * depthShort * rest) / depth;
	return {.depth = depth, .uv = uv};
}

/// A triangle cut by the view's sides one at a time, near, right, left, top and bottom, as the game's clipper cuts it:
/// at each side it is beyond, the part inside is made of two new corners and one or two triangles, each cut again by the
/// sides left; a triangle wholly beyond one side is dropped, and one beyond none goes to `add`
void ClipTriangle(const View& view, std::vector<CutCorner>& corners, size_t a, size_t b, size_t c, uint8_t side,
                  const std::function<void(size_t, size_t, size_t)>& add)
{
	if (side == 0)
	{
		add(a, b, c);
		return;
	}
	const uint8_t next = side >> 1;
	const auto beyondA = (corners[a].beyond & side) != 0;
	const auto beyondB = (corners[b].beyond & side) != 0;
	const auto beyondC = (corners[c].beyond & side) != 0;
	// two new corners, where the edges from a corner inside to one beyond cross the side
	const auto cut = [&](size_t inside1, size_t outside1, size_t inside2, size_t outside2) {
		const auto n = corners.size();
		const auto first = Intersect(view, side, corners[inside1], corners[outside1]);
		const auto second = Intersect(view, side, corners[inside2], corners[outside2]);
		corners.push_back(first);
		corners.push_back(second);
		return std::pair {n, n + 1};
	};
	if (beyondA)
	{
		if (beyondB)
		{
			if (beyondC)
			{
				return;
			}
			const auto [n, n1] = cut(c, a, c, b);
			ClipTriangle(view, corners, c, n, n1, next, add);
		}
		else if (beyondC)
		{
			const auto [n, n1] = cut(b, a, b, c);
			ClipTriangle(view, corners, n, b, n1, next, add);
		}
		else
		{
			const auto [n, n1] = cut(b, a, c, a);
			ClipTriangle(view, corners, n, b, c, next, add);
			ClipTriangle(view, corners, n, c, n1, next, add);
		}
	}
	else if (beyondB)
	{
		if (beyondC)
		{
			const auto [n, n1] = cut(a, b, a, c);
			ClipTriangle(view, corners, a, n, n1, next, add);
		}
		else
		{
			const auto [n, n1] = cut(a, b, c, b);
			ClipTriangle(view, corners, a, n, c, next, add);
			ClipTriangle(view, corners, n, n1, c, next, add);
		}
	}
	else if (beyondC)
	{
		const auto [n, n1] = cut(a, c, b, c);
		ClipTriangle(view, corners, n, a, b, next, add);
		ClipTriangle(view, corners, n, b, n1, next, add);
	}
	else if ((corners[a].beyond | corners[b].beyond | corners[c].beyond) != 0)
	{
		ClipTriangle(view, corners, a, b, c, next, add);
	}
	else
	{
		add(a, b, c);
	}
}
} // namespace

float Depth(const View& view, glm::vec3 point)
{
	return (view.worldToClip * glm::vec4(point, 1.0f)).w;
}

namespace
{
/// Where a sphere's circle falls on the screen, none when the sphere is wholly behind the near plane, and whether the
/// camera is inside it
struct SphereCircle
{
	bool cameraInside {false};
	glm::vec2 centre {0.0f};
	float radius {0.0f};
};

std::optional<SphereCircle> CircleOf(const View& view, glm::vec3 centre, float radius, glm::vec3 origin)
{
	const auto clip = view.worldToClip * glm::vec4(centre, 1.0f);
	if (view.near > clip.w + radius)
	{
		return std::nullopt;
	}
	// The camera within the sphere's reach of the object's origin is inside it
	const auto fromCamera = origin - view.camera;
	if (glm::dot(fromCamera, fromCamera) < radius * radius)
	{
		return SphereCircle {.cameraInside = true};
	}
	const float overDepth = 1.0f / clip.w;
	const glm::vec2 half = view.resolution * 0.5f;
	return SphereCircle {
	    .centre = {(clip.x * overDepth + 1.0f) * half.x, (1.0f - clip.y * overDepth) * half.y},
	    .radius = radius * overDepth * half.x * view.xScale,
	};
}

bool CircleReachesScreen(const View& view, const SphereCircle& circle)
{
	const float x = circle.centre.x;
	const float y = circle.centre.y;
	const float r = circle.radius;
	return !(r + x < 0.0f || x - r > view.resolution.x || r + y < 0.0f || y - r > view.resolution.y);
}
} // namespace

bool CursorOverSphere(const View& view, glm::vec3 centre, float radius, glm::vec3 origin)
{
	const auto circle = CircleOf(view, centre, radius, origin);
	if (!circle.has_value())
	{
		return false;
	}
	// A camera inside the sphere is over it wherever the cursor is
	if (circle->cameraInside)
	{
		return true;
	}
	if (!CircleReachesScreen(view, *circle))
	{
		return false;
	}
	const auto offset = view.cursor - circle->centre;
	return glm::dot(offset, offset) < circle->radius * circle->radius;
}

bool SphereOnScreen(const View& view, glm::vec3 centre, float radius, glm::vec3 origin)
{
	const auto circle = CircleOf(view, centre, radius, origin);
	return circle.has_value() && (circle->cameraInside || CircleReachesScreen(view, *circle));
}

bool PointOnScreen(const View& view, glm::vec3 point)
{
	const auto clip = view.worldToClip * glm::vec4(point, 1.0f);
	if (clip.w < view.near)
	{
		return false;
	}
	const float overDepth = 1.0f / clip.w;
	const glm::vec2 half = view.resolution * 0.5f;
	const float x = (clip.x * overDepth + 1.0f) * half.x;
	const float y = (1.0f - clip.y * overDepth) * half.y;
	// A pixel cut towards nothing is on the screen from just above -1 up to just under the screen's size
	return x > -1.0f && x < view.resolution.x && y > -1.0f && y < view.resolution.y;
}

ClipCorner ToClip(const View& view, glm::vec3 world, glm::vec2 uv)
{
	const auto clip = view.worldToClip * glm::vec4(world, 1.0f);
	return {.x = clip.x, .y = clip.y, .w = clip.w, .uv = uv};
}

AlphaMask MaskOf(std::span<const uint16_t> texels, uint32_t width, uint32_t height)
{
	AlphaMask mask {};
	if (texels.size() < static_cast<size_t>(width) * height)
	{
		mask.fill(true);
		return mask;
	}
	const float acrossStep = static_cast<float>(width) * (1.0f / 64.0f);
	const float downStep = static_cast<float>(height) * (1.0f / 64.0f);
	for (uint32_t row = 0; row < 64; ++row)
	{
		const auto y = static_cast<uint32_t>(static_cast<float>(row) * downStep);
		for (uint32_t column = 0; column < 64; ++column)
		{
			const auto x = static_cast<uint32_t>(static_cast<float>(column) * acrossStep);
			mask[row * 64 + column] = (texels[y * width + x] & 0xF000u) != 0;
		}
	}
	return mask;
}

bool Solid(const AlphaMask& mask, glm::vec2 uv)
{
	const auto column = std::clamp(static_cast<int32_t>(uv.x * 64.0f), 0, 63);
	const auto row = std::clamp(static_cast<int32_t>(uv.y * 64.0f), 0, 63);
	return mask[static_cast<size_t>(row) * 64 + static_cast<size_t>(column)];
}

std::optional<float> FirstHit(const View& view, const Primitive& primitive)
{
	PickScratch scratch;
	return FirstHit(view, primitive, scratch);
}

std::optional<float> FirstHit(const View& view, const Primitive& primitive, PickScratch& scratch)
{
	const auto count = primitive.corners.size();
	auto& beyond = scratch.beyond;
	auto& onScreen = scratch.onScreen;
	auto& drawn = scratch.drawn;
	beyond.assign(count, 0);
	onScreen.resize(count);
	drawn.clear();
	for (size_t i = 0; i < count; ++i)
	{
		beyond[i] = SidesBeyond(view, primitive.corners[i]);
		if (beyond[i] == 0)
		{
			onScreen[i] = Project(view, primitive.corners[i], primitive.skinned);
		}
	}

	// The triangles drawn, cut to the view where they reach past it
	const auto keep = [&drawn, &primitive](const ScreenCorner& a, const ScreenCorner& b, const ScreenCorner& c) {
		if (primitive.twoSided || FacesCamera(a, b, c))
		{
			drawn.push_back({a, b, c});
		}
	};
	for (size_t t = 0; t + 2 < primitive.indices.size(); t += 3)
	{
		const auto i0 = primitive.indices[t];
		const auto i1 = primitive.indices[t + 1];
		const auto i2 = primitive.indices[t + 2];
		if (i0 >= count || i1 >= count || i2 >= count)
		{
			continue;
		}
		if ((beyond[i0] | beyond[i1] | beyond[i2]) == 0)
		{
			keep(onScreen[i0], onScreen[i1], onScreen[i2]);
			continue;
		}
		if ((beyond[i0] & beyond[i1] & beyond[i2]) != 0)
		{
			continue;
		}
		// the corners as the clipper keeps them, the cut corners added after them
		std::vector<CutCorner> corners;
		corners.reserve(k_MostCutCorners);
		for (const auto i : {i0, i1, i2})
		{
			corners.push_back({.clip = primitive.corners[i], .beyond = beyond[i], .screen = onScreen[i]});
		}
		ClipTriangle(view, corners, 0, 1, 2, k_BeforeNear, [&corners, &keep](size_t a, size_t b, size_t c) {
			keep(corners[a].screen, corners[b].screen, corners[c].screen);
		});
	}

	for (const auto& [a, b, c] : drawn)
	{
		if (!Covers(a, b, c, view.cursor))
		{
			continue;
		}
		const auto at = Interpolate(a, b, c, view.cursor);
		if (primitive.mask != nullptr && !Solid(*primitive.mask, at.uv))
		{
			continue;
		}
		return view.near / at.depth;
	}
	return std::nullopt;
}

std::optional<Picked> PickAmong(const View& view, std::span<const Candidate> candidates,
                                const std::function<std::optional<float>(size_t)>& distanceOf)
{
	std::optional<Picked> picked;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		const auto& candidate = candidates[i];
		if (!CursorOverSphere(view, candidate.centre, candidate.radius, candidate.origin))
		{
			continue;
		}
		// Nothing further than the nearest object picked so far by more than its own radius is tested
		if (picked.has_value() && picked->distance + candidate.radius < Depth(view, candidate.centre))
		{
			continue;
		}
		const auto distance = distanceOf(i);
		if (distance.has_value() && (!picked.has_value() || *distance < picked->distance))
		{
			picked = Picked {.index = i, .distance = *distance};
		}
	}
	return picked;
}

bool LandHidesObject(float landDistance, float objectDistance, glm::vec2 landPoint, glm::vec2 objectOrigin,
                     glm::vec2 halfExtents)
{
	if (landDistance > objectDistance)
	{
		return false;
	}
	const auto apart = landPoint - objectOrigin;
	return !(apart.y * apart.y + apart.x * apart.x < halfExtents.y * halfExtents.y + halfExtents.x * halfExtents.x);
}

std::optional<MeshHit> NearestIntersection(std::span<const glm::vec3> corners, std::span<const uint16_t> indices,
                                           glm::vec3 origin, glm::vec3 direction, bool behindAllowed)
{
	constexpr float k_SideOn = 0.005f;
	std::optional<MeshHit> nearest;
	for (size_t t = 0; t + 2 < indices.size(); t += 3)
	{
		if (indices[t] >= corners.size() || indices[t + 1] >= corners.size() || indices[t + 2] >= corners.size())
		{
			continue;
		}
		const auto& v0 = corners[indices[t]];
		const auto& v1 = corners[indices[t + 1]];
		const auto& v2 = corners[indices[t + 2]];
		auto normal = glm::cross(v1 - v0, v2 - v0);
		if (normal != glm::vec3(0.0f))
		{
			normal *= 1.0f / std::sqrt(glm::dot(normal, normal));
		}
		const float facing = glm::dot(direction, normal);
		if (!(facing < -k_SideOn || facing > k_SideOn))
		{
			continue;
		}
		const float distance = (glm::dot(v0, normal) - glm::dot(origin, normal)) / facing;
		if (!behindAllowed && !(distance > 0.0f))
		{
			continue;
		}
		const auto point = distance * direction + origin;
		const int positive = static_cast<int>(glm::dot(glm::cross(v0 - v2, point - v2), normal) > 0.0f) +
		                     static_cast<int>(glm::dot(glm::cross(v2 - v1, point - v1), normal) > 0.0f) +
		                     static_cast<int>(glm::dot(glm::cross(v1 - v0, point - v0), normal) > 0.0f);
		if (positive != 0 && positive != 3)
		{
			continue;
		}
		if (nearest.has_value() && !(distance < nearest->distance))
		{
			continue;
		}
		// Where on the triangle, along its sides from the first corner
		const auto side1 = v1 - v0;
		const auto side2 = v2 - v0;
		const auto offset = point - v0;
		const float d11 = glm::dot(side1, side1);
		const float d12 = glm::dot(side1, side2);
		const float d22 = glm::dot(side2, side2);
		const float denominator = d11 * d22 - d12 * d12;
		float s = 0.0f;
		float u = 0.0f;
		if (denominator != 0.0f)
		{
			const float o1 = glm::dot(offset, side1);
			const float o2 = glm::dot(offset, side2);
			s = (d22 * o1 - d12 * o2) / denominator;
			u = (d11 * o2 - d12 * o1) / denominator;
		}
		nearest = MeshHit {.point = point,
		                   .normal = facing <= 0.0f ? -normal : normal,
		                   .distance = distance,
		                   .firstIndex = static_cast<uint32_t>(t),
		                   .s = s,
		                   .t = u};
	}
	return nearest;
}

std::optional<MeshHit> NearestSkinIntersection(std::span<const glm::vec3> corners, std::span<const uint16_t> indices,
                                               glm::vec3 origin, glm::vec3 direction, bool behindAllowed)
{
	constexpr float k_SideOn = 0.005f;
	std::optional<MeshHit> nearest;
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		if (indices[i] >= corners.size() || indices[i + 1] >= corners.size() || indices[i + 2] >= corners.size())
		{
			continue;
		}
		const auto& v0 = corners[indices[i]];
		const auto& v1 = corners[indices[i + 1]];
		const auto& v2 = corners[indices[i + 2]];
		auto normal = glm::cross(v1 - v0, v2 - v0);
		if (normal != glm::vec3(0.0f))
		{
			normal *= 1.0f / x87::LengthZyx(normal);
		}
		// The game adds every sum here from z to x
		const float facing = x87::DotZyx(direction, normal);
		if (!(facing < -k_SideOn || facing > k_SideOn))
		{
			continue;
		}
		const float distance = (x87::DotZyx(v0, normal) - x87::DotZyx(origin, normal)) / facing;
		if (!behindAllowed && !(distance > 0.0f))
		{
			continue;
		}
		const auto point = distance * direction + origin;
		const int positive = static_cast<int>(x87::DotZyx(glm::cross(v0 - v2, point - v2), normal) > 0.0f) +
		                     static_cast<int>(x87::DotZyx(glm::cross(v2 - v1, point - v1), normal) > 0.0f) +
		                     static_cast<int>(x87::DotZyx(glm::cross(v1 - v0, point - v0), normal) > 0.0f);
		if ((positive != 0 && positive != 3) || (nearest.has_value() && !(distance < nearest->distance)))
		{
			continue;
		}
		// Where on the triangle along its first and third sides, solved across x and y alone
		const auto side1 = v1 - v0;
		const auto side2 = v2 - v0;
		const auto offset = point - v0;
		float s = 0.0f;
		float t = 0.0f;
		if (std::abs(side2.x) <= k_SideOn)
		{
			if (!(std::abs(side1.x) > k_SideOn))
			{
				continue;
			}
			const float across = side2.y - side1.y * side2.x / side1.x;
			if (!(std::abs(across) > k_SideOn))
			{
				continue;
			}
			t = (offset.y - offset.x * side1.y / side1.x) / across;
			s = (offset.x - t * side2.x) / side1.x;
		}
		else
		{
			const float across = side1.y - side2.y * side1.x / side2.x;
			if (!(std::abs(across) > k_SideOn))
			{
				continue;
			}
			s = (offset.y - offset.x * side2.y / side2.x) / across;
			t = (offset.x - side1.x * s) / side2.x;
		}
		if (!(s >= 0.0f && s <= 1.0f && t >= 0.0f && t <= 1.0f))
		{
			continue;
		}
		nearest = MeshHit {.point = point,
		                   .normal = facing <= 0.0f ? -normal : normal,
		                   .distance = distance,
		                   .firstIndex = static_cast<uint32_t>(i),
		                   .s = s,
		                   .t = t};
	}
	return nearest;
}

} // namespace openblack::screen_pick
