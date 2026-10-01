/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SoundMap.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Weather/Atmos.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

const std::array<AtmosTypeInfo, k_AtmosTypeCount> openblack::audio::k_AtmosTypes = {{
    {"ATMOS_TYPE_NONE", nullptr, false},
    {"ATMOS_TYPE_SEA", "ocean.sad", false},
    {"ATMOS_TYPE_STILL_FRESH_WATER", "lake.sad", false},
    {"ATMOS_TYPE_COASTAL", "shore.sad", false},
    {"ATMOS_TYPE_JUNGLE", "jungle.sad", true},
    {"ATMOS_TYPE_ARCTIC", "arctic.sad", false},
    {"ATMOS_TYPE_DESERT", "desert.sad", true},
    {"ATMOS_TYPE_COUNTRYSIDE", "country.sad", true},
    {"ATMOS_TYPE_SWAMP", "swamp.sad", true},
    {"ATMOS_TYPE_RUNNING_WATER", "stream.sad", false},
    {"ATMOS_TYPE_STRATOSPHERE", "high.sad", false},
    {"ATMOS_TYPE_NIGHT", "night.sad", true},
    {"ATMOS_TYPE_RAIN", "rain.sad", false},
    {"ATMOS_TYPE_WIND", "wind.sad", false},
}};

int32_t openblack::audio::GetSurfaceType(glm::vec3 position)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 6;
	}
	auto& island = Locator::terrainSystem::value();
	// MapCoords from an LHPoint: the cell is the 16.16 coordinate x 0.1 (10 units per cell); the grid is 512 cells (more
	// on the editor's bigger maps)
	const auto cx = static_cast<int32_t>(std::floor(position.x / 10.0f));
	const auto cz = static_cast<int32_t>(std::floor(position.z / 10.0f));
	const int32_t last = island.GetCellsPerSide() - 1;
	if (cx < 0 || cx > last || cz < 0 || cz > last)
	{
		return 6;
	}
	const auto& cell = island.GetCell(glm::u16vec2(cx, cz));
	// no land block there: LandIsland answers with its empty cell (all zero but fullWater)
	lnd::LNDCell empty {};
	empty.properties.fullWater = true;
	if (std::memcmp(&cell, &empty, sizeof(empty)) == 0)
	{
		return 6;
	}
	if (cell.properties.hasWater)
	{
		return 7;
	}
	// Terrain::GetMaterialInfo 0x735330: the second material of the cell's altitude in its country
	const auto& countries = island.GetCountries();
	const auto& materials = island.GetMaterialInfo();
	if (cell.properties.country >= countries.size())
	{
		return 3;
	}
	const auto& country = countries[cell.properties.country];
	const auto altitude = std::min<uint16_t>(island.GetCellAltitude(cell), 255);
	const auto material = country.materials[altitude].indices[1];
	if (material >= materials.size())
	{
		return 3;
	}
	const auto& info = Locator::infoConstants::value().terrainMaterial;
	const auto type = materials[material].type;
	const auto surface = type < info.size() ? static_cast<int32_t>(info[type].surfaceSound) : 3;
	return surface >= 1 && surface <= 8 ? surface : 3;
}

int32_t openblack::audio::GetAtmosType(int32_t cellX, int32_t cellZ)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 1;
	}
	const auto& island = Locator::terrainSystem::value();
	// 0x7352B0: x, z are u16 compared with 0x200 (the original's grid); openblack's editor maps can be bigger
	const int32_t cells = island.GetCellsPerSide();
	if (cellX < 0 || cellX >= cells || cellZ < 0 || cellZ >= cells)
	{
		return 1;
	}
	const glm::u16vec2 cell(cellX, cellZ);
	// g_index_block[x >> 4][z >> 4] == 0
	if (!island.HasBlockAt(cell))
	{
		return 1;
	}
	return (island.GetCell(cell).flags >> 2) & 0xF;
}

CameraWeatherInfo openblack::audio::CameraWeather()
{
	if (!Locator::camera::has_value())
	{
		return {};
	}
	// GCamera::Update: GCamera+0x80 = LH3DAtmos::GetWeatherSmooth(camera position, 1)
	const auto smooth = openblack::weather::atmos::GetWeatherSmooth(Locator::camera::value().GetOrigin(), true);
	CameraWeatherInfo info;
	info.temperature = smooth.temperature;
	info.rain = smooth.rain;
	info.snow = smooth.snow;
	info.overcast = smooth.overcast;
	info.windX = smooth.windX;
	info.windZ = smooth.windZ;
	return info;
}

