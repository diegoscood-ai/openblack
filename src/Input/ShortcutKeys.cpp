/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShortcutKeys.h"

#include <bit>
#include <optional>
#include <stdexcept>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/FightWatch.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "EngineConfig.h"
#include "GameActionMapInterface.h"
#include "GameClock.h"
#include "Help/HelpKey.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Locator.h"

namespace openblack::input
{
namespace
{
/// The ground under a point
[[nodiscard]] glm::vec3 GroundUnder(const LandIslandInterface& land, glm::vec3 point)
{
	return {point.x, land.GetHeightAt({point.x, point.z}), point.z};
}

/// Where the player's temple stands, if they have one
[[nodiscard]] std::optional<glm::vec3> PlayerTemplePosition()
{
	std::optional<glm::vec3> position;
	Locator::entitiesRegistry::value().Each<const ecs::components::Temple, const ecs::components::Transform>(
	    [&position](const ecs::components::Temple& temple, const ecs::components::Transform& transform) {
		    if (temple.owner == PlayerNames::PLAYER_ONE)
		    {
			    position = transform.position;
		    }
	    });
	return position;
}

/// Whether an action's press came from the mouse button bound to it rather than from its key: it has a mouse button,
/// and no key of its own held down
[[nodiscard]] bool PressedByMouse(const GameActionInterface& actions, BindableActionMap action)
{
	const auto bindings = actions.GetKeyBindings();
	const auto index = IndexOf(bindings, action);
	if (!index.has_value() || bindings[*index].mouse == MouseInput::None)
	{
		return false;
	}
	const auto& key = bindings[*index].key;
	return !key.has_value() || !actions.IsKeyHeld(key->key);
}

/// The script's interface interaction asked about a bindable action, by its place in the options screen's list
[[nodiscard]] bool IsBlocked(BindableActionMap action)
{
	return help::interface_interaction::IsActionBlocked(std::countr_zero(static_cast<uint64_t>(action)));
}
} // namespace

void ShortcutKeys::Update()
{
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto& actions = Locator::gameActionSystem::value();
	const auto pressed = [&actions](BindableActionMap action) { return actions.GetChanged(action) && actions.Get(action); };

	// TODO(raffclar): Zoom To Creature and the creature's room key belong to the creature mode work, and the leash keys
	// to the leash work, which read them themselves.

	const ShortcutPresses presses {
	    .temple = pressed(BindableActionMap::ZOOM_TO_TEMPLE),
	    .realm = pressed(BindableActionMap::ZOOM_TO_REALM),
	    .villagerNames = pressed(BindableActionMap::SHOW_VILLAGER_NAMES),
	    .help = pressed(BindableActionMap::HELP),
	};
	if (!presses.temple && !presses.realm && !presses.villagerNames && !presses.help)
	{
		return;
	}
	auto* helpSystem = help::Get();
	const ShortcutGates gates {
	    .scriptWideScreen = helpSystem != nullptr && helpSystem->IsScriptWideScreen(),
	    .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
	    .templeKeyBlocked = IsBlocked(BindableActionMap::ZOOM_TO_TEMPLE),
	    .realmKeyBlocked = IsBlocked(BindableActionMap::ZOOM_TO_REALM),
	    .helpKeyBlocked = IsBlocked(BindableActionMap::HELP),
	};
	const ShortcutPresses byMouse {
	    .temple = presses.temple && PressedByMouse(actions, BindableActionMap::ZOOM_TO_TEMPLE),
	    .realm = presses.realm && PressedByMouse(actions, BindableActionMap::ZOOM_TO_REALM),
	    .villagerNames = presses.villagerNames && PressedByMouse(actions, BindableActionMap::SHOW_VILLAGER_NAMES),
	    .help = presses.help && PressedByMouse(actions, BindableActionMap::HELP),
	};
	const auto todo = ShortcutActionsFor(presses, gates, byMouse);

	// The advisors answer about what is in the hand or under it, or the ground
	if (todo.askHelp && helpSystem != nullptr && Locator::dialogueControlSystem::has_value())
	{
		help::help_key::Ask(*helpSystem, Locator::dialogueControlSystem::value(), chlapi::ScriptVm(), game_clock::Turn());
	}

	// The villagers' names are toggled, as the game does
	if (todo.toggleVillagerNames)
	{
		auto& config = Locator::config::value();
		config.showVillagerNames = !config.showVillagerNames;
	}

	if (!todo.zoom.has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	const auto& land = Locator::terrainSystem::value();
	glm::vec3 realmGround;
	try
	{
		const auto extent = land.GetExtent();
		const auto middle =
		    RealmMiddle(extent.minimum, extent.maximum, LandIslandInterface::k_CellSize * LandIslandInterface::k_CellCount);
		realmGround = GroundUnder(land, glm::vec3(middle.x, 0.0f, middle.y));
	}
	catch (const std::runtime_error&)
	{
		// No land is loaded
		return;
	}
	const zoom_to::CameraView current {.origin = camera.GetOrigin(), .focus = camera.GetFocus()};
	std::optional<zoom_to::CameraView> flight;
	if (*todo.zoom == ZoomKey::Temple)
	{
		const auto now = zoom_to::TapTime();
		const bool doubleTap = _zoomTo.IsDoubleTap(now);
		const auto templePosition = PlayerTemplePosition();
		flight = _zoomTo.PressTemple(
		    now, current, GroundUnder(land, current.focus),
		    templePosition.has_value() ? std::optional(GroundUnder(land, *templePosition)) : std::nullopt, realmGround);
		const auto* fightWatch = camera.GetModel().GetFightWatch();
		if (!TempleKeyMovesCamera(doubleTap, fightWatch != nullptr && fightWatch->IsWatchingFight()))
		{
			flight.reset();
		}
	}
	else
	{
		flight = _zoomTo.PressRealm(current, realmGround);
	}
	if (flight.has_value())
	{
		camera.GetModel().SetFlight(flight->origin, flight->focus);
	}
}

} // namespace openblack::input
