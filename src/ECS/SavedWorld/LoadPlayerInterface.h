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
#include <string>

#include <GameTailRecords.h>
#include <InterfaceRecords.h>
#include <entt/entity/entity.hpp>

#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/HandSystemInterface.h"

namespace openblack::gamesave
{
struct WorldRead;
}

namespace openblack::ecs
{
class Registry;
}

// The hand's player's interface status in a saved game's world file, put back into the hand and the gestures, and
// written from them (docs/wiki/engine/saved-games.md#the-saves-in-play). The status keeps the place and hand the sync
// last gave, the camera's eye and focus with them, the throw's data, the hand's movement of the last turn, the object
// in the hand and the last seed type the repeat gesture asks for again. A load puts them back as a new status read from
// the file, and leaves the hand's action in its normal state, with no pick-up or out-of-hand call.
namespace openblack::ecs::saved_hand
{

/// The hand's values the status says, with the object in the hand the load made of the status's held object
[[nodiscard]] systems::SavedHandStatus StatusOf(const gamesave::InterfaceStatusRecord& record,
                                                std::optional<entt::entity> held);

/// The hand's values written over `record`, with `held` the number of the object in the hand (0 for none) and the
/// last seed type when the gestures are there to give it; the status's other values stay as the record has them
void WriteLive(gamesave::InterfaceStatusRecord& record, const systems::SavedHandStatus& status, gamesave::ObjectNumber held,
               std::optional<int32_t> lastSeedType);

/// The drawn hand the game's record keeps, holding `held`, the object the load made of the status's held object
/// `heldNumber`: none unless the drawn hand was in its holding state with that same object
[[nodiscard]] std::optional<systems::SavedDrawnHand>
DrawnHandOf(const gamesave::GameTail& tail, gamesave::ObjectNumber heldNumber, std::optional<entt::entity> held);

/// The drawn hand written over the game's record's hand part, the inverse of DrawnHandOf: the state, the object
/// `heldNumber` (0 for none) and the seed `spellNumber` whose effect plays in the hand (0 for none); while it holds an
/// object, the hold type, whether the object was drawn sorted before the hand and how far it hangs; the hand's place
/// once it has one; and, while an effect plays in the hand, whether it is drawn at the hand's bone. The rest stays as
/// the record has it: the shadow flag, the particles' object, and the values above the drawn hand does not have now
void WriteDrawnHand(gamesave::GameTail& tail, const systems::DrawnHandNow& drawn, gamesave::ObjectNumber heldNumber,
                    gamesave::ObjectNumber spellNumber);

/// What a load put back
struct Restored
{
	/// Whether the file had the hand's player's status
	bool found {false};
	/// The number in the file of the object in the hand, 0 for none
	gamesave::ObjectNumber heldNumber {0};
	/// Whether the load made that object, and so it is in the hand again
	bool heldMade {false};
	/// Whether the drawn hand holds it again
	bool drawnHolding {false};
	/// The last seed type put back
	int32_t lastSeedType {-1};
};

/// A load: the hand's player's status (kept by the player's number, saved_world_write::KeepPlayerInterfaces) is put
/// back into the hand, the object in the hand found among the loaded objects, and its last seed type into the
/// gestures and the player's spell icons, those of them the game has. Run once every loaded object is resolved
Restored Load(const Registry& registry, systems::HandSystemInterface& hand, const gamesave::WorldRead& world,
              const saved_world_write::LoadedObjects& loaded);

/// (debug) One line of what the load put back, for the debug bar's Saved Games menu
[[nodiscard]] std::string ReadoutOf(const Restored& restored);

} // namespace openblack::ecs::saved_hand
