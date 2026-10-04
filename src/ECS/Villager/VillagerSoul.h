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

#include <entt/entity/entity.hpp>

namespace openblack
{
enum class MeshId : uint32_t;
}

// The soul of a dead villager, as runblack.exe W120 draws it (V12 spec §5.3; fn_00828790 / fn_00828900 / fn_00828950 /
// fn_00828990, read with bwdis.py): a 12-byte record {next, LH3DObject*, elapsed ms} at the head of the list 0xEB9A7C,
// made by Villager::Dead 0x76A74B on the first DEAD turn out of the water: a translucent white copy of the villager
// playing its "goto heaven / hell" clip (the clip moves it), alpha 105, fading out in its last 500 ms. In openblack an
// entity with a Transform, Mesh, SkeletalAnimation (the clip), Alpha (the translucent pass), ObjectColour (white) and
// components::VillagerSoul; no Villager, so nothing else treats it as one.

namespace openblack::ecs::components
{
struct VillagerSoul
{
	uint32_t elapsedMs {0};  ///< record +8
	uint32_t durationMs {0}; ///< the clip's duration (anim +0x20, read each frame by fn_00828990)
};
} // namespace openblack::ecs::components

namespace openblack::ecs::villager_soul
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// fn_00828790 0x828821..0x8288EA: Random(0, 100) (the CRT stream, drawn always) < 50 (fcomp; test ah, 1) -> heaven,
/// else hell; the source's current clip name (vt +0x184) "M_P_DEAD1" (0xC38664) -> the pair (247 P_DEAD2_GOTO_HEAVEN,
/// 248 P_DEAD2_GOTO_HELL) (statics 0xEB9A84 / 0xEB9A8C), else (244 P_DEAD1_GOTO_HEAVEN, 245 P_DEAD1_GOTO_HELL) (0xEB9A80 /
/// 0xEB9A88), set by Villager::Dead 0x76A677..0x76A6DA (literal: the pair looks inverted, V12 spec Q-3); heavenForced (a
/// child) -> the heaven clip after the roll
[[nodiscard]] int32_t SoulClip(bool sourceIsDead1Name, bool heavenForced, float roll);
/// fn_00828990 0x8289D2..0x828A0D: 105 (0x69); in the last 500 ms (elapsed > duration - 500, signed) ftol((1 - (elapsed -
/// (duration - 500)) x 0.002) x 105)
[[nodiscard]] uint8_t SoulAlpha(uint32_t elapsed, uint32_t duration);
/// fn_00828990 0x8289AD..0x8289B5: elapsed + 110 > duration (signed jle) -> the soul is freed
[[nodiscard]] bool SoulExpired(uint32_t elapsed, uint32_t duration);

// ---- the souls ---------------------------------------------------------------------------------------------------

/// fn_00828790(source obj3d, mesh, heavenForced): the soul at the source's position and orientation (+0x38, +0x44,
/// +0x48), mesh = MeshPack[mesh], its clip (SoulClip); returns the clip (-1 when nothing was made)
int32_t Create(entt::entity source, MeshId mesh, bool heavenForced);
/// fn_00828950 -> fn_00828990 per soul (from fn_005E5CD0 0x5E6171, once per frame: g_game_time_inc ms): the time, the
/// alpha, the deletion. (approximate) openblack calls it once per game turn with the turn's 100 ms (LivingActionSystem);
/// the clip's time runs per frame with the animations in between. Exact: a frame hook in Game.cpp (pending, Motor)
void Update(uint32_t milliseconds);
} // namespace openblack::ecs::villager_soul
