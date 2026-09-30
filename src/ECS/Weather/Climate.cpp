/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Climate.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <numbers>

#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "Atmos.h"
#include "Calendar.h"
#include "Game.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;
using namespace openblack::weather::climate;

namespace
{
/// 0xC249D8: the temperature's share of its min..max range by hour (0..23)
constexpr std::array<float, 24> k_HourFactor = {0.5f, 0.4f, 0.3f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f,
                                                1.0f, 1.1f, 1.2f, 1.3f, 1.5f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f};
/// 0xC249A4: by month 1..12 (February 0 as in the exe; [0] is never read)
constexpr std::array<float, 13> k_MonthFactor = {0.0f, 0.1f, 0.0f, 0.3f, 0.4f, 0.5f, 0.6f,
                                                 0.8f, 1.0f, 0.7f, 0.4f, 0.3f, 0.2f};
/// MapCoords: metres x 6553.6 (16.16 fixed point of 10 m cells)
constexpr float k_ToMapCoords = 6553.6f;
constexpr float k_FromMapCoords = 0.000152588f;

std::list<Climate> g_climates; ///< g_game+0x205CF4, newest first
Climate* g_world = nullptr;    ///< g_game+0x250534
bool g_climateSystem = true;   ///< 0xC24759
bool g_stormCreation = true;   ///< 0xC24758
int32_t g_lastDay = 0;         ///< 0xDCB8B8
uint32_t g_lastSeason = 0;     ///< 0xDCB8BC
uint32_t g_season = 0;         ///< 0xDCB8C0
int32_t g_nextId = 1;          ///< 0xC2475C

uint32_t Turn()
{
	const auto* game = Game::Instance();
	return game != nullptr ? game->GetTurn() : 0;
}

const GClimateInfo* Info(int32_t index)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& climates = Locator::infoConstants::value().climate;
	return index >= 0 && static_cast<size_t>(index) < climates.size() ? &climates[static_cast<size_t>(index)] : nullptr;
}

// the four seasonal columns of GClimateInfo (spring, summer, autumn, winter)
float RainMin(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->rainMinSpring)[season & 3] : 0.0f;
}
float RainMax(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->rainMaxSpring)[season & 3] : 0.0f;
}
float TempMin(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->tempMinSpring)[season & 3] : 0.0f;
}
float TempMax(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->tempMaxSpring)[season & 3] : 0.0f;
}
float WindMin(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->windMinSpring)[season & 3] : 0.0f;
}
float WindMax(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->windMaxSpring)[season & 3] : 0.0f;
}

/// ftol(ScriptToVisual(ftol(VisualTime))): "RelativeTime", the hour the climate's tables use
int32_t RelativeHour()
{
	auto* game = Game::Instance();
	if (game == nullptr)
	{
		return 12;
	}
	const auto& clock = game->GetDayNightClock();
	const auto visual = static_cast<float>(static_cast<int32_t>(clock.GetVisualTime()));
	return std::clamp(static_cast<int32_t>(clock.ScriptToVisual(visual)), 0, 23);
}

/// fn_00773ED0 / fn_00773F40: the temperature's target for the hour and the month
float TargetTemperature(float min, float max)
{
	const float hour = k_HourFactor[static_cast<size_t>(RelativeHour())];
	const auto month = static_cast<size_t>(std::clamp(calendar::GetMonth(Turn()), 1, 12));
	return hour * k_MonthFactor[month] * (max - min) + min;
}

/// fn_00773F40: the temperature goes towards the target by a tenth of the target's whole degrees each turn (so it
/// keeps oscillating around it: the Land 1 script saved 10.8 / 12)
void UpdateTemperature(Climate& climate, float min, float max)
{
	const float target = TargetTemperature(min, max);
	climate.targetTemperature = target;
	const auto step = static_cast<float>(std::abs(static_cast<int32_t>(target))) * 0.1f;
	if (target > climate.temperature)
	{
		climate.temperature += step;
	}
	else
	{
		climate.temperature -= step;
	}
}

