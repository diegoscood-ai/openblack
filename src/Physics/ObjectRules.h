/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

/// The pure rules for rocks as the physics knocks them and the hand taps them (ecs::object_physics applies them), and
/// where the physics puts an object it lets go of
namespace openblack::physics::objects
{

/// Whether a rock can be broken by tapping it: only one taller than 0.7
[[nodiscard]] bool RockBreaksWhenTapped(float height);

/// Whether the hand can lift a rock: one no wider than 3.6 across the ground (its radius, its scale included)
[[nodiscard]] bool RockFitsInHand(float radius2D);

/// Whether a knock of `g` G wears a rock of `height`: harder than 4 G, on a rock taller than 0.7, and not from a thing
/// that answers it is a rock (a rock, the bonfire or a piece knocked off a building)
[[nodiscard]] bool KnockWearsRock(float g, float height, bool fromARock);
/// A worn rock's life after a knock of `g` G: a two-hundredth of a life for each G past 4
[[nodiscard]] float RockLifeAfterKnock(float life, float g);
/// Whether a worn rock splits in two: its life is under a hundredth
[[nodiscard]] bool RockSplits(float life);

/// A rock split in two makes two halves of this scale of its own, about the cube root of a half: each has half its
/// volume. They stand this far times its 2D radius either side of it
inline constexpr float k_RockHalfFactor = 0.7935f;

/// Where an object stands when the physics lets go of it, as the game keeps a position: x and z truncated to the
/// map's fixed-point coordinates, and the height kept as a height above the land there (the land's height plus the
/// point's height above it, each rounded to a float). Without a land the height is above 0, so it stays as it is
[[nodiscard]] glm::vec3 PlacedOnTheMap(const LandIslandInterface* land, glm::vec3 point);

/// A thing put down gently from a hand becomes an artefact of the nearest building or worship site within this many
/// metres
inline constexpr float k_ArtefactReach = 50.0f;

} // namespace openblack::physics::objects
