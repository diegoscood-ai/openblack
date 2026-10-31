/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScaffoldGhosts.h"

#include <utility>

#include <glm/vec3.hpp>

#include "3D/LandLight.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "GameClock.h"
#include "Graphics/ArgbColour.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::graphics::scaffold_ghosts
{
namespace
{
Debug* DebugState()
{
	return Locator::debugHooks::has_value() ? &Locator::debugHooks::value().Get<Debug>() : nullptr;
}

/// The land's light and colour at a map position, as the scaffold's draws take them: the cell light of the cell the
/// position is in (its cell numbers unsigned, so a position west or north of the map is off it), almost unmixed, with
/// no haze
land_light::Sample CellLightAt(const map_coords::MapCoords& coords)
{
	return land_light::AtCellShift(land_light::GetCells(), land_light::CurrentTable(), map_coords::Cell(coords));
}

/// A see-through model drawn as one of the original's 3D objects given its colour and specular and drawn with the
/// global alpha: blended by the colour's alpha, sorted on its own
particles::mesh_atoms::Instance SeeThrough(entt::id_type mesh, const glm::mat4& model, const land_light::Sample& light,
                                           uint8_t alpha)
{
	return {
	    .meshId = mesh,
	    .model = model,
	    .alpha = static_cast<float>(alpha) / 255.0f,
	    .uv = {0.0f, 0.0f},
	    .translucent = true,
	    .additive = false,
	    .colour = {static_cast<uint8_t>(argb_colour::Red(light.diffuse)),
	               static_cast<uint8_t>(argb_colour::Green(light.diffuse)),
	               static_cast<uint8_t>(argb_colour::Blue(light.diffuse))},
	    .landscapeColour = false,
	    .specular = light.specular,
	    .globalAlpha = true,
	};
}

/// The ghost of the scaffold the player's hand holds, if it shows this frame
std::optional<particles::mesh_atoms::Instance> HeldGhost(Debug* debug)
{
	if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto held = Locator::handSystem::value().GetHeldObject();
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!held.has_value() || !registry.Valid(*held))
	{
		return std::nullopt;
	}
	// only while it offers a building, and with that building's model
	const auto* scaffold = registry.TryGet<const ecs::components::Scaffold>(*held);
	if (scaffold == nullptr || !ecs::scaffolds::HasPlan(*held, false) || scaffold->phantomMesh == 0)
	{
		return std::nullopt;
	}
	const auto pose = ecs::scaffold_ghost::HeldPoseOf(
	    {
	        .fade = scaffold->fade,
	        .appearing = (scaffold->flags & ecs::components::Scaffold::k_FadePhase) != 0,
	        .scale = scaffold->phantomScale,
	        .yAngle = scaffold->phantomAngle,
	    },
	    game_clock::VisualMs(), Locator::infoConstants::value().scaffold.phantomBuildingRotationPerGameTurn);
	if (!pose.has_value())
	{
		return std::nullopt;
	}
	// at the hand's point through a map position and back: on the map's grid, as high above the land as the hand
	const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
	const auto* handTransform = registry.Valid(hand) ? registry.TryGet<const ecs::components::Transform>(hand) : nullptr;
	if (handTransform == nullptr)
	{
		return std::nullopt;
	}
	const auto coords = map_coords::FromWorld(handTransform->position);
	const auto light = CellLightAt(coords);
	const auto alpha = ecs::scaffold_ghost::HeldAlpha(light.diffuse);
	if (debug != nullptr)
	{
		debug->held = pose;
		debug->heldAlpha = alpha;
	}
	return SeeThrough(scaffold->phantomMesh, affine::PlacementMatrix(map_coords::ToWorld(coords), pose->yAngle, pose->scale),
	                  light, alpha);
}
} // namespace

std::vector<particles::mesh_atoms::Instance> Instances()
{
	std::vector<particles::mesh_atoms::Instance> result;
	auto* debug = DebugState();
	if (debug != nullptr)
	{
		debug->held.reset();
	}
	if (debug != nullptr && debug->hidden)
	{
		return result;
	}
	if (auto held = HeldGhost(debug); held.has_value())
	{
		result.push_back(std::move(*held));
	}
	return result;
}
} // namespace openblack::graphics::scaffold_ghosts
