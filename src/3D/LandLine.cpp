/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandLine.h"

#include <cmath>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"

// The line clipped to the map, walked from cell to cell and each cell's two triangles tested. Float arithmetic where
// the original keeps extended values between its stores: (approximate) only at the exact borders of a cell or of a
// triangle.

namespace openblack::land_line
{
namespace
{
/// Metres to cells across x and z, as the game multiplies them: by the float tenth
constexpr float k_PerMetre = 0.1f;
/// A triangle's plane closer to level with the line than this, in height units per cell, is not met by it, and the cell
/// is passed over: the float 0.0001 widened, as the game compares it
constexpr double k_LevelWithLine = 9.9999997473787516e-05;
/// How far, in cells, a cell's triangles reach past its edges and their shared diagonal
constexpr float k_Reach = 1.1f;
constexpr float k_ReachBefore = -0.1f;
constexpr float k_ReachAcross = 0.9f;
/// The share of the line, from the start to the map's edge, the game takes as "never" when the line runs along an axis
constexpr float k_Never = 1e20f;
/// A line flatter than this never meets the sea's level
constexpr float k_SmallestDrop = 0.0001f;
/// 7500 squared: the sea's level counts this close across to the camera
constexpr float k_SeaReachSquared = 5.625e7f;

/// The sides of the map an end of the line is past: 1 before x's edge, 2 past its far edge, 4 and 8 the same for z
[[nodiscard]] uint32_t SidesPast(float x, float z, float farEdge)
{
	uint32_t code = 0;
	if (x < k_EdgeMargin)
	{
		code = 1;
	}
	else if (x > farEdge)
	{
		code = 2;
	}
	if (z < k_EdgeMargin)
	{
		code |= 4;
	}
	else if (z > farEdge)
	{
		code |= 8;
	}
	return code;
}
} // namespace

std::optional<glm::vec2> HitInCell(int32_t x, int32_t z, glm::vec3 from, glm::vec3 to, const CellLookup& cells,
                                   uint16_t cellsPerSide)
{
	// 0 to 511 on both axes in the game's lands
	const auto last = static_cast<int32_t>(cellsPerSide) - 1;
	if (x < 0 || x > last || z < 0 || z > last)
	{
		return std::nullopt;
	}
	const auto corners = cells(x, z);
	if (!corners.has_value())
	{
		return std::nullopt;
	}
	const int32_t h00 = corners->here;
	const int32_t h01 = corners->acrossZ;
	const int32_t h10 = corners->acrossX;
	const int32_t h11 = corners->acrossBoth;
	// the highest and the lowest corner
	const int32_t highX0 = h00 > h10 ? h00 : h10;
	const int32_t highX1 = h01 > h11 ? h01 : h11;
	const auto high = static_cast<float>(highX0 > highX1 ? highX0 : highX1);
	const int32_t lowX0 = h00 < h10 ? h00 : h10;
	const int32_t lowX1 = h01 < h11 ? h01 : h11;
	const auto low = static_cast<float>(lowX0 < lowX1 ? lowX0 : lowX1);
	// both ends under the lowest corner, or both over the highest: no hit
	if (from.y < low && to.y < low)
	{
		return std::nullopt;
	}
	if (!(from.y <= high) && !(to.y <= high))
	{
		return std::nullopt;
	}
	const float dx = to.x - from.x;
	const float dz = to.z - from.z;
	const float dy = to.y - from.y;
	const float u0 = from.x - static_cast<float>(x);
	const float v0 = from.z - static_cast<float>(z);
	const auto localAt = [&](float t) {
		// u = (t dx + from x) - x, v = (t dz + from z) - z, stored as floats
		return glm::vec2(t * dx + from.x - static_cast<float>(x), t * dz + from.z - static_cast<float>(z));
	};
	const auto inFront = [&](glm::vec2 uv) -> std::optional<glm::vec2> {
		// the point (x + u, z + v) is not behind the start: (hit x - from x) dx + (hit z - from z) dz >= 0
		const glm::vec2 world(static_cast<float>(x) + uv.x, static_cast<float>(z) + uv.y);
		if ((world.x - from.x) * dx + (world.y - from.z) * dz < 0.0f)
		{
			return std::nullopt;
		}
		return world;
	};
	// The first triangle: the plane h00 + a u + b v, a = h10 - h00, b = h01 - h00. A flat denominator ends the test
	// without trying the second triangle
	const auto a = static_cast<float>(h10 - h00);
	const auto b = static_cast<float>(h01 - h00);
	const float denominator = (dy - a * dx) - b * dz;
	if (static_cast<double>(std::fabs(denominator)) < k_LevelWithLine)
	{
		return std::nullopt;
	}
	const float t = (((v0 * b + u0 * a) + static_cast<float>(h00)) - from.y) / denominator;
	const auto uv = localAt(t);
	if (!(uv.x > k_Reach) && !(uv.y > k_Reach) && !(uv.x < k_ReachBefore) && !(uv.y < k_ReachBefore) &&
	    !(k_Reach - uv.y < uv.x))
	{
		if (const auto hit = inFront(uv))
		{
			return hit;
		}
	}
	// The second triangle: the plane h11 + sx (u - 1) + sz (v - 1), sx = h11 - h01, sz = h11 - h10, written as
	// (h10 - sx) + v sz + u sx
	const auto sz = static_cast<float>(h11 - h10);
	const auto sx = static_cast<float>(h11 - h01);
	const float denominator2 = (dy - sx * dx) - sz * dz;
	if (static_cast<double>(std::fabs(denominator2)) < k_LevelWithLine)
	{
		return std::nullopt;
	}
	const float t2 = ((((static_cast<float>(h10) - sx) + v0 * sz) + u0 * sx) - from.y) / denominator2;
	const auto uv2 = localAt(t2);
	if (uv2.x > k_Reach || uv2.y > k_Reach || uv2.x < k_ReachBefore || uv2.y < k_ReachBefore || k_ReachAcross - uv2.y > uv2.x)
	{
		return std::nullopt;
	}
	return inFront(uv2);
}

std::optional<glm::vec2> FirstHit(glm::vec3 from, glm::vec3 to, const CellLookup& cells, uint16_t cellsPerSide)
{
	float x0 = from.x;
	float z0 = from.z;
	float y0 = from.y;
	float x1 = to.x;
	float z1 = to.z;
	float y1 = to.y;
	const auto mapCells = static_cast<float>(cellsPerSide); // 512 in the game's lands
	const float high = mapCells - k_EdgeMargin;             // 511.9 for 512 cells
	// the share of the line to the first map edge (0 or the far one) it meets in x and in z, the smaller one
	float t = k_Never;
	if (x0 > x1)
	{
		t = x0 / (x0 - x1);
	}
	else if (x0 < x1)
	{
		t = (mapCells - x0) / (x1 - x0);
	}
	float tz = k_Never;
	if (z0 > z1)
	{
		tz = z0 / (z0 - z1);
	}
	else if (z0 < z1)
	{
		tz = (mapCells - z0) / (z1 - z0);
	}
	if (!(t <= tz))
	{
		t = tz;
	}
	// The end is moved on by t times (end - start), to the edge plus one more (end - start): the walk below follows a
	// line to the map's edge, not the stretch. The game means to skip this when the line never reaches an edge, but its
	// test never passes, so the end is always moved, also without an edge
	x1 = (x1 - x0) * t + x1;
	z1 = (z1 - z0) * t + z1;
	y1 = (y1 - y0) * t + y1;
	// kept within 0.1 to 511.9, the ends cut where they leave it: x's near edge, its far edge, then z's
	uint32_t code0 = SidesPast(x0, z0, high);
	uint32_t code1 = SidesPast(x1, z1, high);
	if ((code0 & code1) != 0)
	{
		return std::nullopt;
	}
	if ((code0 | code1) != 0)
	{
		uint32_t differ = code0 ^ code1;
		// x = edge: the end that is out on that side moves there and gets the sides of its new z alone
		const auto clipX = [&](float edge, uint32_t bit) {
			const float s = (edge - x0) / (x1 - x0);
			const float z = (z1 - z0) * s + z0;
			const float y = (y1 - y0) * s + y0;
			if ((code1 & bit) != 0)
			{
				y1 = y;
				z1 = z;
				x1 = edge;
				code1 = SidesPast(k_EdgeMargin, z, high);
			}
			else
			{
				y0 = y;
				z0 = z;
				x0 = edge;
				code0 = SidesPast(k_EdgeMargin, z, high);
			}
		};
		// z = edge: that end is past no side any more, its x is not tested again
		const auto clipZ = [&](float edge, uint32_t bit) {
			const float s = (edge - z0) / (z1 - z0);
			const float x = (x1 - x0) * s + x0;
			const float y = (y1 - y0) * s + y0;
			if ((code1 & bit) != 0)
			{
				y1 = y;
				z1 = edge;
				code1 = 0;
				x1 = x;
			}
			else
			{
				y0 = y;
				z0 = edge;
				code0 = 0;
				x0 = x;
			}
		};
		if ((differ & 1u) != 0)
		{
			clipX(k_EdgeMargin, 1u);
			differ = code0 ^ code1;
		}
		if ((code0 & code1) != 0)
		{
			return std::nullopt;
		}
		if ((differ & 2u) != 0)
		{
			clipX(high, 2u);
			differ = code0 ^ code1;
		}
		if ((code0 & code1) != 0)
		{
			return std::nullopt;
		}
		if ((differ & 4u) != 0)
		{
			clipZ(k_EdgeMargin, 4u);
			differ = code0 ^ code1;
		}
		if ((code0 & code1) != 0)
		{
			return std::nullopt;
		}
		if ((differ & 8u) != 0)
		{
			clipZ(high, 8u); // (no new `differ` after this one)
		}
		if ((code0 & code1) != 0)
		{
			return std::nullopt;
		}
	}
	// the cells of both ends (cut towards nothing), and the start as the first stretch's start
	int32_t xi = map_coords::FtoL(x0);
	int32_t zi = map_coords::FtoL(z0);
	const int32_t lastZ = map_coords::FtoL(z1);
	const int32_t lastX = map_coords::FtoL(x1);
	glm::vec3 previous(x0, y0, z0);
	const glm::vec3 start(x0, y0, z0);
	const auto test = [&](int32_t cx, int32_t cz, const glm::vec3& stretchFrom, float tx, float tz, float ty) {
		return HitInCell(cx, cz, stretchFrom, glm::vec3(tx, ty, tz), cells, cellsPerSide);
	};
	// the last cell gets the stretch to the (moved) end, and its answer is the answer
	const auto lastCell = [&]() { return test(xi, zi, previous, x1, z1, y1); };
	if (xi == lastX)
	{
		// one column: every cell is tested with the whole clipped line
		for (; zi < lastZ; ++zi)
		{
			if (const auto hit = test(xi, zi, start, x1, z1, y1))
			{
				return hit;
			}
		}
		for (; zi > lastZ; --zi)
		{
			if (const auto hit = test(xi, zi, start, x1, z1, y1))
			{
				return hit;
			}
		}
		return lastCell();
	}
	if (zi == lastZ)
	{
		// one row, the same
		for (; xi < lastX; ++xi)
		{
			if (const auto hit = test(xi, zi, start, x1, z1, y1))
			{
				return hit;
			}
		}
		for (; xi > lastX; --xi)
		{
			if (const auto hit = test(xi, zi, start, x1, z1, y1))
			{
				return hit;
			}
		}
		return lastCell();
	}
	// the slopes, kxz = dx / dz, kzx = 1 / kxz (not dz / dx), kyz = dy / dz, kyx = dy / dx
	const float dx = x1 - x0;
	const float dz = z1 - z0;
	const float kxz = dx / dz;
	const float kzx = 1.0f / kxz;
	const float dy = y1 - y0;
	const float kyz = dy / dz;
	const float kyx = dy / dx;
	const bool zUp = z0 <= z1;
	const bool xUp = x0 <= x1;
	// (port guard) the walk visits at most |dx| + |dz| + 1 cells; a float drift that kept it going ends it here
	const int32_t guard = 4 * static_cast<int32_t>(cellsPerSide) + 4;
	// The current point (x0, z0, y0) moves from cell to cell: across the z edge when the x there is still in the
	// column (FtoL(x) is the column), else across the x edge. Each cell is tested with the stretch from the previous
	// point
	for (int32_t steps = 0; steps < guard; ++steps)
	{
		const bool more = (xUp ? xi < lastX : xi > lastX) || (zUp ? zi < lastZ : zi > lastZ);
		if (!more)
		{
			return lastCell();
		}
		const int32_t cellX = xi;
		const int32_t cellZ = zi;
		if (!zUp)
		{
			// the cell's lower z edge
			const auto edge = static_cast<float>(zi);
			const float toEdge = z0 - edge;
			const float xAt = x0 - toEdge * kxz;
			if (map_coords::FtoL(xAt) == xi)
			{
				x0 = xAt;
				--zi;
				y0 = y0 - toEdge * kyz;
				z0 = edge;
			}
			else if (xUp)
			{
				// the column's upper x edge
				++xi;
				const auto edgeX = static_cast<float>(xi);
				const float across = edgeX - x0;
				z0 = z0 + across * kzx;
				zi = map_coords::FtoL(z0);
				y0 = y0 + across * kyx;
				x0 = edgeX;
			}
			else
			{
				// the column's lower x edge
				const auto edgeX = static_cast<float>(xi);
				const float across = x0 - edgeX;
				z0 = z0 - across * kzx;
				zi = map_coords::FtoL(z0);
				--xi;
				y0 = y0 - across * kyx;
				x0 = edgeX;
			}
		}
		else
		{
			// the cell's upper z edge
			++zi;
			const auto edge = static_cast<float>(zi);
			const float toEdge = edge - z0;
			const float xAt = x0 + toEdge * kxz;
			if (map_coords::FtoL(xAt) == xi)
			{
				x0 = xAt;
				y0 = toEdge * kyz + y0;
				z0 = edge;
			}
			else if (xUp)
			{
				++xi;
				const auto edgeX = static_cast<float>(xi);
				const float across = edgeX - x0;
				z0 = z0 + across * kzx;
				zi = map_coords::FtoL(z0);
				y0 = y0 + across * kyx;
				x0 = edgeX;
			}
			else
			{
				const auto edgeX = static_cast<float>(xi);
				const float across = x0 - edgeX;
				z0 = z0 - across * kzx;
				zi = map_coords::FtoL(z0);
				--xi;
				y0 = y0 - across * kyx;
				x0 = edgeX;
			}
		}
		// the cell just left, tested with the stretch from the previous point to the new one
		if (const auto hit = test(cellX, cellZ, previous, x0, z0, y0))
		{
			return hit;
		}
		previous = glm::vec3(x0, y0, z0);
	}
	return std::nullopt;
}

std::optional<glm::vec2> LandAlong(glm::vec3 from, glm::vec3 to, const CellLookup& cells, uint16_t cellsPerSide)
{
	// The line in cell units: x and z times a tenth, y over the height unit
	const auto hit = FirstHit(glm::vec3(from.x * k_PerMetre, from.y / k_HeightUnit, from.z * k_PerMetre),
	                          glm::vec3(to.x * k_PerMetre, to.y / k_HeightUnit, to.z * k_PerMetre), cells, cellsPerSide);
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	return *hit * k_CellSize;
}

std::optional<SeaCrossing> SeaLevelAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera)
{
	// a line going down (to y <= from y) and not flat meets y = 0 at t = -(from y / dy); it counts when (dx^2 + dz^2)
	// from the camera is no more than 7500^2
	if (!(to.y <= from.y))
	{
		return std::nullopt;
	}
	const float dy = to.y - from.y;
	if (std::fabs(dy) < k_SmallestDrop)
	{
		return std::nullopt;
	}
	const float t = -(from.y / dy);
	SeaCrossing sea {};
	sea.point.x = (to.x - from.x) * t + from.x;
	sea.point.y = (to.z - from.z) * t + from.z;
	const float cx = sea.point.x - camera.x;
	const float cz = sea.point.y - camera.z;
	sea.inReach = cx * cx + cz * cz <= k_SeaReachSquared;
	return sea;
}

