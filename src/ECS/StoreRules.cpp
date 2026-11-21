/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StoreRules.h"

#include <algorithm>

using namespace openblack::ecs;

namespace
{
/// The meat and vegetable bits of a kind's food type
constexpr uint32_t k_EdibleFood = 3;
/// A store holds five piles of wood and one of food
constexpr int64_t k_WoodPiles = 5;

uint32_t Truncated(float value)
{
	return value > 0.0f ? static_cast<uint32_t>(value) : 0u;
}
} // namespace

float store_rules::TreeWoodValue(float life, float multiplier, uint32_t woodValue, float scale, float landBalance)
{
	return life * multiplier * static_cast<float>(woodValue) * scale * landBalance;
}

uint32_t store_rules::TreeWood(float life, float multiplier, uint32_t woodValue, float scale, float landBalance)
{
	return Truncated(TreeWoodValue(life, multiplier, woodValue, scale, landBalance));
}

uint32_t store_rules::DeadTreeWood(float scale, uint32_t woodValue, float multiplier)
{
	return Truncated(static_cast<float>(woodValue) * multiplier * scale);
}

uint32_t store_rules::FenceWood(float life, uint32_t woodValue, float scale)
{
	return Truncated(life * static_cast<float>(woodValue) * scale * scale * scale);
}

uint32_t store_rules::AnimalFood(float foodValue, uint32_t foodType)
{
	return (foodType & k_EdibleFood) != 0 ? Truncated(foodValue) : 0u;
}

uint32_t store_rules::MushroomFood(float foodValue)
{
	return Truncated(foodValue);
}

uint32_t store_rules::RockWood(float scale, uint32_t woodValue)
{
	// the wood value is read as a signed number
	return Truncated(scale * static_cast<float>(static_cast<int32_t>(woodValue)));
}

float store_rules::RockScaleLeft(uint32_t woodLeft, uint32_t woodValue)
{
	return static_cast<float>(woodLeft) / static_cast<float>(static_cast<int32_t>(woodValue));
}

float store_rules::RockAltitudeLeft(float altitude, float scaleLeft, float scale)
{
	const float shrunk = altitude * scaleLeft;
	return shrunk / scale;
}

float store_rules::LastTakenModifier(std::optional<uint32_t> turnTaken, uint32_t now, uint32_t turnsToForget)
{
	if (!turnTaken.has_value())
	{
		return 1.0f;
	}
	// The turns since, in float, as the original's floating point keeps them: exact below 2^24 turns
	const float share = std::min(static_cast<float>(now - *turnTaken) / static_cast<float>(turnsToForget), 1.0f);
	return share * share * share;
}

store_rules::ResourceGift store_rules::GiftToTown(float desireBefore, float desireAfter, float lastTakenModifier,
                                                  float giverMultiplier, float townMultiplier)
{
	float delta = desireBefore - desireAfter;
	delta *= lastTakenModifier;
	return {.alignmentChange = delta, .belief = delta * giverMultiplier * townMultiplier};
}

int64_t store_rules::AmountOverMaximum(bool wood, uint32_t held, uint32_t pileMaximum)
{
	return static_cast<int64_t>(held) - (wood ? k_WoodPiles : 1) * static_cast<int64_t>(pileMaximum);
}

uint32_t store_rules::AskedOfPile(uint32_t asked, int64_t overMaximum)
{
	if (overMaximum <= 0)
	{
		return asked;
	}
	return asked - static_cast<uint32_t>(std::min<int64_t>(asked, overMaximum));
}
