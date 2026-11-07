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

#include <entt/core/fwd.hpp>

// Which parts of a model the scene draws, and how brightly the land's light lights a model. Pure: values only.
namespace openblack::graphics::submesh_draw
{

/// Whether the scene draws a part of a model: never a collision-only part, only a part of the status the draw takes
/// (mesh_status::Drawn), and only a part of the first level of detail (bit 0 of its level of detail mask), but for a
/// window part, which has no level of detail bits and shows by its instance's windows
[[nodiscard]] constexpr bool Drawn(bool physics, bool statusDrawn, uint32_t lodMask, bool window)
{
	return !(physics || !statusDrawn || ((lodMask & 1) != 1 && !window));
}

/// How much brighter than the land's light a model is lit: the hand half again, anything else as it is
[[nodiscard]] constexpr float LightBoost(entt::id_type meshId, entt::id_type handMeshId)
{
	return meshId == handMeshId ? 1.5f : 1.0f;
}

/// Whether a model's triangles are drawn turned round: the hand's when its matrix mirrors it (its determinant below
/// 0), as the original turns every triangle of the hand round as it mirrors it; never any other model's
[[nodiscard]] constexpr bool TrianglesTurnedRound(entt::id_type meshId, entt::id_type handMeshId, float determinant)
{
	return meshId == handMeshId && determinant < 0.0f;
}

} // namespace openblack::graphics::submesh_draw
