/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The creatures' leashes. Each worn rope (Creature/LeashRope.h) is a ribbon facing the camera with a faint strip on the
// land beneath it, built by Graphics/LeashDraw.h and drawn as world triangles at once in leash.raw / leasha.raw: rope
// and shadow in one alpha tested material that writes depth, in one call. While the hand pulls a tied leash's rope,
// that leash is drawn as the hand's two ropes alone. On a land without a worn leash there is nothing to draw. The
// marker over where the leash last sent the local player's creature is handed to the frame's sprites
// (LeashMarkerSprites).

#include <cstdint>

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "Creature/LeashPull.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Graphics/LeashDraw.h"
#include "Graphics/WorldTriangles.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawLeashes(RenderPass viewId, const Camera& camera) const
{
	// the scripts hide the leashes (SET_DRAW_LEASH); none in the temple
	const bool shown = help::script_control::GetCameraControl().drawLeash != 0;
	if (!shown || InTemple() || !Locator::entitiesRegistry::has_value() || !Locator::terrainSystem::has_value() ||
	    !Locator::resources::has_value())
	{
		return;
	}
	// while the hand pulls a tied rope, this player's leash is drawn as the hand's two ropes instead of its own
	const bool handPulls = Locator::handSystem::has_value() &&
	                       Locator::handSystem::value().GetActionStateNumber() == creature_leash::k_PullActionState;
	// through the const registry: a land without a creature gains no storage
	const auto ropes =
	    leash_draw::Ropes(std::as_const(Locator::entitiesRegistry::value()), shown, creature::LocalPlayer(), handPulls);
	if (ropes.empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	using ecs::components::CreatureLeash;
	if (!textures.Contains(CreatureLeash::k_TextureId) || !textures.Contains(CreatureLeash::k_AlphaTextureId))
	{
		return;
	}
	const auto& diffuse = *textures.Handle(CreatureLeash::k_TextureId);
	const auto& alpha = *textures.Handle(CreatureLeash::k_AlphaTextureId);
	const auto& island = Locator::terrainSystem::value();
	const leash_rope::GroundHeight ground = [&island](glm::vec2 point) { return island.GetHeightAt(point); };
	// each point of the rope in the land's light under it, as the models' base colour, with no haze and no land colour
	const bool lit = IsLandLit();
	const leash_draw::LightAt light = [lit](const glm::vec3& point) {
		if (!lit)
		{
			return 0xFFFFFFFFu;
		}
		return land_light::At(land_light::CurrentTable(), glm::vec2(point.x, point.z)).diffuse;
	};
	const auto indices = leash_draw::StripIndices();
	for (const auto* rope : ropes)
	{
		const auto strip = leash_draw::StripOf(leash_draw::Build(*rope, camera.GetOrigin(), ground, light));
		world_triangles::SubmitRaw(viewId, strip, indices, diffuse, alpha, leash_draw::k_Material, *_shaderManager, {},
		                           leash_draw::k_Table);
	}
}

std::optional<std::array<ecs::archetypes::LeashMarkerArchetype::Placed, 2>> Renderer::LeashMarkerSprites() const
{
	// only the land's view draws it, at the pulse the bookmarks' markers took this frame
	if (InTemple() || !Locator::entitiesRegistry::has_value() || !Locator::leashSystem::has_value() ||
	    !Locator::cameraBookmarkSystem::has_value())
	{
		return std::nullopt;
	}
	// only the local player sees their own creature's marker
	const auto local = Locator::leashSystem::value().PlayersCreature(creature::LocalPlayer());
	if (!local.has_value())
	{
		return std::nullopt;
	}
	return ecs::archetypes::LeashMarkerArchetype::Drawn(std::as_const(Locator::entitiesRegistry::value()), *local,
	                                                    Locator::cameraBookmarkSystem::value().GetPulse());
}
