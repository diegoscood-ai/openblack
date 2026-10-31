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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

// The buildings' "needs" signs (4 show needs info rows): a workshop's, and the three over a worship site's altar
// (docs/wiki/magic/worship.md#the-altars-need-signs)

namespace openblack::ecs::show_needs
{
/// The altar's rows, in the order its signs are made, drawn and deleted: how much its dancers want food, rest, and how
/// many of its people hide
inline constexpr uint8_t k_HungerInfo = 0;
inline constexpr uint8_t k_LifeInfo = 1;
inline constexpr uint8_t k_HidingInfo = 2;
inline constexpr uint8_t k_AltarSigns = 3;
/// The workshop's row
inline constexpr uint8_t k_WorkshopInfo = 3;

/// The special point a sign stands at, on the mesh of the building it is drawn with: a workshop's sign at its
/// workshop's point 9 whatever its row, an altar's sign at its worship site's point of the sign's own row (0, 1, 2)
[[nodiscard]] int NeedsPoint(bool altar, uint8_t infoIndex);

/// A new needs sign at `at` for `owner` with that info row: an object (the next creation index) at `at`, its Zoomer
/// and desire 0, then the info's mesh, scale 1. Without a Mesh (not drawn) until its frame draws it. `at` is the owner's
/// world position: the owner's map coordinates (x, z, altitude) are copied as they are, so the sign stands where the
/// owner does and nothing asks the ground
entt::entity Create(const glm::vec3& at, entt::entity owner, uint8_t infoIndex);
/// Sets the desire
void SetDesire(entt::entity visuals, float desire);
/// Deletes the sign (when its owner is deleted)
void Delete(entt::entity visuals, bool now);
/// The signs' part of a building's draw, as the object draw list runs it for a building it draws: a functional
/// workshop not on fire draws its sign, a built worship site its altar's three (on fire or not). Each one's Zoomer
/// steps by `frameSeconds` (the frame's game time in seconds), it turns to `eye` (the camera point) and it is marked
/// drawn this frame. A sign whose building is not drawn does not move
void DrawFor(entt::entity building, float frameSeconds, const glm::vec3& eye);
/// After the frame's object draw list, run or not: the signs drawn this frame have their Mesh, the others lose it
void EndDrawnFrame();
} // namespace openblack::ecs::show_needs
