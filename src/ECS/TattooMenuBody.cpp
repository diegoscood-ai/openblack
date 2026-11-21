/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooMenuBody.h"

#include <cstdint>

#include <exception>
#include <filesystem>
#include <optional>
#include <vector>

#include <PhysiqueFile.h>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/CreatureBody.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureRig.h"
#include "Creature/TattooMenu.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/TattooMenuCreatureArchetype.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/TattooMenuCreature.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/InterfaceCreatures.h"
#include "Graphics/RendererInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The profile's physique file, beside the mind file its creature's saves write
std::filesystem::path PhysiquePath()
{
	return player_creature::SaveTargetsOf(player_creature::ProfileCreatureFile(), player_creature::CurrentMindFolder())
	    .physique;
}

/// The file read whole, or none when it is not there or cannot be read
std::optional<creaturemind::PhysiqueFileData> ReadPhysique()
{
	if (!Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = PhysiquePath();
	if (!fileSystem.Exists(path))
	{
		return std::nullopt;
	}
	try
	{
		return creaturemind::ReadPhysique(fileSystem.ReadAll(path));
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}
} // namespace

bool tattoo_menu_body::ButtonShown(bool restartPending)
{
	const bool kept = Locator::filesystem::has_value() && Locator::filesystem::value().Exists(PhysiquePath());
	const bool shown = Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().IsCreatureShown();
	return tattoo_menu::ButtonShown(kept, shown, restartPending);
}

bool tattoo_menu_body::Open()
{
	if (!Locator::tattooEditorSystem::has_value())
	{
		return false;
	}
	const auto shown = tattoo_menu::FromPhysique(
	    ReadPhysique(), [](CreatureType species) { return archetypes::CreatureArchetype::SpeciesStrength(species); });
	const auto body = archetypes::TattooMenuCreatureArchetype::Create(shown);
	if (!body.has_value())
	{
		return false;
	}
	// The tattoos start as the local player's creature has them, or with none
	std::optional<entt::entity> player;
	if (Locator::playerSystem::has_value())
	{
		player = Locator::playerSystem::value().GetPrimaryCreature(creature::LocalPlayer());
	}
	Locator::tattooEditorSystem::value().OpenFromMenu(player, *body);
	return true;
}

glm::mat4 tattoo_menu_body::View(const creature_tattoo_editor::Orbit& orbit)
{
	return glm::lookAt(orbit.eye, orbit.focus, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 tattoo_menu_body::Projection()
{
	return Locator::camera::has_value()
	           ? graphics::interface_creatures::SpaceProjection(Locator::camera::value().GetProjectionMatrix())
	           : glm::mat4(1.0f);
}

void tattoo_menu_body::Frame(const Viewport& viewport)
{
	if (!Locator::tattooEditorSystem::has_value() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::resources::has_value() || viewport.size.x < 1.0f || viewport.size.y < 1.0f)
	{
		return;
	}
	auto& editor = Locator::tattooEditorSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto shown = editor.GetShown();
	if (!editor.IsOpen() || !shown.has_value() || !registry.Valid(*shown))
	{
		return;
	}
	auto* menu = registry.TryGet<components::TattooMenuCreature>(*shown);
	auto* animation = registry.TryGet<components::CreatureAnimation>(*shown);
	const auto* morph = registry.TryGet<const components::CreatureMorph>(*shown);
	if (menu == nullptr || animation == nullptr || morph == nullptr)
	{
		return;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(menu->shown.species);
	const creature::CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
	const auto& orbit = editor.GetView();
	const auto view = View(orbit);
	const auto projection = Projection();

	// The mouse: where it is on the window, and any of the three buttons held, wherever their events went
	glm::ivec2 mouse {0};
	bool leftHeld = false;
	bool buttonHeld = false;
	if (Locator::gameActionSystem::has_value())
	{
		using input::MouseButton;
		const auto& actions = Locator::gameActionSystem::value();
		mouse = glm::ivec2(actions.GetMousePosition());
		leftHeld = actions.IsMouseButtonHeld(MouseButton::Left);
		buttonHeld =
		    leftHeld || actions.IsMouseButtonHeld(MouseButton::Middle) || actions.IsMouseButtonHeld(MouseButton::Right);
	}

	// The place under the pointer: the body's places as it was posed, seen from the editor's eye over the dialogs'
	// space, in the window's pixels
	std::vector<creature_tattoo_editor::SiteOnScreen> sites;
	if (rig != nullptr && Locator::camera::has_value() && Locator::config::has_value())
	{
		const creature_tattoo_editor::SiteView siteView {
		    .eye = orbit.eye,
		    .worldToClip = projection * view,
		    .nearPlane = Locator::camera::value().GetDrawnNearClip(Locator::config::value().cameraNearClip),
		    .screenSize = glm::ivec2(viewport.size),
		};
		const auto points = creature::PosedTattooPoints(rig->tattooPoints, morph->drawn, animation->boneMatrices, menu->model);
		sites = creature_tattoo_editor::SitesOnScreen(points, siteView);
		for (auto& site : sites)
		{
			site.screen += glm::ivec2(viewport.origin);
		}
	}
	editor.Hover(sites, mouse);

	// Then it moves on by the frame and is posed, its hair on the new pose and its skin as its tattoos now are, and it
	// is drawn over the dialog
	const auto frameMs = game_clock::FrameRealMs();
	const auto body = *shown;
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().PoseMenuCreature(body, frameMs);
	}
	if (Locator::creatureHairSystem::has_value())
	{
		Locator::creatureHairSystem::value().UpdateMenuCreature(body, creature_hair::RealFrameSeconds(frameMs));
	}
	if (Locator::creatureSkinSystem::has_value())
	{
		Locator::creatureSkinSystem::value().Update();
	}
	if (Locator::rendererInterface::has_value())
	{
		Locator::rendererInterface::value().ShowMenuCreature({.body = body,
		                                                      .eye = orbit.eye,
		                                                      .view = view,
		                                                      .projection = projection,
		                                                      .viewOrigin = viewport.origin,
		                                                      .viewSize = viewport.size});
	}
	menu = registry.TryGet<components::TattooMenuCreature>(body);
	animation = registry.TryGet<components::CreatureAnimation>(body);
	if (menu == nullptr || animation == nullptr)
	{
		return;
	}

	// Then its frame: its idle actions, and its look at the point under the pointer ten units into the screen
	const tattoo_menu::Input input {
	    .buttonHeld = buttonHeld,
	    .leftHeld = leftHeld,
	    .pointer = graphics::interface_creatures::PointFromScreen(view, projection, viewport.size,
	                                                              glm::vec2(mouse) - viewport.origin, tattoo_menu::k_LookDepth),
	    .ticks = game_clock::TickCount(),
	    .frameMs = static_cast<int32_t>(frameMs),
	};
	const auto hasAnimation = [rig](size_t index) {
		return rig != nullptr && rig->GetAnimation(creature::CreatureRig::Mesh::Base, index) != nullptr;
	};
	tattoo_menu::Step(menu->life, animation->body, animation->gesture, animation->lookAt, input, orbit, hasAnimation,
	                  [](int32_t n) { return game_random::LocalRand(n); });
	if (auto* eyes = registry.TryGet<components::CreatureEyes>(*shown); eyes != nullptr)
	{
		eyes->lookAt = animation->lookAt;
	}
}
