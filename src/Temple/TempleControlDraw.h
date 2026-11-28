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

#include <functional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// How the temple's rooms draw their controls: the room's draw gives each control's sub-mesh the draw data of its
/// control (docs/wiki/interface/temple-interior.md#the-controls-draw-data). Everything here is pure.
namespace openblack::temple::control_draw
{

/// The sub-meshes of a room's mesh that are its controls, in order: those of the mesh's sub-meshes, 0 to count - 1,
/// that the room's controls take
[[nodiscard]] std::vector<uint32_t> ControlSubMeshes(uint32_t subMeshCount, const std::function<bool(uint32_t)>& isControl);

/// A control's sub-mesh is drawn in the light's colour times its draw data's white: each byte times 255, then down by 8
/// bits, so 255 becomes 254, as the game multiplies a draw data's colour
[[nodiscard]] glm::vec3 TimesWhite(glm::vec3 colour);

/// How far a button held down sinks, along the z of its submesh's frame
inline constexpr float k_HeldButtonSink = 0.3f;
/// Where a button held down moves in its mesh: 0.3 back along its frame's z, as its draw data's matrix moves it in its
/// submesh's frame (the frame's axes as they are, with their lengths)
[[nodiscard]] glm::vec3 HeldButtonOffset(const glm::mat4& frameToMesh);

} // namespace openblack::temple::control_draw
