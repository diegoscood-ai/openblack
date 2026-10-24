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

namespace openblack::ecs::components
{
struct PuzzleTotem;
}

/// The rules of a puzzle totem (components::PuzzleTotem): its height in whole steps, the share of its highest it
/// stands at, and how far into the ground the rest of its travel leaves it. Free of the registry: the archetype places
/// the entity from them. See docs/wiki/magic/puzzle-totem.md
namespace openblack::ecs::puzzle_totem
{

/// The highest step a new puzzle totem has
constexpr int32_t k_DefaultMaxHeight = 100;
/// The highest step of a totem that is only tapped (a totem puzzle game's): it then always stands at all its height
constexpr int32_t k_TapOnly = -1;
/// The part of its own height a totem keeps out of the ground at its lowest
constexpr float k_KeptAboveGround = 0.35f;
/// The reads that blend a totem puzzle game's move from the height before
constexpr int32_t k_BlendReads = 5;
/// The blend's weight per read
constexpr float k_BlendWeight = 0.2f;

/// The float-to-whole-number conversion of the original's arithmetic: towards zero; a NaN or a value out of range
/// gives the lowest whole number
[[nodiscard]] int32_t Truncate(float value);

/// How far the totem travels between its lowest and its highest: its whole height less the part it keeps above the
/// ground, from the mesh's half height and its scale
[[nodiscard]] float Travel(float meshHalfHeight, float scale);

/// The share of its highest the totem stands at, 0 to 1 (1 for a tap-only totem). A totem puzzle game's move blends
/// in over its next reads, and each read counts one off the blend
[[nodiscard]] float Share(components::PuzzleTotem& totem);
/// The share a read gives, without counting the read off the blend
[[nodiscard]] float ShareAsRead(const components::PuzzleTotem& totem);
/// Sets its height from a share: that part of its highest, in whole steps, at most its highest (a tap-only totem
/// keeps its height at k_TapOnly)
void SetShare(components::PuzzleTotem& totem, float share);

/// The script's HEIGHT: its height as a number
[[nodiscard]] float HeightProperty(const components::PuzzleTotem& totem);
/// The script's HEIGHT set: the value in whole steps, towards zero; its highest when the value is not below it (or is
/// no number). There is no lowest
void SetHeightProperty(components::PuzzleTotem& totem, float value);
/// The script's MAX_HEIGHT
[[nodiscard]] float MaxHeightProperty(const components::PuzzleTotem& totem);
/// The script's MAX_HEIGHT set: the value in whole steps, towards zero. The height it stands at is left as it is,
/// even above the new highest
void SetMaxHeightProperty(components::PuzzleTotem& totem, float value);

/// Its altitude above the land at a share: the rest altitude less the travel the share leaves sunk
[[nodiscard]] float AltitudeAt(float restAltitude, float share, float travel);

/// The hand's drag divides the hand's slide by this
constexpr float k_DragDivisor = 1.5f;

/// The hand takes hold of it at the altitude it stands at: the drag starts from there
void StartDrag(components::PuzzleTotem& totem);
/// The drag's offset after the hand slides, between minus the travel and 0
[[nodiscard]] float DragOffset(float offset, float slide, float travel);
/// The altitude the hand holds it at for a drag offset: the whole step of its highest the offset leaves, then the
/// altitude of that step
[[nodiscard]] float DragAltitude(float offset, float restAltitude, float travel, int32_t maxHeight);
/// The hand slides while it holds the totem: the offset and the hand's altitude move, and the altitude to send when it
/// differs from the one last sent. A tap-only totem only takes k_TapOnly as its height
[[nodiscard]] std::optional<float> Drag(components::PuzzleTotem& totem, float slide, float travel);
/// The share of its highest an altitude stands at, between 0 and 1: the part of the travel it is not sunk by
[[nodiscard]] float ShareFromAltitude(float altitude, float restAltitude, float travel);

} // namespace openblack::ecs::puzzle_totem
