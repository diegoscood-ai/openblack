/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexDraw.h"

#include <cstdint>

#include <algorithm>

namespace openblack::graphics::vortex_draw
{

glm::mat4 ZBoxModel(glm::vec3 position, float baseScale)
{
	// the rows of the box's turn (none) times the base scale, and the vortex's point with its height put to 0
	glm::mat4 model(baseScale);
	model[3] = glm::vec4(position.x, 0.0f, position.z, 1.0f);
	return model;
}

std::optional<glm::ivec2> DecalBlock(glm::vec3 position)
{
	constexpr float k_CellsPerMetre = 0.1f;
	constexpr int32_t k_Cells = 512;
	constexpr int32_t k_CellsPerBlock = 16;
	const auto x = static_cast<int32_t>(position.x * k_CellsPerMetre);
	const auto z = static_cast<int32_t>(position.z * k_CellsPerMetre);
	if (x < 0 || x >= k_Cells || z < 0 || z >= k_Cells)
	{
		return std::nullopt;
	}
	return glm::ivec2(x / k_CellsPerBlock, z / k_CellsPerBlock);
}

DecalUv DecalUvTransform(glm::vec3 position, glm::ivec2 block, float baseScale)
{
	constexpr float k_BlockSize = 160.0f;
	constexpr float k_PerBlock = 0.00625f; // 1 / 160
	// the block's corner less the centre, in blocks, then / the base scale, then centred on the texture
	const float scale = 1.0f / baseScale;
	const float cornerX = static_cast<float>(block.x) * k_BlockSize;
	const float cornerZ = static_cast<float>(block.y) * k_BlockSize;
	const float u = (cornerZ - position.z) * k_PerBlock;
	const float v = (cornerX - position.x) * k_PerBlock;
	return {.scale = scale, .offset = {u * scale + 0.5f, v * scale + 0.5f}};
}

DecalTextures DecalTexturesFor(VortexType type)
{
	if (type == VortexType::Volcano)
	{
		return {.mask = "S_Volcano_Base_Alpha", .ring = "S_Volcano_Base"};
	}
	return {.mask = "S_VortexBaseAlphacopy", .ring = "S_VortexBaseMultiRing"};
}

EffectRole RoleOf(uint32_t effect, std::span<const VortexEffects> vortices)
{
	// 0 is no effect, never a vortex's
	if (effect == 0)
	{
		return EffectRole::Other;
	}
	const auto vortex = std::ranges::find_if(
	    vortices, [effect](const VortexEffects& v) { return effect == v.preLandscape || effect == v.postLandscape; });
	if (vortex == vortices.end())
	{
		return EffectRole::Other;
	}
	if (!vortex->shows)
	{
		return EffectRole::Hidden;
	}
	return effect == vortex->preLandscape ? EffectRole::Ground : EffectRole::Other;
}

} // namespace openblack::graphics::vortex_draw
