/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameInputs.h"

#include <cmath>
#include <cstdint>

#include <LNDFile.h>

#include "3D/AffineMatrix.h"
#include "3D/LandBlock.h"
#include "ECS/DrawList/GameBlocks.h"

namespace openblack::ecs::draw_list
{

DrawCamera MakeDrawCamera(const FrameCamera& camera)
{
	const auto matrices = affine::FrameMatrices(camera.eye, camera.focus, camera.horizontalFov, camera.aspect);
	const auto width = static_cast<float>(camera.screen.x);
	const auto height = static_cast<float>(camera.screen.y);
	const auto& toCamera = matrices.worldToCamera.m;
	return {
	    .eye = camera.eye,
	    .focus = camera.focus,
	    .viewDirection = {toCamera[2], toCamera[5], toCamera[8]},
	    .worldToClipping = matrices.worldToClipping,
	    .nearW = camera.nearClip,
	    .half = {width * 0.5f, height * 0.5f},
	    .maxScreen = glm::vec2(static_cast<float>(camera.screen.x - 1), static_cast<float>(camera.screen.y - 1)),
	    .landReflection = camera.landReflection,
	};
}

float NearHalfWidth(float horizontalFov, float nearClip)
{
	// Half the angle is exact in float; the tangent and the product are kept wider and rounded once
	return static_cast<float>(std::tan(static_cast<double>(horizontalFov * 0.5f)) * static_cast<double>(nearClip));
}

OnScreenView MakeOnScreenView(const FrameCamera& camera, const DrawCamera& drawCamera)
{
	return {
	    .worldToClipping = drawCamera.worldToClipping,
	    .eye = drawCamera.eye,
	    .nearClip = drawCamera.nearW,
	    .nearHalfWidth = NearHalfWidth(camera.horizontalFov, camera.nearClip),
	    .screen = camera.screen,
	};
}

void CollectBlocks(std::span<const LandBlock> land, std::vector<systems::DrawListBlock>& out)
{
	out.clear();
	out.reserve(land.size());
	for (const auto& block : land)
	{
		const auto& record = block.GetLndBlock();
		if (!record)
		{
			continue;
		}
		const bool inTable = record->blockX < k_BlockTableSide && record->blockZ < k_BlockTableSide;
		const auto slot =
		    inTable ? static_cast<uint16_t>((record->blockX << 5u) + record->blockZ) : static_cast<uint16_t>(k_BlockSlots);
		out.push_back({
		    .mapPos = block.GetMapPosition(),
		    .highestAltitude = static_cast<int32_t>(record->highestAltitude),
		    .slot = slot,
		    .initial = InitialBlockState(record->valueSorting),
		    .coords = {static_cast<int32_t>(record->blockX), static_cast<int32_t>(record->blockZ)},
		    .cells = record->cells,
		});
	}
}

int32_t DetailIndexOf(uint8_t detailLevel)
{
	constexpr uint8_t k_HighestDetailIndex = 4;
	return static_cast<int32_t>(detailLevel > k_HighestDetailIndex ? k_HighestDetailIndex : detailLevel);
}

} // namespace openblack::ecs::draw_list
