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

// CHL natives of the weather, called from CHLApi.cpp. They pop the VM stack themselves (the handlers' order).

namespace openblack::magic::script
{
/// CREATE of a SCRIPT_OBJECT_TYPE_WEATHER_THING (GScript 0x6F151E): a WeatherThing from GWeatherInfo[subtype]
entt::entity CreateWeatherThing(uint32_t subtype, const glm::vec3& position);
/// 123 CHANGE_WEATHER_PROPERTIES, GScript::ChangeWeatherProperties 0x6FA490 (storm, temperature, rainfall, snowfall,
/// overcast, fallspeed) -> fn_00774400
void ChangeWeatherProperties();
/// 124 CHANGE_LIGHTNING_PROPERTIES 0x6FA6B0 (storm, sheetmin, sheetmax, forkmin, forkmax) -> fn_00774520
void ChangeLightningProperties();
/// 125 CHANGE_TIME_FADE_PROPERTIES 0x6FA570 (storm, duration, fadeTime) -> fn_00774460
void ChangeTimeFadeProperties();
/// 126 CHANGE_CLOUD_PROPERTIES 0x6FA600 (storm, numClouds, blackness, elevation) -> fn_00774500(blackness,
/// ftol(numClouds), elevation)
void ChangeCloudProperties();
/// 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM 0x6FF4E0 (0xC24759 = arg != 0)
void PauseUnpauseClimateSystem();
/// 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM 0x6FF500 (0xC24758)
void PauseUnpauseStormCreationInClimateSystem();
/// 404 KILL_STORMS_IN_AREA 0x6FF520 (position, radius) -> fn_0083F750
void KillStormsInArea();
} // namespace openblack::magic::script
