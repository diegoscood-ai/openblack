/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShadowMath.h"

#include <cmath>

#include <algorithm>
#include <bit>

#include <glm/vec4.hpp>

namespace openblack::graphics::shadow_math
{
namespace
{
/// __ftol 0x7A1400: truncation towards zero
int Ftol(float value)
{
	return static_cast<int>(value);
}

/// fn_00874600's block test of one cell: exists, (visible), nearer than 100000
BlockState BlockOfCell(const BlockAt& blocks, int cellX, int cellZ, int cellLimit, bool& inRange)
{
	inRange = cellX >= 0 && cellX <= cellLimit && cellZ >= 0 && cellZ <= cellLimit;
	if (!inRange)
	{
		return {};
	}
	return blocks(cellX >> 4, cellZ >> 4); // 0x874650 / 0x8746E5: sar 4, shl 5 (32 blocks a row)
}

/// The span tables of fn_0087FF70 / fn_00880050: [0xFA9FC8] the left ends (edges going up), [0xFA97C4] the right ends
/// (edges going down), and the rows [0xFAA7D0] .. [0xFAA7CC] the triangle touched
struct Spans
{
	explicit Spans(int rows)
	    : left(static_cast<size_t>(rows), 0)
	    , right(static_cast<size_t>(rows), 0)
	    , minRow(rows)
	{
	}
	std::vector<int> left;
	std::vector<int> right;
	int minRow;
	int maxRow {0};
};

/// fn_0087FF70 (ecx = y0, edx = y1, the two x on the stack): rows [y0, y1) half open, clipped to [0, rows); x in 16.16
/// fixed point from x0 with the step (x1 - x0) / (y1 - y0), stored >> 16
void Edge(int y0, int y1, float x0, float x1, int rows, Spans& spans)
{
	if (y0 == y1) // 0x87FF7B
	{
		return;
	}
	auto* table = &spans.right; // 0xFA97C4 (0x87FFA4)
	if (y0 > y1)                // 0x87FF81..0x87FF9D: swapped, into 0xFA9FC8
	{
		std::swap(y0, y1);
		std::swap(x0, x1);
		table = &spans.left;
	}
	if (y0 >= rows || y1 <= 0) // 0x87FFAE / 0x87FFB8
	{
		return;
	}
	const float slope = (x1 - x0) / static_cast<float>(y1 - y0); // 0x87FFBE..0x87FFD0: fsub, fidiv
	if (y1 > rows)                                               // 0x87FFCA..0x87FFD6
	{
		y1 = rows;
	}
	if (y0 < 0) // 0x87FFD8..0x87FFE8: x0 - y0 slope (fild, fmul, fsubr), stored as a float
	{
		x0 = x0 - static_cast<float>(y0) * slope;
		y0 = 0;
	}
	spans.minRow = std::min(spans.minRow, y0); // 0x87FFEC..0x87FFF4
	spans.maxRow = std::max(spans.maxRow, y1); // 0x87FFFA..0x880002
	const int count = y1 - y0;
	if (count <= 0)
	{
		return;
	}
	const int step = Ftol(slope * 65536.0f); // [0x8AC408] = 65536 (0x88000E)
	int x = Ftol(x0 * 65536.0f);             // 0x88001D..0x880029
	for (int row = y0; row < y0 + count; ++row)
	{
		(*table)[static_cast<size_t>(row)] = x >> 16; // 0x880030 sar 0x10
		x += step;
	}
}

/// fn_00880050: each row of [minRow, maxRow) fills [max(0, l), min(grid x, r)) of its subrow; the odd subrows set the
/// high nibbles (0x9A3CF4 / 0x9A3D34 / 0x9A3D78), the even ones the low nibbles (0x9A3C74 / 0x9A3CB4 / 0x9A3D74) unless
/// `halfRows` (0x880141..0x880146); two subrows to a texel row (0x88019E..0x8801A7)
void Fill(const Spans& spans, bool halfRows, Coverage& coverage)
{
	const int gridX = coverage.GridX();
	for (int row = spans.minRow; row < spans.maxRow; ++row)
	{
		int left = spans.left[static_cast<size_t>(row)];
		if (left < 0) // 0x8800A0
		{
			left = 0;
		}
		int right = spans.right[static_cast<size_t>(row)];
		if (right > gridX) // 0x8800B4
		{
			right = gridX;
		}
		if (right <= left || left >= gridX || right <= 0) // 0x8800BE..0x8800D0
		{
			continue;
		}
		const bool odd = (row & 1) != 0; // 0x8800E0 test bl, 1
		if (!odd && halfRows)
		{
			continue;
		}
		auto* bytes = coverage.bytes.data() + static_cast<size_t>(row >> 1) * static_cast<size_t>(coverage.texels);
		const int shift = odd ? 4 : 0;
		for (int x = left; x < right; ++x)
		{
			bytes[x >> 2] = static_cast<uint8_t>(bytes[x >> 2] | (1u << ((x & 3) + shift)));
		}
	}
}
} // namespace

float Fade(glm::vec3 position, float ground, glm::vec3 camera, float scale, float meshRadius, const BlockAt& blocks,
           int cellLimit)
{
	// 0x874603..0x874684: the block under the caster, when there is one, must be nearer than 100000
	bool inRange = false;
	const auto centre =
	    BlockOfCell(blocks, Ftol(position.x * k_CellScale), Ftol(position.z * k_CellScale), cellLimit, inRange);
	if (inRange && centre.exists && !(centre.distance < k_BlockFar))
	{
		return 0.0f;
	}
	// 0x87468A..0x874737: any of the 3 x 3 neighbours, x outer (ebp) and z inner (edi), from -1 to 1
	bool found = false;
	for (int i = -1; i < 2 && !found; ++i)
	{
		for (int j = -1; j < 2 && !found; ++j)
		{
			const int cellX = Ftol((static_cast<float>(i) * k_NeighbourStep + position.x) * k_CellScale);
			const int cellZ = Ftol((static_cast<float>(j) * k_NeighbourStep + position.z) * k_CellScale);
			const auto block = BlockOfCell(blocks, cellX, cellZ, cellLimit, inRange);
			found = inRange && block.exists && block.visible && block.distance < k_BlockFar;
		}
	}
	if (!found)
	{
		return 0.0f; // 0x87473D
	}
	// 0x87478E..0x8747EE: dx, dy, dz stored as floats; ((dz dz + dy dy) + dx dx), sqrt, / (scale x radius)
	const float dx = position.x - camera.x;
	const float dy = ground - camera.y;
	const float dz = position.z - camera.z;
	const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
	const float q = distance / (scale * meshRadius);
	if (q < k_FadeFull) // 0x8747F2
	{
		return k_FadeMax;
	}
	if (!(q <= k_FadeGone)) // 0x874811 test ah, 0x41
	{
		return 0.0f;
	}
	return k_FadeMax - (q - k_FadeFull) * k_FadeMax / (k_FadeGone - k_FadeFull); // 0x874822..0x874844
}

int AlphaGeneric(float fade, int base)
{
	return Ftol(static_cast<float>(base) * fade * k_InvByte); // fild base, fmul fade, fmul [0x900058]
}

int AlphaComplex(float fade, int base)
{
	return fade < k_FadeMax ? AlphaGeneric(fade, base) : 255; // 0x815009 / 0x815051
}

glm::vec3 LightGeneric(glm::vec3 position, bool useSun)
{
	if (useSun)
	{
		// [0xEA1C88] = 0xC8F42400, 0x48F42400, 0xC8F42400 (fn_00818920)
		return {std::bit_cast<float>(0xC8F42400u), std::bit_cast<float>(0x48F42400u), std::bit_cast<float>(0xC8F42400u)};
	}
	return {position.x, position.y + k_VerticalLight, position.z};
}

glm::vec3 LightHand(glm::vec3 position)
{
	return {position.x, position.y + k_HandLight, position.z}; // fld [si+0x448], fadd [0x8C7B34]
}

glm::vec3 LightCreature(glm::vec3 position, glm::vec3 light, float meshRadius, float scale)
{
	const float radius = meshRadius * scale * k_CreatureRadii; // 0x815062..0x815081
	float dx = light.x - position.x;                           // kept in st
	float dy = light.y - position.y;                           // [esp+0xc]
	float dz = light.z - position.z;                           // [esp+0x10]
	float horizontal = std::sqrt(dz * dz + dx * dx);           // 0x8150A0..0x8150AE
	if (horizontal < radius)                                   // 0x8150B0
	{
		if (static_cast<double>(horizontal) < k_CreatureNear) // 0x8150BF, a double
		{
			dx = dx + 1.0f;
			dz = dz + 1.0f;
		}
		if (!(dx == 0.0f && dy == 0.0f && dz == 0.0f)) // 0x8150E0..0x81510D
		{
			const float factor = radius / std::sqrt(dy * dy + dz * dz + dx * dx); // 0x81510F..0x815129
			dx = dx * factor;
			dy = factor * dy;
			dz = factor * dz;
		}
		horizontal = std::sqrt(dz * dz + dx * dx); // 0x815143..0x815151
	}
	if (dy / horizontal < 1.0f) // 0x815153..0x815166
	{
		dy = horizontal;
	}
	return {position.x + dx, position.y + dy, position.z + dz}; // 0x81516E..0x8151A5
}

Projection MakeProjection(glm::vec3 position, glm::vec3 light)
{
	return {light, position - light, position.y};
}

glm::vec2 Project(const Projection& projection, const glm::mat4& matrix, glm::vec3 local, Box& box)
{
	const float ty = -projection.baseY + matrix[3][1]; // 0x850954..0x85096E
	const float wx = matrix[2][0] * local.z + matrix[1][0] * local.y + matrix[0][0] * local.x + matrix[3][0];
	const float wy = matrix[0][1] * local.x + matrix[2][1] * local.z + matrix[1][1] * local.y + ty;
	const float wz = matrix[0][2] * local.x + matrix[2][2] * local.z + matrix[1][2] * local.y + matrix[3][2];
	const float h = wy < 0.0f ? 0.0f : wy; // 0x8509D4..0x8509E3
	const float k = wz * projection.dir.z + wx * projection.dir.x;
	if (k < box.kMin) // 0x850A1B
	{
		box.kMin = k;
	}
	const float t = -projection.light.y / (h - projection.light.y); // 0x850A2E fsub, 0x850A3A fdivr [-Ly]
	const glm::vec2 p((wx - projection.light.x) * t + projection.light.x, (wz - projection.light.z) * t + projection.light.z);
	if (p.x < box.x0) // 0x850A70
	{
		box.x0 = p.x;
	}
	if (p.x > box.x1) // 0x850A87, test ah, 0x41
	{
		box.x1 = p.x;
	}
	if (p.y < box.z0) // 0x850A9E
	{
		box.z0 = p.y;
	}
	if (p.y > box.z1) // 0x850AAD
	{
		box.z1 = p.y;
	}
	return p;
}

Coverage::Coverage(int side)
    : texels(side)
    , bytes(static_cast<size_t>(side) * static_cast<size_t>(side), 0)
{
}

void Coverage::Clear()
{
	std::fill(bytes.begin(), bytes.end(), uint8_t {0});
}

void ToGrid(const Box& box, std::span<glm::vec2> points, int texels)
{
	const auto gridX = static_cast<float>(texels * 4); // [0x8C6CA8] = 128
	const auto gridZ = static_cast<float>(texels * 2); // [0x930678] = 64
	const float scaleX = gridX / (box.x1 - box.x0);    // 0x807265..0x807277
	const float scaleZ = gridZ / (box.z1 - box.z0);    // 0x80727E..0x80728A
	for (auto& point : points)
	{
		point.x = (point.x - box.x0) * scaleX;
		if (point.x < 0.0f) // 0x80729C
		{
			point.x = 0.0f;
		}
		else if (point.x > gridX - 1.0f) // [0x8C4A00] = 127
		{
			point.x = gridX - 1.0f;
		}
		point.y = (point.y - box.z0) * scaleZ;
		if (point.y < 0.0f) // 0x8072D5
		{
			point.y = 0.0f;
		}
		else if (point.y > gridZ - 1.0f) // [0x9A2BF8] = 63
		{
			point.y = gridZ - 1.0f;
		}
	}
}

void RasterTriangles(std::span<const glm::vec2> grid, std::span<const uint16_t> indices, bool bothFaces,
                     bool halfRows, Coverage& coverage)
{
	const int rows = coverage.GridZ();
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		if (indices[i] >= grid.size() || indices[i + 1] >= grid.size() || indices[i + 2] >= grid.size())
		{
			continue; // (port guard)
		}
		const auto& p0 = grid[indices[i]];
		const auto& p1 = grid[indices[i + 1]];
		const auto& p2 = grid[indices[i + 2]];
		const int r0 = Ftol(p0.y);
		const int r1 = Ftol(p1.y);
		const int r2 = Ftol(p2.y);
		Spans spans(rows); // [0xFAA7D0] = [0xC39B0C], [0xFAA7CC] = 0 (0x850DB2 / 0x850DBD)
		if (!bothFaces)
		{
			// 0x850D6A..0x850DA1: back faces dropped (C0 of the fcompp)
			const float a = static_cast<float>(r1 - r2) * (p0.x - p2.x);
			const float b = static_cast<float>(r0 - r2) * (p1.x - p2.x);
			if (!(b >= a))
			{
				continue;
			}
			Edge(r0, r2, p0.x, p2.x, rows, spans);
			Edge(r2, r1, p2.x, p1.x, rows, spans);
			Edge(r1, r0, p1.x, p0.x, rows, spans);
		}
		else
		{
			// 0x850EA3..0x850EF0: the walk that makes the spans of either winding
			const float a = static_cast<float>(r2 - r1) * (p0.x - p1.x);
			const float b = static_cast<float>(r0 - r1) * (p2.x - p1.x);
			if (b < a)
			{
				Edge(r0, r2, p0.x, p2.x, rows, spans);
				Edge(r2, r1, p2.x, p1.x, rows, spans);
				Edge(r1, r0, p1.x, p0.x, rows, spans);
			}
			else
			{
				Edge(r0, r1, p0.x, p1.x, rows, spans);
				Edge(r1, r2, p1.x, p2.x, rows, spans);
				Edge(r2, r0, p2.x, p0.x, rows, spans);
			}
		}
		Fill(spans, halfRows, coverage);
	}
}

