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

#include <bitset>
#include <optional>

#include <glm/vec3.hpp>

#include "Particles/PlasmaCommand.h"

namespace openblack
{
enum class PlayerNames : uint8_t;
} // namespace openblack

/// Swapping creatures: a script can move the player's creature's mind into another creature's body, and that creature's
/// mind into the player's old one. What it has learnt, how it feels, its physique and its tattoos go with the mind; each
/// body keeps its name and species. The new body becomes the player's creature, and over the swap each body grows or
/// shrinks to the other's size and takes on the other's alignment, so those go with the mind too, little by little.
/// The Creature Breeder offers the five bonus creatures, which are only there once they have been unlocked.
namespace openblack::creature_swap
{

/// The bonus creatures the breeder can offer, in the order of the unlocks
enum class Bonus : uint8_t
{
	Leopard,
	Horse,
	Mandrill,
	Gorilla,
	Rhino,

	_Count
};
inline constexpr size_t k_BonusCount = static_cast<size_t>(Bonus::_Count);
using Unlocked = std::bitset<k_BonusCount>;
/// The bonus creatures unlocked in the game openblack reproduces: all five, whatever the game's settings hold
inline constexpr Unlocked k_GameUnlocks {(1ULL << k_BonusCount) - 1};

/// The bonus creature a script's species number is, none for any other species
[[nodiscard]] std::optional<Bonus> BonusOf(int32_t species);

/// Whether a species can be offered, as scripts ask: a bonus creature only once unlocked, any other species always
[[nodiscard]] bool IsAvailable(int32_t species, const Unlocked& unlocked);

/// A body's size and alignment, which it takes from the other body over a swap
struct Shape
{
	float size {1.0f};
	float alignment {0.0f};
};

/// Each turn a body closes this share of the gap to the shape it takes on
inline constexpr float k_EaseShare = 0.04f;
/// It has taken on the shape once both its size and its alignment are nearer than this: a hundredth as a float, widened,
/// as the original compares the gaps
inline constexpr double k_Settled = static_cast<double>(0.01f);

/// Whether the body has taken on the shape: both its size and its alignment within the settled distance of it
[[nodiscard]] bool IsSettled(const Shape& now, const Shape& target);

/// One turn of a body taking on the shape: a share of each gap closed, the alignment kept from -1 to 1
[[nodiscard]] Shape Ease(const Shape& now, const Shape& target);

/// The sightings of a miracle a mind keeps as it moves into a body of another species, so that it is as far towards
/// learning it as before: scaled by the times the new species must see it over the times the old one must, rounded
/// down. Kept as they are when the old species never needs to see it.
[[nodiscard]] uint32_t RescaledSightings(uint32_t seen, float neededBefore, float neededAfter);

/// Whether a creature of this owner has a player: any of the eight, the neutral one included. A creature made for no
/// player (PlayerNames::_COUNT, as a script's CREATE makes one, and the player's old body once a swap gives the player
/// the other) has none
[[nodiscard]] bool HasPlayer(PlayerNames owner);

/// While a body eases towards its new shape, its swap beam fires a plasma at the other creature every this many game
/// turns, from a random bone of each
inline constexpr uint32_t k_PlasmaEveryTurns = 3;
/// Its alpha: the least, and how many values above it a draw can add
inline constexpr uint32_t k_PlasmaLeastAlpha = 40;
inline constexpr uint32_t k_PlasmaAlphaDraws = 35;
/// Its life in seconds and its speed
inline constexpr float k_PlasmaLife = 1.5f;
inline constexpr float k_PlasmaSpeed = 1.0f;
/// Two bones nearer than the root of this give the plasma no line between them, so it leaves and arrives straight up
inline constexpr float k_PlasmaNearSquared = 1.0e-4f;

/// Whether the game turn is one on which an easing body's beam fires its plasma
[[nodiscard]] bool IsPlasmaTurn(uint32_t gameTurn);

/// The plasma an easing body's beam fires: from its bone to the other creature's, leaving and arriving along the line
/// between them (straight up when they are too near), normalised by the 3D engine's inverse square root, at the alpha
/// of the draw (0 to k_PlasmaAlphaDraws - 1)
[[nodiscard]] particles::PlasmaCommand SwapPlasma(glm::vec3 ownBone, glm::vec3 otherBone, uint32_t alphaDraw);

/// The player whose colour a creature's swap beam takes: its owner, the neutral player included, or the local player
/// for a creature of no player
[[nodiscard]] PlayerNames BeamPlayer(PlayerNames owner, PlayerNames localPlayer);

} // namespace openblack::creature_swap
