/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownDesireFlags.h"

#include <algorithm>

#include "Common/GUtilsAngle.h"

namespace openblack::ecs::town_desire_flags
{

std::optional<uint32_t> PointOf(int32_t place)
{
	if (place < 0)
	{
		return std::nullopt;
	}
	return k_FirstFlagPoint + static_cast<uint32_t>(place);
}

map_coords::MapCoords StartPlace(const map_coords::MapCoords& town, int32_t place)
{
	// the place times the step, worked out exactly and rounded to a float once
	const auto angle = static_cast<float>(static_cast<double>(place) * static_cast<double>(k_StartAngleStep));
	auto coords = town;
	coords += gutils::GetPosFromAngle(angle, k_StartDistance);
	return coords;
}

bool Shows(bool storeWorking, float level, float showsAfter)
{
	return storeWorking && showsAfter <= level;
}

float DesireAfterFrame(bool shown, float desire)
{
	return shown ? std::min(desire, 1.0f) : desire;
}

void Step(Zoomer& level, bool shown, float desire, float seconds)
{
	if (!shown)
	{
		level.SetPosition(desire);
		return;
	}
	level.SetDestinationWithSpeedAndTime(std::min(desire, 1.0f), 0.0f, 1.0f);
	level.Update(seconds);
}

float Rise(float level)
{
	return level * k_RiseHeight;
}

float DrawnScale(float level, float showsAfter)
{
	const float whole = (1.0f - showsAfter) * k_GrowShare;
	if (whole <= level)
	{
		return 1.0f;
	}
	return (level - showsAfter) / (whole - showsAfter);
}

std::optional<uint32_t> ToolTipWords(TownDesireInfo desire)
{
	// The words follow on from the tooltip "Food Desire", one for each desire in turn
	constexpr uint32_t k_FoodDesireWords = 91;
	switch (desire)
	{
	case TownDesireInfo::ForFood:
	case TownDesireInfo::ForWood:
	case TownDesireInfo::ForProtection:
	case TownDesireInfo::ForMercy:
	case TownDesireInfo::ForAbodes:
	case TownDesireInfo::ForCivicBuilding:
	case TownDesireInfo::ForChildren:
		return k_FoodDesireWords + static_cast<uint32_t>(desire);
	default:
		return std::nullopt;
	}
}

float NeedPercent(float desire)
{
	return std::min(desire, 1.0f) * 100.0f;
}

} // namespace openblack::ecs::town_desire_flags
