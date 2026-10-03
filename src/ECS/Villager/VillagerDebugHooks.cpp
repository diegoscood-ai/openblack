/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager core's test hooks (docs/bw1-notes/villagers.md, section Ganchos de prueba): the trace and the
// OPENBLACK_TEST_VILLAGER_* environment variables. They are not part of the original.

#include <cstdlib>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Components/Town.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "Game.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// OPENBLACK_VILLAGER_TRACE: unset -> off; "1" -> all (-1); else the creation index of one villager
std::optional<int64_t> TraceTarget()
{
	static const std::optional<int64_t> k_Target = []() -> std::optional<int64_t> {
		const char* value = std::getenv("OPENBLACK_VILLAGER_TRACE");
		if (value == nullptr || *value == '\0')
		{
			return std::nullopt;
		}
		const std::string text(value);
		if (text == "1")
		{
			return -1;
		}
		return std::atoll(value);
	}();
	return k_Target;
}

/// "<a>[,<n>]": the value and the creation index it applies to (none: all)
struct ValueFor
{
	std::string value;
	std::optional<int64_t> villager;
};

std::optional<ValueFor> ParseValueFor(const char* name)
{
	const char* raw = std::getenv(name);
	if (raw == nullptr || *raw == '\0')
	{
		return std::nullopt;
	}
	const std::string text(raw);
	const auto comma = text.find(',');
	ValueFor result {text.substr(0, comma), std::nullopt};
	if (comma != std::string::npos)
	{
		result.villager = std::atoll(text.substr(comma + 1).c_str());
	}
	return result;
}

bool Applies(const ValueFor& value, entt::entity villager)
{
	return !value.villager || object_index::Of(villager) == *value.villager;
}

std::vector<entt::entity> Villagers()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Villager, const LivingAction>(
	    [&list](entt::entity entity, const Villager&, const LivingAction&) { list.push_back(entity); });
	return list;
}
} // namespace

bool TraceOn(entt::entity villager)
{
	const auto target = TraceTarget();
	if (!target)
	{
		return false;
	}
	return *target == -1 || object_index::Of(villager) == *target;
}

void Trace(entt::entity villager, const std::string& line)
{
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Villager trace: {} (turn {}): {}", object_index::Of(villager), CurrentTurn(), line);
	}
}

