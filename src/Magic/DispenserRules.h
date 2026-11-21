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
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The miracle dispensers' timing. A dispenser makes a one-shot miracle, a bubble with the miracle's seed inside,
// floating above itself. While the bubble is there it waits; once the bubble is taken it counts the turns, and after
// its period it makes another. Pure functions of a dispenser's timer, and of the tables for the building a new one is.

namespace openblack
{
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

namespace openblack::magic
{

/// A dispenser's bubble floats above it at this share of its height
inline constexpr float k_OrbHeightShare = 1.2f;
/// How near two objects' edges count as touching
inline constexpr float k_TouchTolerance = 0.001f;
/// The seed inside a bubble is drawn at this share of its size, and spins this many radians a second
inline constexpr float k_OrbSeedScale = 0.6f;
inline constexpr float k_OrbSeedSpin = 2.0f;
/// A bubble is added over what is behind it at this share of its colour
inline constexpr float k_OrbShare = 149.0f / 255.0f;

/// Every miracle a dispenser can give the player, in the tables' order. The creature spells that do nothing in the
/// game are left out.
[[nodiscard]] std::span<const MagicType> DispensableMiracles();

struct DispenserTimer
{
	/// Turns counted since the last bubble was taken
	uint32_t tick {0};
	/// Turns between a bubble being taken and the next, 0 for a dispenser that makes none
	uint32_t period {0};
	bool active {false};
};

/// What a dispenser does this turn
enum class DispenserStep : uint8_t
{
	/// Nothing: its bubble is there, it is not active, it has no miracle, its building is not built and repaired, or its
	/// period has not passed
	Wait,
	/// Its bubble has been taken: the count starts again from 0, on the next turn
	OrbTaken,
	/// It makes a new bubble. The count is left as it is: making the bubble starts it again, and a bubble that could
	/// not be made is asked for again on the next turn
	MakeOrb,
};

/// One turn of a dispenser: whether it has a bubble, whether that bubble is still where it made it, whether it has a
/// miracle to give, and whether its building is built and repaired
[[nodiscard]] DispenserStep StepDispenser(DispenserTimer& timer, bool hasOrb, bool orbStillThere, bool hasMagic,
                                          bool builtAndRepaired);

/// The turn that points a model's up along a direction, its other axes kept level where they can be
[[nodiscard]] glm::mat3 FaceTowards(const glm::vec3& direction);

/// Where a dispenser floats its bubble: above it by its height times the share
[[nodiscard]] glm::vec3 OrbPosition(const glm::vec3& base, float height);
/// Whether a bubble still touches its dispenser: the distance between them across the land less both their radii, no
/// more than the touch tolerance
[[nodiscard]] bool OrbTouches(float distance, float dispenserRadius, float orbRadius);

/// The buildings that are miracle dispensers, in the tables' order
[[nodiscard]] std::vector<AbodeInfo> DispenserAbodes(const InfoConstants& info);
/// The dispenser building a new one is when nothing names one: the Norse one the first land gives, else the first
/// there is
[[nodiscard]] std::optional<AbodeInfo> DefaultDispenserAbode(std::span<const AbodeInfo> abodes);

} // namespace openblack::magic
