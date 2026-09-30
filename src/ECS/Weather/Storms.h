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

#include <glm/vec3.hpp>

#include "WeatherInfo.h"

// The registered weather volumes: LH3DStorm (the 0x50-byte descriptor) and GWeather (the 0x3C0-byte object built from
// it, list 0xEEA37C, newest first). Each one adds its WeatherInfo to the LH3DAtmos grid inside its radius
// (GWeather::CalcAtmos 0x8400E0). The storm miracle (UR_CloudGather), the climates' natural storms, the map command
// CREATE_WEATHER_STORM and the CHL weather things create them.

namespace openblack::weather::storms
{
/// A handle: the original keeps the GWeather pointer and asks fn_0083F8D0 whether it is still in the list
using StormId = uint32_t;
constexpr StormId k_NoStorm = 0;

/// LH3DStorm (0x50), defaults of its constructor fn_0083F3F0. Names from the CHL setters (CHANGE_*_PROPERTIES,
/// WeatherThing 0x774400..0x774520).
struct StormDescriptor
{
	glm::vec3 position {0.0f};   ///< +0x00 the current centre (x, height, z)
	float innerRadius {100.0f};  ///< +0x0C full strength inside
	float outerRadius {300.0f};  ///< +0x10 nothing outside
	float fadeInTime {10.0f};    ///< +0x14 seconds (also the fade-out)
	float lifeTime {100.0f};     ///< +0x18 seconds
	float strength {1.0f};       ///< +0x1C the fade at full strength
	int32_t numClouds {8};       ///< +0x20 CHANGE_CLOUD_PROPERTIES
	float blackness {0.5f};      ///< +0x24
	float elevation {160.0f};    ///< +0x28 the clouds' height above the land
	float fallSpeed {1.0f};      ///< +0x2C CHANGE_WEATHER_PROPERTIES (the rain drawing)
	float sheetMin {0.0f};       ///< +0x30 sheet lightning (flash and thunder) every sheetMin..sheetMax s; 0 = none
	float sheetMax {0.0f};       ///< +0x34
	float forkMin {0.0f};        ///< +0x38 fork lightning (a PSys strike) every forkMin..forkMax s
	float forkMax {0.0f};        ///< +0x3C
	float snowCoverRate {1.2f};  ///< +0x40 x snow byte x fade: snow laid on the land (SnowCover, not ported)
	uint32_t unknown0x44 {0};    ///< +0x44 never set
	/// +0x48 what the storm adds (fn_0083F3F0: 10 degrees, rain 100, overcast 100, wind (10, 0))
	WeatherInfo weather {10, 100, 0, 100, 10, 0, 0, 0};
};

/// GWeather (0x3C0; +0x08 is the descriptor)
struct Storm
{
	StormId id {k_NoStorm};
	StormDescriptor descriptor;    ///< +0x08
	float age {0.0f};              ///< +0x58 seconds
	glm::vec3 target {0.0f};       ///< +0x5C where it moves to (its creation point unless someone moves it)
	float speed {1.0f};            ///< +0x68 metres per second towards the target (0 in the ctor fn_0083F4E0)
	bool arrived {true};           ///< +0x6C
	int32_t deleteCounter {0};     ///< +0x94 0 = alive; set by fn_0083F7B0, deleted when it passes 2
	float sheetTimer {0.0f};       ///< +0x98
	float forkTimer {0.0f};        ///< +0x9C
	glm::vec3 drawPosition {0.0f}; ///< +0xA0 the position used by CalcAtmos this turn
	float innerRadius {0.0f};      ///< +0xAC the inner radius, faded in and out with the storm
	float outerRadius {0.0f};      ///< +0xB0
	float fade {0.0f};             ///< +0xB4 0 .. strength
	bool drawClouds {false};       ///< +0x3C0 (the 0x3C8 GWeather of the weather things; bit 10 of the thing's flags)
};

/// fn_0083F6F0 -> fn_0083F590: a new GWeather at the head of the list (speed 1, target = its position)
StormId Create(const StormDescriptor& descriptor);
/// fn_0083F8D0: the storm while it is in the list and not marked for deletion, else nullptr
[[nodiscard]] Storm* Find(StormId id);
/// fn_0083F7B0: marks it (+0x94 = 1); it still updates for two turns and is deleted on the third (fn_0083F840)
void MarkForDeletion(StormId id);
/// vt 0x10 (the deleting destructor; fn_0083F630 unlinks it): gone at once. GClimate::ToBeDeleted 0x7713E0 does this
/// with its storms.
void Destroy(StormId id);
/// fn_0083F750 (KILL_STORMS_IN_AREA): marks every storm whose 2D distance to `position` is below radius + its outer
/// radius
void KillStormsInArea(const glm::vec3& position, float radius);
/// fn_0083F840 from LH3DAtmos::UpdateGame: GWeather::Update of each storm, then the deletion count
void UpdateAll(float seconds);
/// GWeather::CalcAtmos 0x8400E0 of every storm not marked for deletion, newest first (fn_00834E20)
void CalcAtmosAll(const glm::vec3& point, WeatherInfo& weather);
/// GWeather::CalcAtmos 0x8400E0 of one storm
void CalcAtmos(const Storm& storm, const glm::vec3& point, WeatherInfo& weather);
/// Every storm, newest first (the debug hooks and the drawing)
void ForEach(const std::function<void(const Storm&)>& function);
/// InitStaticsValues 0x54A8D1: deletes every storm (a new land)
void Clear();

/// The engine's callbacks for the storms' lightning (GWeather::Update): the fork one creates a PSys strike
/// (0xEEA384 = 0x68E8F0, PSysGlobal::InitializeOneTimeOnly), the sheet one plays the thunder (0xEEA388 = 0x429CE0,
/// GAudio); both get the storm, the point (storm x, z, land height + elevation) and the outer radius. Unset = nothing
/// (the lightning PSys and the thunder are other lanes'); the light flash fn_00837290 is not ported either.
using LightningCallback = std::function<void(const Storm&, const glm::vec3&, float)>;
void SetForkCallback(LightningCallback callback);
void SetSheetCallback(LightningCallback callback);
} // namespace openblack::weather::storms
