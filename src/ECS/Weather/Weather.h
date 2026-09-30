/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "WeatherInfo.h"

// The weather queries the rest of the game uses (GClimate's getters, Weather.cpp of the original). Every query samples
// GClimate::ComputeWeather at a point: the LH3DAtmos 40 m grid (the storms registered in it) plus the climates'
// temperature and wind. The point is an LHPoint: x, z in metres and y the absolute height (the temperature drops above
// 50 m). Wiki: docs/bw1-notes/magic.md ("Tiempo y clima").
//   Weather.h          these queries (the small header other systems include)
//   WeatherInfo.h      the 8-byte WeatherInfo and its byte arithmetic
//   Atmos.{h,cpp}      LH3DAtmos: the 128 x 128 grid of 40 m cells
//   Storms.{h,cpp}     LH3DStorm / GWeather: the registered weather volumes (miracle storms, climate storms)
//   Climate.{h,cpp}    GClimate: the world and local climates, ComputeWeather, ProcessAll, natural storms
//   Calendar.{h,cpp}   the date part of GGameInfo (day of the year, month, season)
//   WeatherThing.{h,cpp}  the CHL weather objects (CREATE WEATHER_THING, CHANGE_*_PROPERTIES)

namespace openblack::weather
{
/// GClimate::ComputeWeather 0x771640 (GClimate::GetWeather 0x771490). `smooth` samples the grid bilinearly
/// (LH3DAtmos::GetWeatherSmooth 0x835180, the camera's choice); every getter below passes false.
[[nodiscard]] WeatherInfo ComputeWeather(const glm::vec3& point, bool smooth = false);

/// GClimate::IsRaining 0x7714B0: rain byte > 0 (false while there is no world climate)
[[nodiscard]] bool IsRainingAt(const glm::vec3& point);
/// GClimate::IsSnowing 0x7714F0: snow byte > 0
[[nodiscard]] bool IsSnowingAt(const glm::vec3& point);
/// fn_00771530: snow-cover byte > 0
[[nodiscard]] bool IsSnowCoveredAt(const glm::vec3& point);
/// GClimate::GetRain 0x771570: the rain byte (0..127; 0 without a world climate)
[[nodiscard]] float GetRainAt(const glm::vec3& point);
/// GClimate::GetSnow 0x7715B0: the snow byte
[[nodiscard]] float GetSnowAt(const glm::vec3& point);
/// GClimate::GetMaxRainingOrSnowing 0x771600: max(rain, snow) bytes. The fire cools with 1 + 0.01 x this (fn_0072EFB0),
/// fireballs steam and trees grow with it.
[[nodiscard]] float GetMaxRainingOrSnowingAt(const glm::vec3& point);
/// GClimate::GetTemp 0x771A80: the temperature byte (degrees; about 20 on the Land 1 lowlands).
/// NOT the fire's ambient temperature: MapCoords::GetTemperature 0x605CC0 is a constant 24.7 (fire::AmbientTemperature).
[[nodiscard]] float GetTemperatureAt(const glm::vec3& point);
/// fn_00771AB0 / fn_00771AE0: the wind bytes
[[nodiscard]] float GetWindXAt(const glm::vec3& point);
[[nodiscard]] float GetWindZAt(const glm::vec3& point);
/// fn_00771B10: the wind as a vector, (windX / 8, 0, windZ / 8). Fire (unused result), UR_CloudMoverNew and
/// UpdateRuleGravityWithFloor (fireballs) read it.
[[nodiscard]] glm::vec3 GetWindAt(const glm::vec3& point, bool smooth = false);
} // namespace openblack::weather