void Resolve(const Coverage& coverage, Texels& texels)
{
	const int side = coverage.texels;
	texels.assign(static_cast<size_t>(side) * static_cast<size_t>(side), 0);
	for (int row = 1; row < side - 1; ++row)
	{
		for (int column = 1; column < side - 1; ++column)
		{
			const auto index = static_cast<size_t>(row) * static_cast<size_t>(side) + static_cast<size_t>(column);
			texels[index] = static_cast<uint8_t>(std::popcount(coverage.bytes[index])); // [0xFA95C4 + 2 mask]
		}
	}
}

void ChromaFilter(std::span<const uint16_t> rendered, Texels& texels)
{
	const auto side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(texels.size()))));
	if (rendered.size() != texels.size())
	{
		return; // (port guard)
	}
	const auto at = [side](int row, int column) {
		return static_cast<size_t>(row) * static_cast<size_t>(side) + static_cast<size_t>(column);
	};
	for (int row = 1; row < side - 1; ++row) // ebx 0x82 .. 0x802 step 0x40
	{
		for (int column = 1; column < side - 1; ++column) // edi = 0x1E
		{
			const int sum = rendered[at(row, column)] + rendered[at(row, column + 1)] + rendered[at(row + 1, column)] +
			                rendered[at(row + 1, column + 1)];
			const int average = (sum / 4) & 0xF000; // cdq, and edx 3, add, sar 2, and 0xF000
			texels[at(row, column)] = static_cast<uint8_t>(texels[at(row, column)] | (average >> 12)); // or word
		}
	}
}

