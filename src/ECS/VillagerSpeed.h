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

namespace openblack
{
struct GVillagerInfo;
}

namespace openblack::ecs
{

/// A villager's walking speed and size like the original (docs/bw1-notes/animation.md; research
/// dev\tmp_dis\anim\speed_exact.md).

/// The info.dat entry of the villager (its tribe and villager number)
const GVillagerInfo* VillagerInfoOf(entt::entity villager);

/// Villager::SetStateSpeed (0x753760) + SetSpeed (0x750ED0): on every top state change, from the speed group entry of
/// the final state (info.dat villagerStateTable speed index), the wounded and town terms, then the per-villager factor
/// (creation index), age, life and sex. Sets WallHug::speed (metres per turn). Nothing changes for a villager controlled
/// by a script (+0x25 & 4, 0x753766) or dancing (Living::IsDancing 0x5ECC10, 0x753772; (aproximado) as TOP == IN_DANCE).
void SetVillagerStateSpeed(entt::entity villager);

/// Villager::SetAge (0x7528C0) scale part: InitialiseScale (0x74FB80) + SetScaleForAge (0x752A90). Adults end in
/// (0.95, 1.05]; children take ageToScale[age - 1] plus a random part of the way to ageToScale[age + 1].
float VillagerScaleForAge(const GVillagerInfo& info, uint32_t age);

} // namespace openblack::ecs
