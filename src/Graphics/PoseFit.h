/******************************************************************************
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

#include <optional>

// Whether a pose fits the mesh it is drawn on. The original poses an object's own (std) mesh and draws a finer mesh
// with those matrices by bone index, with no remap: the drawn mesh's vertices take the matrix of their bone's number.
// Pure: values only.
namespace openblack::graphics::pose_fit
{

/// A pose of `poseBones` matrices fits a mesh whose skinned vertices use bones up to `highestSkinnedBone` (none: no
/// skinned vertex): every bone it uses has a matrix
[[nodiscard]] constexpr bool PoseFits(size_t poseBones, std::optional<uint16_t> highestSkinnedBone)
{
	return poseBones > 0 && (!highestSkinnedBone.has_value() || *highestSkinnedBone < poseBones);
}

} // namespace openblack::graphics::pose_fit
