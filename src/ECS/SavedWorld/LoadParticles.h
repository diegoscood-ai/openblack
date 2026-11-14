/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <InterfaceRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <entt/entity/entity.hpp>

namespace openblack::gamesave
{
struct WorldRead;
}

namespace openblack::particles
{
class Effect;
struct File;
} // namespace openblack::particles

// A particle effect of a saved game's world file made again in the particle engine, as the original's load puts its
// manager, collections, atoms, particles, rules' data and sounds back (docs/wiki/engine/saved-games.md#openblack, the
// particles): the effect goes on from them with its atoms' own random bytes, and no random number is drawn. The owners
// of the effects (the containers, the spells, the shields) make their effects through it.
namespace openblack::ecs::saved_particles
{

/// An effect a save holds: the numbers of its interface and its manager, its manager, and every record of its parts
/// (its collections, atoms, particles, rules' data and sounds) by its number
struct PlannedEffect
{
	gamesave::ObjectNumber interfaceNumber {0};
	gamesave::ObjectNumber managerNumber {0};
	gamesave::ParticleManagerRecord manager;
	std::map<gamesave::ObjectNumber, gamesave::SavedRecord> parts;
	/// The player its manager names (-1 for none), when the plan was told it
	int player {-1};
	/// The players its manager and parts name, by their numbers, resolved by the plan for a maker with no player lookup
	/// of its own (a way in's object mover: its own and the one each carried thing was taken for)
	std::map<gamesave::ObjectNumber, int> players;
};

/// The effect an interface of the file names, with its parts; none for a number that is no interface whose manager was
/// read
[[nodiscard]] std::optional<PlannedEffect> PlanEffect(const gamesave::WorldRead& world, gamesave::ObjectNumber interfaceNumber);

/// What the effect needs of the rest of the load: the player a number names (-1 for none) and the entity the load made
/// of a number (entt::null for none)
struct EffectLinks
{
	std::function<int(gamesave::ObjectNumber)> playerOf;
	std::function<entt::entity(gamesave::ObjectNumber)> entityOf;
};

/// The effect made again from its records, of its file: its values, then its collections, the oldest first, each with
/// its rules still on it and their data, its atoms, the oldest first, each with its values, its particle's creator,
/// its rules' data, its sounds and its sub-collections. None, with the reason, when a part the effect needs is missing
[[nodiscard]] std::unique_ptr<particles::Effect> RestoreEffect(const PlannedEffect& planned,
                                                               std::shared_ptr<const particles::File> file,
                                                               const EffectLinks& links, std::string& why);

/// A spot visual's container a save holds, by its number, with its record and its effect
struct PlannedContainer
{
	gamesave::ObjectNumber number {0};
	gamesave::ParticleContainerRecord record;
	PlannedEffect effect;
};

/// The containers of the game's list (the newest first), with their effects; a container whose effect is not read is
/// left out (its record stays as the load kept it)
[[nodiscard]] std::vector<PlannedContainer> PlanContainers(const gamesave::WorldRead& world,
                                                           const std::vector<gamesave::ObjectNumber>& list,
                                                           const std::function<int(gamesave::ObjectNumber)>& playerOf);

/// What a made container leaves for the rest of the load: the object of each container made, by its number
using MadeContainers = std::unordered_map<gamesave::ObjectNumber, entt::entity>;

/// The containers made again in the engine, the oldest first so that the newest is at the head, each with its effect
/// made again (RestoreEffect, of its spot visual's file), its place, the thing it hangs on, its turns and its strength;
/// `keepSource` is told the numbers of each container, its interface and its manager with the effect's id, `keep` its
/// object. A container whose effect cannot be made is left as the load kept it, with one log line
[[nodiscard]] MadeContainers MakeContainers(const std::vector<PlannedContainer>& containers, const EffectLinks& links,
                                            const std::function<void(gamesave::ObjectNumber, uint32_t, uint64_t)>& keepSource,
                                            const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep);

} // namespace openblack::ecs::saved_particles
