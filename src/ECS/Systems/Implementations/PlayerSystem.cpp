/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PlayerSystem.h"

#include <utility>
#include <vector>

#include "Creature/PrimaryCreature.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatureSlot.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/Components/VirtualInfluence.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// A name out of range falls to the neutral player
size_t IndexOf(openblack::PlayerNames name)
{
	const auto index = static_cast<size_t>(name);
	return index < PlayerSystem::k_Players ? index : static_cast<size_t>(openblack::PlayerNames::NEUTRAL);
}

/// The component moves off the entity that carries it (only the local player's does, and at most one) into the player
/// system's copy, which takes over until the next local player entity is made
template <typename Component>
void MoveOffEntity(openblack::ecs::Registry& registry, Component& withoutEntity)
{
	entt::entity carrier = entt::null;
	std::as_const(registry).Each<const Component>([&carrier](entt::entity entity, const Component&) {
		if (carrier == entt::null)
		{
			carrier = entity;
		}
	});
	if (carrier == entt::null)
	{
		return;
	}
	withoutEntity = registry.Get<Component>(carrier);
	registry.RemoveState<Component>(carrier);
}
} // namespace

void PlayerSystem::RegisterPlayers()
{
	// The players of a land that has gone are forgotten; those already known on this land stay as they were
	const auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(_players, [&registry](const auto& entry) { return !registry.Valid(entry.second); });
	std::vector<std::pair<PlayerNames, entt::entity>> listed;
	registry.Each<const Player>([this, &listed](const entt::entity entity, const Player& player) {
		if (_players.emplace(player.name, entity).second)
		{
			listed.emplace_back(player.name, entity);
		}
	});
	for (const auto& [name, entity] : listed)
	{
		TakeUpCreature(name, entity);
	}
}

void PlayerSystem::AddPlayer(entt::entity playerEntity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& player = registry.Get<components::Player>(playerEntity);
	// A player made for a new land takes the place of the one of that name on the last land
	_players.insert_or_assign(player.name, playerEntity);
	TakeUpCreature(player.name, playerEntity);
}

entt::entity PlayerSystem::GetPlayer(PlayerNames playerName) const
{
	return _players.at(playerName);
}

void PlayerSystem::AddCreature(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	const auto owner = body->owner;
	// It is its owner's creature, whichever they had: the newest made wins, the neutral player's included. A creature
	// of no player is nobody's
	if (static_cast<size_t>(owner) < k_Players)
	{
		SetPrimaryCreature(owner, creature);
	}
	const auto player = _players.find(owner);
	if (player == _players.end() || !registry.Valid(player->second))
	{
		return;
	}
	// kept for the game only: nothing drawn changes
	auto* list = registry.TryGet<PlayerCreatures>(player->second);
	auto& creatures = list != nullptr ? *list : registry.AssignState<PlayerCreatures>(player->second);
	primary_creature::Acquire(creatures.acquired, creature);
}

std::optional<entt::entity> PlayerSystem::GetPrimaryCreature(PlayerNames name) const
{
	// The creature kept for the player: on their entity, or here for a player without one. Each creature made for them
	// takes it (AddCreature), and the leash writes it as its one creature per player changes (LeashSystem::SetLeashable,
	// SetOwner)
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> kept = _creatureWithoutEntity[IndexOf(name)];
	if (const auto entity = EntityOf(name))
	{
		const auto* slot = registry.TryGet<const PlayerCreatureSlot>(*entity);
		kept = slot != nullptr ? slot->creature : std::nullopt;
	}
	// Only a creature that is still there and still theirs
	const auto* body = kept.has_value() && registry.Valid(*kept) ? registry.TryGet<const Creature>(*kept) : nullptr;
	if (body == nullptr || IndexOf(body->owner) != IndexOf(name))
	{
		return std::nullopt;
	}
	return kept;
}

void PlayerSystem::SetPrimaryCreature(PlayerNames name, std::optional<entt::entity> creature)
{
	if (const auto entity = EntityOf(name))
	{
		Locator::entitiesRegistry::value().AssignOrReplaceState<PlayerCreatureSlot>(*entity, PlayerCreatureSlot {creature});
		return;
	}
	_creatureWithoutEntity[IndexOf(name)] = creature;
}

std::optional<entt::entity> PlayerSystem::EntityOf(PlayerNames name) const
{
	const auto player = _players.find(static_cast<PlayerNames>(IndexOf(name)));
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (player == _players.end() || !registry.Valid(player->second))
	{
		return std::nullopt;
	}
	// still that player's: an entity of a land that has gone may have been made again as something else
	const auto* component = registry.TryGet<const Player>(player->second);
	if (component == nullptr || component->name != player->first)
	{
		return std::nullopt;
	}
	return player->second;
}

void PlayerSystem::TakeUpCreature(PlayerNames name, entt::entity playerEntity)
{
	auto& kept = _creatureWithoutEntity[IndexOf(name)];
	if (!kept.has_value())
	{
		return;
	}
	Locator::entitiesRegistry::value().AssignOrReplaceState<PlayerCreatureSlot>(playerEntity, PlayerCreatureSlot {kept});
	kept.reset();
}

void PlayerSystem::KeepForNextLand()
{
	// The last land's entity goes with its registry; these last the whole game
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	MoveOffEntity(registry, _interfaceAlignmentWithoutEntity);
	MoveOffEntity(registry, _influenceCrossingWithoutEntity);
	// and what the hand keeps of their influence past the border, once it has any: a land where it had none leaves what
	// was kept before as it is
	if (const auto carrier = std::as_const(registry).Front<VirtualInfluence>(); carrier != entt::null)
	{
		_keptVirtualInfluence = registry.Get<VirtualInfluence>(carrier);
		registry.RemoveState<VirtualInfluence>(carrier);
	}
}

void PlayerSystem::TakeUpKept(entt::entity playerEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* player = registry.TryGet<const Player>(playerEntity);
	if (player == nullptr || player->name != GetLocalPlayer())
	{
		return;
	}
	// Only the first entity the local player gets carries them: there is one copy only
	if (registry.Size<InterfaceAlignment>() == 0)
	{
		registry.AssignState<InterfaceAlignment>(playerEntity, _interfaceAlignmentWithoutEntity);
	}
	if (registry.Size<InfluenceCrossing>() == 0)
	{
		registry.AssignState<InfluenceCrossing>(playerEntity, _influenceCrossingWithoutEntity);
	}
	// The hand goes on with its virtual influence as the last land left it; on a first land none is kept, and the hand
	// makes its own with the game's start values when it is first asked
	if (_keptVirtualInfluence.has_value() && registry.Size<VirtualInfluence>() == 0)
	{
		registry.AssignState<VirtualInfluence>(playerEntity, *_keptVirtualInfluence);
		_keptVirtualInfluence.reset();
	}
}

void PlayerSystem::ClearPlayers()
{
	_players.clear();
	// the creatures went with the land's registry
	_creatureWithoutEntity.fill(std::nullopt);
}

Alignment& PlayerSystem::Alignment(PlayerNames name)
{
	return _alignment[IndexOf(name)];
}

PlayerMagic& PlayerSystem::MagicWithoutEntity(PlayerNames name)
{
	return _magicWithoutEntity[IndexOf(name)];
}

void PlayerSystem::ClearMagicWithoutEntity()
{
	_magicWithoutEntity.fill(PlayerMagic {});
}
