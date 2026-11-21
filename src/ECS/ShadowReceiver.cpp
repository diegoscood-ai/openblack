/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShadowReceiver.h"

#include <entt/core/hashed_string.hpp>

#include "ECS/Components/Creature.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandFxPart.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Tree.h"

using namespace openblack::ecs;

shadow_receiver::Object shadow_receiver::ObjectOf(const Registry& registry, entt::entity entity)
{
	static constexpr auto k_PowerUpBand = entt::hashed_string("Power_Up_Band");
	const auto* mesh = registry.TryGet<const components::Mesh>(entity);
	const auto* pot = registry.TryGet<const components::Pot>(entity);
	return {
	    .powerUpBand = mesh != nullptr && mesh->id == k_PowerUpBand.value(),
	    .tree = registry.AnyOf<components::Tree, components::DeadTree, components::BigForest>(entity),
	    .forest = registry.AllOf<components::Forest>(entity),
	    .hand = registry.AllOf<components::Hand>(entity),
	    .creature = registry.AllOf<components::Creature>(entity),
	    .orb = registry.AllOf<components::OneOffSpellSeed>(entity),
	    .shield = registry.AllOf<components::MagicShield>(entity),
	    .flowers = registry.AllOf<components::Flowers>(entity),
	    .templePart = registry.AllOf<components::TempleInteriorPart>(entity),
	    .handFxPart = registry.AllOf<components::HandFxPart>(entity),
	    .desireFlag = registry.AllOf<components::TownDesireFlag>(entity),
	    .pot = pot != nullptr ? std::optional(pot->type) : std::nullopt,
	};
}

bool shadow_receiver::ReceivesOf(const Registry& registry, entt::entity entity)
{
	return Receives(ObjectOf(registry, entity));
}
