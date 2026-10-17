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

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "3D/FrameAnim.h"
#include "Graphics/RenderModes.h"
#include "Particles/Creators/Mesh.h"

// The ghost of an object a store took, a pot that was merged or a building an effect destroyed: a copy of its model
// drawn for 500 ms where it was, as it melts away (graphics::frame_anim::GoolooFrame). Draw state only: no entity,
// nothing the game logic reads. Wiki: docs/wiki/rendering/textures.md#frame-animated-textures, the Gooloo row.

namespace openblack::ecs::object_ghosts
{
/// A ghost of the object's model as it is drawn now (its mesh, its matrix), for 500 ms. Nothing for an object
/// without a mesh
void Add(entt::entity object);
/// While it lives, a ghost of the object is made at the matrix it is drawn with now, even after the hand has let it go:
/// the original's stores take an object while it is still in the hand
class KeepDrawnPose
{
public:
	explicit KeepDrawnPose(entt::entity object);
	~KeepDrawnPose();
	KeepDrawnPose(const KeepDrawnPose&) = delete;
	KeepDrawnPose& operator=(const KeepDrawnPose&) = delete;
	KeepDrawnPose(KeepDrawnPose&&) = delete;
	KeepDrawnPose& operator=(KeepDrawnPose&&) = delete;
};
/// Each ghost's time falls by the frame's ms; the ones that ran out go
void Update(float frameMs);
/// The number of ghosts
[[nodiscard]] size_t Count();
/// One of a ghost's two draws in a frame
struct Pass
{
	graphics::frame_anim::UvOffset uv;
	/// Every primitive drawn in this mode with the mask texture (cool_effect), or none: the model's own materials
	std::optional<graphics::render_modes::Mode> mode;
	bool mask;
	/// The ALPHAREF every alpha tested primitive takes
	uint8_t alphaRef;
	/// Drawn only where the depth equals what is there
	bool depthEqual;
};
/// The original's dissolve, `t` ms left of the ghost's 500: first only the depth, through the cool_effect mask in
/// render mode 18 at the moving offset with the ALPHAREF 255 - truncated(255 t / 500), so less of it passes as t
/// falls; then the model's own materials at no offset with an ALPHAREF of 10, only where that depth was written
[[nodiscard]] std::array<Pass, 2> Passes(float t) noexcept;
/// The id of the mask texture in the texture cache
[[nodiscard]] entt::id_type MaskTexture() noexcept;
/// This frame's draws: each ghost's two passes, in order
[[nodiscard]] std::vector<psys::mesh_atoms::Instance> Instances();
/// Every ghost goes (a new map)
void Clear();
} // namespace openblack::ecs::object_ghosts
