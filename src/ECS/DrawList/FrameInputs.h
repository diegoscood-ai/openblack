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

#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/DrawList/BlockCull.h"
#include "ECS/DrawList/ObjectOnScreen.h"
#include "ECS/Systems/DrawListSystemInterface.h"

namespace openblack
{
class LandBlock;
}

/// What one drawn frame gives the object draw list: its camera, as the block cull and the objects' on-screen test read
/// it, and the land's blocks. Pure: every input is a value. docs/bw1-notes/original-frame.md §6
namespace openblack::ecs::draw_list
{

/// The drawn camera of one frame, once nothing else moves it before the draw
struct FrameCamera
{
	/// The drawn eye and focus, the shake included
	glm::vec3 eye {};
	glm::vec3 focus {};
	/// The horizontal field of view, in radians, and the width over the height
	float horizontalFov {};
	float aspect {};
	/// The viewport in pixels
	glm::ivec2 screen {};
	/// The near clip (NearClipFor), worked out from the previous frame's drawn eye
	float nearClip {};
	/// The land is drawn with its reflection
	bool landReflection {};
};

/// The camera as the block cull reads it: the world-to-clipping matrix built from the eye, the focus, the field of view
/// and the aspect (its depth is the distance along the view, whatever the GPU's near plane), the view direction of its
/// world-to-camera matrix, half the viewport, and the screen clamp (the width and the height less one, as floats)
[[nodiscard]] DrawCamera MakeDrawCamera(const FrameCamera& camera);

/// Half the near plane's width: tan(fov / 2) times the near clip, rounded to a float once
[[nodiscard]] float NearHalfWidth(float horizontalFov, float nearClip);

/// The camera as the objects' on-screen test reads it, with the same matrix and near clip as the block cull's
[[nodiscard]] OnScreenView MakeOnScreenView(const FrameCamera& camera, const DrawCamera& drawCamera);

/// The land's blocks in the land's order, as the list reads them: each one's map position, highest altitude, slot in
/// the block table ((block x << 5) + block z, or k_BlockSlots, which has no array, for a block outside the 32 x 32
/// table), the state it starts a land with (InitialBlockState), its place on the block grid and its cell records. A
/// block with no land record is left out. `out` is cleared first
void CollectBlocks(std::span<const LandBlock> land, std::vector<systems::DrawListBlock>& out);

/// The detail index the original would run with for openblack's detail level: the level itself up to 4. Above 4 it
/// is 4, as the original's start-up turns a stored index of 5 or more into 4; the original never runs above 4
[[nodiscard]] int32_t DetailIndexOf(uint8_t detailLevel);

} // namespace openblack::ecs::draw_list
