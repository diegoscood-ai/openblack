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

#include <optional>

#include <entt/fwd.hpp>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/InfluenceCrossing.h"
#include "ECS/Components/InterfaceAlignment.h"
#include "ECS/Components/PlayerAlliances.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/PlayerRivalry.h"
#include "ECS/Components/VirtualInfluence.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
/// The players: their entities, every player's alignment, and the magic of the players without an entity.
/// The alignment lasts the whole game and is kept when another land loads, but a player's entity is made again on each
/// land, so the alignment cannot be a component on that entity: the service keeps it for every player, the neutral one
/// included. A player with an entity has its PlayerMagic on it; the others (the neutral player, a script player not
/// made yet) use the service's, which a land load clears; their creature is kept the same way. What a player keeps from
/// land to land goes through KeepForNextLand and TakeUpKept.
class PlayerSystemInterface
{
public:
	virtual ~PlayerSystemInterface() = default;

	virtual void RegisterPlayers() = 0;
	virtual void AddPlayer(entt::entity playerEntity) = 0;
	[[nodiscard]] virtual entt::entity GetPlayer(PlayerNames name) const = 0;
	/// A creature was made for its owner: it becomes their creature, whichever they had (the newest made wins, the
	/// neutral player's included), and their list remembers it after the creatures they got before
	virtual void AddCreature(entt::entity creature) = 0;
	/// The player's primary creature, the one creature the game keeps for each player: the last one made for them, or
	/// the one a script loads for them later, and none once any creature of theirs has gone (creature_removal); for a
	/// player who leads creatures, the one they lead. Every part of the game that asks for "the player's creature"
	/// (scripts, the leash, Creature Mode) uses this one. A name out of range is the neutral player's
	[[nodiscard]] virtual std::optional<entt::entity> GetPrimaryCreature(PlayerNames name) const = 0;
	/// Makes the creature the player's creature, or leaves them none. Their entity keeps it
	/// (components::PlayerCreatureSlot); for a player without an entity the service keeps it, until an entity of
	/// theirs is listed or a land is loaded. A name out of range is the neutral player's
	virtual void SetPrimaryCreature(PlayerNames name, std::optional<entt::entity> creature) = 0;
	/// As a land is cleared: each player keeps what is theirs rather than the land's, for when they are made again on a
	/// later land. Every player's alliances, and the local player's interface alignment, hand crossing and hand's
	/// virtual influence, move off the last land's entities; every player's alignment is the service's already
	virtual void KeepForNextLand() = 0;
	/// A player made on a land takes up what they kept from the lands before: their alliances, and the first entity the
	/// local player gets carries their interface alignment, hand crossing and the hand's virtual influence again
	virtual void TakeUpKept(entt::entity playerEntity) = 0;
	/// The hand's virtual influence at the map's clear, after KeepForNextLand has moved it off the last land's entity:
	/// the one kept, none when the last land had none. A new game makes it afresh first, as the interface status it
	/// belongs to is made again. A player service that keeps none gives none
	[[nodiscard]] virtual components::VirtualInfluence* VirtualInfluenceAtClearMap([[maybe_unused]] bool newGame)
	{
		return nullptr;
	}
	/// The player at this machine, whose hand the mouse moves. openblack is played by one player, the first.
	[[nodiscard]] virtual PlayerNames GetLocalPlayer() const { return PlayerNames::PLAYER_ONE; }
	/// Whether the computer plays a player: every player but the one at this machine
	[[nodiscard]] virtual bool IsComputerPlayer(PlayerNames name) const
	{
		return name != GetLocalPlayer() && name != PlayerNames::NEUTRAL;
	}
	/// A land is loaded: its player entities went with the registry, so none is listed until the new land makes them,
	/// and the creatures kept for the players without an entity went with it too
	virtual void ClearPlayers() = 0;

	/// Kept across lands. A name out of range is the neutral player's
	[[nodiscard]] virtual components::Alignment& Alignment(PlayerNames name) = 0;
	/// The player's influence power as it was last worked out (influence::CalculateInfluencePower), kept across lands
	/// until it is worked out again; 0 before the first time. A name out of range is the neutral player's
	[[nodiscard]] virtual float InfluencePower([[maybe_unused]] PlayerNames name) const { return 0.0f; }
	virtual void SetInfluencePower([[maybe_unused]] PlayerNames name, [[maybe_unused]] float power) {}
	/// The adults and children of the player's towns counted so far: the players' turn sets it to 0 as the player's
	/// towns start and adds each town's people after its turn, and it is kept until the player's next turn, across
	/// lands; 0 before the first. A name out of range is the neutral player's
	[[nodiscard]] virtual uint32_t TotalPopulation([[maybe_unused]] PlayerNames name) const { return 0; }
	virtual void SetTotalPopulation([[maybe_unused]] PlayerNames name, [[maybe_unused]] uint32_t population) {}
	/// The magic of a player without an entity. A name out of range is the neutral player's
	[[nodiscard]] virtual components::PlayerMagic& MagicWithoutEntity(PlayerNames name) = 0;
	/// A land is loaded: the magic of the players without an entity back to its start values (the alignment stays)
	virtual void ClearMagicWithoutEntity() = 0;
	/// The alliances (ecs::alliances) of a player while no entity carries them: a player who has none on this land, and
	/// every player from a land's clear until the next land makes them again, whose first entity takes them back. None
	/// until one is set. A land load keeps them: they last the whole game. A name out of range is the neutral player's.
	/// A player service that keeps none answers none, and nobody is allied
	[[nodiscard]] virtual std::optional<components::PlayerAlliances> AlliancesWithoutEntity(PlayerNames /*name*/) const
	{
		return std::nullopt;
	}
	/// Keeps them for a player without an entity, or forgets them (none)
	virtual void KeepAlliancesWithoutEntity(PlayerNames /*name*/,
	                                        const std::optional<components::PlayerAlliances>& /*alliances*/)
	{
	}
	/// What the player keeps of the other players and of their own influence (ecs::player_rivalry): kept across lands,
	/// from the game's start. A name out of range is the neutral player's. A player service that keeps none answers
	/// nothing and forgets what it is given
	[[nodiscard]] virtual components::PlayerRivalry Rivalry(PlayerNames /*name*/) const { return {}; }
	virtual void SetRivalry(PlayerNames /*name*/, const components::PlayerRivalry& /*rivalry*/) {}

	/// The local player's interface alignment and hand crossing while no entity carries them: before the first land
	/// makes the local player, and from a land's clear until the next land makes it again. Both last the whole game,
	/// so a land's clear moves them here from the entity and the new land's local player takes them back
	[[nodiscard]] virtual components::InterfaceAlignment& InterfaceAlignmentWithoutEntity() = 0;
	[[nodiscard]] virtual components::InfluenceCrossing& InfluenceCrossingWithoutEntity() = 0;
};
} // namespace openblack::ecs::systems
