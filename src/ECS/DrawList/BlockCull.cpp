/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockCull.h"

#include <cmath>

#include <algorithm>
#include <initializer_list>

#include "Camera/NearClipping.h"
#include "Graphics/Haze.h"

namespace openblack::ecs::draw_list
{

BlockCull CullBlock(const std::array<glm::vec3, 8>& corners, const affine::AffineMatrix& worldToClipping, float nearW)
{
	constexpr uint32_t k_AllCorners = 0xFFu;
	uint32_t nearMask = 0;
	uint32_t right = 0;
	uint32_t left = 0;
	uint32_t top = 0;
	uint32_t bottom = 0;
	for (size_t i = 0; i < corners.size(); ++i)
	{
		const uint32_t bit = 1u << i;
		// the block cull sums each column in the same order as the shadow blocks do
		const glm::vec3 clip = affine::ToClipForShadowBlocks(worldToClipping, corners.at(i));
		const float depth = clip.z;
		if (!(depth >= nearW))
		{
			nearMask |= bit;
		}
		if (clip.x > depth)
		{
			right |= bit;
		}
		else if (-depth > clip.x)
		{
			left |= bit;
		}
		if (clip.y > depth)
		{
			top |= bit;
		}
		else if (-depth > clip.y)
		{
			bottom |= bit;
		}
	}
	const std::array masks {nearMask, right, left, top, bottom};
	if (std::ranges::any_of(masks, [](uint32_t mask) { return mask == k_AllCorners; }))
	{
		return {};
	}
	return {
	    .visible = true,
	    .partlyOutside = std::ranges::any_of(masks, [](uint32_t mask) { return mask != 0; }),
	};
}

float BlockDistance(glm::vec2 blockMapPos, int32_t highestAltitude, bool landReflection, glm::vec3 eye)
{
	const float centreX = blockMapPos.x + k_BlockHalfSide;
	const float centreY = landReflection ? 0.0f : static_cast<float>(highestAltitude) * k_BlockHeightScale * 0.5f;
	const float centreZ = blockMapPos.y + k_BlockHalfSide;
	const float dx = centreX - eye.x;
	const float dy = centreY - eye.y;
	const float dz = centreZ - eye.z;
	return std::sqrt((dz * dz + dy * dy) + dx * dx);
}

BlockState NextBlockState(const BlockState& previous, glm::vec2 blockMapPos, int32_t highestAltitude, const DrawCamera& camera,
                          const LodLines& lines)
{
	BlockState next = previous;
	const auto corners = graphics::haze::BlockCorners(blockMapPos, static_cast<float>(highestAltitude), camera.landReflection);
	const auto cull = CullBlock(corners, camera.worldToClipping, camera.nearW);
	if (!cull.visible)
	{
		next.visibility &= ~(k_InViewBit | k_InViewSecondBit);
		next.lod.meshLod = k_CulledMeshLod;
		next.lod.meshBlending = k_CulledMeshBlending;
		return next;
	}
	next.visibility |= k_InViewBit | k_InViewSecondBit;
	next.partlyOutside = cull.partlyOutside;
	const BlockLod lod = ChooseBlockLod(blockMapPos, lines);
	next.lod.meshLod = lod.meshLod;
	next.lod.meshBlending = lod.meshBlending;
	next.distance = BlockDistance(blockMapPos, highestAltitude, camera.landReflection, camera.eye);
	return next;
}

LodLines LodLinesAtLandCreation(const LodLines& current, int32_t detailIndex)
{
	constexpr int32_t k_HighestDetailIndex = 4;
	if (detailIndex != k_HighestDetailIndex)
	{
		return current;
	}
	return {
	    .inner = {.distance = 1600.0f, .width = 550.0f, .halfWidth = 275.0f},
	    .outer = {.distance = 2500.0f, .width = 550.0f, .halfWidth = 275.0f},
	};
}

LodLine RebuildLodLine(const LodLine& previous, glm::vec3 eye, glm::vec3 viewDirection)
{
	constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
	// the original compares each square with this, strictly
	constexpr float k_SmallestSquare = 0.0001f;
	const glm::vec3 f = viewDirection;
	LodLine line = previous;

	// the point on the view's axis at the line's distance, and the offset of the plane through it facing the view
	const float aheadX = eye.x + line.distance * f.x;
	const float aheadY = eye.y + line.distance * f.y;
	const float aheadZ = line.distance * f.z + eye.z;
	const float offset = -((aheadZ * f.z + aheadY * f.y) + aheadX * f.x);

	// up crossed with the view, each component's products in this order
	line.along = {
	    f.z * k_Up.y - k_Up.z * f.y,
	    k_Up.z * f.x - k_Up.x * f.z,
	    k_Up.x * f.y - f.x * k_Up.y,
	};
	const float squareX = line.along.x * line.along.x;
	const float squareY = line.along.y * line.along.y;
	const float squareZ = line.along.z * line.along.z;

	// where the plane meets the ground: on z = 0 when the line runs more along z than along x, else on x = 0. The
	// third case (along.y the largest) cannot happen, since along.y is always 0 or NaN
	if (squareZ > squareY && squareZ > squareX && squareZ > k_SmallestSquare)
	{
		const float inverse = 1.0f / line.along.z;
		line.point = {inverse * offset, inverse * 0.0f, inverse * 0.0f};
	}
	else if (squareX > k_SmallestSquare)
	{
		const float inverse = 1.0f / line.along.x;
		line.point = {inverse * 0.0f, inverse * 0.0f, inverse * -offset};
	}
	else
	{
		// looking straight down: the direction stays unscaled and the point stays last frame's
		return line;
	}
	const float scale = 1.0f / std::sqrt((squareZ + squareY) + squareX);
	line.along = {scale * line.along.x, scale * line.along.y, scale * line.along.z};
	return line;
}

LodLines RebuildLodLines(const LodLines& previous, glm::vec3 eye, glm::vec3 viewDirection)
{
	return {
	    .inner = RebuildLodLine(previous.inner, eye, viewDirection),
	    .outer = RebuildLodLine(previous.outer, eye, viewDirection),
	};
}

LineSide SideOfLine(const LodLine& line, float x, float z)
{
	const float v = (x - line.point.x) * line.along.z - (z - line.point.z) * line.along.x;
	if (v > line.halfWidth)
	{
		return LineSide::Near;
	}
	if (!(v >= -line.halfWidth))
	{
		return LineSide::Far;
	}
	return LineSide::Band;
}

namespace
{
struct CornerSides
{
	int nearCount {};
	int bandCount {};
	int farCount {};
};

CornerSides SidesOfCorners(const LodLine& line, glm::vec2 blockMapPos)
{
	// the side is added to the block's position before the line's point is taken away
	const float x = blockMapPos.x;
	const float z = blockMapPos.y;
	const float farX = blockMapPos.x + k_BlockSideLength;
	const float farZ = blockMapPos.y + k_BlockSideLength;
	CornerSides sides;
	for (const LineSide side :
	     {SideOfLine(line, x, z), SideOfLine(line, x, farZ), SideOfLine(line, farX, z), SideOfLine(line, farX, farZ)})
	{
		switch (side)
		{
		case LineSide::Near:
			++sides.nearCount;
			break;
		case LineSide::Band:
			++sides.bandCount;
			break;
		case LineSide::Far:
			++sides.farCount;
			break;
		}
	}
	return sides;
}
} // namespace

BlockLod ChooseBlockLod(glm::vec2 blockMapPos, const LodLines& lines)
{
	const CornerSides inner = SidesOfCorners(lines.inner, blockMapPos);
	if (inner.bandCount != 0)
	{
		return {.meshLod = 0, .meshBlending = 1};
	}
	if (inner.nearCount != 0)
	{
		// all near is full detail; near and far corners cross the line
		return {.meshLod = 0, .meshBlending = inner.farCount == 0 ? 0u : 1u};
	}
	const CornerSides outer = SidesOfCorners(lines.outer, blockMapPos);
	if (outer.bandCount != 0)
	{
		return {.meshLod = 1, .meshBlending = 3};
	}
	if (outer.nearCount == 0)
	{
		return {.meshLod = 2, .meshBlending = 4};
	}
	if (outer.farCount == 0)
	{
		return {.meshLod = 1, .meshBlending = 2};
	}
	return {.meshLod = 1, .meshBlending = 3};
}

uint32_t SeamBits(glm::ivec2 blockCoords, uint32_t meshLod, std::span<const uint16_t, k_BlockLookupSize> lookup,
                  std::span<const BlockState> blocks)
{
	const auto coarser = [&](glm::ivec2 at) {
		if (at.x < 0 || at.x >= k_BlocksPerSide || at.y < 0 || at.y >= k_BlocksPerSide)
		{
			return false;
		}
		const uint16_t index =
		    lookup[static_cast<size_t>(at.x) * static_cast<size_t>(k_BlocksPerSide) + static_cast<size_t>(at.y)];
		if (index == 0)
		{
			return false;
		}
		// the original compares the steps as signed numbers
		return static_cast<int32_t>(blocks[index - 1u].lod.meshLod) > static_cast<int32_t>(meshLod);
	};
	uint32_t bits = 0;
	if (coarser(blockCoords + glm::ivec2(-1, 0)))
	{
		bits |= k_SeamLowerX;
	}
	if (coarser(blockCoords + glm::ivec2(0, -1)))
	{
		bits |= k_SeamLowerZ;
	}
	if (coarser(blockCoords + glm::ivec2(0, 1)))
	{
		bits |= k_SeamHigherZ;
	}
	if (coarser(blockCoords + glm::ivec2(1, 0)))
	{
		bits |= k_SeamHigherX;
	}
	return bits;
}

void MarkSeams(std::span<BlockState> blocks, std::span<const glm::ivec2> blockCoords,
               std::span<const uint16_t, k_BlockLookupSize> lookup)
{
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		blocks[i].lod.meshLodType = SeamBits(blockCoords[i], blocks[i].lod.meshLod, lookup, blocks);
	}
}

BlockState InitialBlockState(float valueSorting)
{
	return {
	    .distance = valueSorting,
	    .visibility = 0,
	    .partlyOutside = false,
	    .lod = {},
	};
}

void SortVisible(std::span<const BlockState> blocks, std::vector<uint16_t>& out)
{
	out.clear();
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		const BlockState& block = blocks[i];
		if ((block.visibility & k_InViewBit) == 0)
		{
			continue;
		}
		// walk past every block that is nearer, or whose compare is unordered, and stop at the first that is not
		const auto at = std::ranges::find_if(out, [&](uint16_t listed) { return blocks[listed].distance >= block.distance; });
		out.insert(at, static_cast<uint16_t>(i));
	}
}

float NearClipFor(float heightAboveQuantisedGround, bool closeClipping)
{
	return near_clipping::NearPlane(heightAboveQuantisedGround, closeClipping);
}

} // namespace openblack::ecs::draw_list