/// fn_00773D60, once per game day: while falling only the days count; else the desire grows by two random shares of
/// the season's rainMax x 0.02 (the month, hour and nature terms of GClimateRainInfo are 0: that table is not in
/// info.dat, 0xDCB8D0 stays zeroed)
void UpdateRain(Rain& rain, float min, float max)
{
	(void)min;
	if ((rain.flags & 1) != 0)
	{
		++rain.rainingDays;
		return;
	}
	++rain.dryDays;
	if (--rain.rainingDays < 0)
	{
		rain.rainingDays = 0;
	}
	constexpr float k_MonthRain = 0.0f;  // GClimateRainInfo.month[m]
	constexpr float k_HourRain = 0.0f;   // .hour[RelativeTime]
	constexpr float k_NatureRain = 0.0f; // .natureRainDesire
	const float r1 = GameFloatRand(max * 0.02f);
	const float r2 = GameFloatRand(max * 0.02f);
	rain.desire = r2 + r1 + (k_NatureRain + 1.0f) * rain.desire + k_HourRain + k_MonthRain;
	if (rain.desire > 1.0f)
	{
		rain.desire = 1.0f;
	}
}

/// fn_00774AA0, once per game day: the wind from the season's range along windAngle. The weight is
/// int(exp(-((rain - 0.5) x 5)^2)), 1 only at a desire of exactly 0.5, so it is min + max almost always.
void UpdateWind(Climate& climate, float min, float max, float rain)
{
	const float t = (rain - 0.5f) * 5.0f;
	const auto k = static_cast<float>(static_cast<int32_t>(std::exp(-(t * t))));
	const float c = std::cos(climate.windAngle);
	const float s = std::sin(climate.windAngle);
	const float ax = min * c;
	const float bx = max * c;
	climate.windX = bx - (bx - ax) * k + ax;
	const float az = min * s;
	const float bz = max * s;
	climate.windZ = bz - (bz - az) * k + az;
}

/// GClimate::ToBeDeleted 0x7713E0: out of the list, its storms deleted
void Delete(Climate* climate)
{
	for (auto id : climate->storms)
	{
		storms::Destroy(id);
	}
	if (g_world == climate)
	{
		g_world = nullptr;
	}
	g_climates.remove_if([climate](const Climate& c) { return &c == climate; });
}

/// fn_00771170 / GClimate(0) 0x771020, the parts they share: the season's rain and temperature, the storm defaults
void InitFromInfo(Climate& climate)
{
	const auto* info = Info(climate.info);
	const uint32_t season = calendar::GetSeason(Turn());
	// fn_00773D30(rainMin, rainMax): the desire starts at rainMax and the raining days at int(rainMin x 100)
	climate.rain = {};
	climate.rain.desire = RainMax(info, season);
	climate.rain.rainingDays = static_cast<int32_t>(RainMin(info, season) * 100.0f);
	// fn_00773ED0(tempMin, tempMax)
	climate.temperature = TargetTemperature(TempMin(info, season), TempMax(info, season));
	climate.targetTemperature = climate.temperature;
	// fn_00774A90 does nothing: no wind until the script sets it
	climate.fallSpeed = static_cast<float>(static_cast<double>(WindMax(info, g_season)) * (1.0 / 30.0) + 0.5);
}

