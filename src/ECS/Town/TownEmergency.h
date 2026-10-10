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

#include "Enums.h"

// The town's emergency: a storage pit or a town centre knocked below its functional threshold (abodes::ReduceLife),
// one of them on fire or the town attacked while in the emergency (ProcessTownEmergency) start it; the housed
// villagers are called at once to congregate in the town (242) and the worship stops for its 1200 turns (the town
// info's emergency duration). town_queries::IsInStateOfEmergency tells whether it is running. Fields:
// components::Town::emergencyStartTurn, aggression.lastTurn, savedWorshipPercentage; the worship percentage through
// worship::percentage.
// The attack refresh reads the town's aggression record (ECS/TownAggression.h), written by UpdateAggressor: a
// destructive effect on one of its things, its things burning, a villager of it hurt by a physics impact and a
// physical shield of it struck.

namespace openblack::ecs::town_emergency
{
/// Each abode of the town, each inhabitant: when its final state's row allows it ->
/// SetTopState(242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY). The per-villager part is
/// villager::CallToTownEmergency. Only housed villagers are called
void CallAllVillagersToTownEmergency(entt::entity town);
/// No emergency running -> CallAllVillagersToTownEmergency; then the start = the turn (also when it was set: the start
/// is refreshed, the villagers are not called again)
void SetInStateOfEmergency(entt::entity town);
/// TownProcess step 15: while in the emergency the attack of this turn refreshes it and a worship percentage != 0 is
/// saved and set to 0; else the storage pit or the town centre on fire starts it; else the saved percentage comes back
/// (when the current one is 0) and the saved percentage and the start are cleared
void ProcessTownEmergency(entt::entity town);
/// The town is attacked by the caused player (`causedPlayer`; the neutral player without one) with `aggression`: the
/// town's record takes it (town_aggression::Attacked, with the town info's first-time addition, as the town's own
/// player's harm when the aggressor owns it) and keeps the aggressor and the turn, which ProcessTownEmergency's
/// refresh while attacked reads. (not ported) the town attack guidance sound and the help spirit's remark about a
/// creature's attack
void UpdateAggressor(entt::entity town, std::optional<PlayerNames> causedPlayer, float aggression);
/// An object of a town is harmed by `damage`: its town (town_queries::GetTown; none: nothing) is attacked with the
/// damage times the aggressor value of the object's info (0 when openblack has no info for its kind)
void AttackTown(entt::entity object, float damage, std::optional<PlayerNames> causedPlayer);
/// The player a thing that hit something answers for itself, for an attack that no hand threw: a villager its town's
/// owner (none without a town), a creature its owner, a standing tree none, and every other thing that flies in the
/// physics (rocks, fragments, felled trees, mobile objects and statics, animals, pots, seeds) the neutral player
[[nodiscard]] std::optional<PlayerNames> HitterPlayer(entt::entity hitter);
} // namespace openblack::ecs::town_emergency
