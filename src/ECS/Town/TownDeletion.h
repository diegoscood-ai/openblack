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
#include <cstdint>

#include <entt/entity/entity.hpp>

/// A town's deletion: when its last building leaves it with no plan and nobody living there, and when a script deletes
/// it. docs/wiki/world/buildings.md (a town's deletion)
namespace openblack::ecs::town_deletion
{

/// Whether a town that a building has just left goes: no building and no plan left in it, and nobody, adult or child,
/// living in it
[[nodiscard]] bool EmptiedForGood(size_t buildings, size_t plans, uint32_t people);

/// The town's own part of its deletion, before it is marked: it leaves its player (its magic, its worship site and its
/// temple's building sites go with it), its homeless go off as vagrants, its buildings leave it, its animals, flocks,
/// missionaries and records are let go, its building sites, need flags and rewards are deleted, it leaves the game's
/// list of towns, and the influence is drawn afresh. Nothing for a town already going
void ToBeDeleted(entt::entity town);

} // namespace openblack::ecs::town_deletion