/// FindWhereToCreateStorm 0x772BE0: the world's anywhere (a random 10 m cell of the 512); a local climate's at
/// r^2 x innerRadius (r random 0..1) in a random direction. It retries (20 times at most) while the place is not in any
/// climate's radius and IsWater returns 1 (only off the land: see IsWaterReturnsOne).
glm::vec3 FindWhereToCreateStorm(const Climate& climate)
{
	glm::vec3 place(0.0f);
	for (int32_t tries = 0;;)
	{
		bool outside = true;
		if (!climate.world)
		{
			const float angle = GameFloatRand(std::numbers::pi_v<float> * 2.0f);
			const float r = GameFloatRand(1.0f);
			const float distance = r * r * climate.innerRadius * 0.1f; // in 10 m cells
			const float cx = std::floor(static_cast<float>(climate.x) / 65536.0f);
			const float cz = std::floor(static_cast<float>(climate.z) / 65536.0f);
			place.x = static_cast<float>(static_cast<int32_t>(std::cos(angle) * distance + cx)) * 10.0f;
			place.z = static_cast<float>(static_cast<int32_t>(std::sin(angle) * distance + cz)) * 10.0f;
		}
		else
		{
			place.x = static_cast<float>(GameRand(0x200)) * 10.0f;
			place.z = static_cast<float>(GameRand(0x200)) * 10.0f;
			for (const auto& other : g_climates)
			{
				const auto centre = other.CellCentre();
				if (std::hypot(place.x - centre.x, place.z - centre.z) < other.outerRadius)
				{
					outside = false;
					break;
				}
			}
		}
		++tries;
		if (!outside || tries >= 20 || !IsWaterReturnsOne(place.x, place.z))
		{
			break;
		}
	}
	return place;
}
} // namespace

glm::vec3 Climate::Centre() const
{
	return {static_cast<float>(x) * k_FromMapCoords, y, static_cast<float>(z) * k_FromMapCoords};
}

glm::vec3 Climate::CellCentre() const
{
	return {static_cast<float>((x >> 16) * 10), y, static_cast<float>((z >> 16) * 10)};
}

void climate::Reset()
{
	for (const auto& c : g_climates)
	{
		for (auto id : c.storms)
		{
			storms::Destroy(id);
		}
	}
	g_climates.clear();
	g_world = nullptr;
	g_climateSystem = true;
	g_stormCreation = true;
	g_lastDay = 0;
	g_lastSeason = 0;
	g_season = 0;
	g_nextId = 1;
}

Climate& climate::Create(const glm::vec3& position, int32_t info, float radius1, float radius2, float angle, int32_t id)
{
	Climate climate;
	if (id == 0)
	{
		// GClimate(0) 0x771020: the old world goes first; centre (2560, 2560) (words 0x100), radius 5120, WORLD
		if (g_world != nullptr)
		{
			Delete(g_world);
		}
		climate.id = 0;
		climate.info = 0;
		climate.x = 0x01000000;
		climate.z = 0x01000000;
		climate.y = 0.0f;
		climate.innerRadius = 5120.0f;
		climate.outerRadius = 5120.0f;
		climate.world = true;
		climate.maxStorms = 10;
		InitFromInfo(climate);
	}
	else
	{
		// fn_00771170
		climate.x = static_cast<int32_t>(position.x * k_ToMapCoords);
		climate.z = static_cast<int32_t>(position.z * k_ToMapCoords);
		climate.y = position.y;
		climate.info = info;
		climate.innerRadius = radius1 <= radius2 ? radius1 : radius2;
		climate.outerRadius = radius1 <= radius2 ? radius2 : radius1;
		InitFromInfo(climate);
		climate.windAngle = angle;
		climate.maxStorms = static_cast<int32_t>(radius2 * 0.001f + 1.0f);
		climate.world = false;
		// the id: the counter follows the script's ids
		if (g_nextId == id)
		{
			++g_nextId;
		}
		else if (g_nextId < id)
		{
			g_nextId = id + 1;
		}
		climate.id = id;
	}
	g_climates.push_front(std::move(climate));
	auto& created = g_climates.front();
	if (created.world)
	{
		g_world = &created;
	}
	return created;
}

Climate* climate::Find(int32_t id)
{
	for (auto& c : g_climates)
	{
		if (c.id == id)
		{
			return &c;
		}
	}
	return nullptr;
}

Climate& climate::World()
{
	if (g_world == nullptr)
	{
		Create(glm::vec3(0.0f), 0, 0.0f, 0.0f, 0.0f, 0);
	}
	return *g_world;
}

bool climate::HasWorld()
{
	return g_world != nullptr;
}

void climate::SetRain(int32_t id, const Rain& rain)
{
	Climate* c = id == 0 ? &World() : Find(id);
	if (c != nullptr)
	{
		c->rain = rain;
	}
}

