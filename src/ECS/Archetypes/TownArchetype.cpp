/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownArchetype.h"

#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownBelief.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity TownArchetype::Create(int id, const glm::vec3& position, PlayerNames playerOwner, Tribe tribe)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	// const auto& info = Game::Instance()->GetInfoConstants().town;

	auto& town = registry.Assign<Town>(entity, static_cast<uint32_t>(id));
	town.owner = playerOwner;
	// the ctor's place in g_game +0x205C84 (head insertion 0x73964D..0x739656): a counter that only goes up
	static uint32_t s_creationStamp = 0;
	town.creationStamp = ++s_creationStamp;
	// the Town ctor: GBelief::Init 0x437DD0 at 0x739523 while +0x5D8 is still 0 (so the neutral belief is 0 until the
	// first fold), then +0x5D8 = GTownInfo +0xB8 beliefInNeutralPlayer (0x73966C); +0x5DC = 1.0 (0x739672) is
	// TownBelief's default
	ecs::town_belief::Init(town);
	if (Locator::infoConstants::has_value())
	{
		town.belief.beliefInNeutralPlayer = Locator::infoConstants::value().town.beliefInNeutralPlayer;
	}
	registry.Assign<Tribe>(entity, tribe);
	registry.Assign<TownInfluence>(entity); // its influence (ECS/Influence); the owner is Town +0x2C above
	// the magic types the town holds, its spell icons and its worship site (src/Worship, Town.cpp 0x73D1C0..)
	registry.Assign<TownMagic>(entity);
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& registryContext = registry.Context();
	registryContext.towns.insert({id, entity});

	return entity;
}
