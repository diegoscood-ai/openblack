/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AbodeSoundSystem.h"

#include "3D/DayNightClock.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeSounds.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void AbodeSoundSystem::Draw(entt::entity object, uint32_t frameMs)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::camera::has_value() || !Locator::skySystem::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<ecs::components::Abode>(object))
	{
		return;
	}
	const auto type = ecs::abodes::TypeOf(object);
	if (!type.has_value() || !AbodeSounds::HasAmbience(*type))
	{
		return;
	}
	// Every building drawn rolls the local stream first, then the camera must be near
	const auto chance = AbodeSounds::AmbienceChance(std::chrono::duration<float, std::milli>(static_cast<float>(frameMs)));
	const bool rolledOne = game_random::LocalRand(chance) == 1;
	if (!rolledOne)
	{
		return;
	}
	const auto position = map_coords::ToWorld(ecs::object::MapCoordsOf(object));
	const float distance = AbodeSounds::AmbienceDistance(position, Locator::camera::value().GetOrigin());
	const auto& clock = Locator::skySystem::value().GetClock();
	const auto keys = AbodeSounds::Ambience(rolledOne, distance, clock.SkyType(), clock.GetVisualTime());
	if (!keys.has_value())
	{
		return;
	}
	// Owned by the building and following it, at the distance measured here, as the game passes its own
	static_cast<void>(audio::PlayAnimationEffect(audio::Owner::Thing(object), distance, keys->ToArray(),
	                                             audio::AnimAction::Play, audio::Bank(AbodeSounds::k_AmbienceBank),
	                                             AbodeSounds::k_AmbienceTrack, 0.0f, 0.0f));
	++_sounds;
}
