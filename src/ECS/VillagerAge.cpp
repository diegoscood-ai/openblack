/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAge.h"

#include "3D/MapCoords.h"
#include "ECS/Components/Villager.h"
#include "GameClock.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The table's size at an index. Past its end the game would read the fields after it, which no child reaches (the
/// oldest child is 12): openblack keeps to the last size
float ScaleAt(std::span<const float, 20> ageToScale, uint32_t index)
{
	return index < ageToScale.size() ? ageToScale[index] : ageToScale.back();
}
} // namespace

float villager_age::StartScale(uint32_t age, uint32_t grownUp, std::span<const float, 20> ageToScale, float beforeTable)
{
	if (IsChildAge(age, grownUp))
	{
		return age == 0 ? beforeTable : ScaleAt(ageToScale, age - 1);
	}
	return k_AdultStartScale;
}

float villager_age::GrownScale(uint32_t age, uint32_t grownUp, std::span<const float, 20> ageToScale, float scale,
                               const FloatRandom& random)
{
	if (IsChildAge(age, grownUp))
	{
		const float gap = ScaleAt(ageToScale, age + 1) - scale;
		const float most = gap * k_ChildGrowthShare;
		const float growth = random(most);
		return scale + growth;
	}
	const float first = (k_AdultScaleAboveFull - random(k_AdultScaleRange)) + 1.0f;
	// A size that is not a number draws again too
	if (!(scale >= first))
	{
		// The game draws again rather than keeping the size it compared
		const float second = random(k_AdultScaleRange);
		return (k_AdultScaleAboveFull - second) + 1.0f;
	}
	return scale;
}

bool villager_age::DiesOfOldAge(uint32_t age, const Ages& ages, const FloatRandom& floatRandom, const IntRandom& intRandom)
{
	if (!(age > ages.old))
	{
		return false;
	}
	// Multiplied in turn, each product a float
	const float draw = floatRandom(1.0f);
	const float squared = draw * draw;
	const float cubed = squared * draw;
	// The years between, a whole number made a float, times the cube, truncated
	const auto span = static_cast<float>(static_cast<double>(ages.oldest - ages.old));
	const auto most = static_cast<uint32_t>(map_coords::FtoL(cubed * span));
	const uint32_t extra = intRandom(most);
	return age + extra > ages.oldest;
}

villager_age::Newborn villager_age::MakeNewborn(uint32_t age, uint32_t turn, const NewbornKind& kind,
                                                const FloatRandom& floatRandom, const IntRandom& intRandom)
{
	Newborn born;
	born.age = GivenAge(age, kind.grownUp);
	born.child = IsChildAge(born.age, kind.grownUp);
	born.scale = GrownScale(born.age, kind.grownUp, kind.ageToScale,
	                        StartScale(born.age, kind.grownUp, kind.ageToScale, kind.beforeTable), floatRandom);

	// The game's min(1, draw + hungry) evaluates its argument twice: a first draw below full draws again and keeps the
	// second unclamped
	const float firstFood = floatRandom(k_NewbornFoodRange) + kind.hungryForFood;
	if (firstFood < 1.0f)
	{
		const float secondFood = floatRandom(k_NewbornFoodRange);
		born.food = secondFood + kind.hungryForFood;
	}
	else
	{
		born.food = 1.0f;
	}

	// The same for the turns since it was last checked, against the game's age
	uint32_t sinceChecked = turn;
	if (intRandom(kind.processChecksEvery) < turn)
	{
		sinceChecked = intRandom(kind.processChecksEvery);
	}
	born.lastCheckTurn = turn - sinceChecked;

	born.turnsUntilFirstDecision = static_cast<uint16_t>(intRandom(k_FirstDecisionTurns) + 1);
	return born;
}

uint32_t villager_age::AgeNow(const components::Villager& villager)
{
	return AgeOf(game_clock::Turn(), villager.birthTurn);
}

void villager_age::SetBirthTurnForAge(components::Villager& villager, uint32_t age)
{
	villager.birthTurn = BirthTurnFor(game_clock::Turn(), age);
}
