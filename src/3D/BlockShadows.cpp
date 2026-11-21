/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockShadows.h"

#include <cmath>

#include <algorithm>
#include <utility>

namespace openblack::block_texture
{
namespace
{
constexpr double k_Fixed = 65536.0;

// A whole number times a float, as the game's processor does it: the number loaded exactly, the product rounded once
// to a single float
float IntTimes(int32_t whole, float value)
{
	return static_cast<float>(static_cast<double>(whole) * static_cast<double>(value));
}

// The game's float to integer conversion: truncated, the low 32 bits kept; 0 for a value it can't hold
int32_t Truncate(double value)
{
	constexpr double k_Limit = 9223372036854775808.0; // 2^63
	if (!std::isfinite(value) || std::abs(value) >= k_Limit)
	{
		return 0;
	}
	return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(value))));
}

// The samples of a mask row's dword that a span from sample k (start) or up to sample k (end) covers: the even sample
// rows are the bytes' low nibbles, the odd ones their high nibbles
constexpr uint32_t k_EvenFull = 0x0F0F0F0Fu;
constexpr uint32_t k_OddFull = 0xF0F0F0F0u;

constexpr uint32_t EvenEnd(uint32_t k)
{
	return ((1u << (k + (k / 4) * 4)) - 1u) & k_EvenFull;
}

constexpr uint32_t EvenStart(uint32_t k)
{
	return k_EvenFull & ~((1u << (k + (k / 4) * 4)) - 1u);
}

void Or32(std::span<uint8_t> mask, size_t at, uint32_t bits)
{
	for (size_t k = 0; k < 4; ++k)
	{
		const auto byte = static_cast<uint8_t>(bits >> (8 * k));
		if (byte != 0 && at + k < mask.size())
		{
			mask[at + k] |= byte;
		}
	}
}

// An edge from (xa, ya) to (xb, yb): a horizontal one is left out; one going down is an end of the rows it spans,
// one going up a start. Rows from the top one (in) to the bottom one (out), within the mask; each takes the edge's
// column there, stepped in 16.16
void AddEdge(ShadowRaster& raster, float xa, int32_t ya, float xb, int32_t yb)
{
	if (ya == yb)
	{
		return;
	}
	auto* table = &raster.ends;
	if (ya > yb)
	{
		std::swap(xa, xb);
		std::swap(ya, yb);
		table = &raster.starts;
	}
	if (ya >= raster.clipY || yb <= 0)
	{
		return;
	}
	const auto height = static_cast<int32_t>(static_cast<uint32_t>(yb) - static_cast<uint32_t>(ya));
	// the height divides exactly as a whole number: a single float division while it holds in one
	constexpr int32_t k_ExactFloat = 1 << 24;
	const float run = xb - xa;
	const float slope = height < k_ExactFloat ? run / static_cast<float>(height)
	                                          : static_cast<float>(static_cast<double>(run) / static_cast<double>(height));
	yb = std::min(yb, raster.clipY);
	if (ya < 0)
	{
		xa = xa - IntTimes(ya, slope);
		ya = 0;
	}
	raster.minY = std::min(raster.minY, ya);
	raster.maxY = std::max(raster.maxY, yb);
	const int32_t step = Truncate(static_cast<double>(slope) * k_Fixed);
	auto column = static_cast<uint32_t>(Truncate(static_cast<double>(xa) * k_Fixed));
	for (int32_t y = ya; y < yb; ++y)
	{
		(*table)[static_cast<size_t>(y)] = static_cast<int32_t>(column) >> 16;
		column += static_cast<uint32_t>(step);
	}
}

