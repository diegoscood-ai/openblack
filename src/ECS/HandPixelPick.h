/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

/// The pixel test the hand's cursor search gives a tree: its drawn triangles are projected to the screen, the first one
/// that holds the mouse pixel where its texture's alpha mask is not 0 is the hit. Wiki:
/// docs/bw1-notes/hand-and-interface.md, "A tree under the cursor".
///
/// The products and sums between the values the original stores are worked in double, as its float unit keeps more
/// precision than a float; every stored value is rounded to a float where it is stored.
namespace openblack::ecs::hand_pixel_pick
{

/// A texture's 64 x 64 alpha mask: byte v x 64 + u, the alpha nibble in the top four bits
using Mask = std::array<uint8_t, 64 * 64>;

/// The screen the triangles are projected on
struct Screen
{
	glm::vec2 halfSize; ///< half the width and height in pixels
	glm::vec2 last;     ///< the last pixel's x and y, where a projected vertex is clamped
	float nearClip;     ///< the near clip the world is drawn with
};

/// A vertex in clipping space (x, y and the depth w; z is not read) with its texture coordinates
struct ClipVertex
{
	glm::vec4 clip;
	glm::vec2 uv;
};

/// A vertex on the screen: x and y in pixels, y down, rhw = near clip / w, and its texture coordinates
struct ScreenVertex
{
	float x;
	float y;
	float rhw;
	float u;
	float v;
};

// The bits of an outcode: which side of the view a clipping-space point is out of
inline constexpr uint32_t k_OutsideBottom = 0x2;
inline constexpr uint32_t k_OutsideTop = 0x4;
inline constexpr uint32_t k_OutsideLeft = 0x8;
inline constexpr uint32_t k_OutsideRight = 0x10;
inline constexpr uint32_t k_OutsideNear = 0x20;

/// The point's outcode: in front of the near clip, and inside x and y of -w..w
[[nodiscard]] inline uint32_t Outcode(const glm::vec4& clip, float nearClip)
{
	uint32_t code = clip.w < nearClip ? k_OutsideNear : 0;
	if (clip.x > clip.w)
	{
		code |= k_OutsideRight;
	}
	else if (-clip.w > clip.x)
	{
		code |= k_OutsideLeft;
	}
	if (clip.y > clip.w)
	{
		code |= k_OutsideTop;
	}
	else if (-clip.w > clip.y)
	{
		code |= k_OutsideBottom;
	}
	return code;
}

/// To the screen: x = (x / w + 1) half width, y = half height - (y / w) half height, rhw = near clip / w; a vertex
/// inside the view (`clampToScreen`) is kept within 0..last
[[nodiscard]] inline ScreenVertex Project(const ClipVertex& vertex, const Screen& screen, bool clampToScreen)
{
	const double inverseW = static_cast<float>(1.0 / static_cast<double>(vertex.clip.w));
	const double halfWidth = screen.halfSize.x;
	const double halfHeight = screen.halfSize.y;
	auto x = static_cast<float>((inverseW * vertex.clip.x + 1.0) * halfWidth);
	auto y = static_cast<float>(halfHeight - vertex.clip.y * inverseW * halfHeight);
	if (clampToScreen)
	{
		x = x < 0.0f ? 0.0f : x > screen.last.x ? screen.last.x : x;
		y = y < 0.0f ? 0.0f : y > screen.last.y ? screen.last.y : y;
	}
	const auto rhw = static_cast<float>(screen.nearClip * inverseW);
	return {.x = x, .y = y, .rhw = rhw, .u = vertex.uv.x, .v = vertex.uv.y};
}

/// A triangle cut at the near clip: the part in front of it, a triangle or a quad in the same winding
struct NearClipped
{
	std::array<ClipVertex, 4> vertices {};
	size_t count {0};
};

[[nodiscard]] inline NearClipped ClipToNear(const std::array<ClipVertex, 3>& triangle, float nearClip)
{
	NearClipped out;
	for (size_t i = 0; i < 3; ++i)
	{
		const auto& from = triangle[i];
		const auto& to = triangle[(i + 1) % 3];
		const bool fromIn = from.clip.w >= nearClip;
		if (fromIn)
		{
			out.vertices[out.count++] = from;
		}
		if (fromIn != (to.clip.w >= nearClip))
		{
			const float t = (nearClip - from.clip.w) / (to.clip.w - from.clip.w);
			out.vertices[out.count++] = {.clip = from.clip + (to.clip - from.clip) * t, .uv = from.uv + (to.uv - from.uv) * t};
		}
	}
	return out;
}

/// Facing the camera: (c.y - a.y)(b.x - a.x) - (b.y - a.y)(c.x - a.x) > 0 on the screen, y down
[[nodiscard]] inline bool FacesCamera(const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c)
{
	const double ax = a.x;
	const double ay = a.y;
	return (c.y - ay) * (b.x - ax) - (b.y - ay) * (c.x - ax) > 0.0;
}

/// The pixel is in the triangle, in either winding: with the corners measured from the pixel, the cross product of the
/// first with the second decides the side; when it is above 0 neither of the other two (second with third, third with
/// first) may be below 0, else neither may be above 0. The edges count as inside.
[[nodiscard]] inline bool HoldsPixel(const ScreenVertex& v0, const ScreenVertex& v1, const ScreenVertex& v2, glm::vec2 pixel)
{
	const std::array<glm::vec2, 3> d = {glm::vec2(v0.x - pixel.x, v0.y - pixel.y), glm::vec2(v1.x - pixel.x, v1.y - pixel.y),
	                                    glm::vec2(v2.x - pixel.x, v2.y - pixel.y)};
	const auto cross = [&d](size_t i) {
		const auto& p = d[i];
		const auto& q = d[(i + 1) % 3];
		return static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
	};
	if (cross(0) > 0.0)
	{
		return cross(1) >= 0.0 && cross(2) >= 0.0;
	}
	return cross(1) <= 0.0 && cross(2) <= 0.0;
}

/// What the triangle has at the pixel
struct AtPixel
{
	float rhw; ///< near clip / w
	float u;
	float v;
};

/// The triangle's rhw, u and v at the pixel, perspective-correct: the corners by y (top, middle, bottom; equal ys keep
/// the order they take below), the long edge top to bottom and the short one (top to middle above the middle corner,
/// middle to bottom from it down) at the pixel's row, then along the row between them; u and v go through u rhw and
/// v rhw. A row or a span of no length gives what a division by 0 gives.
[[nodiscard]] inline AtPixel Interpolate(const ScreenVertex& v0, const ScreenVertex& v1, const ScreenVertex& v2,
                                         glm::vec2 pixel)
{
	const ScreenVertex* top = &v0;
	const ScreenVertex* middle = &v1;
	const ScreenVertex* bottom = &v2;
	if (v0.y < v1.y)
	{
		if (!(v1.y < v2.y))
		{
			if (v0.y < v2.y)
			{
				middle = &v2;
				bottom = &v1;
			}
			else
			{
				top = &v2;
				middle = &v0;
				bottom = &v1;
			}
		}
	}
	else if (v0.y < v2.y)
	{
		top = &v1;
		middle = &v0;
	}
	else if (v1.y < v2.y)
	{
		top = &v1;
		middle = &v2;
		bottom = &v0;
	}
	else
	{
		top = &v2;
		bottom = &v0;
	}
	// the corners in double
	struct Corner
	{
		double x;
		double y;
		double rhw;
		double u;
		double v;
	};
	const auto corner = [](const ScreenVertex* p) { return Corner {p->x, p->y, p->rhw, p->u, p->v}; };
	const auto a = corner(top);
	const auto m = corner(middle);
	const auto b = corner(bottom);
	const double row = pixel.y;
	const double column = pixel.x;
	// the long edge
	const double t1 = (a.y - row) / ((a.y - row) - (b.y - row));
	const double r1 = 1.0 - t1;
	const double rhw1 = static_cast<float>(r1 * a.rhw + t1 * b.rhw);
	const double x1 = static_cast<float>(r1 * a.x + t1 * b.x);
	const double u1 = static_cast<float>((a.u * a.rhw * r1 + t1 * b.u * b.rhw) / rhw1);
	const double tv1 = static_cast<float>((a.v * a.rhw * r1 + t1 * b.v * b.rhw) / rhw1);
	// the short edge
	const auto& from = row < m.y ? a : m;
	const auto& to = row < m.y ? m : b;
	const double t2 = (from.y - row) / ((from.y - row) - (to.y - row));
	const double r2 = 1.0 - t2;
	const double rhw2 = static_cast<float>(r2 * from.rhw + t2 * to.rhw);
	const double x2 = static_cast<float>(t2 * to.x + r2 * from.x);
	const double u2 = static_cast<float>((from.u * from.rhw * r2 + to.u * to.rhw * t2) / rhw2);
	const double tv2 = (from.v * from.rhw * r2 + to.v * to.rhw * t2) / rhw2; // not stored before it is used
	// along the row, s from the short edge (0) to the long one (1)
	const double s = (x2 - column) / ((x2 - column) - (x1 - column));
	const double r = 1.0 - s;
	const auto rhw = static_cast<float>(rhw2 * r + rhw1 * s);
	const auto u = static_cast<float>((u2 * rhw2 * r + u1 * rhw1 * s) / rhw);
	const auto v = static_cast<float>((tv2 * rhw2 * r + tv1 * rhw1 * s) / rhw);
	return {.rhw = rhw, .u = u, .v = v};
}

/// A texture coordinate to a row or column of the mask: x 64, truncated toward zero, then clamped to 0..63. Out of
/// the 64-bit range (or not a number) the truncation gives 0; past the 32-bit range only its low 32 bits are kept.
[[nodiscard]] inline int MaskCell(float coordinate)
{
	const double scaled = static_cast<double>(coordinate) * 64.0;
	int32_t cell = 0;
	if (std::abs(scaled) < 9223372036854775808.0)
	{
		cell = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(scaled))));
	}
	return cell < 0 ? 0 : cell > 63 ? 63 : cell;
}