std::optional<glm::vec2> LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera, const CellLookup& cells,
                                        uint16_t cellsPerSide)
{
	if (const auto land = LandAlong(from, to, cells, cellsPerSide))
	{
		return land;
	}
	if (const auto sea = SeaLevelAlong(from, to, camera); sea.has_value() && sea->inReach)
	{
		return sea->point;
	}
	return std::nullopt;
}

std::optional<glm::vec3> UnderPixel(glm::vec3 camera, glm::vec3 nearPoint, bool withSea, const CellLookup& cells,
                                    const std::function<float(glm::vec2)>& heightAt, uint16_t cellsPerSide)
{
	glm::vec3 point;
	if (const auto hit = withSea ? LandOrSeaAlong(camera, nearPoint, camera, cells, cellsPerSide)
	                             : LandAlong(camera, nearPoint, cells, cellsPerSide))
	{
		point = glm::vec3(hit->x, 0.0f, hit->y);
	}
	else if (withSea && camera.y > nearPoint.y)
	{
		// The sea's level wherever the line from the camera goes down
		const float share = -(camera.y / (nearPoint.y - camera.y));
		point = glm::vec3((nearPoint.x - camera.x) * share + camera.x, 0.0f, (nearPoint.z - camera.z) * share + camera.z);
	}
	else
	{
		return std::nullopt;
	}
	point.y = heightAt({point.x, point.z});
	return point;
}

