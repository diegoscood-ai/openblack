/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BirdRules.h"

#include <cmath>

#include <algorithm>

#include "Common/GUtilsAngle.h"

using namespace openblack;
using namespace openblack::animals;

namespace
{
/// A bird's way goes by 16 m squares, over the map's 10 m cells, 512 each way
constexpr int32_t k_WaySquare = 16;
constexpr int32_t k_WaySquareShift = 4;
constexpr float k_CellsPerMetre = 0.1f;
constexpr int32_t k_LastCell = 511;
/// The temple's flock follows its model's height, scaled, 10 m above it
constexpr float k_TempleFlockAbove = 10.0f;
} // namespace

bool birds::CreatureCanPickUp(float heightAboveLand)
{
	return heightAboveLand < k_CreaturePickUpBelow;
}

bool birds::CreatureCanStompOn(float heightAboveLand, float creatureHeight)
{
	return heightAboveLand < creatureHeight * k_CreatureStompBelowShare;
}

AnimalInfo birds::TempleBirdKind(float alignment)
{
	return alignment >= 0.0f ? AnimalInfo::Dove : AnimalInfo::Bat;
}

uint32_t birds::TempleBirdCount(float alignment, uint32_t most, bool built)
{
	if (!built)
	{
		return 0;
	}
	// Cut down to a whole bird
	return static_cast<uint32_t>(std::abs(alignment) * static_cast<float>(most));
}

birds::TempleFlockStep birds::StepTowards(uint32_t members, uint32_t count)
{
	if (members < count)
	{
		return TempleFlockStep::AddOne;
	}
	return members > count ? TempleFlockStep::RemoveOne : TempleFlockStep::None;
}

float birds::TempleFlockHeight(float scale, float modelHeight)
{
	return scale * modelHeight + k_TempleFlockAbove;
}

float birds::NextLegAltitude(float lastGoalAltitude, float variance, float draw, float flockAltitude, float altitudeMin,
                             float altitudeMax)
{
	const float altitude = (lastGoalAltitude + variance) - draw;
	return flockAltitude + altitudeMin < altitude && altitude < flockAltitude + altitudeMax ? altitude : flockAltitude;
}

birds::FormationSlot birds::FormationSlotOf(int32_t members, int32_t indexFromNewest)
{
	int k = members - indexFromNewest;
	int b = 1;
	if (k >= 1)
	{
		do
		{
			++b;
		} while (k >= b * b);
	}
	int s = 1;
	if (k % 2 == 0)
	{
		k -= 1;
		s = -1;
	}
	return {.row = b - 5, .column = ((b * b - k) * s + 1) / 2};
}

map_coords::MapCoords birds::FormationGoal(FormationSlot slot, const map_coords::MapCoords& follower,
                                           const map_coords::MapCoords& leader)
{
	// ConvertGameAngleTo3D(GetAngleFromDXDZ(row, column) + GetAngleFromXZ(me, leader) + 0x400), the mask the
	// conversion's own
	const float radians = gutils::ConvertGameAngleTo3D(gutils::GetAngleFromDXDZ(slot.row, slot.column) +
	                                                   gutils::GetAngleFromXZ(follower, leader) + 0x400);
	// Not AddDistanceFromAngle (row on x, column on z, and the integer multiply then x 10: two roundings):
	// x = ((cos(a) row) 10 + leader.x x 10 x 2^-16) x 65536 / 10 truncated toward zero, z the same with sin and column. The
	// cosine is extended precision and the multiply rounds it once: the cosine in double
	const auto axis = [](double trig, int32_t count, int32_t centre) {
		const auto scaled = static_cast<float>(trig * static_cast<double>(count)); // rounded to a float once
		const float metres = scaled * 10.0f;
		const float sum = metres + map_coords::ToMetres(centre); // the leader's coordinate in metres added
		return map_coords::ToFixedGUtils(sum);                   // back to map coordinates, truncated
	};
	return {axis(std::cos(static_cast<double>(radians)), slot.row, leader.x),
	        axis(std::sin(static_cast<double>(radians)), slot.column, leader.z), 0.0f};
}

float birds::LowGoalStep(float altitude, float goalAltitude, float change)
{
	if (altitude > goalAltitude + change)
	{
		return altitude - change;
	}
	if (altitude < goalAltitude - change)
	{
		return altitude + change;
	}
	return altitude;
}

float birds::HighGoalStep(float absolute, float goalAbsolute, float change, float landNow)
{
	float now = absolute;
	if (goalAbsolute + change < absolute)
	{
		now = absolute - change;
	}
	else if (goalAbsolute - change > absolute)
	{
		now = absolute + change;
	}
	return std::max(now - landNow, k_LowestFlight);
}

int32_t birds::WaySquareCentre(int32_t fixed)
{
	const int32_t metres = map_coords::FtoL(map_coords::ToMetres(fixed));
	// a sixteenth towards zero (15 added below zero, then the arithmetic shift), then the square's middle
	return ((metres + (metres < 0 ? k_WaySquare - 1 : 0)) >> k_WaySquareShift) * k_WaySquare + (k_WaySquare / 2);
}

bool birds::LandAllTheWay(const map_coords::MapCoords& to, const map_coords::MapCoords& from,
                          const std::function<bool(int32_t cellX, int32_t cellZ)>& hasBlock)
{
	const auto toX = static_cast<float>(WaySquareCentre(to.x));
	const auto toZ = static_cast<float>(WaySquareCentre(to.z));
	const float dx = static_cast<float>(WaySquareCentre(from.x)) - toX;
	const float dz = static_cast<float>(WaySquareCentre(from.z)) - toZ;
	// towards the bird; nothing or not a number counts as backwards
	const float signX = dx > 0.0f ? 1.0f : -1.0f;
	const float signZ = dz > 0.0f ? 1.0f : -1.0f;
	const float wayX = signX * dx;
	const float wayZ = signZ * dz;
	float doneX = 0.0f;
	float doneZ = 0.0f;
	while (wayX > doneX || wayZ > doneZ)
	{
		// the axis behind the line goes on; level (or not a number), the longer one
		const float behind = (doneZ * wayX) - (doneX * wayZ);
		const bool stepX = behind == 0.0f || std::isnan(behind) ? wayX > wayZ : behind > 0.0f;
		(stepX ? doneX : doneZ) += static_cast<float>(k_WaySquare);
		const float x = (signX * doneX) + toX;
		const float z = (signZ * doneZ) + toZ;
		const int32_t cellX = map_coords::FtoL(x * k_CellsPerMetre);
		const int32_t cellZ = map_coords::FtoL(z * k_CellsPerMetre);
		if (cellX < 0 || cellX > k_LastCell || cellZ < 0 || cellZ > k_LastCell || !hasBlock(cellX, cellZ))
		{
			return false;
		}
	}
	return true;
}
