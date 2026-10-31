/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The CHL weather natives (CHLApi.cpp forwards to them). The arguments are popped in the handlers' order.

#include "CHLWeather.h"

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherLand.h"
#include "ECS/Weather/WeatherThing.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::weather;

namespace
{
glm::vec3 PopPosition(lhvm::LHVM& vm)
{
	const auto z = vm.Popf();
	const auto y = vm.Popf();
	const auto x = vm.Popf();
	return {x, y, z};
}

/// GetScriptGameThing + the IsWeather test of every CHANGE_*_PROPERTIES native ("Jonty-Weather properties on non
/// weather ...")
bool IsWeatherThing(entt::entity thing)
{
	if (weather_thing::IsWeather(thing))
	{
		return true;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Jonty-Weather properties on non weather object");
	return false;
}
} // namespace

entt::entity magic::script::CreateWeatherThing(uint32_t subtype, const glm::vec3& position)
{
	return weather_thing::Create(position, subtype);
}

void magic::script::ChangeWeatherProperties()
{
	auto& vm = Locator::vm::value();
	const auto fallSpeed = vm.Popf();
	const auto overcast = vm.Popf();
	const auto snowfall = vm.Popf();
	const auto rainfall = vm.Popf();
	const auto temperature = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetWeatherProperties(storm, temperature, rainfall, snowfall, overcast, fallSpeed);
	}
}

void magic::script::ChangeLightningProperties()
{
	auto& vm = Locator::vm::value();
	const auto forkMax = vm.Popf();
	const auto forkMin = vm.Popf();
	const auto sheetMax = vm.Popf();
	const auto sheetMin = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetLightningProperties(storm, sheetMin, sheetMax, forkMin, forkMax);
	}
}

void magic::script::ChangeTimeFadeProperties()
{
	auto& vm = Locator::vm::value();
	const auto fadeTime = vm.Popf();
	const auto duration = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetTimeFadeProperties(storm, duration, fadeTime);
	}
}

void magic::script::ChangeCloudProperties()
{
	auto& vm = Locator::vm::value();
	const auto elevation = vm.Popf();
	const auto blackness = vm.Popf();
	const auto numClouds = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetCloudProperties(storm, blackness, static_cast<int32_t>(numClouds), elevation);
	}
}

bool magic::script::ChangeWeatherInnerOuter(uint32_t object, float inner, float outer)
{
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !weather_thing::IsWeather(thing))
	{
		return false;
	}
	weather_thing::SetInnerOuterProperties(thing, inner, outer);
	return true;
}

bool magic::script::SetWeatherHeadingAndSpeed(entt::entity thing, const glm::vec3& place, float speed)
{
	if (!weather_thing::IsWeather(thing))
	{
		return false;
	}
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(thing);
	const glm::vec3 at = transform != nullptr ? transform->position : glm::vec3(0.0f);
	// its map position, at the land's altitude there plus its own height
	const glm::vec3 from(at.x, LandHeightAt(at.x, at.z) + at.y, at.z);
	weather_thing::SetMovement(thing, openblack::script::property_rules::HeadingVelocity(from, place, speed));
	return true;
}

void magic::script::SetAffectedByWind()
{
	auto& vm = Locator::vm::value();
	const auto object = vm.Pop().uintVal;
	// any value but none lets the wind carry it, as the game keeps the value whole
	const bool affected = vm.Pop().uintVal != 0;
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !ecs::script_held::IsReachable(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_AFFECTED_BY_WIND: Invalid thing");
		return;
	}
	weather_thing::SetAffectedByWind(thing, affected);
}

void magic::script::PauseUnpauseClimateSystem()
{
	auto& vm = Locator::vm::value();
	Locator::weatherSystem::value().SetClimateSystemEnabled(vm.Pop().intVal != 0);
}

void magic::script::PauseUnpauseStormCreationInClimateSystem()
{
	auto& vm = Locator::vm::value();
	Locator::weatherSystem::value().SetStormCreationEnabled(vm.Pop().intVal != 0);
}

void magic::script::KillStormsInArea()
{
	auto& vm = Locator::vm::value();
	const auto radius = vm.Popf();
	const auto position = PopPosition(vm);
	Locator::weatherSystem::value().KillStormsInArea(position, radius);
}
