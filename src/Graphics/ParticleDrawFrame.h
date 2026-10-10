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

#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

#include "Graphics/ParticleDrawPath.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"
#include "Particles/LightSheet.h"
#include "Particles/PSysManager.h"
#include "Particles/Rules/SurfRevol.h"

/// What the particle effects draw in a frame, collected once before the main view is drawn, by the three ways of
/// DrawPath. The renderer puts the sorted things and the queued effects into its Z queue and drains it far to near
namespace openblack::particles::draw
{

/// The sheets a sprite is drawn from and how it blends: sprites next to each other in the draw order that share one
/// are drawn in one call
struct Material
{
	entt::id_type texture;
	entt::id_type alphaTexture;
	graphics::render_modes::Mode mode;

	[[nodiscard]] bool operator==(const Material& other) const = default;
};

/// A particle sprite's material from its sheets and its creator's blend: added to what is behind or not, writing depth
/// or not, always with its alpha
[[nodiscard]] Material SpriteMaterial(std::pair<entt::id_type, entt::id_type> textures, bool additive, bool writeDepth);

enum class ItemKind : uint8_t
{
	Sprite,
	Chain,
	Mesh,
	Mist,
	Surface,
	Fragment,
};

/// One thing of a sorted effect that takes its own place in the Z queue, by its index in the list of its kind: the
/// frame's sorted sprites or chains, or the render context's particle meshes
struct Item
{
	ItemKind kind;
	uint32_t index;
	/// Where it takes its place among what blends
	glm::vec3 sortPoint;
};

/// A queued effect, or the miracle in the hand: all of it drawn at once in its own order, its items in the effect
using Group = psys::manager::OrderedEffect;

struct Frame
{
	/// The sorted effects' things, each to take its own place
	psys::manager::SortedFrame sorted;
	/// The queued effects, each to take one place at its origin
	std::vector<Group> queued;
	/// The effects drawn just after the hand
	std::vector<Group> hand;
	/// The surfaces of revolution of every path
	std::vector<psys::surf_revol::Surface> surfaces;
	/// The landscape vortices' ground effects' surfaces, drawn before the land (Renderer::SplitVortexParticles)
	std::vector<psys::surf_revol::Surface> groundSurfaces;
	/// The exploded pieces of the sorted effects, and those of the others tagged with their atom. Refilled every frame,
	/// kept for their capacity
	graphics::world_triangles::Frame sortedPieces;
	graphics::world_triangles::Frame orderedPieces;
	/// The sheets of light standing this frame (the recognised gestures'), the newest first, each to take its own place
	/// at its origin
	std::vector<psys::surf_revol::Surface> lightSheets;

	/// Empties it, so that no list of an earlier frame survives a frame that collects none
	void Clear();
};

/// The sheets of light are drawn with the stars texture and its alpha file
inline constexpr std::string_view k_LightSheetTexture = "S_LightSheetStars";

/// A built sheet of light as one more surface of the frame's lightSheets, taking its place at sortPoint: the stars
/// texture with its alpha, added to what is behind, no depth write, seen from both sides, its specular added in the
/// same pass. Nothing for a sheet with no triangle, or with more corners than 16-bit indices reach
void AddLightSheet(Frame& frame, std::span<const LightSheet::Vertex> vertices, std::span<const uint32_t> triangles,
                   const glm::vec3& sortPoint);

/// Every sheet of light, given oldest first, built and added the newest first, as the game walks its list. Each takes
/// its place at its middle as it was before this build (so a frame late, and far behind everything on its first
/// frame)
void AddLightSheets(Frame& frame, std::span<const std::shared_ptr<LightSheet>> oldestFirst);

} // namespace openblack::particles::draw
