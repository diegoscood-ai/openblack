/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Test hooks of the weather (documented in docs/bw1-notes/openblack-internals.md):
//   OPENBLACK_TEST_WEATHER="x,z,radius[,rain[,fade[,temperature]]]"  on turn 1 registers a static LH3DStorm built like
//       the storm miracle's (fn_006D5730: inner min(radius, 60), outer min(max(2.5 radius, inner + 20), 80), 20 degrees
//       unless given, rain 100 unless given, overcast 80, no wind, a life of 1e9 s, fade-in `fade` s (default 1)), then
//       logs ComputeWeather at the centre, inside, on the inner and outer radius and outside, on turns 2 and 30
//   OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"  logs ComputeWeather at those points on turns 2 and 30
//   OPENBLACK_WEATHER_TRACE=1  logs every climate (temperature, wind, rain desire, storms) on each new game day and every
//       storm's fade and radii every 50 turns

#include <cstdlib>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "Calendar.h"
#include "Climate.h"
#include "Storms.h"
#include "Weather.h"
#include "WeatherLand.h"
#include "WeatherLoop.h"

using namespace openblack;
using namespace openblack::weather;

namespace
{
bool g_stormDone = false;
storms::StormId g_testStorm = storms::k_NoStorm;
glm::vec3 g_testCentre {0.0f};
float g_testInner = 0.0f;
float g_testOuter = 0.0f;
int32_t g_lastTraceDay = -1;

std::vector<float> ParseNumbers(const std::string& text)
{
	std::vector<float> values;
	std::stringstream stream(text);
	std::string item;
	while (std::getline(stream, item, ','))
	{
		values.push_back(std::strtof(item.c_str(), nullptr));
	}
	return values;
}

void LogAt(const char* label, float x, float z)
{
	const glm::vec3 point(x, LandHeightAt(x, z), z);
	const auto w = weather::ComputeWeather(point, false);
	const auto wind = weather::GetWindAt(point);
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Weather: {} ({:.1f}, {:.1f}, h {:.1f}): temp {} rain {} snow {} overcast {} wind ({}, {}) = "
	                   "({:.3f}, {:.3f}) m/s, GetMaxRainingOrSnowing {:.0f}, GetTemp {:.0f}",
	                   label, x, z, point.y, static_cast<int>(w.temperature), static_cast<int>(w.rain),
	                   static_cast<int>(w.snow), static_cast<int>(w.overcast), static_cast<int>(w.windX),
	                   static_cast<int>(w.windZ), wind.x, wind.z, weather::GetMaxRainingOrSnowingAt(point),
	                   weather::GetTemperatureAt(point));
}

void LogStorms(uint32_t turn)
{
	storms::ForEach([turn](const storms::Storm& s) {
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Weather: turn {} storm {} at ({:.1f}, {:.1f}) age {:.1f}/{:.0f} fade {:.2f} inner {:.1f} "
		                   "outer {:.1f} bytes t{} r{} s{} o{} w({}, {}){}",
		                   turn, s.id, s.drawPosition.x, s.drawPosition.z, s.age, s.descriptor.lifeTime, s.fade,
		                   s.innerRadius, s.outerRadius, static_cast<int>(s.descriptor.weather.temperature),
		                   static_cast<int>(s.descriptor.weather.rain), static_cast<int>(s.descriptor.weather.snow),
		                   static_cast<int>(s.descriptor.weather.overcast), static_cast<int>(s.descriptor.weather.windX),
		                   static_cast<int>(s.descriptor.weather.windZ), s.deleteCounter != 0 ? " (deleting)" : "");
	});
}

