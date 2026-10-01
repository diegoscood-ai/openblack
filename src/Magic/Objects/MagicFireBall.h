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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// MagicFireBall (PSysFireball.cpp 0x682970..0x683390): the invisible Object each ball of a fireball spell carries. It is
// made by the PSys rule AttatchFireBallToAtom (PSys/Rules/Fireball.cpp) and follows its atom; its FireEffect (at
// initialTemperature x the effect's strength) does the burning. The enemy's can be caught; a held fire seed absorbs one.
// Wiki: docs/bw1-notes/miracles.md, "Bola de fuego".

namespace openblack::magic::fireball
{
/// MagicFireBall::Create 0x682970 (pos, &GMagicFireBallInfo[row], atom): the object (not in the map cells), then
/// SetTemperature(strength x initialTemperature, the spell's creator) and +0x58 = !IsScriptCasting
entt::entity Create(const glm::vec3& position, int infoRow, uint32_t effect, uint32_t atomKey, bool hasPlayer,
                    PlayerNames player, bool scriptCast, entt::entity source);

/// AttatchFireBallToAtom's update of its object: the atom's position (x, z; the height above the land), SetScale(atom
/// scale x rule scale); true when the ball has cooled below deletionTemperature (the atom is then deflected)
bool FollowAtom(entt::entity fireball, const glm::vec3& position, float scale, uint32_t turn);

/// The effect's strength (mgr +0x54) now: GetHeatCapacity reads it live (1 without the effect)
[[nodiscard]] float Strength(entt::entity fireball);

/// MagicFireBall::ToBeDeleted 0x682C30: off the fireball list (g_game +0x205C9C), its fire goes
void ToBeDeleted(entt::entity fireball);

/// MagicFireBall::ValidForPlaceInHand 0x682DD0: 1 unless it is the catcher's own (the status player == its player)
[[nodiscard]] bool ValidForPlaceInHand(entt::entity fireball, PlayerNames player);
/// fn_00682EA0 (InterfaceTap 0x682E50 / InterfaceSetInMagicHand 0x682E80): with the hand free and valid, a new FIRE seed
/// at the ball (fn_00728300, seed 2, pu -1, multiplier 1) into the hand, ready (fn_00729900(0)) and fully charged; the
/// ball goes. The seed, or entt::null.
entt::entity Catch(entt::entity fireball, PlayerNames player);
/// MagicFireBall::DeleteAndPutIntoSpellSeed 0x682E10 (SpellSeed::ApplyToMagicFireBall 0x728AE0, a held FIRE seed): the
/// seed's power (+0x8C) x (1 + catchIncreaseFactor), and the ball goes
void DeleteAndPutIntoSpellSeed(entt::entity fireball, entt::entity seed);

/// Every fireball, newest first
[[nodiscard]] const std::vector<entt::entity>& All();
/// Once per turn right after the spells stepped their PSys: a ball its atom did not refresh (the atom, or the whole
/// effect, is gone) goes with it (the atom data's dtor 0x682FA0)
void ProcessTurn(uint32_t turn);
/// A land is loaded
void Clear();
} // namespace openblack::magic::fireball
