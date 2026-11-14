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

#include <SavedRecords.h>
#include <entt/entity/entity.hpp>

// A saved game's workshops made again (docs/wiki/engine/saved-games.md#openblack): the abode's load makes the
// workshop of its row without its sign and its wood pile, and these give it back, as the original's load reads its
// record, the turns left for the scaffold being made, the scaffolds it owns and its three slots; then the pile, the
// sign of the wood it wants (made from the sign's own record) and the scaffolds it holds, once each is made.
namespace openblack::ecs::load_workshops
{

/// The workshop's own values from its record: the turns left for the scaffold being made, the scaffolds it owns and
/// its three slots
void SetValues(entt::entity workshop, const gamesave::WorkshopRecord& record);

/// The workshop's sign of the wood it wants, made from the sign's own record: made as the game makes one, at the place
/// the record keeps, for the workshop, of the row it names, then with its creation number, its desire and its record
/// as saved. entt::null when the row is none of the game's
entt::entity MakeSign(const gamesave::ShowNeedsVisualsRecord& record, entt::entity workshop);

/// The workshop's wood pile and sign, as its record names them, once they are made
void SetPileAndSign(entt::entity workshop, const gamesave::WorkshopRecord& record,
                    const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);

/// The scaffolds the workshop holds, in its record's order, the head of its list first, once they are made; a null
/// one is skipped, as the original's load skips it, and one the load did not make is left out
void SetScaffolds(entt::entity workshop, const gamesave::WorkshopRecord& record,
                  const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);

} // namespace openblack::ecs::load_workshops
