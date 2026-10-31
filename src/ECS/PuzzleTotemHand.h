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

#include "ECS/Systems/VillageTotemSystemInterface.h"

/// The hand's side of a puzzle totem (components::PuzzleTotem): the hand takes hold of it through a locked select,
/// drags it up and down in whole steps of its highest and lets it go. The hand's TOTEM state is the one it holds a
/// village totem in. See docs/wiki/magic/puzzle-totem.md#the-hand-and-the-tap
namespace openblack::ecs::puzzle_totem_hand
{

/// Whether the thing is a puzzle totem
[[nodiscard]] bool IsPuzzleTotem(entt::entity thing);
/// Whether the hand may take hold of it: it works (available, built, its life above the point it stops working at),
/// whoever's hand it is
[[nodiscard]] bool ValidForLockedSelect(entt::entity totem);
/// Whether the hand holds it
[[nodiscard]] bool IsHeld(entt::entity totem);
/// The locked select starts on the pressing player's machine: the drag starts from the altitude it stands at
void StartLockedSelect(entt::entity totem);
/// The start and end packets: the hand holds it, or no longer does (it then stands as high as its height says)
void SetHeld(entt::entity totem, bool held);
/// The mouse's move up the screen since the last drawn frame, in pixels, against the screen's height, slides the
/// totem the hand holds: it is drawn at the altitude the hand holds it at, and each new step is sent
void Slide(entt::entity totem, float upPixels, float screenHeight);
/// Where the hand holding it is, as the village totem's hold: over its place, as high as the hand grips it
[[nodiscard]] std::optional<ecs::systems::VillageTotemSystemInterface::HandHold> GetHandHold(entt::entity totem);
/// The locked select ends on the pressing player's machine: the altitude the hand holds it at is sent once more (a
/// tap-only totem only takes its tap-only height)
void EndLockedSelect(entt::entity totem);
/// The altitude packet: it is put at the altitude, and its height becomes the share of its highest that altitude
/// stands at, in whole steps
void ApplyPosition(entt::entity totem, float altitude);

} // namespace openblack::ecs::puzzle_totem_hand
