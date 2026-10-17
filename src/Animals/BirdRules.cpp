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