void climate::SetTemperature(int32_t id, float temperature, float target)
{
	Climate* c = id == 0 ? g_world : Find(id); // 0x773290 reads the world without making it
	if (c != nullptr)
	{
		c->temperature = temperature;
		c->targetTemperature = target;
	}
}

void climate::SetWind(int32_t id, float windX, float windZ, float angle)
{
	Climate* c = id == 0 ? &World() : Find(id);
	if (c != nullptr)
	{
		c->windX = windX;
		c->windZ = windZ;
		c->windAngle = angle;
	}
}

void climate::CreateScriptStorm(int32_t id, const storms::StormDescriptor& descriptor, float age, float speed,
                                const glm::vec3& target)
{
	Climate* c = Find(id);
	if (c == nullptr)
	{
		return;
	}
	const auto stormId = storms::Create(descriptor);
	if (auto* storm = storms::Find(stormId))
	{
		storm->age = age;
		storm->speed = speed;
		storm->target = target;
	}
	c->storms.push_front(stormId);
}

weather::WeatherInfo climate::ComputeWeather(const glm::vec3& point, bool smooth)
{
	const auto& world = World();
	const weather::WeatherInfo grid = smooth ? atmos::GetWeatherSmooth(point, true) : atmos::GetWeather(point, true);

	// the world's own temperature and wind first ...
	auto temperature = static_cast<int8_t>(static_cast<int32_t>(world.temperature));
	auto windX = static_cast<int8_t>(static_cast<int32_t>(world.windX + 0.0f));
	auto windZ = static_cast<int8_t>(static_cast<int32_t>(world.windZ + 0.0f));
	// ... then every climate in the list, the world again included, weighted by the distance (1 inside the inner
	// radius, 0 at the outer)
	for (const auto& c : g_climates)
	{
		const auto centre = c.Centre();
		const float r = c.outerRadius;
		if (!(centre.x - r <= point.x) || centre.x + r < point.x || !(centre.z - r <= point.z) || centre.z + r < point.z)
		{
			continue;
		}
		const float dx = point.x - centre.x;
		const float dz = point.z - centre.z;
		const float d2 = dx * dx + dz * dz;
		if (d2 > r * r)
		{
			continue;
		}
		float f = d2 <= c.innerRadius * c.innerRadius ? 1.0f
		                                               : 1.0f - (std::sqrt(d2) - c.innerRadius) / (r - c.innerRadius);
		f = f <= 0.0f ? 0.0f : (f < 1.0f ? f : 1.0f);
		const auto t = static_cast<int8_t>(static_cast<int32_t>(c.temperature));
		const auto wx = static_cast<int8_t>(static_cast<int32_t>(c.windX));
		const auto wz = static_cast<int8_t>(static_cast<int32_t>(c.windZ));
		temperature = static_cast<int8_t>(static_cast<int32_t>(static_cast<float>(t) * f + static_cast<float>(temperature)));
		windX = static_cast<int8_t>(static_cast<int32_t>(static_cast<float>(wx) * f + static_cast<float>(windX)));
		windZ = static_cast<int8_t>(static_cast<int32_t>(static_cast<float>(wz) * f + static_cast<float>(windZ)));
	}

	weather::WeatherInfo result;
	result.temperature = ClampAdd(grid.temperature, temperature);
	result.windX = ClampAdd(grid.windX, windX);
	result.windZ = ClampAdd(grid.windZ, windZ);
	result.snowCover = ClampAdd(grid.snowCover, 0);
	result.snow = ClampAdd(0, grid.snow);
	result.rain = ClampAdd(grid.rain, 0);
	result.overcast = ClampAdd(0, grid.overcast);
	result.stamp = 0;
	return result;
}

