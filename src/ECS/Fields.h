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

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{

/// Field::Process 0x529020, once per game turn: every 10th turn (plus the field's offset) a fully sown field grows
/// until it is ripe, and its food with it
void ProcessFieldsTurn(uint32_t turn);

/// Field::RemoveFood 0x5295A0: what the field gives for `amount` (its quirks: unripe costs 1.2 x; a ripe field
/// that runs out is cleared and must be sown again)
uint32_t RemoveFieldFood(entt::entity field, float amount);

/// Field::IsUnripe 0x5298D0 (misnamed in the symbols: true when ripe, growth >= ageRecolt)
[[nodiscard]] bool IsFieldRipe(entt::entity field);

/// Field::ApplyWaterSpell 0x528F30, the field's own part (the water miracle's drop, Magic/Spells/SpellWater.cpp, calls
/// it after the Object part, only when the field is not on fire): crops <= timesToSow -> crops = ftol(timesToSow + 1)
/// = 31, sown at once; else, while growth <= ageRecolt, growth += effectOfWaterSpell (info.dat 2.0) and food +=
/// effectOfWaterSpell x totalFoodInField / ageRecolt. Returns false when it is not a field.
bool ApplyWaterSpellToField(entt::entity field);

/// Field::Draw 0x528570: shown only with growth >= 0.25 x ageGrowth and food >= 25; sinks with its food over 1 s and
/// fades out below 20 % (PileSink / Alpha); sown again at once when empty (no farmers yet)
void UpdateFields(float seconds);

namespace components
{
struct Field;
}

/// Field::Draw 0x528570: the object colour the field's land light is multiplied by (fn_0080BF10), from
/// BlendColor 0x5284C0: growing, olive (full food) to light green; ripening, olive to white; ripe, white
[[nodiscard]] glm::u8vec3 FieldDrawColour(const components::Field& field);

/// The wind lean of the 16 sway slots (Tree::PreDraw 0x74A7C0: T0 = -0.03 cos(phase), the wind angle being always 0,
/// so the lean is along world z only); ripe fields shear their up axis by 1.75 x scale x this, trees by 1 x
[[nodiscard]] float WindSway(uint32_t slot);

} // namespace openblack::ecs
