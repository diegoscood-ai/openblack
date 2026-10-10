/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// What the landscape vortices draw on and under the land (docs/bw1-notes/vortex.md, "Drawing"), without bgfx or ECS
/// so that the tests can run it. The renderer's side is RendererVortex.cpp.
///
/// The game runs the x87 FPU at 24 bits of precision, so every operation here is float, one rounding per operation in
/// the original's order; float to int conversion truncates towards zero.
namespace openblack::graphics::vortex_draw
{

/// The depth box's model matrix: the box stands at the vortex's x and z with its top at height 0, scaled by the
/// vortex's base scale and not turned
[[nodiscard]] glm::mat4 ZBoxModel(glm::vec3 position, float baseScale);

/// The land block (x and z, of 32 x 32) holding the vortex's centre: its cell's x / 10 and z / 10, truncated, on the
/// 512 x 512 cells of a land, / 16. None for a centre off those cells
[[nodiscard]] std::optional<glm::ivec2> DecalBlock(glm::vec3 position);

/// The decal's texture coordinates on its block, from the land's own: the land's u runs along z and its v along x, 0 to
/// 1 across the block. decal = scale x land + offset: centred on the vortex, the textures span 160 x baseScale m
struct DecalUv
{
	float scale;
	glm::vec2 offset;
};
[[nodiscard]] DecalUv DecalUvTransform(glm::vec3 position, glm::ivec2 block, float baseScale);

/// The decal's textures by the vortex's type: the mask (its alpha opens the hole) and the ring laid over the land
struct DecalTextures
{
	std::string_view mask;
	std::string_view ring;
};
[[nodiscard]] DecalTextures DecalTexturesFor(VortexType type);

/// One vortex's effects that are drawn every frame (their effect ids, 0 for none) and whether it shows this frame (its
/// fade is not 0)
struct VortexEffects
{
	uint32_t preLandscape;
	uint32_t postLandscape;
	bool shows;
};
/// What a drawn particle effect is to the vortices this frame
enum class EffectRole : uint8_t
{
	Other,  ///< drawn as any other effect
	Ground, ///< a shown vortex's ground effect: its surfaces are drawn right after the depth boxes, before the land
	Hidden, ///< the ground effect or the effect over the land of a vortex that does not show: not drawn
};
[[nodiscard]] EffectRole RoleOf(uint32_t effect, std::span<const VortexEffects> vortices);

} // namespace openblack::graphics::vortex_draw
