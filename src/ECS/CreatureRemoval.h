/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// Taking a creature out of the game for good, as the game does when a creature is deleted. Everything that holds the
/// creature lets go of it first, in the game's order: the hand and any fight (which the game ends before it gets
/// there), the leash, the miracles and sounds it has going, the player's list of creatures, the reactions it set off,
/// what it was doing and carrying, the view that follows it, and last its physics, its place on the map and the
/// creature itself. As the game's own deletion starts, after the hand and the fight, the local player's creature is
/// written to the profile's files, as the land change writes it. Then its owner is left with no creature, whichever one
/// the game kept for them: no other creature of theirs takes its place.
namespace openblack::ecs::creature_removal
{

/// The game's side of each step, which the game goes through its systems for and tests fake
class WorldInterface
{
public:
	virtual ~WorldInterface() = default;
	/// The hand lets go of the creature if it holds it, or stops stroking it if it is on it
	virtual void LetGoFromHand(entt::entity creature) = 0;
	/// It leaves its fight at once, letting go of its arena; its opponent's fight ends as one given up does
	virtual void EndFight(entt::entity creature) = 0;
	/// The local player's own creature is written to the profile's files with its mind and body as the game's deletion of
	/// it starts, so that one that sparkled away through a vortex still crosses to the next land
	virtual void KeepForNextLand(entt::entity creature) = 0;
	/// Its leash comes off, and it stops being the creature its owner leads; with a leash on or not, its owner's leash is let
	/// go of as one coming off (LeashSystemInterface::ReleaseUnleashed)
	virtual void TakeOffLeash(entt::entity creature) = 0;
	/// The miracles it is casting stop and its beams go; its sounds stop
	virtual void ReleaseEffects(entt::entity creature) = 0;
	/// The reactions it set off in others end
	virtual void RemoveReactions(entt::entity creature) = 0;
	/// It stops what it is doing and where it is walking, and drops what it carries
	virtual void StopActing(entt::entity creature) = 0;
	/// The camera that follows it lets go of it
	virtual void LeaveView(entt::entity creature) = 0;
	/// Out of the physics, its fire and its place on the map, and gone from the registry
	virtual void RemoveFromWorld(entt::entity creature) = 0;
	/// The creature the game keeps for the player (PlayerSystemInterface::GetPrimaryCreature), if any
	[[nodiscard]] virtual std::optional<entt::entity> PlayersCreature(PlayerNames owner) const = 0;
	/// The player is left with no creature. Another creature of theirs it held, still there, stops being the one they
	/// lead and its leash comes off
	virtual void LosePlayersCreature(PlayerNames owner) = 0;
};

/// What a removal did
struct Removed
{
	/// Whose it was
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	/// Whether it was the creature the game kept for its owner
	bool wasPrimary {false};
	/// The other creature the game kept for its owner, which is theirs no more
	std::optional<entt::entity> alsoLost;
	/// The other things that pointed at it and were made to let go: markers, carried things, creatures following it
	size_t referencesCleared {0};
};

/// The player's creature lists forget it
void ForgetInPlayerLists(Registry& registry, entt::entity creature);

/// What else points at the creature lets go of it: the leash's order markers, the things it holds, creatures following
/// it, a hand resting on it. How many did
size_t ForgetReferences(Registry& registry, entt::entity creature);

/// Takes the creature out of the game through every step, in order. None when it isn't a creature
std::optional<Removed> Remove(Registry& registry, entt::entity creature, WorldInterface& world);

/// The game's steps, through its systems
[[nodiscard]] std::unique_ptr<WorldInterface> MakeGameWorld();

/// Takes a creature out of the game it is in, through the game's systems
std::optional<Removed> RemoveFromGame(entt::entity creature);

} // namespace openblack::ecs::creature_removal
