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

#include "ECS/Components/PlayerAlignment.h"

// GAlignment (Alignment.cpp 0x414410..0x4146AD): how an applied effect moves the caster's alignment.

namespace openblack::ecs::effects
{
struct EffectValues;

namespace alignment
{
/// fn_00414660: a change v scaled by the current alignment A (+8): v >= 0 -> v (1 + |A| / 2), v < 0 -> v (1 - |A| / 2)
[[nodiscard]] float ScaleChange(const components::PlayerAlignment& alignment, float change);

/// GAlignment::Update 0x414410 (object, values, life before): nothing unless the life changed. With
/// K = |life0 - life| + GPlayerInfo.applyEffectAlignmentChangeAddition (player 0's +0x64 -> +0x1C) and col = the
/// object's info alignmentType, `pending` gets ScaleChange(values[i] x GAlignmentInfo[i][col] x K) for crush, hit, heal
/// and fly away, and ScaleChange(ConvertTemperatureToDamage(burn) x GAlignmentInfo[0][col] x K). The alignment history
/// (CAlignmentHistory 0xC4CD40, fn_00414E10) is not kept.
void Update(components::PlayerAlignment& alignment, entt::entity object, const EffectValues& values, float lifeBefore);
} // namespace alignment
} // namespace openblack::ecs::effects