void BakeAlpha(Texels& texels, int alpha)
{
	if (alpha == 255) // 0x8075BD..0x8075D1
	{
		return;
	}
	const auto side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(texels.size()))));
	for (int row = 1; row < side - 1; ++row)
	{
		for (int column = 1; column < side - 1; ++column)
		{
			auto& n = texels[static_cast<size_t>(row) * static_cast<size_t>(side) + static_cast<size_t>(column)];
			const int value = ((static_cast<int>(n) << 12) * alpha / 255) & 0xF000; // imul, 0x80808081, sar 7
			n = static_cast<uint8_t>(value >> 12);
		}
	}
}

AlphaMap MakeAlphaMap(std::span<const uint16_t> texels, int width, int height)
{
	AlphaMap map {};
	if (width <= 0 || height <= 0 || texels.size() < static_cast<size_t>(width) * static_cast<size_t>(height))
	{
		return map; // (port guard)
	}
	const float rowStep = static_cast<float>(height) * k_SixtyFourth;  // fild [esp+0x2C], fmul [0x8D8BD0]
	const float columnStep = static_cast<float>(width) * k_SixtyFourth; // fild [esp+0x28], fmul [0x8D8BD0]
	for (int row = 0; row < 64; ++row)
	{
		const int source = Ftol(static_cast<float>(row) * rowStep) * width; // imul by the pitch / 2
		for (int column = 0; column < 64; ++column)
		{
			const auto texel = texels[static_cast<size_t>(source + Ftol(static_cast<float>(column) * columnStep))];
			map[static_cast<size_t>(row * 64 + column)] = static_cast<uint8_t>((texel >> 8) & 0xF0); // byte +1, and 0xF0
		}
	}
	return map;
}

