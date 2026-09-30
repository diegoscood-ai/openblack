/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Atmos.h"

#include <algorithm>
#include <array>

#include "Storms.h"

using namespace openblack::weather;
using namespace openblack::weather::atmos;

namespace
{
std::array<WeatherInfo, k_GridSize * k_GridSize> g_grid {}; ///< 0xEDC350 (0x20000 bytes, allocated by fn_00835AD0)
WeatherInfo g_ambient {};                                    ///< 0xEDC348
uint8_t g_frame = 1;                                         ///< 0xEDC340

/// fn_00834EE0: the cell, or the ambient weather (stamped with the current frame, so it is never recomputed)
WeatherInfo* CellAt(int32_t x, int32_t z)
{
	if (x < 0 || z < 0 || x >= k_GridSize || z >= k_GridSize)
	{
		g_ambient.stamp = g_frame;
		return &g_ambient;
	}
	return &g_grid[static_cast<size_t>(z) * k_GridSize + static_cast<size_t>(x)];
}

/// fn_00834E20: the ambient weather plus every registered storm at the cell's corner (ix x 40, 0, iz x 40)
void Recalc(WeatherInfo* cell, int32_t x, int32_t z)
{
	if (cell == nullptr || cell == &g_ambient)
	{
		return;
	}
	const glm::vec3 corner(static_cast<float>(x * 40), 0.0f, static_cast<float>(z * 40));
	WeatherInfo weather = g_ambient; // fn_00834E10
	storms::CalcAtmosAll(corner, weather);
	weather.stamp = g_frame;
	// fn_00834DD0: SnowCover (0xEDC344) fn_0086CB80(p) x 0.5 clamped to -128..127; SnowCover is not ported
	weather.snowCover = 0;
	*cell = weather;
}

/// The height part of GetWeather / GetWeatherSmooth
void ApplyHeight(WeatherInfo& weather, float height)
{
	if (height > 50.0f)
	{
		if (height > 200.0f)
		{
			weather.temperature = WrapAdd(weather.temperature, -11); // add al, 0xF5
		}
		else
		{
			weather.temperature = WrapAdd(weather.temperature, static_cast<int32_t>((height - 50.0f) * -0.075f));
		}
	}
}

/// fn_00835620 and the smooth sampling: bytes 0..6 lerped (wrapping), the stamp of `a`
WeatherInfo Lerp(const WeatherInfo& a, const WeatherInfo& b, int32_t w)
{
	WeatherInfo result;
	result.temperature = LerpByte(a.temperature, b.temperature, w);
	result.rain = LerpByte(a.rain, b.rain, w);
	result.snow = LerpByte(a.snow, b.snow, w);
	result.overcast = LerpByte(a.overcast, b.overcast, w);
	result.windX = LerpByte(a.windX, b.windX, w);
	result.windZ = LerpByte(a.windZ, b.windZ, w);
	result.snowCover = LerpByte(a.snowCover, b.snowCover, w);
	result.stamp = a.stamp;
	return result;
}
} // namespace

void atmos::Reset()
{
	storms::Clear();
	g_grid.fill(WeatherInfo {});
	g_ambient = {};
	g_frame = 1;
}

WeatherInfo atmos::GetWeather(const glm::vec3& point, bool recalc)
{
	const auto x = static_cast<int32_t>(point.x * 0.025f);
	const auto z = static_cast<int32_t>(point.z * 0.025f);
	WeatherInfo* cell = CellAt(x, z);
	if (g_frame != cell->stamp && recalc)
	{
		Recalc(cell, x, z);
	}
	WeatherInfo weather = *cell;
	ApplyHeight(weather, point.y);
	return weather;
}

WeatherInfo atmos::GetWeatherSmooth(const glm::vec3& point, bool recalc)
{
	const float fx = point.x * 0.025f;
	const float fz = point.z * 0.025f;
	const auto x = static_cast<int32_t>(fx);
	const auto z = static_cast<int32_t>(fz);
	WeatherInfo* c00 = CellAt(x, z);
	WeatherInfo* c10 = CellAt(x + 1, z);
	WeatherInfo* c01 = CellAt(x, z + 1);
	WeatherInfo* c11 = CellAt(x + 1, z + 1);
	if (recalc)
	{
		if (g_frame != c00->stamp)
		{
			Recalc(c00, x, z);
		}
		if (g_frame != c10->stamp)
		{
			Recalc(c10, x + 1, z);
		}
		if (g_frame != c01->stamp)
		{
			Recalc(c01, x, z + 1);
		}
		if (g_frame != c11->stamp)
		{
			Recalc(c11, x + 1, z + 1);
		}
	}
	const auto wx = static_cast<int32_t>((fx - static_cast<float>(x)) * 256.0f);
	const auto wz = static_cast<int32_t>((fz - static_cast<float>(z)) * 256.0f);
	const WeatherInfo row0 = Lerp(*c00, *c10, wx);
	const WeatherInfo row1 = Lerp(*c01, *c11, wx);
	WeatherInfo weather = Lerp(row0, row1, wz);
	if (point.y > 200.0f)
	{
		const int32_t k = std::min(static_cast<int32_t>((point.y - 200.0f) * 0.25f), 256);
		weather = Lerp(weather, g_ambient, k);
	}
	ApplyHeight(weather, point.y);
	return weather;
}

void atmos::UpdateGame(float visualTime, float seconds)
{
	(void)visualTime; // the sun position (fn_0086A160(visualTime) x pi/12), not used here
	storms::UpdateAll(seconds);
	// SnowCover fn_0086C7A0(seconds): not ported
	if (++g_frame == 0)
	{
		for (auto& cell : g_grid)
		{
			cell.stamp = 0;
		}
		g_frame = 1;
	}
}

const WeatherInfo& atmos::Ambient()
{
	return g_ambient;
}

void atmos::SetAmbient(const WeatherInfo& ambient)
{
	g_ambient = ambient;
}

uint8_t atmos::Frame()
{
	return g_frame;
}
