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
#include "ECS/Villager/VillagerCore.h"
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
