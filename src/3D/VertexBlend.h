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

#include <glm/vec3.hpp>

/// Where the parts of a boned body meet, a vertex placed by one bone is drawn part of the way towards a vertex at the
/// same place placed by the next, so the seams at the shoulders, neck and tail bend rather than tear. Each primitive
/// lists its blends: the vertex that moves, the vertex it moves towards and how far. Only the position moves, after both
/// vertices have been placed by their own bones; the normals, colours and texture coordinates stay. The game's meshes
/// never move a vertex twice, nor towards a vertex that moves itself, so each vertex has at most one partner.
namespace openblack::vertex_blend
{
/// A blend as a primitive lists it, its vertices counted in the primitive
struct Blend
{
	uint16_t vertex;
	uint16_t towards;
	float weight;
};

/// What a vertex of a submesh is blended with: the vertex it moves towards, counted in the submesh, and how far
struct Partner
{
	int32_t vertex {-1};
	float weight {0.0f};

	[[nodiscard]] bool Blended() const { return vertex >= 0; }
};

/// Every vertex's partner in a submesh of primitives, from the blends each lists in turn: primitiveVertices[i] vertices
/// and primitiveBlends[i] blends for primitive i. The weight is kept as the file has it. Blends naming vertices outside
/// their primitive are left out, and so are those of a vertex towards itself, which would not move it.
[[nodiscard]] std::vector<Partner> Partners(std::span<const uint32_t> primitiveVertices,
                                            std::span<const uint32_t> primitiveBlends, std::span<const Blend> blends);

/// Where a vertex placed at `placed` is drawn when blended `weight` of the way towards its partner placed at `towards`:
/// placed + (towards - placed) x weight, in that order
[[nodiscard]] glm::vec3 Towards(const glm::vec3& placed, const glm::vec3& towards, float weight);

/// The positions of a submesh placed by their bones, with the blends applied
void Apply(std::span<glm::vec3> positions, std::span<const Partner> partners);
} // namespace openblack::vertex_blend