void climate::CreateStorm(Climate& climate, uint32_t turn)
{
	(void)turn;
	const auto* info = Info(climate.info);
	const float rainMax = RainMax(info, g_season);
	const float rainMin = RainMin(info, g_season);
	const auto days = static_cast<float>(climate.rain.rainingDays);
	// rained enough this season: the desire starts again (fn_00773D50)
	if (!(rainMax > days * 0.01f))
	{
		climate.rain.desire = 0.0f;
		climate.rain.dryDays = 0;
		return;
	}
	if (static_cast<int32_t>(climate.storms.size()) >= climate.maxStorms)
	{
		return;
	}
	const glm::vec3 place = FindWhereToCreateStorm(climate);
	uint32_t size;
	if (climate.world)
	{
		size = GameRand(1000);
	}
	else
	{
		const auto centre = climate.Centre();
		size = static_cast<uint32_t>(static_cast<int32_t>(std::hypot(place.x - centre.x, place.z - centre.z)));
	}
	size = std::clamp<uint32_t>(size, 160, 900);

	storms::StormDescriptor d;
	// fn_0083F4A0(pos, inner, outer): the 10 m cell at a height of 300
	d.position = glm::vec3(place.x, 300.0f, place.z);
	d.innerRadius = static_cast<float>(size);
	d.outerRadius = static_cast<float>(static_cast<int32_t>(static_cast<double>(size) * 1.1));
	d.fadeInTime = 10.0f;
	d.lifeTime = rainMax * 10.0f * 10.0f;
	if (!(rainMin < days * 0.01f))
	{
		d.lifeTime = (rainMax * 100.0f - days) * 10.0f;
	}
	if (d.lifeTime < 20.0f)
	{
		d.lifeTime = 20.0f;
	}
	d.strength = 1.0f;
	const auto temperature = static_cast<int8_t>(static_cast<int32_t>(climate.temperature));
	d.elevation = static_cast<float>(climate.stormElevation);
	d.fallSpeed = climate.fallSpeed;
	d.weather = {};
	d.weather.temperature = temperature;
	// blackness exp(-((t - 30) / 15)^2); above 30 degrees (or with +0x84) a thunderstorm with the climate's lightning
	const float x = static_cast<float>(temperature);
	const auto u = static_cast<float>((static_cast<double>(x) - 30.0) * (1.0 / 15.0));
	d.blackness = std::exp(-(u * u));
	if (x > 30.0f || climate.lightning != 0)
	{
		d.sheetMin = climate.sheetMin;
		d.sheetMax = climate.sheetMax;
		d.forkMax = climate.forkMax;
		d.forkMin = climate.forkMin;
		d.blackness = static_cast<float>(climate.lightning);
	}
	d.weather.overcast = static_cast<int8_t>(static_cast<int32_t>(d.blackness * 100.0f));
	if (temperature < 0)
	{
		d.weather.snow = 100;
		d.weather.rain = 0;
	}
	else
	{
		// snow share exp(-(t x 0.2)^2): all rain above about 10 degrees
		const auto v = static_cast<float>(static_cast<double>(x) * 0.2);
		const auto snow = static_cast<int32_t>(static_cast<double>(std::exp(-(v * v))) * 100.0);
		d.weather.snow = static_cast<int8_t>(snow);
		d.weather.rain = static_cast<int8_t>(100 - snow);
	}
	d.weather.windX = static_cast<int8_t>(static_cast<int32_t>(climate.windX));
	d.weather.windZ = static_cast<int8_t>(static_cast<int32_t>(climate.windZ));
	if (d.lifeTime > 0.0f)
	{
		if (d.lifeTime < 8.0f)
		{
			d.lifeTime = 20.0f;
		}
		const auto id = storms::Create(d);
		climate.storms.push_front(id);
		climate.rain.flags |= 1;
		// (the original's GDebug message; there is no "game" logger in the unit tests)
		if (auto logger = spdlog::get("game"); logger)
		{
			SPDLOG_LOGGER_INFO(logger,
			                   "Weather: climate {} makes a storm at ({:.0f}, {:.0f}) r {:.0f}/{:.0f}, life {:.0f} s, "
			                   "{} deg, rain {} snow {}",
			                   climate.id, d.position.x, d.position.z, d.innerRadius, d.outerRadius, d.lifeTime,
			                   static_cast<int>(d.weather.temperature), static_cast<int>(d.weather.rain),
			                   static_cast<int>(d.weather.snow));
		}
	}
	// fn_00773D50: the desire starts again
	climate.rain.desire = 0.0f;
	climate.rain.dryDays = 0;
}

