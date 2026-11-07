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

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

// Which hand let go of a thing last: each player's interface keeps the last thing its hand threw or dropped, on the
// player's entity (components::LastDropped). A player with no entity keeps nothing
namespace openblack::ecs::last_dropped
{

/// A hand threw the object (a creature's throw does not count): the player's interface keeps it as the last thing it let
/// go of, whatever another interface kept
void MarkThrown(Registry& registry, PlayerNames player, entt::entity object);

/// A hand let go of the object: the interface found to have let go of it last (WhoLastDropped) forgets it, then the
/// player's interface keeps it in place of the last thing it let go of
void RecordDrop(Registry& registry, PlayerNames player, entt::entity object);

/// The player whose interface keeps the object as the last thing it let go of; when more than one does, the first in
/// player order. None for no object, and the neutral player has no interface
[[nodiscard]] std::optional<PlayerNames> WhoLastDropped(const Registry& registry, entt::entity object);

} // namespace openblack::ecs::last_dropped
