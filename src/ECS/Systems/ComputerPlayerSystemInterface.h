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
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct ComputerMind;
struct ComputerPlayer;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

/// The computer minds of the players. A land script switches a mind on. At its first turn after that, the mind starts on
/// the land: it gives the player a creature when the player has none, sizes the creature and teaches it its miracles.
class ComputerPlayerSystemInterface
{
public:
	virtual ~ComputerPlayerSystemInterface() = default;

	/// A land script switches the mind of the player on or off. The player is already one of the land's.
	virtual void SwitchOn(PlayerNames player, bool on) = 0;
	/// A script pauses the mind of the player or lets it go on
	virtual void Pause(PlayerNames /*player*/, bool /*paused*/) {}
	/// A land script names the player whose creature gives the size of this player's creature
	virtual void SetCreatureLike(PlayerNames player, PlayerNames like) = 0;
	/// In the turn of the players: each mind that is switched on and did not start on the land starts, and the hand of
	/// each mind that is on, not paused and not held by the wide screen goes through its steps
	virtual void ProcessTurn() = 0;

	// ---- a script takes the player's hand --------------------------------------------------------------------------

	/// The mind is switched on and the hand, held by the script, moves to the point at the speed: across the land
	/// above the ground, or straight to the point's height
	virtual void MoveHand(PlayerNames /*player*/, const glm::vec3& /*point*/, float /*speed*/, bool /*overGround*/) {}
	/// The hand, held by the script, is put at the point at its next turn: above the ground, or at the point's height
	virtual void PutHand(PlayerNames /*player*/, const glm::vec3& /*point*/, bool /*overGround*/) {}
	/// The land under the hand as of its last turn, or none when the land has no such player
	[[nodiscard]] virtual std::optional<glm::vec3> HandOnGround(PlayerNames /*player*/) const { return std::nullopt; }
	/// The hand has no step left; false when the land has no such player
	[[nodiscard]] virtual bool IsHandReady(PlayerNames /*player*/) const { return false; }
	/// The player's speed and the speed of the steps under way
	virtual void SetHandSpeed(PlayerNames /*player*/, float /*speed*/) {}
	/// The script lets go of the hand: the mind is no longer paused, the hand no longer held and its steps take the
	/// player's speed
	virtual void Release(PlayerNames /*player*/) {}
	/// The hand gives up its steps and lets go of what it holds, and the queued actions are dropped
	virtual void ClearActions(PlayerNames /*player*/) {}

	// ---- a script names actions of the player's mind ---------------------------------------------------------------

	/// The action named is forced on the two things now; whether it gave the hand steps
	virtual bool ForceAction(PlayerNames /*player*/, std::string_view /*name*/, entt::entity /*first*/, entt::entity /*second*/)
	{
		return false;
	}
	/// The action named is queued on the two things, to be forced when the hand is free
	virtual void QueueAction(PlayerNames /*player*/, std::string_view /*name*/, entt::entity /*first*/, entt::entity /*second*/)
	{
	}

	// ---- a script sets the player's mind ---------------------------------------------------------------------------

	/// The weight of every node of the player's mind that the name names
	virtual void SetPersonality(PlayerNames /*player*/, std::string_view /*name*/, float /*value*/) {}
	/// How much every node of the player's mind that the name names is held back
	virtual void SetSuppression(PlayerNames /*player*/, std::string_view /*name*/, float /*value*/) {}
	/// How the player's mind feels about the other player
	virtual void SetAttitude(PlayerNames /*player*/, PlayerNames /*toward*/, float /*value*/) {}
	/// How the player's mind feels about the other player, as a script reads it
	[[nodiscard]] virtual float Attitude(PlayerNames /*player*/, PlayerNames /*toward*/) const { return 0.0f; }
	/// The personality file at the path is read over the player's mind, and over the names of the tree's rows for every
	/// mind; nothing when there is no such file
	virtual void LoadPersonality(PlayerNames /*player*/, std::string_view /*path*/) {}
	/// The player's mind is written to the personality file at the path
	virtual void SavePersonality(PlayerNames /*player*/, std::string_view /*path*/) {}
	/// As a land is cleared: each player's mind is kept for the player's entity on the next land
	virtual void KeepForNextLand() {}
	/// The debug trace of the player's mind on or off (openblack's): each turn of it writes one line to the game's log
	virtual void SetTraced(PlayerNames /*player*/, bool /*on*/) {}

	// ---- a saved game ----------------------------------------------------------------------------------------------

	/// The player's mind as a save writes it: its entity's, else the one kept from the land before; none when the
	/// player has none yet (a mind is made when first set, as a new one starts)
	[[nodiscard]] virtual const components::ComputerMind* SavedMind(PlayerNames /*player*/) const { return nullptr; }
	/// A load puts back the player's computer player as the save has it: its switches on the player's entity (none for
	/// a player whose switches are a new one's) and its mind, on its entity when it has one, else kept for its entity
	/// to take up as a mind kept from the land before is
	virtual void Restore(PlayerNames /*player*/, const components::ComputerPlayer& /*state*/,
	                     const components::ComputerMind& /*mind*/)
	{
	}
};

} // namespace openblack::ecs::systems
