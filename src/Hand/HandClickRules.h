/*******************************************************************************
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

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/HandClicked.h"

/// The rules of the things and places the player clicks with the Action button, which the scripts ask about
/// ("OBJECT clicked", "POSITION clicked radius R").
///
/// A thing is clicked when the hand taps it, except a reward while the leash is on and tied to nothing, and when the
/// button is let go over it while the hand is doing nothing. Let go over the land instead, the place under the hand is
/// clicked and the thing is forgotten.
///
/// Pure functions, tested on their own; the hand's system and the script natives apply them.
namespace openblack::hand_click
{

/// A click is forgotten once this long has gone by in game time
inline constexpr float k_ForgetSeconds = 15.0f;

/// Whether a click made on a turn is forgotten by now: more than 15 seconds of game turns have gone by. The turns gone
/// by are counted unsigned, so a click from a later turn (the turns start again on a new land) is long forgotten.
/// Single precision, as the game measures it.
[[nodiscard]] bool IsForgotten(uint32_t turn, uint32_t clickedTurn, uint32_t millisecondsPerTurn);

/// Each frame: a place and a thing clicked too long ago are forgotten, each on its own
void Forget(ecs::components::HandClicked& clicked, uint32_t turn, uint32_t millisecondsPerTurn);

/// Whether a tap on a thing marks it: never a reward while the leash is on and tied to nothing
[[nodiscard]] bool MarksThing(bool holdsLooseLeash, bool isReward);

/// The thing is clicked now
void ClickThing(ecs::components::HandClicked& clicked, entt::entity thing, uint32_t turn);

/// The place is clicked now, and the thing clicked before is forgotten
void ClickPlace(ecs::components::HandClicked& clicked, const map_coords::MapCoords& place, uint32_t turn);

/// The Action button is let go while the hand does nothing: the thing under the hand is clicked when it marks, and
/// otherwise the place under it, or the map's corner when the hand points at no land
void ClickReleased(ecs::components::HandClicked& clicked, std::optional<entt::entity> under, bool marks,
                   const std::optional<map_coords::MapCoords>& place, uint32_t turn);

/// Whether this thing is the one last clicked
[[nodiscard]] bool IsThingClicked(const ecs::components::HandClicked& clicked, entt::entity thing);

/// Whether the place last clicked is within a radius in metres of a position, measured along the ground as the game
/// measures map positions. A forgotten place is the map's corner, which still counts.
[[nodiscard]] bool IsPlaceClicked(const ecs::components::HandClicked& clicked, const map_coords::MapCoords& position,
                                  float radius);

/// A script forgets the thing clicked
void ClearThing(ecs::components::HandClicked& clicked);
/// A script forgets the place clicked
void ClearPlace(ecs::components::HandClicked& clicked);

} // namespace openblack::hand_click
