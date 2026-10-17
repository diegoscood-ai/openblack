/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PlayerProfileSystem.h"

#include "ECS/PlayerCreature.h"

using namespace openblack::ecs::systems;

bool PlayerProfileSystem::CurrentProfileHasCreature() const
{
	return ecs::player_creature::ProfileHasCreature();
}
