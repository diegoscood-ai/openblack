/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VortexSystem.h"

#include "ECS/Vortex.h"
#include "GameClock.h"

using namespace openblack;
using namespace openblack::ecs::systems;

entt::entity VortexSystem::Create(glm::vec3 position, VortexType type)
{
	return ecs::vortex::Create(position, type);
}

bool VortexSystem::StartFadeOut(entt::entity vortex)
{
	return ecs::vortex::StartFadeOut(vortex);
}

void VortexSystem::SetParameters(entt::entity vortex, entt::entity town, glm::vec3 position, float a, float b,
                                 entt::entity flock)
{
	ecs::vortex::SetParameters(vortex, town, position, a, b, flock);
}

void VortexSystem::ProcessTurn()
{
	ecs::vortex::ProcessAll(game_clock::Turn());
}

void VortexSystem::UpdateGroundEffects(float seconds)
{
	ecs::vortex::UpdateGroundEffects(seconds);
}

void VortexSystem::UpdateOverLandEffects(float seconds)
{
	ecs::vortex::UpdateOverLandEffects(seconds);
}