glm::vec4 ChromaVertex(const Projection& projection, const Box& box, const glm::mat4& matrix, glm::vec3 local, glm::vec2 uv)
{
	// 0x84B804..0x84B82A: 32 / (x1 - x0) and 32 / (z1 - z0) ([0x8CF134] = 32), stored as floats
	const float scaleX = k_ChromaSide / (box.x1 - box.x0);
	const float scaleZ = k_ChromaSide / (box.z1 - box.z0);
	const float base = projection.baseY - projection.light.y; // 0x84B7E0..0x84B7F7: si+0x18 - si+0x448
	// 0x84B83F..0x84B88D: the object's matrix [0xEA1AE8]+0x14..0x40, (x m00 + z m20) + y m10 + t
	const float wx = local.x * matrix[0][0] + local.z * matrix[2][0] + local.y * matrix[1][0] + matrix[3][0];
	const float wy = local.x * matrix[0][1] + local.z * matrix[2][1] + local.y * matrix[1][1] + matrix[3][1];
	const float wz = local.x * matrix[0][2] + local.z * matrix[2][2] + local.y * matrix[1][2] + matrix[3][2];
	const float t = base / (wy - projection.light.y); // 0x84B8B5..0x84B8C2: fsub [si+0x448], fdivr
	float x = ((wx - projection.light.x) * t + projection.light.x - box.x0) * scaleX;
	if (x < 1.0f) // 0x84B8E0 [0x8AA390]
	{
		x = 1.0f;
	}
	else if (!(x <= k_ChromaSide - 1.0f)) // 0x84B8F9 [0x92B6F4] = 31, test ah, 0x41
	{
		x = k_ChromaSide - 1.0f;
	}
	float z = ((wz - projection.light.z) * t + projection.light.z - box.z0) * scaleZ;
	if (z < 1.0f)
	{
		z = 1.0f;
	}
	else if (!(z <= k_ChromaSide - 1.0f))
	{
		z = k_ChromaSide - 1.0f;
	}
	return {x, z, uv.x * 63.0f, uv.y * 63.0f}; // [0x9A2BF8] = 63
}

