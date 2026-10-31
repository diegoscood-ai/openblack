/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{
struct TotemWorship;
}

// How many of a town's villagers worship: the player drags the town's totem statue (a locked select, sent as a
// network packet) to set the percentage; the town sends or calls back villagers to match it. See
// docs/wiki/magic/worship.md#the-worship-percentage-and-the-villagers.

namespace openblack::worship::percentage
{
/// 0 without a worship site; else stored, also on the totem statue (and as the share the hand holds it at), and
/// GetWorshipersNeeded(1, 1) villagers sent (AdjustWorshipersWorshipping(need, 1, 0))
void SetWorshipPercentage(entt::entity town, float percentage);
[[nodiscard]] float GetWorshipPercentage(entt::entity town);
/// A worship disciple has set off: while the town still wants more nothing changes; else, with people, its share
/// becomes (needed + 1.4 + on the way + worshipping) / people, at most 1 (worshipper_rules::ShareAfterDisciple), so
/// that the share keeps the disciple at worship
void IncrementWorshipPercentage(entt::entity town);

/// target = pct > 0 ? max(1, int(pop x pct + 0.5))
/// : 0; result = target - (worshipping + on the way) + the go-home requests; *out = result > 0 && current >= target
[[nodiscard]] int GetWorshipersNeeded(entt::entity town, bool countOnWay, bool countGoHome, bool* out);

/// Two passes (the second also takes the villagers flagged 0x200); n > 0: the available villagers, the highest
/// WorshipScore (the nearest) first, go (CheckWorshipActivity, with life above damageThresholdToGoHome unless
/// skipLifeCheck); n < 0: those at or on the way to the site, the lowest score (the farthest) first, are sent back
/// (state 163)
void AdjustWorshipersWorshipping(entt::entity town, int count, bool skipLifeCheck, bool requireReachable);

/// The town's villagers on the way to the site, and its count of villagers at the site
void AddVillagerOnWay(entt::entity town, entt::entity villager);
void RemoveVillagerOnWay(entt::entity town, entt::entity villager);
void AddWorshipper(entt::entity town);
void RemoveWorshipper(entt::entity town);

/// The order AdjustWorshipersWorshipping takes the villagers in: gutils::GetDistanceModifier (the villager's distance
/// to the site's centre (the site's local point (12.55, 0, -26.1)), the centre's distance to the town + 100) x life^3.
/// The modifier falls off with the distance, so the nearest villagers score highest and go first.
[[nodiscard]] float WorshipScore(entt::entity villager);

/// The statue's rise sent on to a new percentage: its Zoomer eases there from where it stands, in the distance from
/// the percentage it was last set to x village_totem::k_EaseMsPerShare milliseconds (put there at once below the
/// Zoomer's shortest move), and it is rising
void SetRise(ecs::components::TotemWorship& worship, float percentage);
/// The rise moved on by some milliseconds, the Zoomer's step with its terms summed in the order of the statue's draw
void StepRise(ecs::components::TotemWorship& worship, float milliseconds);
/// Whether the rising statue has just arrived (within village_totem::k_ArrivedWithin of its percentage): it is then
/// no longer rising, so this answers once per rise
[[nodiscard]] bool TakeRiseArrival(ecs::components::TotemWorship& worship);

/// Per frame, the logic part of the statue's draw: the rise advanced by the engine clock, the plinth at 8 x the
/// smoothed percentage and the icon standing on it; while the hand holds it, at 8 x the share it holds it at
void UpdateTotems();
/// From the town centre's process: the rise advanced by the engine clock; arrived (within 0.005) while rising: the
/// rising loop goes and the stop sound plays. `statue` is the plinth's entity
void ProcessTotem(entt::entity statue);

/// The totem a picked plinth or icon belongs to: the plinth itself, or the plinth the icon stands on
[[nodiscard]] std::optional<entt::entity> TotemOf(entt::entity picked);

/// The statue's own percentage set, as a statue with no town takes it: its rise eases there. `statue` is the plinth's
/// entity
void SetStatuePercentage(entt::entity statue, float percentage);
/// The percentage the statue was last set to; 0 when it never was
[[nodiscard]] float GetStatuePercentage(entt::entity statue);
} // namespace openblack::worship::percentage
