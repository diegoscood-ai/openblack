/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Effects/Reactions.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
/// The game's reactions: the list in creation order, the next id, whether a game turn is running, and the handlers
/// each Living class registered (ecs::effects::reactions goes through it). A reaction is made and spread once, by
/// ecs::effects::reactions; here it is kept until its initiator takes it away
class ReactionSystemInterface
{
public:
	virtual ~ReactionSystemInterface() = default;

	/// Gives the reaction the next id, puts it at the tail of the list and returns the id
	[[nodiscard]] virtual uint32_t Add(effects::reactions::Reaction reaction) = 0;
	/// The reaction by its id; null when gone (or for 0)
	[[nodiscard]] virtual effects::reactions::Reaction* Find(uint32_t id) = 0;
	/// Every reaction, in creation order
	[[nodiscard]] virtual std::vector<effects::reactions::Reaction>& List() = 0;
	[[nodiscard]] virtual std::span<const effects::reactions::Reaction> GetReactions() const = 0;

	/// Every reaction an initiator made goes, as when a miracle goes
	virtual void RemoveFrom(entt::entity initiator) = 0;
	/// The reactions of one kind an initiator made go, as a magic tree that catches fire stops being looked at
	virtual void RemoveFrom(entt::entity initiator, openblack::Reaction type) = 0;
	/// One reaction goes
	virtual void Remove(uint32_t id) = 0;
	/// The reactions with these ids go, in this order: each is no longer available, then each class's shut-down
	/// handler is called for it; once every one is shut down they leave the list, the others keep their order
	virtual void Remove(std::span<const uint32_t> ids) = 0;
	/// Whether a reaction is still in the list
	[[nodiscard]] virtual bool IsActive(uint32_t id) const = 0;
	/// Whether an initiator has a reaction in the list, available or not
	[[nodiscard]] virtual bool HasReaction(entt::entity initiator) const = 0;

	/// Between the start and the end of a game turn's logic
	virtual void SetInTurn(bool inTurn) = 0;
	[[nodiscard]] virtual bool InTurn() const = 0;

	/// One more villager joined a reaction: the counter goes up and its new value is returned, so a later joiner
	/// always has a higher number
	[[nodiscard]] virtual uint32_t NextJoinOrder() = 0;
	/// The join counter back to 0. Reset leaves it alone
	virtual void ResetJoinOrder() = 0;

	/// The handler a Living class applies a reaction with; null while the class has none
	virtual void SetReactionHandler(effects::reactions::LivingClass living,
	                                effects::reactions::LivingReactionHandler handler) = 0;
	[[nodiscard]] virtual effects::reactions::LivingReactionHandler
	ReactionHandler(effects::reactions::LivingClass living) const = 0;
	/// The handler a Living class shuts a reaction down with, for its followers
	virtual void SetShutDownHandler(effects::reactions::LivingClass living,
	                                effects::reactions::LivingShutDownHandler handler) = 0;
	/// Every class's shut-down handler, in class order (null where a class has none)
	[[nodiscard]] virtual std::span<const effects::reactions::LivingShutDownHandler> ShutDownHandlers() const = 0;

	/// A new land: no reactions, ids from 1 again, outside a turn. The handlers and the join counter stay
	virtual void Reset() = 0;
};
} // namespace openblack::ecs::systems
