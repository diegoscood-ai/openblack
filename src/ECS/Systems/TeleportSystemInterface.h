/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The teleport miracle's stones. Each cast leaves an invisible stone with a swirling pool on the land; a player's stones
/// form a network. Villagers walking past one of them turn aside into it when jumping to another of the player's
/// stones saves them enough of their walk, and come out of the stone that leaves them closest to where they were going.
/// A stone goes with its miracle.
///
/// The points are map positions, as the miracles keep them: x and z on the map in metres, y above the land.
class TeleportSystemInterface
{
public:
	virtual ~TeleportSystemInterface() = default;

	/// Whether a stone may be put at a point: nothing standing fixed on the land within its reach
	[[nodiscard]] virtual bool CanPlaceStone(glm::vec3 point) const = 0;

	/// Whether a villager walking past a stone would save enough of its walk by jumping from it to another of the
	/// stone's player's stones: what makes it take up the stone's reaction at all
	[[nodiscard]] virtual bool ShouldReact(entt::entity stone, entt::entity living) const = 0;
	/// The jump from a stone to the player's stone that leaves the traveller closest to where it was going; forced, it
	/// takes any other stone. Whether it jumped.
	virtual bool DoTeleport(entt::entity stone, entt::entity living, bool forced) = 0;
	/// A villager dropped by a player's hand onto a stone of theirs jumps at once, if there is another stone: it lands
	/// at the stone, decides what to do, and jumps to the stone nearest where that takes it. Whether it jumped.
	virtual bool DropOnStone(entt::entity villager, entt::entity stone, PlayerNames dropper) = 0;
	/// The stone a worshipper of a player heads for to reach a far worship site, if going through the stones is shorter
	/// than the limit. The worshipper and the site are points of the world.
	[[nodiscard]] virtual std::optional<entt::entity> RouteStoneFor(PlayerNames player, glm::vec3 worshipper, glm::vec3 site,
	                                                                float maxDistance) const = 0;

	/// Once a game turn, with the players: the travellers that are gone or no longer react to their stone are dropped
	virtual void ProcessTurn() = 0;
	/// A new land: no stones
	virtual void Reset() = 0;

	// Ours: the other steps the stones take part in

	/// Whether a villager held over a stone may be dropped onto it: the stone is of the villager's player, and that
	/// player has another stone to jump to
	[[nodiscard]] virtual bool CanDropOnStone(entt::entity villager, entt::entity stone) const = 0;
	/// A villager turning aside into a stone is listed with where it was going, in place of any earlier entry
	virtual void RegisterDestination(entt::entity stone, entt::entity living, glm::vec3 destination) = 0;
	/// Every frame, with the seconds of game time that have passed: the pools follow their stones
	virtual void Update(float seconds) = 0;
	/// The test hooks read from the environment, once a turn
	virtual void RunDebugHooks() = 0;
};

} // namespace openblack::ecs::systems
