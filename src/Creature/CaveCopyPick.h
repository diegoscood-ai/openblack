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

#include <array>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/ScreenPick.h"
#include "Creature/CreatureMorph.h"

/// How the temple's creature room finds the copy of the player's creature under the cursor, as the game does while it
/// draws the copy: the cursor must be over the body's bounding sphere, and then over one of the triangles of the body as
/// it is drawn, posed and shaped. Then the creature is the room's target ahead of the others.
namespace openblack::cave_copy_pick
{

/// The body's bounding sphere in the world: its centre, its radius (the mesh's half diagonal times its scale) and the
/// body's origin
struct Sphere
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
	glm::vec3 origin {0.0f};
};

/// A primitive of a drawn sub-mesh: its triangles, three indices each, in the part's indices
struct Primitive
{
	uint32_t first {0};
	uint32_t count {0};
	/// Drawn from both sides: no triangle facing away is dropped
	bool twoSided {false};
};

/// A drawn sub-mesh of the body, its corners in the world
struct Part
{
	std::vector<glm::vec3> corners;
	std::vector<glm::vec2> uvs;
	std::vector<uint16_t> indices;
	std::vector<Primitive> primitives;
};

/// A sub-mesh's corners posed in the world: each corner's position in its bone's space, pulled towards the shape's
/// meshes by how far along each axis the body is (a mesh whose corners don't match the base's in number stands in as
/// the base), then placed by its bone and the body's matrix. A sub-mesh that isn't skinned, or a body without bones,
/// takes the body's matrix alone; a bone past the pose's takes the first.
[[nodiscard]] std::vector<glm::vec3> PoseCorners(const glm::mat4& model, std::span<const glm::mat4> bones, bool skinned,
                                                 std::span<const uint16_t> skinBones, std::span<const glm::vec3> base,
                                                 const std::array<std::span<const glm::vec3>, 3>& shapes,
                                                 const creature_morph::Morph& morph);

/// Whether the cursor is over the body: over its sphere, then over the first triangle of any part that covers it
[[nodiscard]] bool CursorOver(const screen_pick::View& view, const Sphere& sphere, std::span<const Part> parts);

} // namespace openblack::cave_copy_pick
