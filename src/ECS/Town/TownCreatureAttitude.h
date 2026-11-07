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

#include <span>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Creature/CreatureMindTables.h"
#include "ECS/Components/Town.h"

// A town's attitude to each creature its people have met: a record made the first time one of its villagers reacts to
// the creature, which says whether the town fears it, respects it or neither. Wiki:
// docs/wiki/creature/mimicry.md#town-step-16.

namespace openblack::ecs::town_creature_attitude
{

/// A record just made, before the town has judged the creature
constexpr uint32_t k_New = 0;
constexpr uint32_t k_None = 1;
constexpr uint32_t k_Fear = 3;
constexpr uint32_t k_Respect = 4;

/// A record of the creature at the front, the newest first, with the attitude of a new one and both turns `turn`;
/// nothing when the creature has one already
void CreateRecord(std::vector<components::TownCreatureRecord>& records, entt::entity creature, uint32_t turn);
/// The attitude of the first record of the creature, the newest first; k_New when there is none
[[nodiscard]] uint32_t AttitudeOf(std::span<const components::TownCreatureRecord> records, entt::entity creature);

/// An attitude's name, for the debug readouts
[[nodiscard]] std::string_view Name(uint32_t attitude);

/// What the town makes of the creature as it judges it: the family of the last action it carried out to its end,
/// whether its plan is done to a villager, and whether it is hungry and holds that villager
struct Judgement
{
	creature_mind_tables::TownAttitudeFamily family {creature_mind_tables::TownAttitudeFamily::None};
	bool actsOnVillager {false};
	bool hungryHoldingIt {false};
};
/// The attitude a judgement gives: fear when hungry and holding the villager it acts on, whatever it did; else none,
/// fear or respect by the family, the eating actions fear when done to a villager and none otherwise
[[nodiscard]] uint32_t NewAttitude(const Judgement& judgement);
/// The record takes a new attitude other than none: when it changes, and every turn fear is seen again, with the turn;
/// true when it changed, which the creature is told
[[nodiscard]] bool Store(components::TownCreatureRecord& record, uint32_t attitude, uint32_t turn);
/// An attitude other than none falls back to none once more whole seconds have gone since it was set than it lasts
/// (15 for a new record, 30 for fear, 10 for respect), counted in turns of `msPerTurn`
void TimeOut(components::TownCreatureRecord& record, uint32_t turn, uint32_t msPerTurn);

/// Each turn of the town: the records of creatures being deleted go; the town judges each other creature by its held
/// plan when that plan can be carried out (a town in the same place as an earlier one leaves it to that one), tells the
/// creature when its attitude changes, and lets each attitude time out
void UpdateAttitudes(entt::entity town);

/// The town's record of the creature, made now (nothing without a town)
void CreateRecord(entt::entity town, entt::entity creature);
/// The town's attitude to the creature; k_New without a town or a record
[[nodiscard]] uint32_t AttitudeOf(entt::entity town, entt::entity creature);

} // namespace openblack::ecs::town_creature_attitude
