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

/// Field::Draw 0x528570: shown only with growth >= 0.25 x ageGrowth and food >= 25; sinks with its food over 1 s and
/// fades out below 20 % (PileSink / Alpha); sown again at once when empty (no farmers yet)
void UpdateFields(float seconds);

} // namespace openblack::ecs
