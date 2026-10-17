/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureEating.h"

#include <cmath>

#include "3D/MapCoords.h"

using namespace openblack;

namespace
{
/// The game's test of a difference against zero, which an unordered (not a number) value also passes
[[nodiscard]] bool ZeroOrUnordered(float value)
{
	return !(value < 0.0f || value > 0.0f);
}
} // namespace

bool creature_eating::CanBeEaten(const EatCheck& check)
{
	switch (check.as)
	{
	case Eaten::AnObject:
		return check.mayPickUp;
	case Eaten::AMobileStatic:
		return !check.isToy;
	case Eaten::Never:
		break;
	}
	return false;
}

uint32_t creature_eating::PortionAmount(float drawnSize, uint32_t sourceAmount)
{
	// The product is a float, and its cut keeps the 32 bits as they are: compared unsigned
	const auto amount = static_cast<uint32_t>(map_coords::FtoL(k_PortionAmountPerSize * drawnSize));
	return amount < sourceAmount ? amount : sourceAmount;
}

float creature_eating::PileApproach(float pileRadius, float creatureRadius)
{
	return (pileRadius + creatureRadius) * k_PileApproachShare;
}

float creature_eating::PortionScale(float drawnSize)
{
	return drawnSize * k_PortionScalePerSize;
}

glm::vec2 creature_eating::PortionPoint(glm::vec2 creature, glm::vec2 source, float pickUpRange)
{
	auto difference = source - creature;
	if (!ZeroOrUnordered(difference.x) || !ZeroOrUnordered(difference.y))
	{
		const float share = pickUpRange * k_PortionRangeShare;
		const float length = std::sqrt((difference.y * difference.y) + (difference.x * difference.x));
		const float factor = share / length;
		difference.x = difference.x * factor;
		difference.y = difference.y * factor;
	}
	return {difference.x + creature.x, creature.y + difference.y};
}

creature_eating::MealEnd creature_eating::JudgeMeal(float foodValue, bool mushroom, bool pileFood, bool poisoned)
{
	if (mushroom || pileFood)
	{
		return MealEnd::Eaten;
	}
	// "Not at least", so that not a number is not food too
	if (!(static_cast<double>(foodValue) >= k_LeastFoodValue) || poisoned)
	{
		return poisoned ? MealEnd::Poisoned : MealEnd::NotFood;
	}
	return MealEnd::Eaten;
}

float creature_eating::HungerAfterMeal(float hunger, float foodValue, float realSize, float foodToEnergy)
{
	// The size is kept when it is below the largest meal size or not a number
	const float mealSize =
	    realSize < creature_physiology::k_MaxMealSize || std::isnan(realSize) ? realSize : creature_physiology::k_MaxMealSize;
	const float perEnergy = mealSize * foodToEnergy;
	const float energy = foodValue / perEnergy;
	const float left = hunger - energy;
	// Below 0 or not a number gives 0, above 1 gives 1
	if (!(left >= 0.0f))
	{
		return 0.0f;
	}
	if (left > 1.0f)
	{
		return 1.0f;
	}
	return left;
}

bool creature_eating::IsFoodPileOutsideStoragePit(bool magicFood, std::optional<float> pitRadius, float distance)
{
	if (!magicFood)
	{
		return false;
	}
	if (!pitRadius.has_value())
	{
		return true;
	}
	// Inside only when the pit's reach is beyond the distance: equal, or not a number, is outside
	return !(*pitRadius * k_PitReachPerRadius > distance);
}

bool creature_eating::StoragePitHasFood(float pitRadius, float drawnSize, bool hasPile, bool pileFlagged, uint32_t pileAmount)
{
	// Too wide only when the radius is beyond the limit: equal, or not a number, passes
	if (pitRadius > drawnSize * k_PitRadiusPerSize)
	{
		return false;
	}
	return hasPile && !pileFlagged && pileAmount != 0;
}

bool creature_eating::FieldHasFood(float growth, float food)
{
	return growth > 0.0f && food > 0.0f;
}

bool creature_eating::TreeTallEnough(float height, float drawnSize)
{
	// Short only when below the limit: equal is tall enough, and not a number is not
	return height >= drawnSize * k_TreeHeightPerSize;
}
