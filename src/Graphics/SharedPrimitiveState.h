/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/L3DSubMesh.h"

namespace openblack::graphics::shared_primitive_state
{

/// Whether `next`, drawn right after `previous` with the same texture, can keep the render state, uniforms and
/// samplers set for `previous` instead of setting its own (Renderer::DrawSubMesh). Every material field the draw
/// reads into that state must match: the mode and blending, the alpha test, the depth write, the wrapping, the texture
/// offset, the partly built flags and the culling, since the game sets the culling from each primitive's own material
/// (two-sided or not). The material colour is left out: it is only read by untextured primitives, which never share.
[[nodiscard]] constexpr bool SharesState(const L3DSubMesh::Primitive& previous, const L3DSubMesh::Primitive& next) noexcept
{
	return next.materialType == previous.materialType && next.blend == previous.blend &&
	       next.thresholdAlpha == previous.thresholdAlpha && next.depthWrite == previous.depthWrite &&
	       next.alphaCutoutThreshold == previous.alphaCutoutThreshold && next.wrap == previous.wrap &&
	       next.uvOffset == previous.uvOffset && next.partialBuildCap == previous.partialBuildCap &&
	       next.partialBuild == previous.partialBuild && next.twoSided == previous.twoSided;
}

} // namespace openblack::graphics::shared_primitive_state
