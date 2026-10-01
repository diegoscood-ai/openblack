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

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/// The temporary marks on the ground (fn_00825240, 0x0C bytes: +0 the next one, +4 the LH3DObject, +8 the life in ms;
/// list 0xEB9A00, the newest first): a morphable LH3DObject (Create(1) 0x825277) of the MeshPack mesh 0x251
/// (MeshId::TreeRootsPile; the first call keeps it in [0xEB9A04]) that melts into the land once (UpdateMelting
/// 0x8252C3, land_morph), lasts 15000 ms (0x3A98) and fades over its last second. Two makers: an uprooted tree's crater
/// (fn_008251F0 from fn_0074BD20, HandTrees.cpp) and an explosion on dry land (fn_008251C0 from UR_Explosion::InitCollection
/// 0x67E395). Wiki: rendering-objects.md, "Mallas pegadas al suelo".
namespace openblack::ecs::ground_marks
{

constexpr int32_t k_LifeMs = 15000;        ///< +8 (0x8252DD)
constexpr int32_t k_FadeMs = 1000;         ///< 0x82537C (0x3E8)
constexpr float k_FadeAlphaPerMs = 0.255f; ///< [0x9A2BA8]
constexpr float k_ExplosionScale = 8.0f;   ///< [0x9357D4]

/// fn_00825240 with LH3DObject::SetPosition 0x423140 (vt+0x20: the position, a turn about Y and a uniform scale) done by
/// the caller's rotation, and the SmokyStuff::Create(pos, 1, 1.0, 0xFFFFFFFF) at 0x8252EB (mode 1, 0x823DA7). The Mac
/// symbols name the class RootsPile (__ct__9RootsPileFRC7LHPointffl, DrawAll__9RootsPileFv). entt::null without the mesh.
entt::entity Create(const glm::vec3& position, const glm::mat3& rotation, float scale);

/// fn_008251C0 (UR_Explosion::InitCollection 0x67E37A..0x67E395): angle = PSysFloatRand(2 pi), scale 8; SetPosition's
/// turn is the rows (c, 0, s), (0, 1, 0), (-s, 0, c) (0x4231B3..0x42321F)
entt::entity CreateExplosionMark(const glm::vec3& position, float angle);

/// fn_00825350 (from fn_005E5CD0 0x5E6197, once a frame): a mark with life <= 1000 ms takes alpha ftol(life x 0.255)
/// (+0x4C's top byte, then vt+0x48 with 1); life -= g_game_time_inc ([0xEA9EC0], ms); at 0 or less it goes
/// (fn_00825300). The marks whose entity went with the map are dropped.
void Update(float gameMilliseconds);

/// ClearAllStuff 0x82AED0 (from GGame::ClearMap 0x552F22): the list is emptied with the map
void Clear();

} // namespace openblack::ecs::ground_marks