// The rows the triangle spans, each from its start column (in) to its end column (out), within the mask
void RenderTriangle(ShadowRaster& raster, std::span<uint8_t> mask, int32_t pitch, uint32_t oddRowsOnly)
{
	if (raster.maxY - raster.minY <= 0)
	{
		return;
	}
	const auto stride = static_cast<size_t>((pitch >> 2) * 4);
	for (int32_t y = raster.minY; y < raster.maxY; ++y)
	{
		const int32_t start = std::max(raster.starts[static_cast<size_t>(y)], 0);
		const int32_t end = std::min(raster.ends[static_cast<size_t>(y)], raster.clipX);
		const bool odd = (y & 1) != 0;
		if (end <= start || start >= raster.clipX || end <= 0 || (!odd && oddRowsOnly != 0))
		{
			continue;
		}
		const uint32_t shift = odd ? 4 : 0;
		const uint32_t full = odd ? k_OddFull : k_EvenFull;
		const auto first = static_cast<uint32_t>(start);
		const auto last = static_cast<uint32_t>(end);
		const size_t row = static_cast<size_t>(y >> 1) * stride;
		const size_t firstWord = first >> 4;
		const size_t lastWord = last >> 4;
		size_t at = row + firstWord * 4;
		if (firstWord == lastWord)
		{
			Or32(mask, at, (EvenStart(first & 15) & EvenEnd(last & 15)) << shift);
			continue;
		}
		Or32(mask, at, EvenStart(first & 15) << shift);
		at += 4;
		for (size_t word = firstWord + 1; word < lastWord; ++word, at += 4)
		{
			Or32(mask, at, full);
		}
		Or32(mask, at, EvenEnd(last & 15) << shift);
	}
}

// (yA - yC) (xB - xC) against (yB - yC) (xA - xC) for corners in that order, as single floats: false when either is
// NaN
bool Faces(float xa, int32_t ya, float xb, int32_t yb, float xc, int32_t yc)
{
	const float left = IntTimes(static_cast<int32_t>(static_cast<uint32_t>(yb) - static_cast<uint32_t>(yc)), xa - xc);
	const float right = IntTimes(static_cast<int32_t>(static_cast<uint32_t>(ya) - static_cast<uint32_t>(yc)), xb - xc);
	return !std::isnan(left) && !std::isnan(right) && !(right < left);
}

// A vertex placed by `m` and moved along the line from the sun to the ground; kept at or above it when `clamp`
glm::vec2 ProjectVertex(const std::array<float, 12>& m, glm::vec3 v, glm::vec3 sun, bool clamp)
{
	const float x = ((m[6] * v.z + m[3] * v.y) + v.x * m[0]) + m[9];
	float y = ((m[1] * v.x + m[7] * v.z) + m[4] * v.y) + m[10];
	const float z = ((m[2] * v.x + m[8] * v.z) + m[5] * v.y) + m[11];
	if (clamp && (std::isnan(y) || y < 0.0f))
	{
		y = 0.0f;
	}
	const float along = -sun.y / (y - sun.y);
	return {(x - sun.x) * along + sun.x, (z - sun.z) * along + sun.z};
}
} // namespace

ShadowRaster::ShadowRaster(int32_t size)
    : clipX(4 * size)
    , clipY(2 * size)
    , minY(2 * size)
{
}

std::vector<glm::vec2> ProjectShadow(std::span<const glm::vec3> vertices, const std::array<float, 12>& matrix,
                                     float objectHeight, glm::vec3 sun)
{
	auto m = matrix;
	m[10] = -objectHeight + m[10];
	std::vector<glm::vec2> ground;
	ground.reserve(vertices.size());
	for (const auto& v : vertices)
	{
		ground.push_back(ProjectVertex(m, v, sun, true));
	}
	return ground;
}

std::vector<glm::vec2> ProjectSkinnedShadow(std::span<const glm::vec3> vertices, std::span<const BoneRun> runs,
                                            std::span<const std::array<float, 12>> bones, float objectHeight, glm::vec3 sun)
{
	std::vector<glm::vec2> ground;
	ground.reserve(vertices.size());
	size_t next = 0;
	for (const auto& run : runs)
	{
		if (run.bone >= bones.size())
		{
			break;
		}
		auto m = bones[run.bone];
		m[10] = m[10] + -objectHeight;
		for (uint16_t i = 0; i < run.count && next < vertices.size(); ++i, ++next)
		{
			ground.push_back(ProjectVertex(m, vertices[next], sun, false));
		}
	}
	return ground;
}

glm::vec2 ShadowMaskPoint(glm::vec2 ground, glm::vec2 blockOrigin, int32_t size)
{
	const float rowScale = static_cast<float>(size) * 0.0125f;
	const float columnScale = static_cast<float>(size) * 0.025f;
	return {(ground.y - blockOrigin.y) * columnScale, (ground.x - blockOrigin.x) * rowScale};
}

