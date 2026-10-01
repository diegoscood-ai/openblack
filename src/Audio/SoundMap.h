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

#include <array>

#include <glm/vec3.hpp>

// GetSurfaceType (used by the miracles' SpellSounds) forwards to ecs::sea_cells::GetSurfaceType, the single source;
// the atmos part below is the water session's (W15).

namespace openblack::audio
{

/// GSoundMap::GetSurfaceType 0x71D8E0: the "surface" attribute of the .sad anim effect tables at a world position. 6
/// off the map (outside the cell grid or no land block there), 7 on a water cell (not MapCoords::IsLand 0x603720:
/// cell flag 0x10 set), else the surfaceSound of the cell's terrain material (1..8; anything else 3).
[[nodiscard]] int32_t GetSurfaceType(glm::vec3 position);

/// ATMOS_TYPE: the ambient zones (table 0x9CB048, 14 x {name, bank, day only})
enum class AtmosType : uint8_t
{
	None,
	Sea,
	StillFreshWater,
	Coastal,
	Jungle,
	Arctic,
	Desert,
	Countryside,
	Swamp,
	RunningWater,
	Stratosphere,
	Night,
	Rain,
	Wind,

	_Count
};
inline constexpr size_t k_AtmosTypeCount = static_cast<size_t>(AtmosType::_Count);

struct AtmosTypeInfo
{
	const char* name; ///< "ATMOS_TYPE_SEA" (the Dump's names)
	const char* bank; ///< the .sad of Audio\SFX\Atmos as openblack names its sound group ("ocean.sad"), nullptr for NONE
	bool dayOnly;     ///< +8: faded by the weather and silenced at night (jungle, desert, countryside, swamp, night)
};
/// 0x9CB048
extern const std::array<AtmosTypeInfo, k_AtmosTypeCount> k_AtmosTypes;

/// Terrain::GetAtmosType 0x7352B0: `(word[cell + 6] >> 10) & 0xF` = bits 2..5 of the cell's flags byte (LNDCell::flags);
/// 1 (SEA) outside the 512 x 512 cells or where no land block is.
[[nodiscard]] int32_t GetAtmosType(int32_t cellX, int32_t cellZ);

/// GCamera+0x80 (WeatherInfo, filled by GCamera::Update with LH3DAtmos::GetWeatherSmooth): the weather at the camera
struct CameraWeatherInfo
{
	int8_t temperature {0};
	int8_t rain {0};
	int8_t snow {0};
	int8_t overcast {0};
	int8_t windX {0};
	int8_t windZ {0};
};
/// The camera's weather: weather::atmos::GetWeatherSmooth 0x835180 (recalc) at the render camera's position, the call
/// GCamera::Update makes for GCamera+0x80 (byte 3 = overcast, then wind x / z).
[[nodiscard]] CameraWeatherInfo CameraWeather();

namespace sound_map
{

/// GSoundMap::Update 0x71D6F0 (CalculateRadiusPointAndDistance 0x71D800 + UpdateFromMap 0x71D720 + the volumes
/// fn_0071DD60), then GSoundMap::Dump 0x71D990 when OPENBLACK_ATMOS_TRACE is set. Once per game turn from GGame::EndTurn
/// (0x54E960). `skyType` = LH3DSky's 0..2 ([0xFA26BC], DayNightClock::GetSkyType).
void Update(float skyType);

/// +0xB4: the 14 atmos volumes 0..1 of the last Update (GAudio fn_00429100 copies them as the banks' targets)
[[nodiscard]] const std::array<float, k_AtmosTypeCount>& GetVolumes();
/// +0xEC: the receiver's (camera's) x of the last Update. fn_00429100 copies 15 floats from +0xB4, so this one lands in
/// GAudio's current[0] (NONE, no bank): only the atmos trace shows it.
[[nodiscard]] float GetReceiverX();

/// OPENBLACK_ATMOS_TRACE=<n>: the original's Dump lines every n turns (1 = every turn, like GDebug::SetMessage)
[[nodiscard]] bool TraceThisTurn();

} // namespace sound_map

} // namespace openblack::audio
