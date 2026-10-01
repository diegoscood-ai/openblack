/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WaterQueries.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Stream.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "Locator.h"

using namespace openblack;

namespace
{
namespace map_coords = ecs::map_coords;
using map_coords::MapCoords;
constexpr double k_FixedToWorld = map_coords::k_MetresPerFixed; // [0x8AA3A4] (10 / 65536), for the distances

/// MapCoords(LHPoint) 0x603160 for x and z (fld; fmul [0x8AC400]; __ftol); `altitude` carries the point's y as is
MapCoords FromPoint(glm::vec3 point)
{
	return {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z), point.y};
}

glm::vec3 ToPoint(const MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z)};
}

/// MapCoords::InBounds (0x6042C0): the cell is inside the game map ([g_game+0x59C8] x [g_game+0x59C4] cells)
bool InBounds(const LandIslandInterface& island, const MapCoords& coords)
{
	return map_coords::InBounds(coords, island.GetCellsPerSide());
}

uint32_t Bits(float value)
{
	uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

float FromBits(uint32_t bits)
{
	float value;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

/// fn_0074F590: 1024 mantissas (bits 13..22) of 1 / sqrt over [0.5, 2), indexed by the exponent's low bit and the 9
/// top mantissa bits
const std::array<uint32_t, 1024>& InverseSqrtTable()
{
	static const auto s_table = [] {
		std::array<uint32_t, 1024> table {};
		for (uint32_t i = 0; i < table.size(); ++i)
		{
			const float f = FromBits((0x3F800000u & 0xFF003FFFu) | ((i & 0x3FFu) << 14u));
			const auto r = static_cast<float>(1.0 / std::sqrt(static_cast<double>(f))); // fsqrt; fdivr 1; fst dword
			table[i] = r == 1.0f ? 0x7FE000u : (Bits(r) & 0x7FE000u);
		}
		return table;
	}();
	return s_table;
}

/// _FUN_0074F620: GUtils' table inverse square root (about 10 bits of mantissa)
float FastInverseSqrt(float value)
{
	const uint32_t bits = Bits(value);
	const uint32_t exponent = ((0xBE000000u - (bits & 0x7F800000u)) >> 1u) & 0x7F800000u;
	return FromBits(exponent | InverseSqrtTable()[(bits >> 14u) & 0x3FFu]);
}

/// hypotenuse(int, int) (0x74F680): 16.16 in, 16.16 out
int32_t Hypotenuse(int32_t dx, int32_t dz)
{
	const double x = dx * (1.0 / 65536.0); // fild; fmul [0x99A1D4]
	const double z = dz * (1.0 / 65536.0);
	const auto squared = static_cast<float>(x * x + z * z); // fstp dword
	return static_cast<int32_t>(65536.0 / FastInverseSqrt(squared)); // fdivr qword 65536.0; __ftol
}

/// hypotenuse(float, float) (0x74F6C0): 0 when both are within 0.0001
double Hypotenuse(float dx, float dz)
{
	if (std::abs(dx) <= 0.0001f && std::abs(dz) <= 0.0001f)
	{
		return 0.0;
	}
	const auto squared = static_cast<float>(static_cast<double>(dx) * dx + static_cast<double>(dz) * dz);
	return 1.0 / FastInverseSqrt(squared);
}

/// GUtils::GetDistanceInMetres (0x74CD70): GetDistance (0x74CCB0) then ConvertWholeDistanceToMeters (0x74DCC0)
double DistanceInMetres(const MapCoords& a, const MapCoords& b)
{
	return k_FixedToWorld * Hypotenuse(b.x - a.x, b.z - a.z);
}

std::optional<MapCoords> NearestCoastal(const LandIslandInterface& island, const MapCoords& from, double radius)
{
	MapCoords coords = from;
	map_coords::Spiral spiral; // GUtils::Spiral (0x74D7E0)
	for (int32_t steps = 999999; steps != 0;) // 0x74E2FF
	{
		// fcomp; test ah, 0x41; je: stop once farther than the radius
		if (!(DistanceInMetres(from, coords) <= radius))
		{
			return std::nullopt;
		}
		if (InBounds(island, coords) && ecs::sea_cells::IsCoastal(island, map_coords::Cell(coords)))
		{
			return coords;
		}
		--steps;
		map_coords::AddCells(coords, spiral.Next());
	}
	return std::nullopt;
}

std::optional<MapCoords> NearestStreamPos(const LandIslandInterface& island, const MapCoords& from, float radius)
{
	// the LHPoint of `from` (its y, GetAltitude + from.y, is not used by the xz distance)
	const auto x = map_coords::ToMetres(from.x);
	const auto z = map_coords::ToMetres(from.z);
	std::optional<glm::vec3> best;
	// the streams of [g_game+0x205C5C] (newest first: the GStream ctor pushes at the head, 0x733A8E..0x733AA0) and
	// their points (+0x14, next +0xC, script order). entt walks a storage from its last element to its first, so the
	// view also gives the newest stream first (streams are only removed all together, with the map). The order only
	// matters for two points at exactly the same distance: the first one found wins.
	Locator::entitiesRegistry::value().Each<const ecs::components::Stream>([&](const ecs::components::Stream& stream) {
		for (const auto& point : stream.points)
		{
			// GUtils::GetDistance(LHPoint, LHPoint) 0x74CDE0: xz, the differences stored as floats
			const double distance = Hypotenuse(point.x - x, point.z - z);
			if (distance < radius) // test ah, 1: strictly closer
			{
				radius = static_cast<float>(distance); // fst dword
				best = point;
			}
		}
	});
	if (!best)
	{
		return std::nullopt;
	}
	MapCoords out {map_coords::ToFixed(best->x), map_coords::ToFixed(best->z), 0.0f};
	// y: the point's altitude above the ground at MapCoords(x * 65536 * 0.1, z * 65536 * 0.1): (x * 2^16) * 0.1f is
	// rounded like x * 6553.6f (= 0.1f * 2^16), so the same MapCoords
	out.altitude = best->y - island.GetHeightAt(map_coords::ToMetres(out));
	return out;
}

bool NearestDrinkingWater(const LandIslandInterface& island, const MapCoords& from, MapCoords& out, float radius)
{
	if (const auto river = NearestStreamPos(island, from, radius))
	{
		out = *river;
		// fn_00605CD0: a coast no farther than the river (from `from`, not from the river point)
		const auto riverDistance = static_cast<float>(DistanceInMetres(from, out)); // fstp dword
		if (const auto coast = NearestCoastal(island, from, riverDistance))
		{
			out = *coast;
		}
		return true; // 0x74E3D6: found, whatever the coast search says
	}
	if (const auto coast = NearestCoastal(island, from, radius))
	{
		out = *coast;
		return true;
	}
	return false;
}

const LandIslandInterface* Island()
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}
} // namespace

