/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerScriptMemory.h"

#include <utility>

#include <fmt/format.h>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/WalkGoal.h"
#include "Locator.h"

namespace openblack::ecs::script_memory
{
using components::ScriptAnimation;
using components::ScriptMemory;
using components::ScriptStateArguments;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

ScriptMemory& Make(entt::entity villager)
{
	auto& registry = Entities();
	if (auto* memory = registry.TryGet<ScriptMemory>(villager); memory != nullptr)
	{
		return *memory;
	}
	// simulation state only: the drawn instances are not rebuilt
	return registry.AssignState<ScriptMemory>(villager);
}

ScriptMemory* Of(entt::entity villager)
{
	return Entities().TryGet<ScriptMemory>(villager);
}

const ScriptMemory* Find(entt::entity villager)
{
	return std::as_const(Entities()).TryGet<const ScriptMemory>(villager);
}

void KeepInMind(entt::entity villager)
{
	auto& registry = Entities();
	auto* memory = registry.TryGet<ScriptMemory>(villager);
	if (memory == nullptr)
	{
		return;
	}
	// the thing its walk followed: openblack's villagers walk to no thing yet
	memory->target = entt::null;
	// where its walk goes, with the goal's height above the land
	const auto* wallHug = registry.TryGet<const components::WallHug>(villager);
	memory->destination = wallHug != nullptr ? map_coords::FromMetres(wallHug->goal) : map_coords::MapCoords {};
	memory->destination.altitude = walk_goal::AltitudeOf(villager);
	// the script's place and number, and its pair of whole numbers
	const auto* arguments = registry.TryGet<const ScriptStateArguments>(villager);
	memory->arguments = arguments != nullptr ? std::optional(*arguments) : std::nullopt;
	const auto* clipAndPlays = registry.TryGet<const ScriptAnimation>(villager);
	memory->clipAndPlays = clipAndPlays != nullptr ? std::optional(*clipAndPlays) : std::nullopt;
	// a walk that never had a destination stands for one to where it is, unless kept from the playing state. A villager
	// is always on the land in openblack (no walk onto a structure), so its height is 0
	if (!HasDestination(memory->destination) && memory->state != VillagerStates::ScriptPlayAnim)
	{
		if (const auto* transform = registry.TryGet<const components::Transform>(villager); transform != nullptr)
		{
			memory->destination = map_coords::FromMetres(glm::xz(transform->position));
		}
	}
	if (DebugOn())
	{
		const auto to = map_coords::ToMetres(memory->destination);
		Debug(villager, fmt::format("script memory kept: state {}, its walk to ({:.2f}, {:.2f}), {:.2f} up",
		                            static_cast<int>(memory->state), to.x, to.y, memory->destination.altitude));
	}
}

void GiveBack(entt::entity villager)
{
	auto& registry = Entities();
	const auto* memory = registry.TryGet<const ScriptMemory>(villager);
	if (memory == nullptr)
	{
		return;
	}
	// one line for the debug runs (the real input check of realinput.script_memory reads it)
	if (DebugOn())
	{
		const auto to = map_coords::ToMetres(memory->destination);
		Debug(villager, fmt::format("script memory given back: its walk to ({:.2f}, {:.2f}), {:.2f} up", to.x, to.y,
		                            memory->destination.altitude));
	}
	// the destination, its height with it
	if (auto* wallHug = registry.TryGet<components::WallHug>(villager); wallHug != nullptr)
	{
		walk_goal::Set(villager, wallHug->goal, map_coords::ToMetres(memory->destination));
		walk_goal::SetAltitude(villager, memory->destination.altitude);
	}
	// the script's place and number, and its pair of whole numbers, as they were kept
	if (memory->arguments.has_value())
	{
		registry.AssignOrReplaceState<ScriptStateArguments>(villager, memory->arguments->place, memory->arguments->number);
	}
	else if (registry.AllOf<ScriptStateArguments>(villager))
	{
		registry.RemoveState<ScriptStateArguments>(villager);
	}
	if (memory->clipAndPlays.has_value())
	{
		registry.AssignOrReplaceState<ScriptAnimation>(villager, memory->clipAndPlays->clip, memory->clipAndPlays->playsLeft);
	}
	else if (registry.AllOf<ScriptAnimation>(villager))
	{
		registry.RemoveState<ScriptAnimation>(villager);
	}
}

bool DebugOn()
{
	const auto logger = spdlog::get("game");
	return logger != nullptr && logger->should_log(spdlog::level::debug);
}

void Debug(entt::entity villager, std::string_view what)
{
	// a runtime debug line: the release build keeps it, and only a log at debug writes it
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->debug("Villager {}: {}", static_cast<uint32_t>(villager), what);
	}
}

void RecordStateClip(entt::entity villager, int32_t clip)
{
	if (auto* memory = Of(villager); memory != nullptr)
	{
		memory->stateClip = clip;
	}
}

void KeepForcedClip(entt::entity villager, int32_t clip)
{
	auto& memory = Make(villager);
	KeepInMind(villager);
	memory.state = VillagerStates::InvalidState;
	memory.stateClip = clip;
}
} // namespace openblack::ecs::script_memory
