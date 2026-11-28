/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

/// The posed bones of the villagers and animals drawn in a frame, gathered into one floating-point texture that the
/// vertex shader reads (vs_object_palette_instanced.sc). Each instance's bones are found by where they start in it,
/// which its instance carries, so the posed instances of a mesh are drawn in one instanced draw instead of one draw
/// each with their bones uploaded as uniforms.
namespace openblack::graphics::bone_palette
{

/// Each bone is four texels: the columns of its matrix, the same four vectors a bone uploaded as a uniform holds, so
/// that the shader meets the vertex with the very same matrix
constexpr std::size_t k_TexelsPerBone = 4;
/// The texture's width in texels: 256 bones a row, a bone never split between two rows
constexpr uint16_t k_Width = 1024;

/// Adds the bones to the end of the palette and returns the index of the first of them
uint32_t Append(std::vector<glm::vec4>& texels, std::span<const glm::mat4> bones);

/// The rows of the texture the palette fills, at least one
[[nodiscard]] uint16_t RowsFor(std::size_t texels) noexcept;

/// The bone back from the palette, as the vertex shader reads it: for tests
[[nodiscard]] glm::mat4 BoneAt(std::span<const glm::vec4> texels, uint32_t bone);

/// Where an instance's bones start, carried in the w of its model matrix's third column, which the object shaders
/// never read (the first column's w is the fade, the second's the texture offset)
void SetFirstBone(glm::mat4& model, uint32_t bone) noexcept;
[[nodiscard]] uint32_t FirstBone(const glm::mat4& model) noexcept;

/// Whether draws in this render state (bgfx state bits) leave the same pixels in whatever order they are made: tested
/// against and writing the depth, with an ordering depth test, and not blended. Only fragments at exactly the same
/// depth could then tell the orders apart
[[nodiscard]] bool DrawsInAnyOrder(uint64_t state) noexcept;

} // namespace openblack::graphics::bone_palette
