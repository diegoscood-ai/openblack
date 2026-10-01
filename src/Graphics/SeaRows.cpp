/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SeaRows.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/vec4.hpp>

using namespace openblack::graphics;

namespace
{
/// LH3DP3's clip codes (fn_008797F0): 0x20 in front of the near plane, 0x10 / 0x08 right / left, 0x04 / 0x02 top / bottom
/// of the 90-degree frustum of the pre-scaled camera matrix (|x'| <= z, |y'| <= z); there is no far plane
constexpr uint32_t k_ClipNear = 0x20;

/// A vertex of the clipper: clip x and y and the view depth z (= clip w), with its clip code
struct ClipVertex
{
	float x;
	float y;
	float z;
	uint32_t code;
};

uint32_t ClipCode(float x, float y, float z, float nearDistance)
{
	// fn_008797F0 0x87985A..0x8798C3 (the new vertices of fn_0081DD90 get the same test)
	uint32_t code = z < nearDistance ? k_ClipNear : 0;
	if (x > z)
	{
		code |= 0x10;
	}
	else if (-z > x)
	{
		code |= 0x08;
	}
	if (y > z)
	{
		code |= 0x04;
	}
	else if (-z > y)
	{
		code |= 0x02;
	}
	return code;
}

/// The recursive triangle clipper fn_0081A760 (x87) / fn_007A3A50 (SSE, [0xEA9EB4] = IsPentium4). Both make the same
/// triangle list: checked against a Unicorn emulation of fn_00879500 with both paths (tmp_dis\agua\re\emu_sea_range.py).
/// One plane per bit of the mask, from 0x20 down. Each cut appends two new vertices to the table (g_NumClipVerts += 2)
/// and leaves either one smaller triangle or two; the first of the two is clipped by a recursive call with the next
/// planes. The sea draws with g_NoBackfaceCull = 1 (0x8795AC), so no triangle is culled.
class Clipper
{
public:
	Clipper(std::vector<ClipVertex>& vertices, std::vector<uint16_t>& triangles, float nearDistance)
	    : _vertices(vertices)
	    , _triangles(triangles)
	    , _near(nearDistance)
	{
	}

	void Clip(uint16_t a, uint16_t b, uint16_t c, uint32_t mask)
	{
		for (;;)
		{
			if (mask == 0)
			{
				Emit(a, b, c);
				return;
			}
			const uint32_t outA = _vertices[a].code & mask;
			const uint32_t outB = _vertices[b].code & mask;
			const uint32_t outC = _vertices[c].code & mask;
			if (outA != 0)
			{
				if (outB != 0)
				{
					if (outC != 0)
					{
						return; // 0x81A7BB
					}
					// 0x81A7C1: (c, c-a, c-b)
					const auto n = Intersect(mask, c, a);
					Intersect(mask, c, b);
					a = c;
					b = n;
					c = static_cast<uint16_t>(n + 1);
				}
				else if (outC != 0)
				{
					// 0x81A835: (b-a, b, b-c)
					const auto n = Intersect(mask, b, a);
					Intersect(mask, b, c);
					a = n;
					c = static_cast<uint16_t>(n + 1);
				}
				else
				{
					// 0x81A856: (b-a, b, c) now, then (b-a, c, c-a)
					const auto n = Intersect(mask, b, a);
					Intersect(mask, c, a);
					mask >>= 1;
					Clip(n, b, c, mask);
					a = n;
					b = c;
					c = static_cast<uint16_t>(n + 1);
					continue;
				}
			}
			else if (outB != 0)
			{
				if (outC != 0)
				{
					// 0x81A8C8: (a, a-b, a-c)
					const auto n = Intersect(mask, a, b);
					Intersect(mask, a, c);
					b = n;
					c = static_cast<uint16_t>(n + 1);
				}
				else
				{
					// 0x81A8E9: (a, a-b, c) now, then (a-b, c-b, c)
					const auto n = Intersect(mask, a, b);
					Intersect(mask, c, b);
					mask >>= 1;
					Clip(a, n, c, mask);
					a = n;
					b = static_cast<uint16_t>(n + 1);
					continue;
				}
			}
			else if (outC != 0)
			{
				// 0x81A928: (a-c, a, b) now, then (a-c, b, b-c)
				const auto n = Intersect(mask, a, c);
				Intersect(mask, b, c);
				mask >>= 1;
				Clip(n, a, b, mask);
				a = n;
				c = static_cast<uint16_t>(n + 1);
				continue;
			}
			else if ((_vertices[a].code | _vertices[b].code | _vertices[c].code) == 0)
			{
				Emit(a, b, c); // 0x81A971: nothing left to clip
				return;
			}
			mask >>= 1;
		}
	}

private:
	void Emit(uint16_t a, uint16_t b, uint16_t c)
	{
		// 0x81AA7E
		_triangles.push_back(a);
		_triangles.push_back(b);
		_triangles.push_back(c);
	}

