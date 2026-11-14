/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownCreatureAttitude.h"

#include <algorithm>
#include <array>
#include <optional>

#include "Creature/CreatureHelp.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Town;
using openblack::ecs::components::TownCreatureRecord;

namespace
{
const TownCreatureRecord* Find(std::span<const TownCreatureRecord> records, entt::entity creature)
{
	const auto found = std::ranges::find(records, creature, &TownCreatureRecord::creature);
	return found != records.end() ? &*found : nullptr;
}

/// How far from the town's own place another town would be judging the creature in its stead
constexpr float k_JudgingRadius = 10.0f;
/// How long, in whole seconds, each attitude lasts before it falls back to none: a new record, none, an attitude
/// nothing gives, fear and respect. Any other lasts no time
constexpr std::array<float, 5> k_AttitudeSeconds {15.0f, 0.0f, 10.0f, 30.0f, 10.0f};

/// The new attitude the town takes from the creature's held plan, when that plan can be carried out as it stands
std::optional<uint32_t> Judge(const Registry& registry, entt::entity creature)
{
	using creature_desires::Desire;
	const auto* mind = registry.TryGet<const components::CreatureMindState>(creature);
	if (mind == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return std::nullopt;
	}
	auto& minds = Locator::creatureMindSystem::value();
	const auto* tables = minds.GetTables();
	const auto holds = minds.HeldPlanOf(creature);
	if (!holds.has_value())
	{
		return std::nullopt;
	}
	const auto& plan = *holds;
	if (tables == nullptr ||
	    !creature_planner::IsValid(plan, tables->rules.at(static_cast<size_t>(plan.desire)).searchesForGoal,
	                               creature_mind_tables::NeedsInstrument(plan.action)))
	{
		return std::nullopt;
	}
	// what the plan is done to: the creature itself when it names nothing
	const auto object = plan.object.has_value() ? static_cast<entt::entity>(*plan.object) : creature;
	const bool villager = registry.Valid(object) && registry.AllOf<components::Villager>(object);
	std::optional<entt::entity> held;
	if (Locator::creatureObjectActionSystem::has_value())
	{
		held = Locator::creatureObjectActionSystem::value().GetHeld(creature);
	}
	return town_creature_attitude::NewAttitude({
	    .family = creature_mind_tables::TownAttitudeFamilyOf(mind->lastCompletedAction),
	    .actsOnVillager = villager,
	    .hungryHoldingIt = plan.desire == Desire::Hunger && villager && held == object,
	});
}

Town* TownOf(entt::entity town)
{
	if (!Locator::entitiesRegistry::has_value() || town == entt::null)
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}
} // namespace

void town_creature_attitude::CreateRecord(std::vector<TownCreatureRecord>& records, entt::entity creature, uint32_t turn)
{
	if (Find(records, creature) != nullptr)
	{
		return;
	}
	records.insert(records.begin(), {.creature = creature, .attitude = k_New, .setTurn = turn, .resetTurn = turn});
}

uint32_t town_creature_attitude::AttitudeOf(std::span<const TownCreatureRecord> records, entt::entity creature)
{
	const auto* record = Find(records, creature);
	return record != nullptr ? record->attitude : k_New;
}

std::string_view town_creature_attitude::Name(uint32_t attitude)
{
	switch (attitude)
	{
	case k_New:
		return "nothing yet";
	case k_None:
		return "nothing much";
	case k_Fear:
		return "fear";
	case k_Respect:
		return "respect";
	default:
		return "an attitude nothing gives";
	}
}

uint32_t town_creature_attitude::NewAttitude(const Judgement& judgement)
{
	using creature_mind_tables::TownAttitudeFamily;
	if (judgement.hungryHoldingIt)
	{
		return k_Fear;
	}
	switch (judgement.family)
	{
	case TownAttitudeFamily::Fear:
		return k_Fear;
	case TownAttitudeFamily::Respect:
		return k_Respect;
	case TownAttitudeFamily::Eating:
		return judgement.actsOnVillager ? k_Fear : k_None;
	case TownAttitudeFamily::None:
		break;
	}
	return k_None;
}

bool town_creature_attitude::Store(TownCreatureRecord& record, uint32_t attitude, uint32_t turn)
{
	const bool changed = attitude != record.attitude;
	if (changed || attitude == k_Fear)
	{
		record.attitude = attitude;
		record.setTurn = turn;
	}
	return changed;
}

void town_creature_attitude::TimeOut(TownCreatureRecord& record, uint32_t turn, uint32_t msPerTurn)
{
	constexpr uint32_t k_MsPerSecond = 1000;
	const uint32_t turnsPerSecond = msPerTurn != 0 ? k_MsPerSecond / msPerTurn : 0;
	// (openblack) a turn longer than a second, which the original divides by zero on, never times out
	if (record.attitude == k_None || turnsPerSecond == 0)
	{
		return;
	}
	const uint32_t seconds = (turn - record.setTurn) / turnsPerSecond;
	const float lasts = record.attitude < k_AttitudeSeconds.size() ? k_AttitudeSeconds.at(record.attitude) : 0.0f;
	if (static_cast<float>(seconds) > lasts)
	{
		record.attitude = k_None;
		record.resetTurn = turn;
	}
}

void town_creature_attitude::UpdateAttitudes(entt::entity town)
{
	if (TownOf(town) == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto turn = game_clock::Turn();
	const auto place = object::MapCoordsOf(town);
	// the town is fetched again at each use: telling the creature may change the registry
	const auto records = [&registry, town]() -> std::vector<TownCreatureRecord>& {
		return registry.Get<Town>(town).creatureRecords;
	};
	for (size_t i = 0; i < records().size();)
	{
		const auto creature = records().at(i).creature;
		if (!IsAvailable(creature))
		{
			records().erase(records().begin() + static_cast<std::ptrdiff_t>(i));
			continue;
		}
		if (map_cells::GetNearestTown(place, k_JudgingRadius) == town)
		{
			if (const auto attitude = Judge(registry, creature); attitude.has_value() && *attitude != k_None)
			{
				if (Store(records().at(i), *attitude, turn) && Locator::creatureHelpSystem::has_value())
				{
					Locator::creatureHelpSystem::value().Raise(
					    creature, {.kind = creature_help::Kind::TownAttitudeToCreature, .parameter = *attitude});
				}
			}
		}
		TimeOut(records().at(i), turn, game_clock::MsPerTurn());
		++i;
	}
}

void town_creature_attitude::CreateRecord(entt::entity town, entt::entity creature)
{
	if (auto* t = TownOf(town); t != nullptr)
	{
		CreateRecord(t->creatureRecords, creature, game_clock::Turn());
	}
}

uint32_t town_creature_attitude::AttitudeOf(entt::entity town, entt::entity creature)
{
	const auto* t = TownOf(town);
	return t != nullptr ? AttitudeOf(t->creatureRecords, creature) : k_New;
}
