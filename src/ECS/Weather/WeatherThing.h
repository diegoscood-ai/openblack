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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Storms.h"

// WeatherThing (WeatherThing.cpp, 0x88 bytes, list g_game+0x205C54 newest first): the CHL's weather object
// (CREATE SCRIPT_OBJECT_TYPE_WEATHER_THING, script object type 15). It owns a GWeather built from GWeatherInfo[subtype]
// and keeps a copy of its descriptor (+0x28); the CHANGE_*_PROPERTIES natives edit that copy and UpdateStats 0x774370
// writes it back to the storm.

namespace openblack::ecs::components
{
struct WeatherThing
{
	weather::storms::StormDescriptor descriptor;               ///< +0x28
	weather::storms::StormId storm {weather::storms::k_NoStorm}; ///< +0x78
	bool affectedByWind {false};                               ///< +0x7C SetAffectedByWind 0x55DF20
};
} // namespace openblack::ecs::components

namespace openblack::weather::weather_thing
{
/// CHL CREATE (GScript 0x6F151E -> 0x7741F0 -> ctor 0x774030(pos, 100, 300, 100, &GWeatherInfo[subtype])): inner 100,
/// outer 300, a life of 100 s, clouds at 500, the info's temperature, wetness, snowFall, overCast and wind as the
/// storm's bytes (fn_00770EA0); the storm is made at once (fn_00770F10)
entt::entity Create(const glm::vec3& position, uint32_t weatherInfo);
/// WeatherThing::ProcessWeatherThings 0x7741A0 (GGame::ProcessTurn, after GLandAlignement::UpdateTime):
/// WeatherThing::Process 0x774230 of each: a thing whose storm is gone forgets it (and would be deleted if no script
/// held it: openblack keeps it); with the wind on, the storm drifts by ComputeWeather's wind x 0.01 (fn_00771A30); the
/// thing moves with its storm and copies its descriptor back
void ProcessWeatherThings();
/// UpdateStats 0x774370: the copy to the storm (or a new storm), and new lightning timers (min + rand(max - min))
void UpdateStats(entt::entity thing);
/// The things of the land, forgotten with the land (the registry is reset)
void Reset();

/// fn_00774400 (CHANGE_WEATHER_PROPERTIES): temperature (whole degrees), rain, snow and overcast x 100, fall speed
void SetWeatherProperties(entt::entity thing, float temperature, float rainfall, float snowfall, float overcast,
                          float fallSpeed);
/// fn_00774460 (CHANGE_TIME_FADE_PROPERTIES): lifeTime, fadeInTime
void SetTimeFadeProperties(entt::entity thing, float duration, float fadeTime);
/// fn_00774500 (CHANGE_CLOUD_PROPERTIES): blackness, number of clouds (int), elevation
void SetCloudProperties(entt::entity thing, float blackness, int32_t numClouds, float elevation);
/// fn_00774520 (CHANGE_LIGHTNING_PROPERTIES): sheet and fork ranges
void SetLightningProperties(entt::entity thing, float sheetMin, float sheetMax, float forkMin, float forkMax);
/// WeatherThing::SetMovement 0x774480: the wind bytes, clamp(v x 8, -127, 127)
void SetMovement(entt::entity thing, const glm::vec3& movement);
/// fn_00774550 / fn_00774580: the storm's target and speed
void SetTarget(entt::entity thing, const glm::vec3& target);
void SetSpeed(entt::entity thing, float speed);
void SetAffectedByWind(entt::entity thing, bool on);
/// GameThingWithPos::IsWeather (vt 0x3FC): only WeatherThing says yes
[[nodiscard]] bool IsWeather(entt::entity thing);
} // namespace openblack::weather::weather_thing
