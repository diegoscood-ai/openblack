/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ViaPoint.h"

#include <cmath>
#include <cstdint>

using namespace openblack;

namespace
{
/// b - a on the map's 32-bit units, wrapping as the game's integer subtraction does
int32_t Delta(int32_t a, int32_t b)
{
	return static_cast<int32_t>(static_cast<uint32_t>(b) - static_cast<uint32_t>(a));
}

// The game works this out with every result rounded to a float, but a map-unit difference is used as the exact integer
// it is, which can be wider than a float. In double these operations are exact (or, for the quotient, rounded far
// below a float's precision), so one rounding to float gives the game's result.

/// a x b, rounded once to a float
float Product(double a, double b)
{
	return static_cast<float>(a * b);
}

/// a / b, rounded once to a float
float Quotient(double a, double b)
{
	return static_cast<float>(a / b);
}

/// a + b, rounded once to a float
float Sum(double a, double b)
{
	return static_cast<float>(a + b);
}

/// The length of a map-unit difference, its components exact in the products. The second one is also stored as a
/// float on the way, and that stored copy is the one multiplied by it
float Length(int32_t first, int32_t second, bool secondStored)
{
	const auto a = static_cast<double>(first);
	const auto b = static_cast<double>(second);
	const float aa = Product(a, a);
	const float bb = Product(b, secondStored ? static_cast<double>(static_cast<float>(second)) : b);
	return std::sqrt(aa + bb);
}
} // namespace

fire::ViaPoint fire::GetViaPoint(const map_coords::MapCoords& from, const map_coords::MapCoords& to,
                                 const map_coords::MapCoords& centre, float radius, float margin, float side)
{
	ViaPoint result;
	const float rad = radius * map_coords::k_FixedPerMetre;
	const float mar = margin * map_coords::k_FixedPerMetre;
	const int32_t ux = Delta(from.x, centre.x);
	const int32_t uz = Delta(from.z, centre.z);
	const float d = Length(ux, uz, true);
	if (!(d > rad))
	{
		result.inside = true;
		return result;
	}
	const float reach = rad + mar;
	const float s = reach / d;
	// Not a number when the start is within the margin: then no detour is taken
	const float c = std::sqrt(1.0f - s * s);
	const float uxn = Quotient(ux, d);
	const float uzn = static_cast<float>(uz) / d;
	const int32_t tx = Delta(from.x, to.x);
	const int32_t tz = Delta(from.z, to.z);
	const float length = Length(tx, tz, true);
	constexpr float k_Degenerate = 0.0001f;
	if (length < k_Degenerate)
	{
		return result;
	}
	const float txn = Quotient(tx, length);
	const float tzn = static_cast<float>(tz) / length;
	const float dot = tzn * uzn + txn * uxn;
	if (!(dot > c))
	{
		return result;
	}
	const float cross = txn * uzn - uxn * tzn;
	const bool leansLeft = !(cross >= 0.0f);
	const bool sideA = leansLeft ? !(side > 0.0f) : side < 0.0f;
	float angle = 0.0f;
	if (sideA)
	{
		result.point.x = map_coords::FtoL(Sum((-(c * uzn) - s * uxn) * reach, centre.x));
		result.point.z = map_coords::FtoL(Sum((c * uxn - s * uzn) * reach, centre.z));
		angle = -std::acos(dot);
	}
	else
	{
		result.point.x = map_coords::FtoL(Sum((c * uzn - s * uxn) * reach, centre.x));
		result.point.z = map_coords::FtoL(Sum((-(c * uxn) - s * uzn) * reach, centre.z));
		angle = std::acos(dot);
	}
	result.detour = true;
	if (Length(Delta(centre.x, to.x), Delta(centre.z, to.z), false) < rad)
	{
		result.inside = true;
		result.angle = angle;
		return result;
	}
	if (Length(Delta(from.x, result.point.x), Delta(from.z, result.point.z), false) < length)
	{
		result.angle = angle;
	}
	return result;
}

fire::WayRound fire::FindWayRound(const map_coords::MapCoords& from, const map_coords::MapCoords& destination,
                                  const Circle& reactedTo, std::span<const Circle> group, float margin)
{
	WayRound way {.target = destination};
	auto via = GetViaPoint(from, way.target, reactedTo.centre, reactedTo.radius, margin, 0.0f);
	float side = 0.0f;
	if (via.angle != 0.0f)
	{
		side = via.angle;
		way.target = via.point;
	}
	if (via.detour && via.inside)
	{
		way.outcome = WayRoundOutcome::Stop;
		return way;
	}
	int detours = 0;
	for (size_t i = 0; i < group.size();)
	{
		via = GetViaPoint(from, way.target, group[i].centre, group[i].radius, margin, side);
		if (via.angle == 0.0f && !(via.detour && via.inside))
		{
			++i;
			continue;
		}
		side = via.angle;
		way.target = via.point;
		// Every fire is looked at again on the way to the new point
		i = 0;
		if (++detours >= k_MostDetours)
		{
			way.outcome = WayRoundOutcome::GiveUp;
			return way;
		}
	}
	return way;
}