glm::vec3 KeptInReach(glm::vec3 point)
{
	constexpr float k_Middle = k_CellSize * k_MapCells * 0.5f;
	constexpr float k_PickReach = k_CellSize * 1536.0f * 0.5f;
	auto fromMiddle = glm::vec3(point.x - k_Middle, point.y, point.z - k_Middle);
	const float length = std::sqrt(fromMiddle.z * fromMiddle.z + fromMiddle.y * fromMiddle.y + fromMiddle.x * fromMiddle.x);
	if (!(k_PickReach < length))
	{
		return point;
	}
	if (fromMiddle.x != 0.0f || fromMiddle.y != 0.0f || fromMiddle.z != 0.0f)
	{
		fromMiddle *= k_PickReach / length;
	}
	return {fromMiddle.x + k_Middle, fromMiddle.y, fromMiddle.z + k_Middle};
}

CellLookup CellsOf(const LandIslandInterface& island)
{
	return [&island](int32_t x, int32_t z) -> std::optional<CellHeights> {
		// the cell must be in a block; its corners are read from the cell's own block
		const glm::u16vec2 cell(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
		if (!island.HasBlockAt(cell))
		{
			return std::nullopt;
		}
		const auto corners = island.GetCellCorners(cell);
		return CellHeights {.here = corners[0], .acrossX = corners[2], .acrossZ = corners[1], .acrossBoth = corners[3]};
	};
}

} // namespace openblack::land_line
