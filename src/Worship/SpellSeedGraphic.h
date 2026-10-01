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
#include <glm/vec3.hpp>

#include "Enums.h"

// SpellSeedGraphic (SpellIcon.cpp 0x726E70..0x727700): the seed's mesh floating over a spell icon or inside a one-shot
// orb, its holder effect (GSpellSeedInfo.holderParticle) and its power-up band.

namespace openblack::worship::seed_graphic
{
/// SpellSeedGraphic::Create 0x726F60 (pos, seed, player, scale, pu) -> CreateGraphic 0x726F00 -> fn_00727190: the mesh
/// (GSpellSeedInfo.mesh) at pos.y + unknown0x150 x scale (-1.5, fn_007270E0), the holder effect at pos.y +
/// unknown0x154 x scale, and the band when pu != -1. worldPosition: the point it floats at.
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seed, PlayerNames player, float scale, int powerUp);

/// SpellSeedGraphic::ToBeDeleted 0x726FE0: out of the list, its 3D objects and effect go
void Delete(entt::entity graphic);

/// SpellSeedGraphic::SetPowerUpType 0x727060: the level (the band object appears for a level and stays when the level
/// goes back to -1; DrawSpellGraphic draws it pu + 1 times, none at -1)
void SetPowerUpType(entt::entity graphic, int powerUp);
/// SpellSeedGraphic::SetAutoUpdate 0x727680
void SetAutoUpdate(entt::entity graphic, bool autoUpdate);
/// +0x58: the power-up band's size factor (0.5 on a worship icon, WorshipSpellIcon::UpdateGraphicsWithPULevels 0x77F320)
void SetBandScale(entt::entity graphic, float bandScale);

/// SpellSeedGraphic::DrawUpdateAtPos 0x727630 (OneOffSpellSeed::Draw 0x518E90 every drawn frame, with
/// GetSpellGraphicPos 0x72A840: the orb's drawn matrix applied to its mesh box centre, scale = orb scale x 0.6):
/// +0x54 = scale, fn_007270E0 (the mesh at point + unknown0x150 x scale, the effect at + unknown0x154 x scale), and
/// fn_007274D0: the holder PSys moved there, magnitude = scale, stepped by milliseconds
void DrawUpdateAtPos(entt::entity graphic, const glm::vec3& point, float scale, float milliseconds);
/// SpellSeedGraphic::UpdateOnly 0x727590: the holder PSys stepped by milliseconds
void UpdateOnly(entt::entity graphic, float milliseconds);
/// SpellSeedGraphic::DrawSpellGraphic 0x519AD0 (player seeds): the mesh turning about y at 2 rad/s, at
/// GSpellSeedInfo.scale x scale, drawn with the owner's alpha (0xFF opaque; the orb gives 0x95); the bands
void DrawSpellGraphic(entt::entity graphic, uint8_t alpha, float milliseconds);
/// SpellIcon::DrawSpellSeedGraphic 0x726D30 for every worship-site and town-centre icon: UpdateOnly + DrawSpellGraphic
/// with alpha 0xFF, every frame
void UpdateIconGraphics(float milliseconds);

/// fn_00727350 (Spell::ProcessSpells, every turn): the auto-updated graphics step their holder effect (fn_007273A0);
/// every 30 turns fn_00727440 redoes the FLYING_FLOCK mesh for the player's alignment
void ProcessTurn();

/// The PSys global phase [0xD4EBF8] (PSysGlobal::DrawLoop 0x68F680: + ms x 0.001 / 3.33, 0..1), per frame
void UpdatePhase(float milliseconds);
[[nodiscard]] float Phase();
} // namespace openblack::worship::seed_graphic
