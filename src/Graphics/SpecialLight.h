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

#include <L3DFile.h>
#include <glm/vec2.hpp>

/// A model drawn as a special light, as the light that shines out of an opened reward chest: its primitives of the
/// additive mode that writes no depth (render mode 13) are not lit. Each takes the light's own colour, solid on the
/// vertices of its second group and clear on all the others, and its texture turns about its middle with the engine's
/// clock; the land's colour and the haze are still added over it. Its other primitives are drawn as any model's.
/// docs/wiki/scripts/map-script-objects.md#reward-chests-create_reward-239-create_reward_in_town-240
namespace openblack::graphics::special_light
{

/// The light's colour, ARGB: the same for every model drawn so
inline constexpr uint32_t k_Colour = 0xFF203DE8u;

/// The opened chest's model makes its first sub-mesh's eighth primitive (the cone of light) one of the special light's
/// primitives, with its texture tiled, when the chest opens
inline constexpr uint8_t k_ChestLightSubMesh = 0;
inline constexpr uint8_t k_ChestLightPrimitive = 7;

/// How far the texture has turned by the engine's clock, in radians: the milliseconds times -pi / 10000 in single
/// precision, half a turn every ten seconds the negative way
[[nodiscard]] float TextureTurn(int32_t engineMs);

/// A vertex's texture coordinates turned about the middle of the texture: (u - 0.5) cos + (v - 0.5) sin + 0.5 across,
/// (u - 0.5) sin - (v - 0.5) cos + 0.5 down, so the texture is also mirrored top to bottom
[[nodiscard]] glm::vec2 TurnedUv(glm::vec2 uv, float cosTurn, float sinTurn);

/// The bone that tells the vertices of a primitive's second group from the others: none when the primitive has fewer
/// than two groups, or when another of its groups has that bone too
[[nodiscard]] std::optional<uint16_t> SolidBone(std::span<const l3d::L3DVertexGroup> groups);

} // namespace openblack::graphics::special_light