namespace
{
/// A vertex of fn_00881DE0: x, y (the row), u, v
struct ChromaPoint
{
	int x;
	int y;
	int u;
	int v;
};

/// fn_00881A60's table entry (12 bytes): x, u, v
struct ChromaEdge
{
	int x {0};
	int u {0};
	int v {0};
};

/// fn_00881A60: a horizontal edge stores its two ends as they are, the lesser x into [+0x10] (u, v NOT in 16.16, as
/// the original); else from the upper vertex, [+0x10] going down and [+0x14] going up, rows y0..y1 inclusive, x, u, v
/// in 16.16 with the steps ftol(d (1 / (y1 - y0 + 1)) 65536) ([0x8AA390] = 1, [0x8AC408] = 65536)
void ChromaEdgeOf(ChromaPoint a, ChromaPoint b, std::vector<ChromaEdge>& down, std::vector<ChromaEdge>& up)
{
	if (a.y == b.y) // 0x881A77
	{
		const auto& left = a.x < b.x ? a : b;
		const auto& right = a.x < b.x ? b : a;
		down[static_cast<size_t>(a.y)] = {left.x, left.u, left.v};
		up[static_cast<size_t>(a.y)] = {right.x, right.u, right.v};
		return;
	}
	auto* table = &down; // [ecx+0x10] (0x881B9D)
	if (a.y > b.y)       // 0x881B72
	{
		std::swap(a, b);
		table = &up; // [ecx+0x14]
	}
	const int count = b.y - a.y + 1;
	const float inverse = 1.0f / static_cast<float>(count);
	const int stepX = static_cast<int>(static_cast<float>(b.x - a.x) * inverse * 65536.0f);
	const int stepU = static_cast<int>(static_cast<float>(b.u - a.u) * inverse * 65536.0f);
	const int stepV = static_cast<int>(static_cast<float>(b.v - a.v) * inverse * 65536.0f);
	int x = a.x << 16;
	int u = a.u << 16;
	int v = a.v << 16;
	for (int row = a.y; row < a.y + count; ++row)
	{
		(*table)[static_cast<size_t>(row)] = {x >> 16, u, v}; // 0x881C33..0x881C41: x sar 16, u and v as they are
		x += stepX;
		u += stepU;
		v += stepV;
	}
}
} // namespace