	/// fn_0081DD90 / fn_007A1480: the point of the edge from the inside vertex to the outside one on the plane of the
	/// mask, t = d(in) / (d(in) - d(out)), p = in (1 - t) + out t, appended to the table with its own clip code. The
	/// screen position of a new vertex is clamped to 0..g_MaxScreen (0xC2AB00 = width - 1, height - 1) at 0x81E0F1.
	uint16_t Intersect(uint32_t mask, uint16_t inside, uint16_t outside)
	{
		// the jump table 0x81E17C: the distance to the plane of the mask
		const auto distance = [this, mask](const ClipVertex& v) {
			switch (mask)
			{
			case k_ClipNear:
				return v.z - _near;
			case 0x10:
				return v.z - v.x;
			case 0x08:
				return v.z + v.x;
			case 0x04:
				return v.z - v.y;
			default:
				return v.z + v.y;
			}
		};
		const auto in = _vertices[inside];
		const auto out = _vertices[outside];
		const float dIn = distance(in);
		const float t = dIn / (dIn - distance(out));
		ClipVertex v {};
		v.x = in.x * (1.0f - t) + out.x * t;
		v.y = in.y * (1.0f - t) + out.y * t;
		v.z = in.z * (1.0f - t) + out.z * t;
		// 0x81E063: the jump table 0x81E1B4 only tests the planes after the one being cut (never the near plane again)
		v.code = ClipCode(v.x, v.y, v.z, _near) & (mask - 1);
		_vertices.push_back(v);
		return static_cast<uint16_t>(_vertices.size() - 1);
	}

	std::vector<ClipVertex>& _vertices;
	std::vector<uint16_t>& _triangles;
	float _near;
};
} // namespace

