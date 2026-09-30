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
#include <glm/vec2.hpp>

// The miracle's look on the player's hand: PHandFX (PSysHandFX.cpp, CHand +0x4948: the glowing S_Hand_Flow second pass
// and the Power_Up_Band rings) and the in-hand particle effect of the seed (CHand fn_0046E7B0 / DrawSpellInHand 0x46E680,
// CHand +0x494C). Wiki: docs/bw1-notes/magic.md, "La mano".

namespace openblack::magic::hand_fx
{
/// vt 0 RemoveAllPermBands 0x68D060
void RemoveAllPermBands();
/// vt 4 DoRemoveFromHandVisual 0x68CE90 (every scribble cancel): G_ShakeHand_01 and one band flying off
void DoRemoveFromHandVisual();
/// vt 8 AddSpellToHandVisuals 0x68DE20: five temporary bands 0.1 s apart (2.4 s later if delayed) and
/// G_SpellPowerUpBand
void AddSpellToHandVisuals(bool delayed);
/// vt 0xC SetPULevel 0x68DDA0: permanent bands added or removed until there are `level` (at most 5)
void SetPULevel(int level, bool delayed);
/// vt 0x14
[[nodiscard]] int GetPULevel();
/// vt 0x18 / 0x1C / 0x20: the tribal power column (PowerSpinRunner) of a tribe whose tribal power is above 1. No
/// tribe gains any in the vanilla game (GetTribalPowerTribe is always -1): not drawn.
void StartTribalPowerRing(int tribe);
void StopTribalPowerRing();
void ReleaseOrCreateTribalPowerRing();

/// PHandFX::DrawHandFX 0x68DD60 -> Draw 0x68D0C0, every frame (seconds = g_game_time_inc x 0.001, 0 while paused)
void Update(float seconds);

/// The hand's second pass with the flowing texture (for the renderer): alpha 0 = not drawn
struct Glow
{
	float alpha {0.0f};        ///< +0x54 x 255 (0.8 while a miracle is in the hand)
	glm::vec2 uvOffset {0.0f}; ///< the 8 x 4 atlas cell of the frame
};
[[nodiscard]] Glow GetGlow();

/// CHand fn_0046E7B0: the seed's in-hand effect (GMagicInfo.particleTypeInHand of its level), replacing the old one
void CreateInHandEffect(entt::entity seed);
/// CHand fn_0046E890
void ReleaseInHandEffect();
/// CHand::DrawSpellInHand 0x46E680, every frame: to the hand (or its bone), strength = the seed's PSys power, magnitude
/// = the hand's scale, a step of max(1, g_game_time_inc) ms; drawn only once the seed is ready
void UpdateInHandEffect(float milliseconds);

/// A land is loaded: the bands and the effect go
void Reset();
} // namespace openblack::magic::hand_fx