namespace
{
/// fn_00772330: one climate's turn
void ProcessClimate(Climate& climate, bool newDay, uint32_t turn)
{
	(void)climate::World();
	const auto* info = Info(climate.info);
	if (g_climateSystem)
	{
		UpdateTemperature(climate, TempMin(info, g_season), TempMax(info, g_season));
	}

	// its storms: forget the gone ones; the rest drift with the atmosphere's wind (x 0.01 m per turn) and, on a new
	// day, start fading when they left the climate (a world storm: when it is in any climate), or when it rained enough
	auto it = climate.storms.begin();
	while (it != climate.storms.end())
	{
		storms::Storm* storm = storms::Find(*it);
		if (storm == nullptr)
		{
			const auto gone = *it;
			climate.storms.remove(gone);
			if (climate.storms.empty())
			{
				climate.rain.flags &= ~1;
				break;
			}
			// the original goes on after the list's head
			it = std::next(climate.storms.begin());
			continue;
		}
		auto& d = storm->descriptor;
		const weather::WeatherInfo w = atmos::GetWeather(d.position, true);
		d.position.x += static_cast<float>(w.windX) * 0.01f;
		d.position.z += static_cast<float>(w.windZ) * 0.01f;
		if (newDay)
		{
			const float fadeOutAge = d.lifeTime - (d.fadeInTime + d.fadeInTime);
			if (climate.world)
			{
				for (const auto& other : g_climates)
				{
					const auto centre = other.CellCentre();
					if (std::hypot(d.position.x - centre.x, d.position.z - centre.z) < other.outerRadius &&
					    fadeOutAge > storm->age)
					{
						storm->age = fadeOutAge;
						climate.rain.desire = 1.0f;
						break;
					}
				}
			}
			else
			{
				const auto centre = climate.CellCentre();
				if (std::hypot(d.position.x - centre.x, d.position.z - centre.z) > climate.outerRadius &&
				    fadeOutAge > storm->age)
				{
					storm->age = fadeOutAge;
					climate.rain.desire = 1.0f;
				}
			}
			if (RainMax(info, g_season) < static_cast<float>(climate.rain.rainingDays) * 0.01f &&
			    d.lifeTime - (d.fadeInTime + d.fadeInTime) > storm->age)
			{
				storm->age = d.lifeTime - (d.fadeInTime + d.fadeInTime);
			}
		}
		++it;
	}

	if (newDay && g_climateSystem)
	{
		UpdateRain(climate.rain, RainMin(info, g_season), RainMax(info, g_season));
		UpdateWind(climate, WindMin(info, g_season), WindMax(info, g_season), climate.rain.desire);
		if (climate.rain.desire == 1.0f && g_stormCreation)
		{
			CreateStorm(climate, turn);
		}
	}
}
} // namespace

void climate::ProcessAll(uint32_t turn)
{
	(void)World();
	// (the GDebug messages about the climate at the hand are skipped)
	bool newDay = false;
	const auto day = static_cast<int32_t>(calendar::GetDayOfMonth(turn));
	if (g_lastDay != day)
	{
		newDay = true;
		g_lastDay = day;
		const uint32_t season = calendar::GetSeason(turn);
		if (g_lastSeason != season)
		{
			g_season = season;
			g_lastSeason = season;
		}
	}
	for (auto& c : g_climates)
	{
		ProcessClimate(c, newDay, turn);
	}
}

void climate::SetClimateSystemEnabled(bool on)
{
	g_climateSystem = on;
}

void climate::SetStormCreationEnabled(bool on)
{
	g_stormCreation = on;
}

bool climate::IsClimateSystemEnabled()
{
	return g_climateSystem;
}

bool climate::IsStormCreationEnabled()
{
	return g_stormCreation;
}

uint32_t climate::CurrentSeason()
{
	return g_season;
}

void climate::ForEach(const std::function<void(const Climate&)>& function)
{
	for (const auto& c : g_climates)
	{
		function(c);
	}
}
