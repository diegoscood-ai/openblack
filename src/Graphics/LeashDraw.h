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

#include <array>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>

#include "Creature/LeashRope.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct CreatureLeash;
}

/// What the renderer draws of a creature's leash: the rope's ribbon facing the camera and its shadow flat on the land,
/// as world triangles already lit, in the leash texture. Only the vertices are made here; the renderer submits them.
namespace openblack::graphics::leash_draw
{

/// The leash's one material, for the rope and its shadow alike: blended and alpha tested on the texture's alpha times
/// the vertex alpha, writing depth, two-sided and tiling, ALPHAREF 0x38
inline constexpr render_modes::Material k_Material {render_modes::Mode::TexturedChromaAlpha,
                                                    render_modes::k_TwoSided | render_modes::k_Tiling, 0x38};
/// The mode table the leash is drawn through: the global alpha one, which scales ALPHAREF by the object alpha
inline constexpr render_modes::Table k_Table = render_modes::Table::GlobalAlpha;

/// The light at a point of the rope, 0xAARRGGBB, its alpha included
using LightAt = std::function<uint32_t(const glm::vec3&)>;

/// The corners of a ribbon, two at each point, in leash_rope::BuildRibbon's order
using Corners = std::array<world_triangles::Vertex, leash_rope::k_RibbonVertexCount>;

/// One rope as drawn this frame. It is drawn at once, right after the models: nothing sorts it among the blended things.
struct Draw
{
	/// The rope, both corners of each point in the light at the point, transparent at the two end points
	Corners rope;
	/// The shadow, black and faint, fading out at both ends
	Corners shadow;
};

/// The rope's ribbon facing an eye and its shadow on the land, lit
[[nodiscard]] Draw Build(const leash_rope::Rope& rope, const glm::vec3& eye, const leash_rope::GroundHeight& ground,
                         const LightAt& light);

/// The rope and its shadow are one strip, drawn in one call: at each point the rope's two corners, then the shadow's two
inline constexpr size_t k_StripVertexCount = leash_rope::k_PointCount * 4;
/// Between each pair of points the rope's two triangles, then the shadow's two
inline constexpr size_t k_StripIndexCount = (leash_rope::k_PointCount - 1) * 12;
using Strip = std::array<world_triangles::Vertex, k_StripVertexCount>;

/// The rope's and the shadow's corners as one strip
[[nodiscard]] Strip StripOf(const Draw& draw);
/// The strip's triangles, segment by segment along the rope: in each, the rope's quad and then the shadow's, so that
/// they blend in the order the game draws them
[[nodiscard]] std::array<uint16_t, k_StripIndexCount> StripIndices();

/// Whether a creature's leash is drawn: it wears one, and its rope has been placed from its ends
[[nodiscard]] bool IsDrawn(const ecs::components::CreatureLeash& leash);

/// Every rope to draw this frame, none while the scripts hide the leashes. Looked up through the const registry, so a
/// land without a creature gains no storage.
[[nodiscard]] std::vector<const leash_rope::Rope*> Ropes(const ecs::Registry& registry, bool shown);

} // namespace openblack::graphics::leash_draw
