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

// How many of a town's villagers worship (Town.cpp 0x73C060..0x73E3F0, TotemStatue.cpp 0x738260..0x738960): the
// player drags the town's totem statue (the locked select 0x738500..0x7386A0, packet 0x29) to set the percentage; the
// town sends or calls back villagers to match it. Research: sources.md §4.1; wiki magic.md (M7).

namespace openblack::worship::percentage
{
/// Town::SetWorshipPercentage 0x73C060: 0 without a worship site; else stored, the totem statue's
/// (TotemStatue::SetWorshipPercentage 0x738270), and GetWorshipersNeeded(1, 1) villagers sent
/// (AdjustWorshipersWorshipping(need, 1, 0))
void SetWorshipPercentage(entt::entity town, float percentage);
[[nodiscard]] float GetWorshipPercentage(entt::entity town);

/// Town::GetWorshipersNeeded 0x73C860 (countOnWay, countGoHome, &out): target = pct > 0 ? max(1, int(pop x pct + 0.5))
/// : 0; result = target - (worshipping + on the way) + the go-home requests; *out = result > 0 && current >= target
[[nodiscard]] int GetWorshipersNeeded(entt::entity town, bool countOnWay, bool countGoHome, bool* out);

/// Town::AdjustWorshipersWorshipping 0x73C0F0 (n, skipLifeCheck, requireReachable): two passes (the second also takes the
/// villagers flagged 0x200); n > 0: the available villagers nearest the site's centre first go (CheckWorshipActivity,
/// with life above damageThresholdToGoHome unless skipLifeCheck); n < 0: those at or on the way to the site, the
/// farthest first, are sent back (state 163)
void AdjustWorshipersWorshipping(entt::entity town, int count, bool skipLifeCheck, bool requireReachable);

/// Town::AddVillagerOnWayToWorshipSite 0x73E300 / RemoveVillagerOnWayToWorshipSite 0x73E360, fn_0073E3E0 / fn_0073E3F0
/// (the town's count of villagers at the site)
void AddVillagerOnWay(entt::entity town, entt::entity villager);
void RemoveVillagerOnWay(entt::entity town, entt::entity villager);
void AddWorshipper(entt::entity town);
void RemoveWorshipper(entt::entity town);

/// fn_0073C590, the order AdjustWorshipersWorshipping takes the villagers in: GetDistanceModifier(the villager's
/// distance to the site's centre (CalculateCentrePos 0x77DD40, the site's local point (12.55, 0, -26.1)), the
/// centre's distance to the town + 100) x life^2. It grows with the distance: the farthest go first.
[[nodiscard]] float WorshipScore(entt::entity villager);
/// GUtils::SigmoidThreshold 0x74F170
[[nodiscard]] float SigmoidThreshold(float x, float threshold);

/// TotemStatue::Draw 0x738960 (per frame): the plinth rises 8 x the smoothed percentage and the icon stands on it
void UpdateTotems(float seconds);

/// The totem drag (TotemStatue::ValidForLockedSelectProcess 0x738500, NetworkUnfriendlyLockedSelect 0x7386A0:
/// pct = clamp(pct + dy x 0.1, 0, 1)), for the hand: the town of a totem statue it may drag (same player, the statue,
/// the player's citadel heart and the town's worship site built), or entt::null
[[nodiscard]] entt::entity TotemTown(entt::entity statue);
} // namespace openblack::worship::percentage
