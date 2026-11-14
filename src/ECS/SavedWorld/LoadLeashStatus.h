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

#include <InterfaceRecords.h>
#include <entt/entity/entity.hpp>

#include "ECS/SavedWorld/WorldWriter.h"
#include "Enums.h"

namespace openblack::gamesave
{
struct WorldRead;
}

namespace openblack::ecs
{
class Registry;
namespace components
{
struct CreatureLeash;
}
namespace systems
{
class LeashSystemInterface;
}
} // namespace openblack::ecs

// The leash's status of each player in a saved game's world file, put back into the leash of the creature the player
// leads, and written from it (docs/wiki/engine/saved-games.md#the-saves-in-play). The status is the player's leash:
// whether it is on in the player's hand, whether it works, its type (the one worn, else the one picked), the force it
// pulls with, what it is tied to, its two lengths and the game turn it was last tied. A load reads them back, the
// leash going on again as it was; then, at the load's end, each leash is switched on or off again as its status says,
// with the mood and the temple pick of putting it on, and no sound or rule.
namespace openblack::ecs::saved_leash
{

/// What putting a status back did
enum class Restored : uint8_t
{
	/// The leash is as the status says
	Done,
	/// The status says the leash is tied to an object the load did not make: the leash is left as it was
	TiedToNothingMade,
};

/// Puts the status back into the leash of the creature the player leads: whether a leash put on works, the leash
/// picked and the turn of the last tie, and, when the status is on, the leash worn in the player's hand, tied to
/// `tiedTo` when the status names an object, with the status's two lengths and its rope laid afresh. A status that is
/// off takes any leash off
[[nodiscard]] Restored Restore(components::CreatureLeash& leash, const gamesave::LeashStatusRecord& record, PlayerNames player,
                               std::optional<entt::entity> tiedTo);

/// The values of the status the leash of the creature the player leads gives, written over `record`: whether the
/// player holds the leash on, whether it works, its type (the worn one's, else the one picked), `tiedTo` (the number of
/// what it is tied to, 0 for none) and the turn of the last tie, and, while it is on, its two lengths. The force and, with
/// no leash on, the lengths stay as the record has them
void WriteLive(gamesave::LeashStatusRecord& record, const components::CreatureLeash& leash, PlayerNames player,
               gamesave::ObjectNumber tiedTo);

/// A load: each loaded player's leash status (kept by the player's number, saved_world_write::KeepPlayerInterfaces) is
/// put back into the leash of the creature that player leads, what it is tied to found among the loaded objects. A
/// player who leads no creature keeps the status only for the next save. Run once every loaded object is resolved, so
/// that the leashes the creatures know are back first
void Load(Registry& registry, const systems::LeashSystemInterface& leashes, const gamesave::WorldRead& world,
          const saved_world_write::LoadedObjects& loaded);

/// The load's end for the leashes, after the hand's status is back: each loaded player's leash is switched on or off
/// again as its status's switch says (LeashSystemInterface::SwitchAtLoad), on the creature that player leads
void SwitchAgain(systems::LeashSystemInterface& leashes, const gamesave::WorldRead& world,
                 const saved_world_write::LoadedObjects& loaded);

} // namespace openblack::ecs::saved_leash
