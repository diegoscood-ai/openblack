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

#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightWatch.h"
#include "Camera/StartCamera.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "EngineConfig.h"
#include "GameActionMapInterface.h"
#include "GameClock.h"
#include "Help/HelpKey.h"
#include "Help/HelpProfile.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Locator.h"

namespace openblack::input
{
namespace
{
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

/// The Show Villager Details key: the living thing under the hand (a villager, an animal or a creature) has its own
/// details turned on or off, and with none every villager's are
void ToggleDetails()
{
	std::optional<entt::entity> thing;
	if (Locator::handSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		thing = Locator::handSystem::value().GetCursorObject();
	}
	auto& registry = Locator::entitiesRegistry::value();
	const bool valid = thing.has_value() && registry.Valid(*thing);
	const bool living =
	    valid && registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(*thing);
	if (DetailsKeyTakesTheThing(valid, living))
	{
		const bool shown = !registry.AllOf<ecs::components::ShowsDetails>(*thing);
		if (shown)
		{
			registry.AssignState<ecs::components::ShowsDetails>(*thing);
		}
		else
		{
			registry.RemoveState<ecs::components::ShowsDetails>(*thing);
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Villager details: thing {} {}", entt::to_integral(*thing),
		                    shown ? "on" : "off");
		return;
	}
	auto& config = Locator::config::value();
	config.showVillagerDetails = !config.showVillagerDetails;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Villager details: every villager {}", config.showVillagerDetails ? "on" : "off");
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
	    .villagerDetails = pressed(BindableActionMap::SHOW_VILLAGER_DETAILS),
	};
	if (!presses.temple && !presses.realm && !presses.villagerNames && !presses.help && !presses.villagerDetails)
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
	    .villagerDetails = presses.villagerDetails && PressedByMouse(actions, BindableActionMap::SHOW_VILLAGER_DETAILS),
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

	// The details: of the living thing under the hand, else of every villager
	if (todo.toggleVillagerDetails)
	{
		ToggleDetails();
	}

	if (!todo.zoom.has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	// The keys fly the world camera: while Creature Mode has the camera, it gives it back first, and the keys start
	// afresh with a new world camera from the view drawn
	if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		const auto drawnOrigin = camera.GetOrigin();
		const auto drawnFocus = camera.GetFocus();
		Locator::creatureModeSystem::value().Leave();
		if (auto* world = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()); world != nullptr)
		{
			world->PlaceAt(drawnOrigin, drawnFocus);
		}
		_zoomTo.Restart();
	}
	const auto& land = Locator::terrainSystem::value();
	glm::vec2 realm;
	try
	{
		const auto extent = land.GetExtent();
		realm = RealmMiddle(extent.minimum, extent.maximum, LandIslandInterface::k_CellSize * LandIslandInterface::k_CellCount);
	}
	catch (const std::runtime_error&)
	{
		// No land is loaded
		return;
	}
	const auto altitude = [&land](glm::vec2 place) { return land.GetHeightAt(place); };
	const zoom_to::CameraView current {.origin = camera.GetOrigin(), .focus = camera.GetFocus()};
	auto* world = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel());
	std::optional<zoom_to::Flight> flight;
	if (*todo.zoom == ZoomKey::Temple)
	{
		const auto now = zoom_to::TapTime();
		// A single tap looks as far ahead as the world camera finds the land, found again from the view drawn; watching a
		// fight it starts no flight
		std::optional<float> clickDistance;
		const auto* fightWatch = camera.GetModel().GetFightWatch();
		if (!_zoomTo.IsDoubleTap(now) && world != nullptr &&
		    TempleKeyMovesCamera(false, fightWatch != nullptr && fightWatch->IsWatchingFight()))
		{
			clickDistance = world->RefreshClickDistance(camera);
		}
		const auto templePosition = PlayerTemplePosition();
		flight = _zoomTo.PressTemple(now, current, clickDistance,
		                             templePosition.has_value() ? std::optional(glm::vec2(templePosition->x, templePosition->z))
		                                                        : std::nullopt,
		                             realm, altitude);
	}
	else
	{
		flight = _zoomTo.PressRealm(current, realm, altitude);
	}
	if (!flight.has_value())
	{
		return;
	}
	Fly(*flight);
}

void ShortcutKeys::Fly(const zoom_to::Flight& flight)
{
	auto& camera = Locator::camera::value();
	if (auto* world = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()); world != nullptr)
	{
		world->FlyToPlace(flight.view.origin, flight.view.focus, flight.rise);
	}
	else
	{
		camera.GetModel().SetFlight(flight.view.origin, flight.view.focus);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Place keys: {} flight rising {} a metre", flight.toPlace ? "place" : "single tap",
	                    flight.rise);
	// The camera help hears of every flight to the temple or over the realm, and back, but not of a single tap's
	if (flight.toPlace)
	{
		help_profile::CameraHelpCallback(help_profile::CameraReason::ZoomToCitadel, 0);
	}
}

void ShortcutKeys::StartAtTemple()
{
	const auto temple = PlayerTemplePosition();
	if (!temple.has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	const auto& land = Locator::terrainSystem::value();
	camera.SetFocus(*temple).SetOrigin(start_camera::OriginOver(*temple));
	const zoom_to::CameraView current {.origin = camera.GetOrigin(), .focus = camera.GetFocus()};
	Fly(_zoomTo.StartAtTemple(current, glm::vec2(temple->x, temple->z),
	                          [&land](glm::vec2 place) { return land.GetHeightAt(place); }));
}

} // namespace openblack::input