void DrawShadowTriangles(ShadowRaster& raster, std::span<uint8_t> mask, int32_t pitch, std::span<const glm::vec2> points,
                         std::span<const std::array<uint16_t, 3>> triangles, bool doubleSided, bool temple,
                         uint32_t oddRowsOnly)
{
	for (const auto& [ia, ib, ic] : triangles)
	{
		if (ia >= points.size() || ib >= points.size() || ic >= points.size())
		{
			continue;
		}
		const float xa = points[ia].x;
		const float xb = points[ib].x;
		const float xc = points[ic].x;
		const int32_t ya = Truncate(points[ia].y);
		const int32_t yb = Truncate(points[ib].y);
		const int32_t yc = Truncate(points[ic].y);
		if (!doubleSided && !temple)
		{
			if (!Faces(xa, ya, xb, yb, xc, yc))
			{
				continue;
			}
			raster.minY = raster.clipY;
			raster.maxY = 0;
			AddEdge(raster, xa, ya, xc, yc);
			AddEdge(raster, xc, yc, xb, yb);
			AddEdge(raster, xb, yb, xa, ya);
		}
		else
		{
			raster.minY = raster.clipY;
			raster.maxY = 0;
			// the winding picked the other way round: with B as the reference corner
			if (!Faces(xa, ya, xc, yc, xb, yb))
			{
				AddEdge(raster, xa, ya, xc, yc);
				AddEdge(raster, xc, yc, xb, yb);
				AddEdge(raster, xb, yb, xa, ya);
			}
			else
			{
				AddEdge(raster, xa, ya, xb, yb);
				AddEdge(raster, xb, yb, xc, yc);
				AddEdge(raster, xc, yc, xa, ya);
			}
		}
		RenderTriangle(raster, mask, pitch, oddRowsOnly);
	}
}

namespace
{
// A texel row of the texture shadow: its nearest and farthest texel the edges reached, with the texture coordinates
// there
struct AlphaRow
{
	bool reached {false};
	int32_t near {0};
	int32_t far {0};
	float nearU {0.0f};
	float farU {0.0f};
	float nearV {0.0f};
	float farV {0.0f};
};

struct AlphaCorner
{
	float row;
	float column;
	float u;
	float v;
};

void Reach(std::vector<AlphaRow>& rows, int32_t row, int32_t column, float u, float v)
{
	if (row < 0 || row > static_cast<int32_t>(rows.size()) - 1)
	{
		return;
	}
	auto& line = rows[static_cast<size_t>(row)];
	if (!line.reached)
	{
		line = {.reached = true, .near = column, .far = column, .nearU = u, .farU = u, .nearV = v, .farV = v};
	}
	else if (line.near > column)
	{
		line.near = column;
		line.nearU = u;
		line.nearV = v;
	}
	else if (line.far < column)
	{
		line.far = column;
		line.farU = u;
		line.farV = v;
	}
}

// From texel to texel (each end truncated) along the longer axis, both ends included, from the end with the lower row
void WalkAlphaEdge(std::vector<AlphaRow>& rows, const AlphaCorner& a, const AlphaCorner& b)
{
	const auto& [from, to] = a.row > b.row ? std::pair {b, a} : std::pair {a, b};
	int32_t row = static_cast<int32_t>(from.row);
	int32_t column = static_cast<int32_t>(from.column);
	int32_t rowSpan = static_cast<int32_t>(to.row) - row;
	int32_t columnSpan = static_cast<int32_t>(to.column) - column;
	const int32_t stepRow = rowSpan < 0 ? -1 : 1;
	const int32_t stepColumn = columnSpan < 0 ? -1 : 1;
	rowSpan = std::abs(rowSpan);
	columnSpan = std::abs(columnSpan);
	float u = from.u;
	float v = from.v;
	int32_t error = 0;
	Reach(rows, row, column, u, v);
	if (rowSpan < columnSpan)
	{
		const float du = (to.u - from.u) / static_cast<float>(columnSpan);
		const float dv = (to.v - from.v) / static_cast<float>(columnSpan);
		for (int32_t i = 0; i < columnSpan; ++i)
		{
			column += stepColumn;
			v += dv;
			error += rowSpan;
			u += du;
			if (error > columnSpan)
			{
				row += stepRow;
				error -= columnSpan;
			}
			Reach(rows, row, column, u, v);
		}
		return;
	}
	if (rowSpan == 0)
	{
		return;
	}
	const float du = (to.u - from.u) / static_cast<float>(rowSpan);
	const float dv = (to.v - from.v) / static_cast<float>(rowSpan);
	for (int32_t i = 0; i < rowSpan; ++i)
	{
		row += stepRow;
		u += du;
		error += columnSpan;
		v += dv;
		if (error > rowSpan)
		{
			column += stepColumn;
			error -= rowSpan;
		}
		Reach(rows, row, column, u, v);
	}
}

void FillAlphaRows(std::vector<AlphaRow>& rows, std::span<uint8_t> mask, int32_t size, std::span<const uint8_t> alpha)
{
	constexpr int32_t k_Side = 64;
	for (int32_t row = 0; row < size; ++row)
	{
		auto& line = rows[static_cast<size_t>(row)];
		if (!line.reached)
		{
			continue;
		}
		const int32_t span = line.far - line.near;
		float du = 0.0f;
		float dv = 0.0f;
		if (span != 0)
		{
			const float inverse = 1.0f / static_cast<float>(span);
			du = (line.farU - line.nearU) * inverse;
			dv = (line.farV - line.nearV) * inverse;
		}
		float u = line.nearU;
		float v = line.nearV;
		const int32_t last = std::min(line.far, size - 1);
		for (int32_t column = line.near; column <= last; ++column)
		{
			if (column >= 0)
			{
				const auto ui = static_cast<int32_t>(std::lrint(u + 0.5f)) & (k_Side - 1);
				const auto vi = static_cast<int32_t>(std::lrint(v + 0.5f)) & (k_Side - 1);
				mask[static_cast<size_t>(row * size + column)] |= alpha[static_cast<size_t>(vi * k_Side + ui)];
			}
			u += du;
			v += dv;
		}
		line.reached = false;
	}
}
} // namespace

