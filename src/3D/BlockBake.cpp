/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockBake.h"

#include <algorithm>

namespace openblack::block_texture
{
namespace
{
constexpr float k_BlockSide = 160.0f;
} // namespace

BakeScratch::BakeScratch(int32_t size)
    : size(size)
    , mask(static_cast<size_t>(size + 1) * static_cast<size_t>(size + 1), 0)
    , raster(size)
    , table(ShadowTable(false))
{
}

void BakePrints(std::span<uint16_t> texels, glm::uvec2 block, glm::vec2 mapPosition, const BakeSources& sources,
                BakeScratch& scratch)
{
	const int32_t size = scratch.size;
	if (texels.size() < static_cast<size_t>(size) * static_cast<size_t>(size))
	{
		return;
	}
	if (scratch.lowResolution != sources.lowResolution)
	{
		scratch.table = ShadowTable(sources.lowResolution);
		scratch.lowResolution = sources.lowResolution;
	}
	scratch.raster.clipX = 4 * size;
	scratch.raster.clipY = 2 * size;
	const glm::vec2 origin {static_cast<float>(block.x) * k_BlockSide, static_cast<float>(block.y) * k_BlockSide};
	bool shadowed = false;
	for (const auto& object : sources.objects)
	{
		if (!object.castsShadow && !object.printsFootprint)
		{
			continue;
		}
		if (object.castsShadow && (sources.layers & k_BakeShadows) != 0)
		{
			if (!shadowed)
			{
				std::fill_n(scratch.mask.begin(), static_cast<size_t>(size) * static_cast<size_t>(size), uint8_t {0});
				shadowed = true;
			}
			if (object.shadowFromAlpha)
			{
				DrawTextureShadow(scratch.mask, size, block, object.placement, object.alphaShadow);
			}
			for (const auto& part : object.shadowFromAlpha ? std::span<const ShadowPart> {} : object.shadow)
			{
				const auto ground = part.runs.empty()
				                        ? ProjectShadow(part.vertices, object.placement, object.placement[10])
				                        : ProjectSkinnedShadow(part.vertices, part.runs, part.bones, object.placement[10]);
				std::vector<glm::vec2> points;
				points.reserve(ground.size());
				for (const auto& point : ground)
				{
					points.push_back(ShadowMaskPoint(point, origin, size));
				}
				DrawShadowTriangles(scratch.raster, scratch.mask, size, points, part.triangles, part.doubleSided,
				                    object.temple);
			}
		}
		if (object.footprint.has_value() && (sources.layers & k_BakeFootprints) != 0)
		{
			PrintFootprint(texels, size, block, *object.footprint, object.placement, PrintMode::Colour);
		}
	}
	// the land's own footprints whose box meets the block's square (a box the comparisons can't order is left out by
	// the far sides' tests and kept by the near sides')
	const glm::vec2 far = mapPosition + k_BlockSide;
	for (const auto& print : (sources.layers & k_BakeLandPrints) != 0 ? sources.landPrints : std::span<const LandPrint> {})
	{
		if (mapPosition.x > print.box.z || !(far.x >= print.box.x) || mapPosition.y > print.box.w || !(far.y >= print.box.y))
		{
			continue;
		}
		PrintFootprint(texels, size, block, print.picture, print.placement, print.mode, print.strength);
	}
	if (sources.snow != nullptr && (sources.layers & k_BakeSnow) != 0)
	{
		PrintSnow(texels, size, block, *sources.snow);
	}
	if (shadowed)
	{
		ApplyShadow(texels, std::span(scratch.mask).first(static_cast<size_t>(size) * static_cast<size_t>(size)),
		            scratch.table);
	}
}

} // namespace openblack::block_texture
