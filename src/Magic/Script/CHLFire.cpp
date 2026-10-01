/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLFire.h"

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// GScript::GetScriptGameThing 0x70D220 ("Thing not valid") and the dynamic cast to Object ("Thing not object"): the
/// port's objects are the entities with a transform
entt::entity ScriptObject(uint32_t value)
{
	const auto entity = static_cast<entt::entity>(value);
	auto& registry = Locator::entitiesRegistry::value();
	if (value == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		return entt::null;
	}
	if (!registry.AllOf<components::Transform>(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not object");
		return entt::null;
	}
	return entity;
}
} // namespace

void magic::script::IsOnFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	vm.Pushb(object != entt::null && fire::IsOnFire(object));
}

void magic::script::IsFireNear()
{
	auto& vm = Locator::vm::value();
	const auto radius = vm.Popf();
	const auto z = vm.Popf();
	const auto y = vm.Popf();
	const auto x = vm.Popf();
	static_cast<void>(y);
	auto& registry = Locator::entitiesRegistry::value();
	bool found = false;
	for (const auto* burning : fire::All())
	{
		// MapCoords::FindNearForScript walks the map cells: a fireball (not in the map) is not found. TODO(M7): a worship
		// site answers with its totem's position (WorshipSite::GetTotemPos 0x77CF30)
		if (burning->object == entt::null || !burning->IsOnFire() || !fire::traits::IsObjectInMap(burning->object))
		{
			continue;
		}
		const auto* transform = registry.TryGet<const components::Transform>(burning->object);
		if (transform != nullptr && glm::length(glm::vec2(transform->position.x - x, transform->position.z - z)) <= radius)
		{
			found = true;
			break;
		}
	}
	vm.Pushb(found);
}

void magic::script::SetTemperature()
{
	auto& vm = Locator::vm::value();
	const auto temperature = vm.Popf();
	const auto object = ScriptObject(vm.Pop().uintVal);
	if (object != entt::null)
	{
		fire::SetTemperature(object, temperature, entt::null);
	}
}

void magic::script::SetOnFire()
{
	auto& vm = Locator::vm::value();
	const auto speed = vm.Popf();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object == entt::null)
	{
		return;
	}
	if (enable)
	{
		fire::SetOnFire(object, speed);
	}
	else
	{
		const auto& transform = Locator::entitiesRegistry::value().Get<const components::Transform>(object);
		fire::SetTemperature(object, fire::AmbientTemperature(transform.position), entt::null);
	}
}

void magic::script::SetHurtByFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object != entt::null)
	{
		fire::traits::SetNotHurtByFire(object, !enable);
	}
}

void magic::script::SetSetOnFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object != entt::null)
	{
		fire::traits::SetCannotBeSetOnFire(object, !enable);
	}
}
