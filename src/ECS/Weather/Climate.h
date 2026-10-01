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

#include <functional>
#include <list>

#include <glm/vec3.hpp>

#include "Storms.h"
#include "WeatherInfo.h"

// GClimate (Weather.cpp / WeatherRain.cpp of the original, 0x88 bytes, list g_game+0x205CF4, newest first): the world's
// climate (id 0, g_game+0x250534, centre (2560, 2560), radius 5120) and the local ones of the map script
// (CREATE_WEATHER_CLIMATE*). Each adds its temperature and wind to ComputeWeather within its radius; once per game day
// its rain desire grows and at 1 it makes a natural storm (an LH3DStorm) somewhere in it.

namespace openblack::weather::climate
{
/// The rain part (+0x34, 16 bytes; CREATE_WEATHER_CLIMATE_RAIN sets all four)
struct Rain
{
	float desire {0.0f};       ///< +0x34 0..1 ("The Desire To Rain")
	int32_t dryDays {0};       ///< +0x38 days without falling
	int32_t rainingDays {0};   ///< +0x3C ("Raining Turns", counted per game day): x 0.01 against the season's rainMax
	uint8_t flags {0};         ///< +0x40 bit 0: a storm of this climate is falling
};

struct Climate
{
	int32_t id {0};                   ///< +0x28
	int32_t info {0};                 ///< +0x2C GClimateInfo index (the world's is always 0, WORLD)
	int32_t x {0};                    ///< +0x14 MapCoords 16.16 (metres x 6553.6)
	int32_t z {0};                    ///< +0x18
	float y {0.0f};                   ///< +0x1C
	float innerRadius {5120.0f};      ///< +0x20 the smaller of the script's radii
	float outerRadius {5120.0f};      ///< +0x24
	Rain rain;                        ///< +0x34
	float temperature {0.0f};         ///< +0x44
	float targetTemperature {0.0f};   ///< +0x48 recomputed every turn from the time of day and the month
	float windX {0.0f};               ///< +0x4C
	float windZ {0.0f};               ///< +0x50
	float windAngle {0.0f};           ///< +0x54 (radians; the 5th ctor argument, 0 from the script, then _WIND's third)
	int32_t maxStorms {10};           ///< +0x58 10 for the world, else int(radius2 x 0.001 + 1)
	std::list<storms::StormId> storms; ///< +0x5C its natural storms, newest first
	bool world {false};               ///< +0x64 bit 0
	int32_t stormElevation {500};     ///< +0x68 (0x1F4; ctor 0x77113E, fn_00771170 0x7712C6)
	float fallSpeed {0.5f};           ///< +0x6C windMax[season] x 1/30 [0x980518] + 0.5 [0x8AB260] (0x77114B)
	float unknown0x70 {1.0f};         ///< +0x70 (ctor 0x771137, fn_00771170 0x7712B6)
	// +0x74..+0x80: 5 / 60 (0x40A00000 / 0x42700000) stored at 0x77111B..0x771134 and 0x7712A6..0x7712C3
	float sheetMin {5.0f};            ///< +0x74 the lightning of its hot storms
	float sheetMax {60.0f};           ///< +0x78
	float forkMin {5.0f};             ///< +0x7C
	float forkMax {60.0f};            ///< +0x80
	/// +0x84 lightning below 30 degrees too; fn_00771170 zeroes it (0x7712E0), no other writer found (UNVERIFIED)
	uint8_t lightning {0};

	/// The centre as every reader of it builds its LHPoint: "fild; fmul [0x8AA3A4]" (= 10 / 65536,
	/// ecs::map_coords::ToMetres) on x and z. ComputeWeather, ProcessAll (0x771DA0) and FindWhereToCreateStorm
	/// (0x772D3E, 0x772D6F) all do it the same way
	[[nodiscard]] glm::vec3 Centre() const;
	/// The centre as ProcessClimate (fn_00772330) builds it for its two GetDistance 0x74CDE0 calls: the unsigned high
	/// word alone, times 10 ("xor eax, eax; mov ax, [esi+0x16]; lea eax, [eax+eax*4]; shl eax, 1; fild", and the same
	/// on +0x1A: 0x7724A6..0x7724DE, and on [ebp+0x16]/[ebp+0x1A]: 0x772510..0x772548), so without the fraction
	[[nodiscard]] glm::vec3 CellCentre() const;
};

/// InitStaticsValues 0x54A829..0x54A849 and a cleared GClimate list (a new land): no climate, the climate system and
/// the storm creation on, the day, season and id counters back to 0 / 0 / 1
void Reset();

/// fn_00771300 (CREATE_WEATHER_CLIMATE, case 60 0x7171F5): id 0 makes the world climate (GClimate(0) 0x771020, which
/// replaces the old one and ignores the rest), any other a local one (fn_00771170: radii sorted, the season's rain and
/// temperature). Both go to the head of the list.
Climate& Create(const glm::vec3& position, int32_t info, float radius1, float radius2, float angle, int32_t id);
/// fn_007731B0: the first climate with that id (the newest), or nullptr
[[nodiscard]] Climate* Find(int32_t id);
/// g_game+0x250534, created on demand (fn_00771300 with zeros) by every caller
Climate& World();
[[nodiscard]] bool HasWorld();

/// CREATE_WEATHER_CLIMATE_RAIN (0x717250 -> 0x773200): the four values; id 0 = the world (made if missing)
void SetRain(int32_t id, const Rain& rain);
/// CREATE_WEATHER_CLIMATE_TEMP (0x7172A2 -> 0x773290): +0x44 and +0x48 (id 0 needs the world to exist already)
void SetTemperature(int32_t id, float temperature, float target);
/// CREATE_WEATHER_CLIMATE_WIND (0x7172E0 -> 0x7732D0): +0x4C..+0x54
void SetWind(int32_t id, float windX, float windZ, float angle);
/// CREATE_WEATHER_STORM (case 0x717328): a storm of that climate (nothing if the id is unknown)
void CreateScriptStorm(int32_t id, const storms::StormDescriptor& descriptor, float age, float speed,
                       const glm::vec3& target);

/// GClimate::ComputeWeather 0x771640
[[nodiscard]] WeatherInfo ComputeWeather(const glm::vec3& point, bool smooth);

/// GClimate::ProcessAll 0x771BE0, once per game turn (GGame::ProcessTurn, after the scripts)
void ProcessAll(uint32_t turn);
/// GClimate::CreateStorm 0x772E00
void CreateStorm(Climate& climate, uint32_t turn);

/// PAUSE_UNPAUSE_CLIMATE_SYSTEM 0x6FF4E0 (0xC24759): the temperature, rain and wind updates
void SetClimateSystemEnabled(bool on);
/// PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM 0x6FF500 (0xC24758)
void SetStormCreationEnabled(bool on);
[[nodiscard]] bool IsClimateSystemEnabled();
[[nodiscard]] bool IsStormCreationEnabled();
/// 0xDCB8C0: the season the climates use (changed on the first day of a new season)
[[nodiscard]] uint32_t CurrentSeason();

/// Every climate, newest first
void ForEach(const std::function<void(const Climate&)>& function);
} // namespace openblack::weather::climate