void ChromaTriangle(const std::array<glm::vec4, 3>& vertices, const AlphaMap& map, std::span<uint16_t> target, int side)
{
	if (side <= 0 || target.size() < static_cast<size_t>(side) * static_cast<size_t>(side))
	{
		return; // (port guard)
	}
	std::array<ChromaPoint, 3> points {};
	for (size_t i = 0; i < 3; ++i)
	{
		// 0x84B9AA..0x84BA4C: ftol of x, z, u, v; 0x881DE9..0x881E83: x and y clamped to [0, width - 1] / [0, height - 1]
		points[i] = {Ftol(vertices[i].x), Ftol(vertices[i].y), Ftol(vertices[i].z), Ftol(vertices[i].w)};
		points[i].x = std::clamp(points[i].x, 0, side - 1);
		points[i].y = std::clamp(points[i].y, 0, side - 1);
	}
	// 0x881E86..0x881EEA: a (ebx) the upper vertex
	auto* a = &points[0];
	auto* b = &points[1];
	auto* c = &points[2];
	const bool aFirst = a->y < b->y || (a->y == b->y && a->x >= b->x); // 0x881E8C..0x881E96
	if (aFirst)
	{
		// 0x881E98: c above a (or level and to the right) -> swap a, c
		if (!(a->y < c->y || (a->y == c->y && a->x >= c->x)))
		{
			std::swap(a, c);
		}
	}
	else
	{
		// 0x881EC5: b below c (or level and to the left) -> swap a, c; else swap a, b
		if (b->y > c->y || (b->y == c->y && b->x < c->x))
		{
			std::swap(a, c);
		}
		else
		{
			std::swap(a, b);
		}
	}
	// 0x881EAD..0x881EE8: b and c
	if (b->x > c->x)
	{
		std::swap(b, c);
	}
	else if (b->x == c->x)
	{
		if (a->x <= b->x ? b->y < c->y : b->y > c->y)
		{
			std::swap(b, c);
		}
	}
	const int minRow = a->y;              // [ebp+4] (0x881EED)
	const int maxRow = std::max(b->y, c->y); // [ebp+8] (0x881EF0..0x881F00)
	std::vector<ChromaEdge> down(static_cast<size_t>(side));
	std::vector<ChromaEdge> up(static_cast<size_t>(side));
	ChromaEdgeOf(*a, *b, down, up); // 0x881F03
	ChromaEdgeOf(*b, *c, down, up); // 0x881F0C
	ChromaEdgeOf(*c, *a, down, up); // 0x881F15
	// fn_00882080: rows minRow..maxRow inclusive, each span from [+0x10] to [+0x14] (or the other way), inclusive
	for (int row = minRow; row <= maxRow; ++row)
	{
		const auto& left = down[static_cast<size_t>(row)];
		const auto& right = up[static_cast<size_t>(row)];
		int count = right.x - left.x + 1; // 0x8820D7..0x8820E1
		int x = left.x;
		int u = left.u;
		int v = left.v;
		int stepU = 0;
		int stepV = 0;
		if (count < 0) // 0x8820E1 jns
		{
			count = left.x - right.x + 1;
			x = right.x;
			u = right.u;
			v = right.v;
			stepU = (left.u - right.u) / count; // cdq, idiv
			stepV = (left.v - right.v) / count;
		}
		else if (count != 0)
		{
			stepU = (right.u - left.u) / count;
			stepV = (right.v - left.v) / count;
		}
		auto* texel = target.data() + static_cast<size_t>(row) * static_cast<size_t>(side);
		for (int i = 0; i < count; ++i)
		{
			const int index = ((v >> 10) & ~0x3F) + (u >> 16); // 0x882141..0x882150
			const int px = x + i;
			if (index >= 0 && index < static_cast<int>(map.size()) && px >= 0 && px < side) // (port guard)
			{
				texel[px] = static_cast<uint16_t>(texel[px] | ((map[static_cast<size_t>(index)] & 0xE0) << 7));
			}
			u += stepU;
			v += stepV;
		}
	}
}

