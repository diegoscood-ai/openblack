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

#include "Enums.h"
#include "Hand/HandPickUpRules.h"

/// What every god's hand asks of a thing before it goes into the hand: the player's hand and a computer player's hand
/// make the same refusals, each for its own player. Wiki: docs/wiki/hand/grab-hold-and-throw.md#what-the-hand-can-take
namespace openblack::ecs::hand_place_in_hand
{

/// The thing can be acted on by a hand: it is there and not being deleted, and a building only once it is started
[[nodiscard]] bool Interactable(entt::entity object);

/// The seed a thing out of the hand gives a hand of the player: a spell seed itself, a teleport stone its spell's seed,
/// each when the seed's own check lets that player's hand take it; entt::null otherwise
[[nodiscard]] entt::entity SeedToPlaceInHand(entt::entity object, PlayerNames handPlayer);

/// Whether a thing may be placed in a hand of the player, by its class: an allowlist (hand_grab::PlaceInHandRuleOf), so
/// buildings, totems, features, creatures and everything not named never go in. Only a seed and a fireball depend on
/// the hand's player
[[nodiscard]] bool ValidForPlaceInHand(entt::entity object, PlayerNames handPlayer);

/// Whether the thing passes one of the place-in-hand's checks for a hand of the player; the hand answers whether it
/// is empty itself
[[nodiscard]] bool Passes(entt::entity object, hand_pick_up::PlaceInHandCheck check, bool handEmpty, PlayerNames handPlayer);

} // namespace openblack::ecs::hand_place_in_hand
