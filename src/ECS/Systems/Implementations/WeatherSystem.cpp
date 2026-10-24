/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WeatherSystem.h"

#include "ECS/Weather/Atmos.h"
#include "ECS/Weather/Calendar.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/LightningFlash.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/Weather.h"
#include "ECS/Weather/WeatherLoop.h"

using namespace openblack;
using namespace openblack::ecs::systems;

// Each method is the weather function the game called before, unchanged. Those functions reach the state through
// Locator::weatherSystem, which holds this system.

void WeatherSystem::Reset()
{
	weather::OnLoadMap();
}

void WeatherSystem::CreateClimate(int32_t index, uint32_t info, glm::vec2 position, float radius1, float radius2)
{
	// The script's position across the land, at height 0, and no wind angle until CREATE_WEATHER_CLIMATE_WIND
	weather::climate::Create(glm::vec3(position.x, 0.0f, position.y), static_cast<int32_t>(info), radius1, radius2, 0.0f,
	                         index);
}

void WeatherSystem::SetClimateRain(int32_t index, float desire, int32_t dryDays, int32_t rainingDays, int32_t raining)
{
	const weather::climate::Rain rain {
	    .desire = desire,
	    .dryDays = dryDays,
	    .rainingDays = rainingDays,
	    .flags = static_cast<uint8_t>(raining),
	};
	weather::climate::SetRain(index, rain);
}

void WeatherSystem::SetClimateTemperature(int32_t index, float temperature, float targetTemperature)
{
	weather::climate::SetTemperature(index, temperature, targetTemperature);
}

void WeatherSystem::SetClimateWind(int32_t index, float windX, float windZ, float angle)
{
	weather::climate::SetWind(index, windX, windZ, angle);
}

void WeatherSystem::SetClimateSystemEnabled(bool enabled)
{
	weather::climate::SetClimateSystemEnabled(enabled);
}

void WeatherSystem::SetStormCreationEnabled(bool enabled)
{
	weather::climate::SetStormCreationEnabled(enabled);
}

bool WeatherSystem::IsClimateSystemEnabled() const
{
	return weather::climate::IsClimateSystemEnabled();
}

bool WeatherSystem::IsStormCreationEnabled() const
{
	return weather::climate::IsStormCreationEnabled();
}

void WeatherSystem::KillStormsInArea(glm::vec3 position, float radius)
{
	weather::storms::KillStormsInArea(position, radius);
}

ecs::components::WeatherInfo WeatherSystem::GetWeather(const glm::vec3& position)
{
	return weather::ComputeWeather(position, false);
}

ecs::components::WeatherInfo WeatherSystem::GetWeatherSmooth(const glm::vec3& position)
{
	return weather::ComputeWeather(position, true);
}

float WeatherSystem::GetOvercast(const glm::vec3& position)
{
	// The overcast byte of the atmosphere alone, sampled smoothly, times 0.01 and not clamped
	return static_cast<float>(weather::atmos::GetWeatherSmooth(position, true).overcast) * 0.01f;
}

uint8_t WeatherSystem::GetLightningFlash(const glm::vec3& camera) const
{
	return weather::LightningFlashAtCamera(camera);
}

double WeatherSystem::GetDaysFromStart(uint32_t turn) const
{
	return weather::calendar::GetDaysFromStart(turn);
}

uint32_t WeatherSystem::GetSeason(uint32_t turn) const
{
	return weather::calendar::GetSeason(turn);
}
