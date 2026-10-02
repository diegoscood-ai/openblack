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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// FireGraphic (a PSysBase, 0xD0 bytes, vtable 0x9997D4; fn_00731160 create, fn_00731560 update, fn_00732200 draw): the
// burning object's flames (S_Fire.raw, mode 13), the steam of a hot object being cooled and the smoke of a fire that
// went out (S_SpriteSheet3.raw, modes 13 and 6). It is updated with the frame time when drawn (FireEffect::Draw
// 0x730330) and Z-sorted as one object. Also the tint the fire gives the burning object (tree colour fn_0074B3A0,
// charring fn_00730570, glow GetFireEffectCharingColor 0x730480).

namespace openblack::ecs::fire
{
struct FireEffect;

namespace graphic
{
/// fn_00731160 (CreateSprites 0x730AD0): only for an object with a 3D object
void Create(FireEffect& fire);
/// ~FireGraphic 0x7313C0
void Destroy(FireEffect& fire);
/// fn_00731560 for every fire, `seconds` of frame time (g_game_time_inc x 0.001)
void Update(float seconds);
/// The turn hook of the fire graphics: OPENBLACK_FIRE_TRACE (the bursts read game_clock::Turn(), g_game +0x205A40)
void SetTurn(uint32_t turn);
void Clear();

/// Tree::Draw's fire part fn_0074B3A0: the colour a burning tree is drawn with (x/256 per channel of its colour):
/// life > 0.9 ? max(50, 255 - (1 - life) 2550) : 50. nullopt when the object has no fire.
[[nodiscard]] std::optional<glm::u8vec3> TreeDrawColour(entt::entity object);
/// fn_00730570: the charring grey 255 - int(charring x 255) x 175 / 256 (80 when fully charred)
[[nodiscard]] uint8_t CharringGrey(const FireEffect& fire);
/// GetFireEffectCharingColor 0x730480: the glow, (1 - charring) x clamp(0.001 T (1 + 0.2 noise)) x 255 as RGB(c 180 /
/// 256, c 60 / 256, 0)
[[nodiscard]] glm::u8vec3 CharringGlow(const FireEffect& fire, float turnTime);
} // namespace graphic
} // namespace openblack::ecs::fire