namespace
{
/// GSoundMap+0x08: AtmosMapTypeInfo[14] (12 bytes each)
struct Entry
{
	uint16_t count {0};
	float dist2 {65535.0f}; ///< 0x477FFF00
	int16_t nearestX {0};
	int16_t nearestZ {0};
};

/// The one GSoundMap (g_game+0x250058)
struct State
{
	std::array<Entry, k_AtmosTypeCount> entries;
	uint16_t total {0};                         ///< +0xB0: cells of a type other than NONE
	std::array<float, k_AtmosTypeCount> volumes {}; ///< +0xB4
	glm::vec3 receiver {0.0f};                  ///< +0xEC
	int32_t receiverX {0};                      ///< +0xF8 MapCoords 16.16 cells
	int32_t receiverZ {0};                      ///< +0xFC
	float radius {50.0f};                       ///< +0x104
	float heightAboveLand {0.0f};               ///< +0x108
	float receiverHeight {0.0f};                ///< +0x10C
	uint32_t turn {0};
};
State g_Map;

float Altitude(float x, float z)
{
	// LH3DIsland::GetAltitude: the interpolated land height (0 off the island)
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
}

/// MapCoords(LHPoint): world x 6553.6 (16.16 fixed point of 10-unit cells)
int32_t ToMapCoord(float world)
{
	return static_cast<int32_t>(world * 6553.6f);
}

/// GSoundMap::Reset 0x71D6D0
void Reset()
{
	for (auto& entry : g_Map.entries)
	{
		entry.count = 0;
		entry.dist2 = 65535.0f;
	}
	g_Map.total = 0;
}

/// GSoundMap::AddAtmosType 0x71E260 + AtmosMapTypeInfo::Add 0x71D510: distances from the receiver to the cell's corner
void AddAtmosType(int32_t type, int32_t cellX, int32_t cellZ)
{
	auto& entry = g_Map.entries[(type >= 0 && type < static_cast<int32_t>(k_AtmosTypeCount)) ? type : 0];
	const float cornerX = static_cast<float>(cellX) * 10.0f;
	const float cornerZ = static_cast<float>(cellZ) * 10.0f;
	const float dx = cornerX - static_cast<float>(g_Map.receiverX) * 10.0f / 65536.0f;
	const float dz = cornerZ - static_cast<float>(g_Map.receiverZ) * 10.0f / 65536.0f;
	const float d2 = dx * dx + dz * dz;
	if (d2 < entry.dist2)
	{
		entry.dist2 = d2;
		entry.nearestX = static_cast<int16_t>(cellX * 10);
		entry.nearestZ = static_cast<int16_t>(cellZ * 10);
	}
	++entry.count;
	if (type != 0)
	{
		++g_Map.total;
	}
}

/// GSoundMap::CalculateRadiusPointAndDistance 0x71D800
void CalculateRadiusPointAndDistance(const GSoundInfo& info)
{
	// LH3DTech::g_camera 0xEA1DB8: the render camera's position
	const auto camera = Locator::camera::value().GetOrigin();
	g_Map.receiver = camera;
	g_Map.receiverX = ToMapCoord(camera.x);
	g_Map.receiverZ = ToMapCoord(camera.z);
	g_Map.receiverHeight = camera.y;
	g_Map.heightAboveLand = camera.y - Altitude(camera.x, camera.z);
	g_Map.radius = info.radiusForMinAtmosVolume;
}

/// GSoundMap::UpdateFromMap 0x71D720: the (2r/10 + 1)^2 cells around the receiver (11 x 11 with r = 50)
void UpdateFromMap()
{
	const auto r = static_cast<int32_t>(g_Map.radius / 10.0f * 65536.0f);
	Reset();
	for (int32_t cx = (g_Map.receiverX - r) >> 16; cx <= (g_Map.receiverX + r) >> 16; ++cx)
	{
		for (int32_t cz = (g_Map.receiverZ - r) >> 16; cz <= (g_Map.receiverZ + r) >> 16; ++cz)
		{
			AddAtmosType(GetAtmosType(cx, cz), cx, cz);
		}
	}
}

/// fn_0071DAC0: 1 below normalAtmosFadeStartHeight of the camera over the land at p, 0 above the end height
float HeightFade(const GSoundInfo& info, float x, float z)
{
	const float h = g_Map.receiver.y - Altitude(x, z);
	if (h < info.normalAtmosFadeStartHeight)
	{
		return 1.0f;
	}
	if (h <= info.normalAtmosFadeEndHeight)
	{
		return std::max(0.0f, 1.0f - (h - info.normalAtmosFadeStartHeight) /
		                                 (info.normalAtmosFadeEndHeight - info.normalAtmosFadeStartHeight));
	}
	return 0.0f;
}

/// radiusForMax/MinAtmosVolume: 1 up to 20 from the nearest cell of the type, 0 at 50
float Radial(const GSoundInfo& info, size_t type)
{
	const float d = std::sqrt(g_Map.entries[type].dist2);
	if (d <= info.radiusForMaxAtmosVolume)
	{
		return 1.0f;
	}
	return std::max(0.0f, 1.0f - (d - info.radiusForMaxAtmosVolume) /
	                                 (info.radiusForMinAtmosVolume - info.radiusForMaxAtmosVolume));
}

float NearestFade(const GSoundInfo& info, size_t type)
{
	return HeightFade(info, g_Map.entries[type].nearestX, g_Map.entries[type].nearestZ);
}

/// fn_0071DD60: the 14 volumes
void CalculateVolumes(const GSoundInfo& info, float skyType, const CameraWeatherInfo& weather)
{
	auto& vol = g_Map.volumes;
	// fn_0071DB80: the stratosphere, from the camera's absolute height (receiverHeight)
	const float y = g_Map.receiverHeight;
	float high = 0.0f;
	if (y < info.atmosphereHeight)
	{
		high = 0.0f;
	}
	else if (y < info.atmosphereMaxVolHeight)
	{
		high = std::min(1.0f, (y - info.atmosphereHeight) / (info.atmosphereMaxVolHeight - info.atmosphereHeight));
	}
	else if (y < info.spaceHeight)
	{
		high = std::max(0.0f, 1.0f - (y - info.atmosphereMaxVolHeight) / (info.spaceHeight - info.atmosphereMaxVolHeight));
	}

	// [0xFA26BC] LH3DSky type 0 day, 1 dusk, 2 night
	const float night = std::max(0.0f, skyType - 1.0f);
	float clear = (weather.snow > 0 ? 1.0f - weather.snow * 0.01f : 1.0f) - weather.rain * 0.01f;
	clear = std::max(clear, 0.0f);
	const float bad = 1.0f - clear;
	const float maxFade = info.weatherPercentageForMaxFade * 0.01f;
	const float weatherFade = bad < maxFade ? 1.0f - bad / maxFade : 0.0f;
	const float windX = weather.windX;
	const float windZ = weather.windZ;
	float wind = (std::sqrt(windX * windX + windZ * windZ) - 15.0f) * (1.0f / 30.0f);
	wind = wind > 1.0f ? 1.0f : (wind < 0.01f ? 0.0f : wind);

	vol[static_cast<size_t>(AtmosType::Stratosphere)] = high;
	vol[0] = 0.0f;
	for (size_t t = 1; t <= static_cast<size_t>(AtmosType::RunningWater); ++t)
	{
		if (g_Map.entries[t].count == 0)
		{
			vol[t] = 0.0f;
			continue;
		}
		float f = Radial(info, t);
		if (k_AtmosTypes[t].dayOnly)
		{
			f *= weatherFade * (1.0f - night);
		}
		vol[t] = f * NearestFade(info, t);
	}
	// the coast again, then the sea gives way to it (min(sea, 1 - coast))
	constexpr auto k_Sea = static_cast<size_t>(AtmosType::Sea);
	constexpr auto k_Coast = static_cast<size_t>(AtmosType::Coastal);
	float coast = 0.0f;
	if (g_Map.entries[k_Coast].count != 0)
	{
		coast = Radial(info, k_Coast);
		vol[k_Coast] = coast * NearestFade(info, k_Coast);
	}
	if (g_Map.entries[k_Sea].count != 0)
	{
		const float f = std::min(Radial(info, k_Sea), 1.0f - coast);
		vol[k_Sea] = f * NearestFade(info, k_Sea);
	}
	else
	{
		vol[k_Sea] = 0.0f;
	}
	// fn_0071E210(SEA): the sea's share of the typed cells
	const float seaFraction = g_Map.total != 0 ? static_cast<float>(g_Map.entries[k_Sea].count) / g_Map.total : 0.0f;
	vol[static_cast<size_t>(AtmosType::Night)] =
	    HeightFade(info, g_Map.receiver.x, g_Map.receiver.z) * (1.0f - seaFraction) * weatherFade * night;
	const float maxRain = info.weatherPercentageForMaxWeatherVolume;
	vol[static_cast<size_t>(AtmosType::Rain)] = weather.rain < maxRain ? weather.rain / maxRain : 1.0f;
	vol[static_cast<size_t>(AtmosType::Wind)] = wind;
}

const char* SurfaceName(int32_t surface)
{
	// 0xC22270
	static constexpr std::array<const char*, 9> k_Names = {
	    "SOUND SURFACE TYPE NONE",       "SOUND SURFACE TYPE GRASS",         "SOUND SURFACE TYPE GRAVEL",
	    "SOUND SURFACE TYPE HARD",       "SOUND SURFACE TYPE MUD",           "SOUND SURFACE TYPE SNOW",
	    "SOUND SURFACE TYPE DEEP WATER", "SOUND SURFACE TYPE SHALLOW WATER", "SOUND SURFACE TYPE LOOSE FOLIAGE"};
	return surface >= 0 && surface < static_cast<int32_t>(k_Names.size()) ? k_Names[surface] : "?";
}

/// GSoundMap::Dump 0x71D990 (GDebug::SetMessage lines)
void Dump()
{
	std::array<char, 256> line {};
	// X, Z = the high words of the receiver's MapCoords (its cell)
	std::snprintf(line.data(), line.size(), "Sound Map Calc Update X=%d Z=%d %s Count=%d",
	              static_cast<int>((static_cast<uint32_t>(g_Map.receiverX) >> 16) & 0xFFFF),
	              static_cast<int>((static_cast<uint32_t>(g_Map.receiverZ) >> 16) & 0xFFFF),
	              SurfaceName(GetSurfaceType(g_Map.receiver)), g_Map.total);
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
	// GInterface+0x3B8: the hand's MapCoords [G: taken as the player's (left) hand position]
	if (Locator::handSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(hand))
		{
			const auto& position = registry.Get<ecs::components::Transform>(hand).position;
			std::snprintf(line.data(), line.size(), "Sound Map At Hand X=%d Z=%d %s Count=%d",
			              static_cast<int>((static_cast<uint32_t>(ToMapCoord(position.x)) >> 16) & 0xFFFF),
			              static_cast<int>((static_cast<uint32_t>(ToMapCoord(position.z)) >> 16) & 0xFFFF),
			              SurfaceName(GetSurfaceType(position)), g_Map.total);
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
		}
	}
	std::snprintf(line.data(), line.size(), "Sound Radius=%3.3f Distance=%3.3f DistanceAboveLand=%3.3f",
	              static_cast<double>(g_Map.radius), static_cast<double>(g_Map.receiverHeight),
	              static_cast<double>(g_Map.heightAboveLand));
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", line.data());
}