void LogClimates(uint32_t turn)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Weather: turn {} day of year {:.2f} (month {}, day {:.2f}, season {} / {})",
	                   turn, calendar::GetDayOfYear(turn), calendar::GetMonth(turn), calendar::GetDayOfMonth(turn),
	                   calendar::GetSeason(turn), climate::CurrentSeason());
	climate::ForEach([](const climate::Climate& c) {
		const auto centre = c.Centre();
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Weather:   climate {}{} info {} at ({:.1f}, {:.1f}) r {:.1f}/{:.1f}: temp {:.2f} (target "
		                   "{:.2f}) wind ({:.2f}, {:.2f}) angle {:.3f}, rain desire {:.4f} dry {} raining {} falling {}, "
		                   "{} storms (max {})",
		                   c.id, c.world ? " (world)" : "", c.info, centre.x, centre.z, c.innerRadius, c.outerRadius,
		                   c.temperature, c.targetTemperature, c.windX, c.windZ, c.windAngle, c.rain.desire,
		                   c.rain.dryDays, c.rain.rainingDays, c.rain.flags & 1, c.storms.size(), c.maxStorms);
	});
}
} // namespace

void weather::ResetDebugHooks()
{
	g_stormDone = false;
	g_testStorm = storms::k_NoStorm;
	g_lastTraceDay = -1;
}

void weather::RunDebugHooks(uint32_t turn)
{
	if (const char* text = std::getenv("OPENBLACK_TEST_WEATHER"); text != nullptr && !g_stormDone && turn >= 1)
	{
		g_stormDone = true;
		const auto v = ParseNumbers(text);
		if (v.size() >= 3)
		{
			const float radius = v[2];
			storms::StormDescriptor d;
			d.position = glm::vec3(v[0], LandHeightAt(v[0], v[1]), v[1]);
			d.innerRadius = radius > 60.0f ? 60.0f : radius;
			d.outerRadius = std::min(std::max(2.5f * radius, d.innerRadius + 20.0f), 80.0f);
			d.fadeInTime = v.size() >= 5 ? v[4] : 1.0f;
			d.lifeTime = 1e9f;
			d.strength = 1.0f;
			d.numClouds = 0;
			d.weather = {};
			d.weather.temperature = static_cast<int8_t>(v.size() >= 6 ? static_cast<int32_t>(v[5]) : 20);
			d.weather.rain = static_cast<int8_t>(v.size() >= 4 ? static_cast<int32_t>(v[3]) : 100);
			d.weather.overcast = 80;
			g_testStorm = storms::Create(d);
			g_testCentre = d.position;
			g_testInner = d.innerRadius;
			g_testOuter = d.outerRadius;
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Weather: test storm {} at ({:.1f}, {:.1f}) inner {:.1f} outer {:.1f} rain {} temp {}",
			                   g_testStorm, v[0], v[1], d.innerRadius, d.outerRadius, static_cast<int>(d.weather.rain),
			                   static_cast<int>(d.weather.temperature));
		}
	}
	if (turn == 2 || turn == 30)
	{
		if (g_testStorm != storms::k_NoStorm)
		{
			LogStorms(turn);
			const float x = g_testCentre.x;
			const float z = g_testCentre.z;
			const float mid = (g_testInner + g_testOuter) * 0.5f;
			LogAt("centre", x, z);
			LogAt("inside (inner / 2)", x + g_testInner * 0.5f, z);
			LogAt("inner radius", x + g_testInner, z);
			LogAt("between (mid)", x + mid, z);
			LogAt("outer - 1", x + g_testOuter - 1.0f, z);
			LogAt("outside (outer + 10)", x + g_testOuter + 10.0f, z);
		}
		if (const char* points = std::getenv("OPENBLACK_TEST_WEATHER_AT"); points != nullptr)
		{
			std::stringstream stream(points);
			std::string item;
			while (std::getline(stream, item, ';'))
			{
				const auto v = ParseNumbers(item);
				if (v.size() >= 2)
				{
					LogAt("point", v[0], v[1]);
				}
			}
		}
	}
	if (std::getenv("OPENBLACK_WEATHER_TRACE") != nullptr)
	{
		const auto day = static_cast<int32_t>(calendar::GetDayOfMonth(turn));
		if (day != g_lastTraceDay)
		{
			g_lastTraceDay = day;
			LogClimates(turn);
		}
		if (turn % 50 == 0)
		{
			LogStorms(turn);
		}
	}
}