namespace openblack::ecs::water_queries
{

float GetDistanceInMetres(glm::vec3 a, glm::vec3 b)
{
	return static_cast<float>(DistanceInMetres(FromPoint(a), FromPoint(b)));
}

std::optional<glm::vec3> FindNearestCoastalTo(const LandIslandInterface& island, glm::vec3 from, float radius)
{
	const auto coords = NearestCoastal(island, FromPoint(from), radius);
	return coords ? std::optional(ToPoint(*coords)) : std::nullopt;
}

std::optional<glm::vec3> FindNearestStreamPosTo(const LandIslandInterface& island, glm::vec3 from, float radius)
{
	const auto coords = NearestStreamPos(island, FromPoint(from), radius);
	return coords ? std::optional(ToPoint(*coords)) : std::nullopt;
}

bool FindNearestDrinkingWater(const LandIslandInterface& island, glm::vec3 from, glm::vec3& out, float radius)
{
	MapCoords coords = FromPoint(out);
	if (!NearestDrinkingWater(island, FromPoint(from), coords, radius))
	{
		return false;
	}
	out = ToPoint(coords);
	return true;
}

bool FindNearestDrinkingWater(const LandIslandInterface& island, DrinkingWater& water, glm::vec3 abodePosition,
                              float radius)
{
	// 0x407038: the flag bit takes the answer, +0x80 changes only where water was found
	water.found = FindNearestDrinkingWater(island, abodePosition, water.position, radius);
	return water.found;
}

std::optional<glm::vec3> GetNearestWaterPos(const DrinkingWater& water)
{
	return water.found ? std::optional(water.position) : std::nullopt;
}

std::optional<glm::vec3> FindNearestCoastalTo(glm::vec3 from, float radius)
{
	const auto* island = Island();
	return island != nullptr ? FindNearestCoastalTo(*island, from, radius) : std::nullopt;
}

std::optional<glm::vec3> FindNearestStreamPosTo(glm::vec3 from, float radius)
{
	const auto* island = Island();
	return island != nullptr ? FindNearestStreamPosTo(*island, from, radius) : std::nullopt;
}

bool FindNearestDrinkingWater(glm::vec3 from, glm::vec3& out, float radius)
{
	const auto* island = Island();
	return island != nullptr && FindNearestDrinkingWater(*island, from, out, radius);
}

} // namespace openblack::ecs::water_queries