void RunDebugHooks(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	// OPENBLACK_TEST_VILLAGER_SHOT="<turn>,<path>[;<turn>,<path>...]": a screenshot at those game turns (one a turn)
	static std::vector<std::pair<uint32_t, std::string>> shots = [] {
		std::vector<std::pair<uint32_t, std::string>> list;
		const char* value = std::getenv("OPENBLACK_TEST_VILLAGER_SHOT");
		std::string text = value != nullptr ? value : "";
		size_t start = 0;
		while (start < text.size())
		{
			const auto end = std::min(text.find(';', start), text.size());
			const auto item = text.substr(start, end - start);
			if (const auto comma = item.find(','); comma != std::string::npos)
			{
				list.emplace_back(static_cast<uint32_t>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1));
			}
			start = end + 1;
		}
		return list;
	}();
	for (auto it = shots.begin(); it != shots.end(); ++it)
	{
		if (turn >= it->first && Game::Instance() != nullptr)
		{
			Game::Instance()->RequestScreenshot(it->second);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager test: screenshot at turn {} -> {}", turn, it->second);
			shots.erase(it);
			break;
		}
	}
	if (turn == 2)
	{
		// OPENBLACK_TEST_VILLAGER_LIFE="<life>[,<n>]"
		if (const auto life = ParseValueFor("OPENBLACK_TEST_VILLAGER_LIFE"))
		{
			const float value = static_cast<float>(std::atof(life->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*life, entity))
				{
					life::SetLife(entity, value);
					Trace(entity, fmt::format("test: life set to {:.3f}", value));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_FOOD="<food>[,<n>]": the belly (+0xE8)
		if (const auto food = ParseValueFor("OPENBLACK_TEST_VILLAGER_FOOD"))
		{
			const float value = static_cast<float>(std::atof(food->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*food, entity))
				{
					registry.Get<Villager>(entity).food = value;
					Trace(entity, fmt::format("test: food set to {:.3f}", value));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_NOTHING="<r>[,<n>]": the GameRand(9) of the next SetupNothingToDo
		if (const auto nothing = ParseValueFor("OPENBLACK_TEST_VILLAGER_NOTHING"))
		{
			const auto r = static_cast<uint32_t>(std::atoi(nothing->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*nothing, entity))
				{
					ForceNextNothingRoll(entity, r);
					Trace(entity, fmt::format("test: next SetupNothingToDo r={}", r));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_AGE="<age>[,<n>]" (V4): Living::SetAge (the birth turn) only, no meshes nor flags (to
		// try 12 -> 13 and the old age)
		if (const auto age = ParseValueFor("OPENBLACK_TEST_VILLAGER_AGE"))
		{
			const auto value = static_cast<uint32_t>(std::atoi(age->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*age, entity))
				{
					SetAgeBirthTurn(entity, value, turn);
					Trace(entity, fmt::format("test: age set to {}", value));
				}
			}
		}
		// OPENBLACK_TEST_HOMELESS=<n> (V4): Villager::MakeHomeless of the villager n (129, then 36 without an abode)
		if (const char* homeless = std::getenv("OPENBLACK_TEST_HOMELESS"); homeless != nullptr)
		{
			const auto n = std::atoll(homeless);
			for (const auto entity : Villagers())
			{
				if (object_index::Of(entity) == n)
				{
					const bool made = MakeHomeless(entity);
					Trace(entity, fmt::format("test: MakeHomeless = {}", made ? 1 : 0));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_POISONED=<n>
		if (const char* poisoned = std::getenv("OPENBLACK_TEST_VILLAGER_POISONED"); poisoned != nullptr)
		{
			const auto n = std::atoll(poisoned);
			for (const auto entity : Villagers())
			{
				if (object_index::Of(entity) == n && !registry.AllOf<Poisoned>(entity))
				{
					registry.Assign<Poisoned>(entity);
					Trace(entity, "test: poisoned");
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_STATE="<state>[,<n>]": villager::SetTopState and its code
		if (const auto state = ParseValueFor("OPENBLACK_TEST_VILLAGER_STATE"))
		{
			const auto s = static_cast<VillagerStates>(std::atoi(state->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*state, entity) && registry.Valid(entity))
				{
					const auto code = SetTopState(entity, s);
					Trace(entity, fmt::format("test: SetTopState({}) = {:#x}, top {} final {}", static_cast<uint32_t>(s), code,
					                          static_cast<uint32_t>(GetState(entity, Index::Top)),
					                          static_cast<uint32_t>(GetState(entity, Index::Final))));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_BORN_IN_WATER="x,z": a Celtic housewife of 25 there (VillagerArchetype::Create)
		if (const char* water = std::getenv("OPENBLACK_TEST_VILLAGER_BORN_IN_WATER"); water != nullptr)
		{
			float x = 0.0f;
			float z = 0.0f;
			if (std::sscanf(water, "%f,%f", &x, &z) == 2)
			{
				const glm::vec3 position(x, 0.0f, z);
				const auto type = GVillagerInfo::Find(Tribe::CELTIC, VillagerNumber::Housewife);
				const auto entity = archetypes::VillagerArchetype::Create(position, position, type, 25, false);
				const auto* action = registry.TryGet<const LivingAction>(entity);
				Trace(entity, fmt::format("test: born at ({:.1f}, {:.1f}): water {} -> state {}, counter {}", x, z,
				                          pot_resource::IsWater(position) ? 1 : 0,
				                          static_cast<uint32_t>(GetState(entity, Index::Top)),
				                          action != nullptr ? action->turnsUntilStateChange : 0));
			}
		}
	}
	// the trace's summary every 100 turns
	if (TraceTarget() && turn % 100 == 0)
	{
		// V4: per town, who is inside (+0xE0 & 4), asleep (120), in a tent (238), and the homeless / vagrants lists
		registry.Each<const Town>([&](entt::entity town, const Town& t) {
			uint32_t inside = 0;
			uint32_t asleep = 0;
			uint32_t tents = 0;
			for (const auto entity : Villagers())
			{
				const auto& v = registry.Get<const Villager>(entity);
				if (v.town != town)
				{
					continue;
				}
				inside += (v.flags & Villager::k_FlagAtHome) != 0 ? 1 : 0;
				const auto top = GetState(entity, Index::Top);
				asleep += top == VillagerStates::SleepingAtHome ? 1 : 0;
				tents += top == VillagerStates::SleepInTent ? 1 : 0;
			}
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager trace: home: town {} inside {} asleep {} tents {} homeless {} vagrants {}",
			                   t.id, inside, asleep, tents, t.homelessVillagers.size(), town_villagers::Vagrants().size());
		});
		for (const auto entity : Villagers())
		{
			if (!TraceOn(entity))
			{
				continue;
			}
			const auto& v = registry.Get<const Villager>(entity);
			const auto& action = registry.Get<const LivingAction>(entity);
			// where it is and openblack's walk (the PathfindingSystem's move tag: L linear, O orbit, E exit circle, S
			// step through, F final step, A arrived, - none)
			const auto* transform = registry.TryGet<const components::Transform>(entity);
			const auto* wallHug = registry.TryGet<const components::WallHug>(entity);
			const char* walk = registry.AnyOf<components::MoveStateLinearTag>(entity)        ? "L"
			                   : registry.AnyOf<components::MoveStateOrbitTag>(entity)       ? "O"
			                   : registry.AnyOf<components::MoveStateExitCircleTag>(entity)  ? "E"
			                   : registry.AnyOf<components::MoveStateStepThroughTag>(entity) ? "S"
			                   : registry.AnyOf<components::MoveStateFinalStepTag>(entity)   ? "F"
			                   : registry.AnyOf<components::MoveStateArrivedTag>(entity)     ? "A"
			                                                                                 : "-";
			Trace(entity,
			      fmt::format("summary: life {:.6f} food {:.4f} top {} final {} previous {} tssc {} counter {} age {} "
			                  "at ({:.1f}, {:.1f}) goal ({:.1f}, {:.1f}) walk {}",
			                  v.life, v.food, action.states[0], action.states[1], action.states[2],
			                  action.turnsSinceStateChange, action.turnsUntilStateChange, GetAge(entity),
			                  transform != nullptr ? transform->position.x : 0.0f,
			                  transform != nullptr ? transform->position.z : 0.0f, wallHug != nullptr ? wallHug->goal.x : 0.0f,
			                  wallHug != nullptr ? wallHug->goal.y : 0.0f, walk));
		}
	}
}
} // namespace openblack::ecs::villager
