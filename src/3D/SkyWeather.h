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

/// The lightning flash at the camera that the landscape light table reads every frame (fn_00869850). It tints the
/// whole frame through the table: the land, its reflection, the sea (table[255]), the rings created in that frame and
/// the distance haze. The other input of the table, the overcast at the camera ([0xFA2754]), has a single source:
/// Clouds::WeatherOvercastAtCamera (weather::atmos::GetWeatherSmooth(camera).byte3 x 0.01).
/// The flash itself is the weather's: weather::LightningFlashAtCamera (ECS/Weather/LightningFlash, the storms' flash
/// objects at GWeather +0x70, updated every frame by weather::UpdateFrame).
namespace openblack::sky_weather
{

/// [0xFA2768] (LH3DAtmos::Update3D 0x83587C..0x835903): 0, or ftol(clamp(storm+0x88, 0, 1) * 255) when the camera is
/// inside the nearest live storm (dXZ^2 < ((inner + outer) / 2)^2). storm+0x88 = f1 * intensity (fn_00837200:
/// s = 1 - age, 0.1 between 0.2 and 0.5 s, gone at 0.8 s; intensity 0.5 fork / 1.0 sheet lightning).
/// weather::LightningFlashAtCamera at the camera's position (0 without a camera).
[[nodiscard]] uint8_t LightningFlash();

} // namespace openblack::sky_weather
