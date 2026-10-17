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

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// SpellSeedGraphic: the seed's mesh floating over a spell icon or inside a one-shot orb, its holder effect
// (GSpellSeedInfo.holderParticle) and its power-up band.

namespace openblack::worship::seed_graphic
{
/// The mesh (GSpellSeedInfo.mesh) at pos.y + meshHeight x scale (-1.5), the holder effect at pos.y +
/// holderHeight x scale, and the band when pu != -1. worldPosition: the point it floats at.
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seed, PlayerNames player, float scale, int powerUp);

/// The matrix the glints on a new graphic read (SpellSeedGraphic::glintModel): its mesh's matrix as it is made, with no
/// angle and the graphic's scale on the diagonal, without the seed info's scale. A scale of 1 gives the identity at the
/// mesh position; any other is put on the zeroed matrix's diagonal, the position added to its zeroed translation
[[nodiscard]] glm::mat4 CreationGlintModel(const glm::vec3& meshPosition, float scale);
/// The matrix the glints read after DrawSpellGraphic has drawn the mesh: at the mesh position, turned by the spin, at
/// size (the seed info's scale x the graphic's), as an object is placed. The holder effect steps before the draw, so its
/// glints read the draw before
[[nodiscard]] glm::mat4 DrawnGlintModel(const glm::vec3& meshPosition, float spin, float size);

/// Removes the graphic with its 3D objects and effect
void Delete(entt::entity graphic);

/// The power-up level (the band object appears for a level and stays when the level goes back to -1;
/// DrawSpellGraphic draws it pu + 1 times, none at -1)
void SetPowerUpType(entt::entity graphic, int powerUp);
/// Auto-updated graphics step their holder effect every turn (ProcessTurn)
void SetAutoUpdate(entt::entity graphic, bool autoUpdate);
/// The power-up band's size factor (0.5 on a worship icon)
void SetBandScale(entt::entity graphic, float bandScale);

/// Called by a one-shot orb every drawn frame (point: the orb's drawn matrix applied to its mesh box centre, scale =
/// orb scale x 0.6): the mesh at point + meshHeight x scale, the effect at + holderHeight x scale, and the holder
/// effect moved there, magnitude = scale, stepped by milliseconds
void DrawUpdateAtPos(entt::entity graphic, const glm::vec3& point, float scale, float milliseconds);
/// The holder effect stepped by milliseconds
void UpdateOnly(entt::entity graphic, float milliseconds);
/// Player seeds: the mesh turning about y at 2 rad/s, at GSpellSeedInfo.scale x scale, drawn with the owner's alpha
/// (0xFF opaque; the orb gives 0x95); the bands
void DrawSpellGraphic(entt::entity graphic, uint8_t alpha, float milliseconds);
/// For every worship-site and town-centre icon: UpdateOnly + DrawSpellGraphic with alpha 0xFF, every frame
void UpdateIconGraphics(float milliseconds);

/// Every turn, with the spells: the auto-updated graphics step their holder effect; every 30 turns the FLYING_FLOCK
/// mesh is redone for the player's alignment
void ProcessTurn();

/// How many band levels a graphic shows at a power-up: pu + 1, none at -1 (each level is drawn twice)
[[nodiscard]] int BandLevels(int powerUp);
/// A band's alpha in a graphic drawn at `alpha`: (60 x alpha) >> 8, so 34 in a one-shot orb (149) and 59 on an icon
[[nodiscard]] uint8_t BandAlpha(uint8_t alpha);
/// The band's matrix as rows (world = sum local_i row_i): identity, rows 1 and 2 swapped with the old row 1 negated,
/// then turned in (x, z) by base + the band angle, in (x, y) by 0.3, in (x, z) by k and in (x, y) by 0.2; base, k = 0,
/// -1 for the first band level, 0.5, 1 for the others
[[nodiscard]] glm::mat3 BandRotation(float bandAngle, size_t index);

/// The particle systems' global phase (+ ms x 0.001 / 3.33, 0..1), per frame
void UpdatePhase(float milliseconds);
[[nodiscard]] float Phase();
} // namespace openblack::worship::seed_graphic
