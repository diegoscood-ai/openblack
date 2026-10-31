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

#include <array>
#include <functional>
#include <optional>
#include <string_view>

#include <glm/vec3.hpp>

/// The poo desire's second action, the fart. It is carried out with the poo's own agenda, and only the moment the poo
/// would drop tells them apart: a creature whose plan is the fart lets a cloud of puffs out behind it instead of the
/// lump, and its poo, its poo desire and its count of poos stay as they were; an evil creature's fart makes those near
/// it faint. Docs: docs/wiki/creature/plans-and-needs.md#the-poo-desires-fart.
namespace openblack::creature_fart
{
/// The action's name in the game's table
inline constexpr std::string_view k_Action = "Fart";
/// How many puffs a fart lets out, and their colour: grey, a quarter opaque
inline constexpr size_t k_Puffs = 15;
inline constexpr uint32_t k_PuffColour = 0x40808080;
/// For the size the body is drawn at: the size of each puff, and how far behind the body the cloud is blown, which is
/// also the width of the random spread about it
inline constexpr float k_PuffSizePerSize = 4.0f;
inline constexpr float k_CloudPerSize = 15.0f;
/// A body whose alignment is below this makes those near it faint as it farts
inline constexpr float k_FaintingBelow = -0.5f;

/// Draws a number from 0 up to a bound, as the game's synchronised random numbers do
using FloatRandom = std::function<float(float)>;

/// Whether the moment of the poo is a fart: the plan carried out now is the fart, by its row of the game's table
[[nodiscard]] bool Farts(std::optional<uint32_t> current, std::optional<uint32_t> fart);

/// Whether a fart makes those near the creature faint, by the alignment its body shows: below k_FaintingBelow, or no
/// number at all, as the game's comparison lets it through
[[nodiscard]] bool MakesFaint(float alignment);

/// The size of each puff for the size the body is drawn at
[[nodiscard]] float PuffSize(float drawnSize);

/// The velocities the puffs fly out at, in order, from where the creature stands. `yaw` is the body's own turn about
/// the vertical. Each one is the way behind the body times the cloud's size, plus a random spread of that size on each
/// axis about the middle: three draws a puff, the first for z, then y, then x. Rounded step by step as the game's
/// floating point does.
[[nodiscard]] std::array<glm::vec3, k_Puffs> PuffVelocities(float yaw, float drawnSize, const FloatRandom& random);

} // namespace openblack::creature_fart
