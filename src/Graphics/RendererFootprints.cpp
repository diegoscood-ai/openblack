/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The creatures' walking prints, drawn every frame just after the land and its shadows and before the citadel hearts,
// as one list of world triangles in the misc0 material (docs/wiki/rendering/frame.md#draw-stages-in-order, row 4j).
// They are not baked into the land's textures.

#include <cstdint>

#include <vector>

#include <entt/core/hashed_string.hpp>

#include "Creature/CreatureFootprints.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/RenderModes.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawCreatureFootprints(RenderPass viewId) const
{
	if (!Locator::footprintSystem::has_value())
	{
		return;
	}
	const auto& footprints = Locator::footprintSystem::value();
	if (!footprints.IsShown() || footprints.GetDrawnPrints().empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/misc0").value();
	constexpr entt::id_type k_Alpha = entt::hashed_string("raw/misc0a").value();
	if (!textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return;
	}
	const auto list = creature_footprints::DrawListOf(footprints.GetDrawnPrints());
	std::vector<world_triangles::Vertex> vertices;
	vertices.reserve(list.corners.size());
	for (const auto& corner : list.corners)
	{
		vertices.push_back({.position = corner.position, .uv = corner.uv, .abgr = world_triangles::ToAbgr(corner.argb)});
	}
	// the misc0 material: blended by the texture's alpha times the print's, no depth written, both sides drawn
	world_triangles::SubmitRaw(viewId, vertices, list.indices, *textures.Handle(k_Texture), *textures.Handle(k_Alpha),
	                           render_modes::materials::k_Misc0, *_shaderManager);
}
