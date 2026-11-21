/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockPrints.h"

#include <cmath>

#include <algorithm>
#include <bit>
#include <utility>

namespace openblack::block_texture
{
namespace
{
constexpr float k_BlockSide = 160.0f;
// one texel a block side over 160, as the game's single float holds 1 / 160
constexpr float k_PerUnit = 0.00625f;
constexpr int32_t k_MaxSize = 256;

// A corner of a triangle in the texture's texels, and the picture's texel it takes
struct Corner
{
	float x;
	float z;
	float u;
	float v;
};

// What the edges of a triangle reached in one texel row (along x): the nearest and farthest z, each with the picture's
// texel there
struct Row
{
	bool reached {false};
	int32_t nearZ {0};
	int32_t farZ {0};
	float nearU {0.0f};
	float farU {0.0f};
	float nearV {0.0f};
	float farV {0.0f};
};

using Rows = std::array<Row, k_MaxSize>;

// To the nearest whole, ties to even, as the game's conversions round
int32_t Nearest(float value)
{
	return static_cast<int32_t>(std::lrint(value));
}

void Reach(Rows& rows, int32_t size, int32_t x, int32_t z, float u, float v)
{
	if (x < 0 || x > size - 1)
	{
		return;
	}
	auto& row = rows[static_cast<size_t>(x)];
	if (!row.reached)
	{
		row = {.reached = true, .nearZ = z, .farZ = z, .nearU = u, .farU = u, .nearV = v, .farV = v};
	}
	else if (row.nearZ > z)
	{
		row.nearZ = z;
		row.nearU = u;
		row.nearV = v;
	}
	else if (row.farZ < z)
	{
		row.farZ = z;
		row.farU = u;
		row.farV = v;
	}
}

// From texel to texel along the longer axis, from the end with the lower x, both ends included; the error counter
// starts at 0 and steps the other axis once it is past the longer one's length
void WalkEdge(Rows& rows, int32_t size, const Corner& a, const Corner& b)
{
	const auto& [from, to] = a.x > b.x ? std::pair {b, a} : std::pair {a, b};
	int32_t x = Nearest(from.x);
	int32_t z = Nearest(from.z);
	int32_t dx = Nearest(to.x) - x;
	int32_t dz = Nearest(to.z) - z;
	const int32_t stepX = dx < 0 ? -1 : 1;
	const int32_t stepZ = dz < 0 ? -1 : 1;
	dx = std::abs(dx);
	dz = std::abs(dz);
	float u = from.u;
	float v = from.v;
	Reach(rows, size, x, z, u, v);
	int32_t error = 0;
	if (dx < dz)
	{
		const float du = (to.u - from.u) / static_cast<float>(dz);
		const float dv = (to.v - from.v) / static_cast<float>(dz);
		for (int32_t i = 0; i < dz; ++i)
		{
			v += dv;
			z += stepZ;
			error += dx;
			u += du;
			if (error > dz)
			{
				x += stepX;
				error -= dz;
			}
			Reach(rows, size, x, z, u, v);
		}
		return;
	}
	if (dx == 0)
	{
		return;
	}
	const float du = (to.u - from.u) / static_cast<float>(dx);
	const float dv = (to.v - from.v) / static_cast<float>(dx);
	for (int32_t i = 0; i < dx; ++i)
	{
		u += du;
		x += stepX;
		error += dz;
		v += dv;
		if (error > dx)
		{
			z += stepZ;
			error -= dx;
		}
		Reach(rows, size, x, z, u, v);
	}
}

uint16_t BlendColour(uint16_t land, uint16_t print)
{
	const uint32_t s = land;
	const uint32_t f = print;
	const uint32_t a = (f >> 8) & 0xF0;
	const uint32_t keep = 255 - a;
	const uint32_t r = (((s & 0xF00) * keep + (f & 0xF00) * a) / 255) & 0xF00;
	const uint32_t g = (((s & 0xF0) * keep + (f & 0xF0) * a) / 255) & 0xF0;
	const uint32_t b = (((s & 0xF) * keep + (f & 0xF) * a) / 255) & 0xF;
	return static_cast<uint16_t>((s & 0xF000) | r | g | b);
}

uint16_t BlendAlpha(uint16_t land, uint16_t print, uint32_t strength)
{
	constexpr uint32_t k_Opaque = 0xF000;
	const uint32_t landAlpha = land & 0xF000u;
	uint32_t printAlpha = print & 0xF000u;
	if (strength != k_Opaque)
	{
		printAlpha = std::min(printAlpha + strength, k_Opaque);
	}
	return static_cast<uint16_t>((land & 0xFFFu) | std::min(landAlpha, printAlpha));
}

// Each row from its nearest z to its farthest (within the texture), the picture's texel stepped along by the row's
// own step and taken at the nearest whole texel, clamped into the picture; the rows are cleared for the next triangle
template <typename Blend>
void FillRows(Rows& rows, std::span<uint16_t> texels, int32_t size, const PrintPicture& picture, Blend blend)
{
	for (int32_t x = 0; x < size; ++x)
	{
		auto& row = rows[static_cast<size_t>(x)];
		if (!row.reached)
		{
			continue;
		}
		const auto length = static_cast<float>(row.farZ - row.nearZ);
		float du = 0.0f;
		float dv = 0.0f;
		if (length != 0.0f)
		{
			const float inverse = 1.0f / length;
			du = (row.farU - row.nearU) * inverse;
			dv = (row.farV - row.nearV) * inverse;
		}
		float u = row.nearU;
		float v = row.nearV;
		const int32_t last = std::min(row.farZ, size - 1);
		for (int32_t z = row.nearZ; z <= last; ++z)
		{
			if (z >= 0)
			{
				const int32_t column = std::clamp(Nearest(u), 0, picture.width - 1);
				const int32_t line = std::clamp(Nearest(v), 0, picture.height - 1);
				auto& texel = texels[static_cast<size_t>(size) * static_cast<size_t>(x) + static_cast<size_t>(z)];
				texel = blend(texel, picture.texels[static_cast<size_t>(picture.width * line + column)]);
			}
			u += du;
			v += dv;
		}
		row.reached = false;
	}
}
} // namespace

void PrintFootprint(std::span<uint16_t> texels, int32_t size, glm::uvec2 block, const PrintPicture& picture,
                    const std::array<float, 12>& placement, PrintMode mode, uint32_t strength)
{
	if (size <= 0 || size > k_MaxSize || texels.size() < static_cast<size_t>(size) * static_cast<size_t>(size) ||
	    picture.width <= 0 || picture.height <= 0 ||
	    picture.texels.size() < static_cast<size_t>(picture.width) * static_cast<size_t>(picture.height))
	{
		return;
	}
	const float originX = static_cast<float>(block.x) * k_BlockSide;
	const float originZ = static_cast<float>(block.y) * k_BlockSide;
	const auto texelsPerSide = static_cast<float>(size);
	const uint32_t alphaStrength = (strength & 0xF0u) << 8;
	const auto corner = [&](glm::vec2 local, glm::vec2 texel) {
		// the right axis's and the forward axis's ground components, then the position; adding 0 makes -0 into 0
		const float worldX = ((placement[6] * local.y + placement[0] * local.x) + 0.0f) + placement[9];
		const float worldZ = ((placement[2] * local.x + placement[8] * local.y) + 0.0f) + placement[11];
		return Corner {
		    .x = (worldX - originX) * texelsPerSide * k_PerUnit,
		    .z = (worldZ - originZ) * texelsPerSide * k_PerUnit,
		    .u = texel.x,
		    .v = texel.y,
		};
	};
	Rows rows {};
	for (const auto& triangle : picture.triangles)
	{
		const auto a = corner(triangle.corners[0], triangle.texels[0]);
		const auto b = corner(triangle.corners[1], triangle.texels[1]);
		const auto c = corner(triangle.corners[2], triangle.texels[2]);
		WalkEdge(rows, size, a, b);
		WalkEdge(rows, size, b, c);
		WalkEdge(rows, size, c, a);
		if (mode == PrintMode::Colour)
		{
			FillRows(rows, texels, size, picture, BlendColour);
		}
		else
		{
			FillRows(rows, texels, size, picture,
			         [alphaStrength](uint16_t land, uint16_t print) { return BlendAlpha(land, print, alphaStrength); });
		}
	}
}

std::array<uint8_t, 256> ShadowTable(bool lowResolution)
{
	std::array<uint8_t, 256> table {};
	for (uint32_t i = 0; i < table.size(); ++i)
	{
		const auto covered = static_cast<uint32_t>(std::popcount(i));
		const auto value = static_cast<uint8_t>(covered * 255 / 8);
		table.at(i) = lowResolution ? static_cast<uint8_t>(value >> 1) : value;
	}
	return table;
}

void ApplyShadow(std::span<uint16_t> texels, std::span<const uint8_t> mask, const std::array<uint8_t, 256>& table)
{
	const size_t count = std::min(texels.size(), mask.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (mask[i] == 0)
		{
			continue;
		}
		const uint32_t keep = 255u - (static_cast<uint32_t>(table.at(mask[i])) >> 1);
		const uint32_t texel = texels[i];
		const uint32_t r = ((texel & 0xF00u) * keep >> 8) & 0xF00u;
		const uint32_t g = ((texel & 0xF0u) * keep >> 8) & 0xF0u;
		const uint32_t b = ((texel & 0xFu) * keep >> 8) & 0xFu;
		texels[i] = static_cast<uint16_t>((texel & 0xF000u) | r | g | b);
	}
}

void PrintSnow(std::span<uint16_t> texels, int32_t size, glm::uvec2 block, const SnowLayer& snow)
{
	constexpr int32_t k_CellsPerBlock = 4;
	constexpr int32_t k_NoiseSide = 256;
	constexpr int32_t k_Fixed = 0x10000;
	constexpr int32_t k_Shallow = 5 * k_Fixed;
	constexpr int32_t k_Cells = SnowLayer::k_CellsPerSide;
	if (size < k_CellsPerBlock || size > k_MaxSize || texels.size() < static_cast<size_t>(size) * static_cast<size_t>(size) ||
	    snow.depths.size() < static_cast<size_t>(k_Cells * k_Cells) ||
	    snow.noise.size() < static_cast<size_t>(k_NoiseSide * k_NoiseSide) ||
	    snow.whites.size() < static_cast<size_t>(k_NoiseSide * k_NoiseSide))
	{
		return;
	}
	const int32_t perCell = size / k_CellsPerBlock;
	const int32_t noiseStep = k_NoiseSide / size;
	// a corner's depth in 16.16, truncated; 0 off the map
	const auto corner = [&snow](int32_t x, int32_t z) {
		if (x < 0 || z < 0 || x >= k_Cells || z >= k_Cells)
		{
			return 0;
		}
		return static_cast<int32_t>(snow.depths[static_cast<size_t>(z * k_Cells + x)] * static_cast<float>(k_Fixed));
	};
	// the share of the land's nibble and of the white: w x c / 16, rounded down
	const auto part = [](int32_t weight, uint32_t nibble) { return static_cast<uint32_t>(weight) * nibble >> 4; };
	for (int32_t i = 0; i < k_CellsPerBlock; ++i)
	{
		const auto cellX = static_cast<int32_t>(block.x) * k_CellsPerBlock + i;
		for (int32_t j = 0; j < k_CellsPerBlock; ++j)
		{
			const auto cellZ = static_cast<int32_t>(block.y) * k_CellsPerBlock + j;
			const int32_t nearNear = corner(cellX, cellZ);
			const int32_t nearFar = corner(cellX, cellZ + 1);
			const int32_t farNear = corner(cellX + 1, cellZ);
			const int32_t farFar = corner(cellX + 1, cellZ + 1);
			if (nearNear <= k_Shallow && nearFar <= k_Shallow && farNear <= k_Shallow && farFar <= k_Shallow)
			{
				continue;
			}
			const int32_t stepNear = (farNear - nearNear) / perCell;
			const int32_t stepFar = (farFar - nearFar) / perCell;
			int32_t rowStart = nearNear;
			int32_t rowEnd = nearFar;
			for (int32_t r = 0; r < perCell; ++r)
			{
				const int32_t step = (rowEnd - rowStart) / perCell;
				int32_t depth = rowStart;
				rowStart += stepNear;
				rowEnd += stepFar;
				const int32_t x = i * perCell + r;
				const int32_t noiseRow = i * 64 + r * noiseStep;
				for (int32_t k = 0; k < perCell; ++k)
				{
					const int32_t z = j * perCell + k;
					const auto noiseAt = static_cast<size_t>(noiseRow * k_NoiseSide + j * 64 + k * noiseStep);
					const int32_t level = ((depth >> 16) - snow.noise[noiseAt]) >> 3;
					depth += step;
					if (level <= 0)
					{
						continue;
					}
					auto& texel = texels[static_cast<size_t>(size) * static_cast<size_t>(x) + static_cast<size_t>(z)];
					const uint32_t white = snow.whites[noiseAt];
					uint32_t colour = 0;
					if (level > 15)
					{
						colour = white << 8 | white << 4 | 0xFu;
					}
					else
					{
						const uint32_t land = texel;
						const uint32_t whitePart = part(level, white);
						const int32_t landWeight = 16 - level;
						colour = (part(landWeight, (land >> 8) & 0xFu) + whitePart) << 8 |
						         (part(landWeight, (land >> 4) & 0xFu) + whitePart) << 4 |
						         (part(landWeight, land & 0xFu) + whitePart);
					}
					texel = static_cast<uint16_t>((texel & 0xF000u) | colour);
				}
			}
		}
	}
}

} // namespace openblack::block_texture
