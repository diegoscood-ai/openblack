/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerArchetype.h"

#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerAlignment.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity PlayerArchetype::Create(PlayerNames name)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Player>(entity, name);
	// the GPlayer magic fields; PLAYER_ONE is the human at this interface (+0x8E0 = 1, inf). Its GAlignment (+0x60)
	// is not on the entity: it lives with the player across lands (Magic/Core/Players, AlignmentOf)
	auto& magic = registry.Assign<PlayerMagic>(entity);
	magic.playerType = name == PlayerNames::PLAYER_ONE ? 1 : 0;
	return entity;
}
