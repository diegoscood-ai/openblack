/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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

/// SpellSeedGraphic::SetPowerUpType 0x727060: the level (the band appears for a level; it stays, as in the original,
/// when the level goes back to -1)
void SetPowerUpType(entt::entity graphic, int powerUp);
/// SpellSeedGraphic::SetAutoUpdate 0x727680
void SetAutoUpdate(entt::entity graphic, bool autoUpdate);
/// +0x58: the alpha the graphic is drawn with
void SetAlpha(entt::entity graphic, float alpha);

/// fn_00727350 (Spell::ProcessSpells, every turn): the auto-updated graphics step their holder effect (fn_007273A0).
/// The others are stepped when drawn (SpellSeedGraphic::UpdateOnly 0x727590, DrawSpellGraphic 0x519AD0); openblack's
/// effect manager steps every holder effect once per turn.
void ProcessTurn();

/// The PSys global phase [0xD4EBF8] (PSysGlobal::DrawLoop 0x68F680: + ms x 0.001 / 3.33, 0..1), per frame
void UpdatePhase(float milliseconds);
[[nodiscard]] float Phase();
} // namespace openblack::worship::seed_graphic
