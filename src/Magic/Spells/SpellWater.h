/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// SpellWater (0xF4 bytes, vtable 0x8F553C, GMagicWaterInfo::AllocSpell 0x5FAC70): the water miracle (MAGIC_TYPE 22
// WATER and 23 WATER_PU1). Its only override is Process 0x724ED0: one drop per game turn at a random point around the
// cast position, which gets the default effect (burn -4000: it cools fires) and the ApplyWaterSpell (vt 0x67C) of every
// object within reach (fields are sown and grow, trees grow or seed a sapling, burning objects start the "putting out
// the fire" reaction), and a ring on the land every 0.1 s. The cloud and the rain cone are its PSys (SF_Water /
// SF_WaterPU1). Wiki: docs/bw1-notes/magic.md, "Agua".

namespace openblack::magic::water
{
/// SpellWater +0xEC / +0xF0 (fn_00724EC0 clears both at allocation)
struct SpellWaterData
{
	float lastRipple {0.0f};              ///< +0xEC the age of the last ring
	uint32_t puttingOutFireReaction {0}; ///< +0xF0 REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (34), 0 none
};

/// fn_005FACE0: the rain radius, 6 m for WATER, 12 m for WATER_PU1, 1 for anything else
[[nodiscard]] float RainRadius(MagicType type);
/// fn_005FACC0: the ring growth, 2 for WATER, 4 for WATER_PU1, 1 for anything else
[[nodiscard]] float RippleGrowth(MagicType type);
/// GMagicWaterInfo::GetRippleEvery 0x5FAD00: 0.1 (seconds of spell age)
constexpr float k_RippleEvery = 0.1f;
/// SpellWater::Process 0x7251C5..0x7251E5: the ring colours, picked with GameRand(5)
constexpr std::array<uint32_t, 5> k_RippleColours = {0xFF80CBC5u, 0xFF8599C5u, 0xFFBA97B2u, 0xFFB9CA86u, 0xFFBD9C8Au};
/// GameThingWithPos::GetPower 0x56FE60 (fld 1.0), not overridden by Spell: the drop's reach is 2.5 x this
constexpr float k_SpellPower = 1.0f;
/// 0x72510E: the reach factor (fmul 2.5, 0x8C581C)
constexpr float k_ReachFactor = 2.5f;

/// 0x724F6A..0x724F7A: the drop's distance from the cast position, GameFloatRand(R) x 0.7 + 0.3 (0.3 .. 0.7 R + 0.3;
/// it is not R x (0.7 rand + 0.3)); `random` is the GameFloatRand(R) draw, 0 .. R
[[nodiscard]] constexpr float DropDistance(float random) { return random * 0.7f + 0.3f; }
/// 0x725108..0x72511D: ApplyWaterSpell when 2.5 x GetPower > distance(object, drop) - object radius (strictly)
[[nodiscard]] constexpr bool InReach(float distance, float objectRadius)
{
	return k_SpellPower * k_ReachFactor > distance - objectRadius;
}
/// 0x72517F..0x7251A7: a ring when GetRippleEvery < age - lastRipple, strictly, in single precision (the age grows by
/// 0.1 a turn, so with float rounding a ring comes on most turns but not all of them)
[[nodiscard]] bool RippleDue(float age, float lastRipple);

/// Object::ApplyWaterSpell vt 0x67C and its overrides, the part the drop does to one object: Object 0x63A8E0, Tree
/// 0x74C390, Field 0x528F30 (the symbol list has no other). Returns the original's float result (Object 0, Tree 1,
/// Field the Object's).
float ApplyWaterSpell(entt::entity object, entt::entity spell);

/// The spell's state, nullptr when it is not a SpellWater
[[nodiscard]] const SpellWaterData* DataOf(entt::entity spell);
} // namespace openblack::magic::water