/// The mask is not 0 at u, v (u the column, v the row)
[[nodiscard]] inline bool MaskOpaque(const Mask& mask, float u, float v)
{
	return mask[static_cast<size_t>(MaskCell(v)) * 64 + static_cast<size_t>(MaskCell(u))] != 0;
}

/// One drawn triangle (a, b, c, in the primitive's order) under the mouse pixel: none when all three corners are out of
/// the same side of the view. One in front of the near clip is projected whole; one across it is cut there first and
/// its pieces tried in turn. A piece must face the camera (unless the primitive is two-sided), hold the pixel and,
/// with a mask, have it not 0 at the pixel's u, v. Returns the view depth at the pixel, near clip / rhw.
[[nodiscard]] inline std::optional<float> TriangleHit(const std::array<ClipVertex, 3>& triangle, bool twoSided,
                                                      const Mask* mask, const Screen& screen, glm::vec2 pixel)
{
	const std::array<uint32_t, 3> codes = {Outcode(triangle[0].clip, screen.nearClip),
	                                       Outcode(triangle[1].clip, screen.nearClip),
	                                       Outcode(triangle[2].clip, screen.nearClip)};
	if ((codes[0] & codes[1] & codes[2]) != 0)
	{
		return std::nullopt;
	}
	const auto piece = [&](const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c) -> std::optional<float> {
		if ((!twoSided && !FacesCamera(a, b, c)) || !HoldsPixel(a, b, c, pixel))
		{
			return std::nullopt;
		}
		const auto at = Interpolate(a, b, c, pixel);
		if (mask != nullptr && !MaskOpaque(*mask, at.u, at.v))
		{
			return std::nullopt;
		}
		return screen.nearClip / at.rhw;
	};
	if (((codes[0] | codes[1] | codes[2]) & k_OutsideNear) == 0)
	{
		// a corner out of a side of the view keeps its place off the screen
		return piece(Project(triangle[0], screen, codes[0] == 0), Project(triangle[1], screen, codes[1] == 0),
		             Project(triangle[2], screen, codes[2] == 0));
	}
	const auto clipped = ClipToNear(triangle, screen.nearClip);
	for (size_t i = 1; i + 1 < clipped.count; ++i)
	{
		const auto hit = piece(Project(clipped.vertices[0], screen, false), Project(clipped.vertices[i], screen, false),
		                       Project(clipped.vertices[i + 1], screen, false));
		if (hit.has_value())
		{
			return hit;
		}
	}
	return std::nullopt;
}

/// A depth at the pixel (near clip / rhw, the view depth w) as a distance along the pick's ray, whose own depth is
/// startDepth + t x depthPerUnit. The original's cursor search compares view depths; on one ray the distance along it
/// orders the objects in the same way, and it is the unit the other objects' picks give. Nothing when the depth is not
/// ahead of the ray's start.
[[nodiscard]] inline std::optional<float> AlongRay(float depth, float startDepth, float depthPerUnit)
{
	const float along = (depth - startDepth) / depthPerUnit;
	if (!std::isfinite(along) || along <= 0.0f)
	{
		return std::nullopt;
	}
	return along;
}

} // namespace openblack::ecs::hand_pixel_pick
