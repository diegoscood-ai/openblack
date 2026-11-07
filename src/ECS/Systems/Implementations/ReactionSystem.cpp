/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ReactionSystem.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>
#include <vector>

#include "ECS/Dances.h"
#include "ECS/Effects/ReactionTurnRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::systems
{
// Inside the namespace, so these win over openblack's own Reaction enum
using effects::reactions::LivingClass;
using effects::reactions::LivingReactionHandler;
using effects::reactions::LivingShutDownHandler;
using effects::reactions::Reaction;

uint32_t ReactionSystem::Add(Reaction reaction)
{
	reaction.id = _nextId++;
	_reactions.push_back(reaction);
	return reaction.id;
}

Reaction* ReactionSystem::Find(uint32_t id)
{
	if (id == 0)
	{
		return nullptr;
	}
	const auto it = std::ranges::find(_reactions, id, &Reaction::id);
	return it != _reactions.end() ? &*it : nullptr;
}

std::vector<Reaction>& ReactionSystem::List()
{
	return _reactions;
}

std::span<const Reaction> ReactionSystem::GetReactions() const
{
	return _reactions;
}

void ReactionSystem::RemoveFrom(entt::entity initiator)
{
	// newest first, as the original walks its list from the head
	std::vector<uint32_t> ids;
	for (const auto& reaction : std::views::reverse(_reactions))
	{
		if (reaction.initiator == initiator)
		{
			ids.push_back(reaction.id);
		}
	}
	Remove(ids);
}

void ReactionSystem::RemoveFrom(entt::entity initiator, openblack::Reaction type)
{
	// newest first, as the original walks its list from the head
	std::vector<uint32_t> ids;
	for (const auto& reaction : std::views::reverse(_reactions))
	{
		if (reaction.initiator == initiator && reaction.type == type)
		{
			ids.push_back(reaction.id);
		}
	}
	Remove(ids);
}

void ReactionSystem::Remove(uint32_t id)
{
	Remove(std::span(&id, 1));
}

void ReactionSystem::Remove(std::span<const uint32_t> ids)
{
	// The handlers may remove or add reactions meanwhile: each one is looked up again by its id
	for (const auto id : ids)
	{
		if (auto* entry = Find(id); entry != nullptr)
		{
			entry->available = false;
		}
		for (const auto handler : _shutDownHandlers)
		{
			if (handler != nullptr)
			{
				handler(id);
			}
		}
		// then its deletion takes its dance with it, the dancers it still has going back to deciding what to do
		if (auto* entry = Find(id); entry != nullptr && entry->dance != entt::null)
		{
			const auto dance = std::exchange(entry->dance, entt::null);
			if (Locator::danceSystem::has_value() && Locator::entitiesRegistry::has_value() &&
			    dances::IsDance(Locator::entitiesRegistry::value(), dance))
			{
				Locator::danceSystem::value().Destroy(dance);
			}
		}
	}
	// the walk's cursor leaves a reaction that goes for the next older one that stays (the list as it is now: a handler
	// that removed others has moved it already)
	std::vector<uint32_t> listed;
	listed.reserve(_reactions.size());
	std::ranges::transform(_reactions, std::back_inserter(listed), &Reaction::id);
	_cursor = effects::reaction_turn::CursorAfterRemoval(listed, ids, _cursor);
	std::erase_if(_reactions, [ids](const Reaction& reaction) { return std::ranges::find(ids, reaction.id) != ids.end(); });
}

uint32_t ReactionSystem::Cursor() const
{
	return _cursor;
}

void ReactionSystem::SetCursor(uint32_t reaction)
{
	_cursor = reaction;
}

bool ReactionSystem::IsActive(uint32_t id) const
{
	return std::ranges::any_of(_reactions, [id](const Reaction& reaction) { return reaction.id == id; });
}

bool ReactionSystem::HasReaction(entt::entity initiator) const
{
	return std::ranges::any_of(_reactions, [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
}

void ReactionSystem::SetPlayerOfNewest(entt::entity initiator, PlayerNames player)
{
	// The reactions are in the order that they started, so the newest is the last
	const auto newest = std::find_if(_reactions.rbegin(), _reactions.rend(),
	                                 [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
	if (newest != _reactions.rend())
	{
		newest->player = player;
	}
}

void ReactionSystem::SetInTurn(bool inTurn)
{
	_inTurn = inTurn;
}

bool ReactionSystem::InTurn() const
{
	return _inTurn;
}

uint32_t ReactionSystem::NextJoinOrder()
{
	return ++_joinOrder;
}

void ReactionSystem::ResetJoinOrder()
{
	_joinOrder = 0;
}

void ReactionSystem::SetReactionHandler(LivingClass living, LivingReactionHandler handler)
{
	_handlers.at(static_cast<std::size_t>(living)) = handler;
}

LivingReactionHandler ReactionSystem::ReactionHandler(LivingClass living) const
{
	return _handlers.at(static_cast<std::size_t>(living));
}

void ReactionSystem::SetShutDownHandler(LivingClass living, LivingShutDownHandler handler)
{
	_shutDownHandlers.at(static_cast<std::size_t>(living)) = handler;
}

std::span<const LivingShutDownHandler> ReactionSystem::ShutDownHandlers() const
{
	return _shutDownHandlers;
}

void ReactionSystem::Reset()
{
	_reactions.clear();
	_nextId = 1;
	_cursor = 0;
	_inTurn = false;
}
} // namespace openblack::ecs::systems