void DrawTextureShadow(std::span<uint8_t> mask, int32_t size, glm::uvec2 block, const std::array<float, 12>& placement,
                       std::span<const TextureShadowPart> parts, glm::vec3 sun)
{
	constexpr float k_BlockSide = 160.0f;
	constexpr float k_PerUnit = 0.00625f;
	if (size <= 0 || mask.size() < static_cast<size_t>(size) * static_cast<size_t>(size))
	{
		return;
	}
	// the object's matrix moved to the block, its height dropped
	auto m = placement;
	m[9] = placement[9] - static_cast<float>(block.x) * k_BlockSide;
	m[10] = 0.0f;
	m[11] = placement[11] - static_cast<float>(block.y) * k_BlockSide;
	const float sunX = (m[9] - placement[9]) + sun.x;
	const float sunY = sun.y;
	const float sunZ = (m[11] - placement[11]) + sun.z;
	const auto texelsPerSide = static_cast<float>(size);
	std::vector<AlphaRow> rows(static_cast<size_t>(size));
	for (const auto& part : parts)
	{
		if (part.alpha.size() < 64 * 64 || part.uvs.size() < part.positions.size())
		{
			continue;
		}
		const auto corner = [&](uint16_t index) {
			const auto p = part.positions[index];
			const auto uv = part.uvs[index];
			const float x = ((m[6] * p.z + m[3] * p.y) + m[0] * p.x) + m[9];
			float y = ((m[1] * p.x + m[7] * p.z) + m[4] * p.y) + m[10];
			const float z = ((m[5] * p.y + m[2] * p.x) + m[8] * p.z) + m[11];
			if (y < 0.0f)
			{
				y = 0.0f;
			}
			const float along = sunY / (sunY - y);
			return AlphaCorner {
			    .row = (((x - sunX) * along + sunX) * texelsPerSide) * k_PerUnit,
			    .column = (((z - sunZ) * along + sunZ) * texelsPerSide) * k_PerUnit,
			    .u = uv.x * 63.0f + 0.5f,
			    .v = uv.y * 63.0f + 0.5f,
			};
		};
		for (const auto& [ia, ib, ic] : part.triangles)
		{
			if (ia >= part.positions.size() || ib >= part.positions.size() || ic >= part.positions.size())
			{
				continue;
			}
			const auto a = corner(ia);
			const auto b = corner(ib);
			const auto c = corner(ic);
			WalkAlphaEdge(rows, a, b);
			WalkAlphaEdge(rows, b, c);
			WalkAlphaEdge(rows, c, a);
			FillAlphaRows(rows, mask, size, part.alpha);
		}
	}
}

} // namespace openblack::block_texture