std::optional<sea::ScreenRange> sea::ComputeScreenRange(const glm::mat4& viewProjection, glm::vec2 viewportSize,
                                                        float nearDistance)
{
	// fn_00879500: the quad on the sea level [0xFA936C] = 0, vertices 0..3 = (-12440, -12440), (17560, -12440),
	// (17560, 17560), (-12440, 17560) (0x879537..0x87956F), triangles (0, 2, 1) and (0, 3, 2)
	const std::array<glm::vec2, 4> corners = {{
	    {k_RowsQuadMinimum, k_RowsQuadMinimum},
	    {k_RowsQuadMaximum, k_RowsQuadMinimum},
	    {k_RowsQuadMaximum, k_RowsQuadMaximum},
	    {k_RowsQuadMinimum, k_RowsQuadMaximum},
	}};
	std::vector<ClipVertex> vertices;
	vertices.reserve(32);
	for (const auto& c : corners)
	{
		const auto p = viewProjection * glm::vec4(c.x, 0.0f, c.y, 1.0f);
		vertices.push_back({p.x, p.y, p.w, ClipCode(p.x, p.y, p.w, nearDistance)});
	}
	std::vector<uint16_t> triangles;
	triangles.reserve(64);
	// 0x87957C..0x8796ED. To see if the quad needs clipping at all the original ORs the codes of vertices 0, 1, 2 and of
	// the table's entry 4 ([0xE3B5F0], a slip for 3 at [0xE3B5EC]). Entry 4 is a stale code left by whatever went through
	// LH3DP3 before, which cannot be known here. If it and codes 0..2 are 0 while vertex 3 is outside, the original draws
	// both triangles unclipped with vertex 3's raw camera x', y', z' (a garbage top). That needs corners 0, 1 and 2 on
	// screen at once: none of 200000 random cameras over the island's disc (radius 5120, height 3..4000) does it, so the
	// stale entry is taken as 0 and the case is kept.
	constexpr uint32_t k_StaleCode4 = 0;
	const uint32_t codes012 = vertices[0].code | vertices[1].code | vertices[2].code;
	Clipper clipper(vertices, triangles, nearDistance);
	if ((codes012 | k_StaleCode4) == 0)
	{
		triangles = {0, 2, 1, 0, 3, 2};
	}
	else
	{
		if ((vertices[0].code & vertices[1].code & vertices[2].code & vertices[3].code) != 0)
		{
			return std::nullopt; // 0x8795CA: the whole quad is outside one plane
		}
		if (codes012 != 0)
		{
			clipper.Clip(0, 2, 1, k_ClipNear);
		}
		else
		{
			triangles = {0, 2, 1};
		}
		if ((vertices[3].code | vertices[0].code | vertices[2].code) != 0)
		{
			clipper.Clip(0, 3, 2, k_ClipNear);
		}
		else
		{
			triangles.insert(triangles.end(), {0, 3, 2});
		}
	}
	if (triangles.empty())
	{
		return std::nullopt; // 0x879649
	}

	// 0x879651..0x879762: top starts at the screen height and bottom at -1. Over the clipped triangle list in order, a y
	// above bottom becomes the bottom, *else* a y under top becomes the top, so the very first vertex only ever sets the
	// bottom. y = (1 - y' / z) * height / 2 (fn_008797F0 0x8798E6); vertex 3 unclipped uses its raw values (see above).
	ScreenRange range {viewportSize.y, -1.0f, 0.0f, 0.0f};
	const float maximumY = viewportSize.y - 1.0f;
	for (const auto index : triangles)
	{
		const auto& v = vertices[index];
		float y = (1.0f - v.y / v.z) * 0.5f * viewportSize.y;
		float inverseDepth = 1.0f / v.z;
		if (index >= corners.size())
		{
			y = y < 0.0f ? 0.0f : (y > maximumY ? maximumY : y); // a clip vertex (0x81E130..0x81E15D)
		}
		else if (v.code != 0)
		{
			// vertex 3 in the stale-code case: never projected, so Table1 still holds y' and z' (+0x04, +0x0C; the
			// 1 / z is later divided by the near plane)
			y = v.y;
			inverseDepth = v.z / nearDistance;
		}
		if (y > range.bottom)
		{
			range.bottom = y;
			range.inverseDepthBottom = inverseDepth;
		}
		else if (y < range.top)
		{
			range.top = y;
			range.inverseDepthTop = inverseDepth;
		}
	}
	// 0x879768..0x8797E2
	if (range.bottom < 0.0f || range.top > viewportSize.y - 1.0f)
	{
		return std::nullopt;
	}
	if (range.bottom > viewportSize.y - 1.0f)
	{
		range.bottom = viewportSize.y - 1.0f;
	}
	if (range.top < 0.0f)
	{
		range.top = 0.0f;
	}
	return range;
}

sea::Rows sea::MakeRows(const ScreenRange& range)
{
	// fn_00879930 0x879AD7..0x879B3E
	Rows rows {};
	rows.first = static_cast<int>(range.top); // __ftol truncates
	const int bottom = static_cast<int>(range.bottom);
	rows.count = (bottom - rows.first + 2) / 2;
	rows.inverseDepth = range.inverseDepthTop;
	// with n = 1 the original divides by 0; only row 0 is then used at its own depth
	rows.inverseStep =
	    rows.count > 1 ? (range.inverseDepthBottom - range.inverseDepthTop) / static_cast<float>(rows.count - 1) : 0.0f;
	// 0x879D5A: y of row 0 > 0.5
	rows.softTop = static_cast<float>(rows.first) > 0.5f;
	return rows;
}

void sea::Drift::ScrollRows(float milliseconds, glm::vec2 wind, float period)
{
	// 0x879963: k = g_game_time_inc * -1/330 (0x9A3C24)
	const float k = milliseconds * (-1.0f / 330.0f);
	_rows += wind * k;
	_rows -= glm::vec2(std::trunc(_rows.x / period), std::trunc(_rows.y / period)) * period;
}

void sea::Drift::ScrollLevel0(float milliseconds, glm::vec2 wind)
{
	// 0x87A130: k = g_game_time_inc * -1/330000 (0x9A3C2C)
	const float k = milliseconds * (-1.0f / 330000.0f);
	_level0 += wind * k;
	_level0 -= glm::vec2(std::trunc(_level0.x), std::trunc(_level0.y));
}
