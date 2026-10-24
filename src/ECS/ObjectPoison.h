/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

// A thing's poison as the game asks and sets it on any thing, the scripts' poison commands among others: each kind of
// thing keeps it its own way (docs/wiki/scripts/script-commands.md#poison).

namespace openblack::ecs::object_poison
{
/// Whether the thing is poisoned: a villager, an animal or a creature its poisoned bit (ecs::life); a pot or a pile its
/// own flag, and a storage pit when its food pile or one of its wood piles is there and poisoned
/// (object_resources::IsPoisoned); a toadstool always; anything else never
[[nodiscard]] bool IsPoisoned(entt::entity thing);

/// The thing poisoned or cured: a villager, an animal or a creature its poisoned bit; a pot or a pile its own flag; a
/// storage pit its food pile, when it has one there, which this can only poison: a cure leaves a poisoned food pile
/// poisoned, and the wood piles are never touched. Anything else stays as it is
void SetPoisoned(entt::entity thing, bool poisoned);
} // namespace openblack::ecs::object_poison
