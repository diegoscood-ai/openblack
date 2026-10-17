/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLFire.h"

#include <cstdio>
#include <cstdlib>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The script's thing ("Thing not valid") that must be an object ("Thing not object"): the port's objects are the
/// entities with a transform
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

/// The game's fires; stops with a message when there are none (before the game or after it has gone)
ecs::systems::FireSystemInterface& Fires()
{
	if (!Locator::fireSystem::has_value())
	{
		std::fputs("script fire commands: no fires in the locator (Locator::fireSystem)\n", stderr);
		std::abort();
	}
	return Locator::fireSystem::value();
}
} // namespace

void magic::script::IsOnFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	vm.Pushb(object != entt::null && Fires().IsOnFire(object));
}

void magic::script::IsFireNear()
{
	auto& vm = Locator::vm::value();
	const auto radius = vm.Popf();
	const auto z = vm.Popf();
	const auto y = vm.Popf();
	const auto x = vm.Popf();
	vm.Pushb(Fires().IsFireNear(glm::vec3(x, y, z), radius));
}

void magic::script::SetTemperature()
{
	auto& vm = Locator::vm::value();
	const auto temperature = vm.Popf();
	const auto object = ScriptObject(vm.Pop().uintVal);
	if (object != entt::null)
	{
		Fires().SetTemperature(object, temperature, entt::null);
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
		Fires().SetOnFire(object, speed);
	}
	else
	{
		Fires().PutOut(object);
	}
}

void magic::script::SetHurtByFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object != entt::null)
	{
		Fires().SetHurtByFire(object, enable);
	}
}

void magic::script::SetSetOnFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object != entt::null)
	{
		Fires().SetCanBeSetOnFire(object, enable);
	}
}