float LandT(float baseY, float lightY, float ground)
{
	return (baseY - lightY) / (ground - lightY);
}

bool TouchesBlock(const Box& box, int blockX, int blockZ)
{
	return static_cast<float>(blockX) * k_BlockSize <= box.x1 && static_cast<float>(blockX + 1) * k_BlockSize >= box.x0 &&
	       static_cast<float>(blockZ) * k_BlockSize <= box.z1 && static_cast<float>(blockZ + 1) * k_BlockSize >= box.z0;
}

bool BlockVisible(const std::array<glm::vec3, 8>& corners, const glm::mat4& worldToClip, float nearW)
{
	uint32_t nearCodes = 0;
	uint32_t right = 0;
	uint32_t left = 0;
	uint32_t top = 0;
	uint32_t bottom = 0;
	for (uint32_t i = 0; i < corners.size(); ++i)
	{
		const uint32_t bit = 1u << i;
		const auto clip = worldToClip * glm::vec4(corners[i], 1.0f);
		if (clip.w < nearW) // 0x8773E4
		{
			nearCodes |= bit;
		}
		if (clip.x > clip.w) // 0x8773F3
		{
			right |= bit;
		}
		else if (-clip.w > clip.x) // 0x877404
		{
			left |= bit;
		}
		if (clip.y > clip.w) // 0x877417
		{
			top |= bit;
		}
		else if (-clip.w > clip.y) // 0x87742A
		{
			bottom |= bit;
		}
	}
	return nearCodes != 0xFF && right != 0xFF && left != 0xFF && top != 0xFF && bottom != 0xFF; // 0x8774A1..0x8774E2
}

} // namespace openblack::graphics::shadow_math