uint32_t TracePeriod()
{
	static const uint32_t k_Period = [] {
		const char* value = std::getenv("OPENBLACK_ATMOS_TRACE");
		if (value == nullptr)
		{
			return 0u;
		}
		const int n = std::atoi(value);
		return n > 0 ? static_cast<uint32_t>(n) : 1u;
	}();
	return k_Period;
}
} // namespace

void sound_map::Update(float skyType)
{
	++g_Map.turn;
	if (!Locator::terrainSystem::has_value() || !Locator::camera::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value().sound;
	CalculateRadiusPointAndDistance(info);
	UpdateFromMap();
	CalculateVolumes(info, skyType, CameraWeather());
	if (TraceThisTurn())
	{
		Dump();
		// (openblack) the volumes themselves, which the original only shows as the banks' targets
		std::string volumes;
		for (size_t t = 1; t < k_AtmosTypeCount; ++t)
		{
			volumes += fmt::format(" {}={:.3f}({})", k_AtmosTypes[t].name + 11, g_Map.volumes[t], g_Map.entries[t].count);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "(openblack) Sound map volumes (count):{}", volumes);
	}
}

const std::array<float, k_AtmosTypeCount>& sound_map::GetVolumes()
{
	return g_Map.volumes;
}

float sound_map::GetReceiverX()
{
	return g_Map.receiver.x;
}

bool sound_map::TraceThisTurn()
{
	const auto period = TracePeriod();
	return period != 0 && g_Map.turn % period == 0;
}
