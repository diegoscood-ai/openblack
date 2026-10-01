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
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "Locator.h"

using namespace openblack;

namespace
{
constexpr double k_WorldToFixed = 6553.6f;         // [0x8AC400]: 65536 / 10
constexpr double k_FixedToWorld = 0.000152587890625; // [0x8AA3A4], 10 * [0x8AC41C] (10 / 65536)

/// MapCoords: x and z 16.16 fixed cells (the high word is the cell), y the height above the ground
struct MapCoords
{
	int32_t x;
	int32_t z;
	float y;
};

MapCoords FromWorld(glm::vec3 point)
{
	// MapCoords(LHPoint) 0x603160: fld; fmul [0x8AC400]; __ftol (truncated)
	return {static_cast<int32_t>(point.x * k_WorldToFixed), static_cast<int32_t>(point.z * k_WorldToFixed), point.y};
}

glm::vec3 ToWorld(const MapCoords& coords)
{
	return {static_cast<float>(coords.x * k_FixedToWorld), coords.y, static_cast<float>(coords.z * k_FixedToWorld)};
}

/// The cell: the high words, read unsigned ("xor edx, edx; mov dx, [ecx + 2]")
glm::ivec2 CellOf(const MapCoords& coords)
{
	return {static_cast<int32_t>(static_cast<uint32_t>(coords.x) >> 16u),
	        static_cast<int32_t>(static_cast<uint32_t>(coords.z) >> 16u)};
}

/// MapCoords::operator+=(JustMapXZ) (0x605470): 16-bit adds to the high words, the fractions stay
void AddCells(MapCoords& coords, int16_t dx, int16_t dz)
{
	const auto move = [](int32_t value, int16_t d) {
		const auto word = static_cast<uint16_t>((static_cast<uint32_t>(value) >> 16u) + static_cast<uint16_t>(d));
		return static_cast<int32_t>((static_cast<uint32_t>(word) << 16u) | (static_cast<uint32_t>(value) & 0xFFFFu));
	};
	coords.x = move(coords.x, dx);
	coords.z = move(coords.z, dz);
}

/// MapCoords::InBounds (0x6042C0): the cell is inside the game map ([g_game+0x59C8] x [g_game+0x59C4] cells)
bool InBounds(const LandIslandInterface& island, const MapCoords& coords)
{
	const auto cell = CellOf(coords);
	const int32_t cells = island.GetCellsPerSide();
	return cell.x < cells && cell.y < cells;
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

/// GUtils::Spiral (0x74D7E0), the direction table 0xDA59FC (crt_xc_fn_TribeInfo_0074CA00)
struct Spiral
{
	static constexpr std::array<std::array<int16_t, 2>, 4> k_Steps {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
	int32_t direction {1};
	int32_t count {1};

	const std::array<int16_t, 2>& Next()
	{
		if (--count == 0)
		{
			++direction;
			count = direction / 2;
		}
		return k_Steps[static_cast<size_t>(direction & 3)];
	}
};

std::optional<MapCoords> NearestCoastal(const LandIslandInterface& island, const MapCoords& from, double radius)
{
	MapCoords coords = from;
	Spiral spiral;
	for (int32_t steps = 999999; steps != 0;) // 0x74E2FF
	{
		// fcomp; test ah, 0x41; je: stop once farther than the radius
		if (!(DistanceInMetres(from, coords) <= radius))
		{
			return std::nullopt;
		}
		if (InBounds(island, coords) && ecs::sea_cells::IsCoastal(island, CellOf(coords)))
		{
			return coords;
		}
		--steps;
		const auto& step = spiral.Next();
		AddCells(coords, step[0], step[1]);
	}
	return std::nullopt;
}

std::optional<MapCoords> NearestStreamPos(const LandIslandInterface& island, const MapCoords& from, float radius)
{
	// the LHPoint of `from` (its y, GetAltitude + from.y, is not used by the xz distance)
	const auto x = static_cast<float>(from.x * k_FixedToWorld);
	const auto z = static_cast<float>(from.z * k_FixedToWorld);
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
	MapCoords out {static_cast<int32_t>(best->x * k_WorldToFixed), static_cast<int32_t>(best->z * k_WorldToFixed), 0.0f};
	// y: the point's altitude above the ground at MapCoords(x * 65536 * 0.1, z * 65536 * 0.1)
	const MapCoords ground {static_cast<int32_t>(best->x * 65536.0 * 0.1f), static_cast<int32_t>(best->z * 65536.0 * 0.1f),
	                        0.0f};
	const auto groundWorld = ToWorld(ground);
	out.y = best->y - island.GetHeightAt({groundWorld.x, groundWorld.z});
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
	return static_cast<float>(DistanceInMetres(FromWorld(a), FromWorld(b)));
}

std::optional<glm::vec3> FindNearestCoastalTo(const LandIslandInterface& island, glm::vec3 from, float radius)
{
	const auto coords = NearestCoastal(island, FromWorld(from), radius);
	return coords ? std::optional(ToWorld(*coords)) : std::nullopt;
}

std::optional<glm::vec3> FindNearestStreamPosTo(const LandIslandInterface& island, glm::vec3 from, float radius)
{
	const auto coords = NearestStreamPos(island, FromWorld(from), radius);
	return coords ? std::optional(ToWorld(*coords)) : std::nullopt;
}

bool FindNearestDrinkingWater(const LandIslandInterface& island, glm::vec3 from, glm::vec3& out, float radius)
{
	MapCoords coords = FromWorld(out);
	if (!NearestDrinkingWater(island, FromWorld(from), coords, radius))
	{
		return false;
	}
	out = ToWorld(coords);
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
