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
#include <vector>

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
	std::vector<uint32_t> ids;
	for (const auto& reaction : _reactions)
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
	std::vector<uint32_t> ids;
	for (const auto& reaction : _reactions)
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
	}
	std::erase_if(_reactions, [ids](const Reaction& reaction) { return std::ranges::find(ids, reaction.id) != ids.end(); });
}

bool ReactionSystem::IsActive(uint32_t id) const
{
	return std::ranges::any_of(_reactions, [id](const Reaction& reaction) { return reaction.id == id; });
}

bool ReactionSystem::HasReaction(entt::entity initiator) const
{
	return std::ranges::any_of(_reactions, [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
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
	_inTurn = false;
}
} // namespace openblack::ecs::systems
