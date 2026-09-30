/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Reactions.h"

#include <algorithm>
#include <unordered_map>

#include <spdlog/spdlog.h>

using namespace openblack;
using namespace openblack::ecs::effects;

namespace
{
std::vector<reactions::Reaction> g_Reactions;
uint32_t g_NextId = 1;
uint32_t g_Turn = 0;
std::unordered_map<int, reactions::SpreadHandler> g_SpreadHandlers;
} // namespace

uint32_t reactions::CreateReaction(entt::entity initiator, openblack::Reaction type, PlayerNames player, bool stamp)
{
	// (type & 0xFF) == -1 never holds for the byte, but the callers check reactionType != -1 first
	if (type == openblack::Reaction::None)
	{
		return 0;
	}
	Reaction reaction;
	reaction.id = g_NextId++;
	reaction.initiator = initiator;
	reaction.type = type;
	reaction.player = player;
	reaction.turnCreated = stamp ? g_Turn : 0;
	g_Reactions.push_back(reaction);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Reaction {}: type {} by entity {}", reaction.id, static_cast<int>(type),
	                    static_cast<uint32_t>(initiator));
	// SpreadReaction(GetPos, GetMapCellSpiralSizeFromRadius(GetRadius), type, initiator, this)
	if (const auto it = g_SpreadHandlers.find(static_cast<int>(type)); it != g_SpreadHandlers.end() && it->second != nullptr)
	{
		const auto copy = reaction; // the handler may create or remove reactions
		it->second(copy);
	}
	return reaction.id;
}

void reactions::SetSpreadHandler(openblack::Reaction type, SpreadHandler handler)
{
	g_SpreadHandlers[static_cast<int>(type)] = handler;
}

void reactions::RemoveAllReactionsInitiatedByObject(entt::entity initiator)
{
	std::erase_if(g_Reactions, [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
}

void reactions::RemoveAllReactionsOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type)
{
	std::erase_if(g_Reactions, [initiator, type](const Reaction& reaction) {
		return reaction.initiator == initiator && reaction.type == type;
	});
}

void reactions::Stamp(uint32_t reaction)
{
	for (auto& entry : g_Reactions)
	{
		if (entry.id == reaction)
		{
			entry.turnCreated = g_Turn;
		}
	}
}

void reactions::SetRadius(uint32_t reaction, float radius)
{
	for (auto& entry : g_Reactions)
	{
		if (entry.id == reaction)
		{
			entry.radius = radius;
		}
	}
}

void reactions::SetInitiator(uint32_t reaction, entt::entity initiator)
{
	for (auto& entry : g_Reactions)
	{
		if (entry.id == reaction)
		{
			entry.initiator = initiator;
		}
	}
}

uint32_t reactions::GetReactionInitiatedBy(entt::entity initiator)
{
	const auto it = std::find_if(g_Reactions.begin(), g_Reactions.end(),
	                             [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
	return it == g_Reactions.end() ? 0 : it->id;
}

const std::vector<reactions::Reaction>& reactions::All()
{
	return g_Reactions;
}

const reactions::Reaction* reactions::Find(uint32_t id)
{
	if (id == 0)
	{
		return nullptr;
	}
	const auto it = std::find_if(g_Reactions.begin(), g_Reactions.end(), [id](const Reaction& reaction) { return reaction.id == id; });
	return it != g_Reactions.end() ? &*it : nullptr;
}

uint32_t reactions::Turn()
{
	return g_Turn;
}

void reactions::SetTurn(uint32_t turn)
{
	g_Turn = turn;
}

void reactions::Clear()
{
	g_Reactions.clear();
	g_NextId = 1;
}
