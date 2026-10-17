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

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The water miracle (MAGIC_TYPE 22 WATER and 23 WATER_PU1). Its only override is the turn: one drop per game turn at a
// random point around the cast position, which gets the default effect (burn -4000: it cools fires) and the
// ApplyWaterSpell of every object within reach (fields are sown and grow, trees grow or seed a sapling, burning objects
// start the "putting out the fire" reaction), and a ring on the land every 0.1 s. The cloud and the rain cone are its
// PSys (SF_Water / SF_WaterPU1). Wiki: docs/wiki/magic/food-wood-water-heal.md#water.

namespace openblack::magic::water
{
/// The water spell's own state (both cleared at allocation)
struct SpellWaterData
{
	float lastRipple {0.0f};             ///< The age of the last ring
	uint32_t puttingOutFireReaction {0}; ///< REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (34), 0 none
};

/// The rain radius, 6 m for WATER, 12 m for WATER_PU1, 1 for anything else
[[nodiscard]] float RainRadius(MagicType type);
/// The ring growth, 2 for WATER, 4 for WATER_PU1, 1 for anything else
[[nodiscard]] float RippleGrowth(MagicType type);
/// A ring every 0.1 seconds of spell age
constexpr float k_RippleEvery = 0.1f;
/// The ring colours, picked with GameRand(5)
constexpr std::array<uint32_t, 5> k_RippleColours = {0xFF80CBC5u, 0xFF8599C5u, 0xFFBA97B2u, 0xFFB9CA86u, 0xFFBD9C8Au};
/// The spell's power (a spell keeps the default 1.0): the drop's reach is 2.5 x this
constexpr float k_SpellPower = 1.0f;
/// The reach factor
constexpr float k_ReachFactor = 2.5f;

/// The drop's distance from the cast position, GameFloatRand(R) x 0.7 + 0.3 (0.3 .. 0.7 R + 0.3;
/// it is not R x (0.7 rand + 0.3)); `random` is the GameFloatRand(R) draw, 0 .. R
[[nodiscard]] constexpr float DropDistance(float random)
{
	return random * 0.7f + 0.3f;
}
/// The drop's reach, 2.5 x the spell's power
constexpr float k_Reach = k_SpellPower * k_ReachFactor;
/// ApplyWaterSpell when the reach > distance(object, drop) - object radius (strictly)
[[nodiscard]] constexpr bool InReach(float distance, float objectRadius, float reach = k_Reach)
{
	return reach > distance - objectRadius;
}
/// A ring when k_RippleEvery < age - lastRipple, strictly, in single precision (the age grows by
/// 0.1 a turn, so with float rounding a ring comes on most turns but not all of them)
[[nodiscard]] bool RippleDue(float age, float lastRipple);

/// What the drop does to one object: any object, a tree or a field (no other kind differs). Returns the original's
/// float result (any object 0, a tree 1, a field the any-object part's).
float ApplyWaterSpell(entt::entity object, entt::entity spell);

/// A drop of the spell's rain lands at a point: every object but the spell, in the 3 x 3 cells from the point's, whose
/// edge is within `reach` of it gets ApplyWaterSpell, then, with a growth, the drop leaves a ring on the land. Returns
/// the objects watered, in the order they were (an object in several of the cells once for each), and traces them
std::vector<entt::entity> LandDrop(entt::entity spell, const glm::vec3& drop, float reach, std::optional<float> ringGrowth);

/// The spell's state, nullptr when it is not a water spell
[[nodiscard]] const SpellWaterData* DataOf(entt::entity spell);
} // namespace openblack::magic::water
