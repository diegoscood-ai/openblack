/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerNeeds.h"

using namespace openblack::ecs;

// Every comparison is made as the game makes it, so that a value that is not a number goes the same way. Where the
// game's test also passes when the two cannot be ordered, it is written here as the opposite test negated

float villager_needs::DesireForFood(float food)
{
	// Not a number is kept, and so is its desire
	const float full = !(food >= 1.0f) ? food : 1.0f;
	// Multiplied in turn, as the game does
	return 1.0f - full * full * full;
}

float villager_needs::LifeDesireFromLife(float life, float goHomeLife)
{
	const float least = !(goHomeLife >= life) ? goHomeLife : life;
	const float share = (life - least) / (1.0f - goHomeLife);
	return 1.0f - share * share;
}

float villager_needs::OwnDesiresTrigger(const TriggerInputs& in)
{
	if (in.woken)
	{
		return 0.0f;
	}
	const float food = in.hungry ? in.foodDesire : 0.0f;
	const float pastThreshold = in.lifeDesire - in.ownDesireThreshold;
	const float life = pastThreshold > 0.0f ? pastThreshold : 0.0f;
	const float greater = food > life ? food : life;
	// A food desire that is not a number is not taken as the greater but is taken as the lesser: the trigger is not a
	// number
	const float lesser = !(food >= life) ? food : life;
	const float trigger = lesser * k_LesserNeedShare + greater;
	// A child's trigger that is not a number becomes the least
	if (in.child && !(trigger > k_ChildLeastTrigger))
	{
		return k_ChildLeastTrigger;
	}
	// No more than 1, and one that is not a number is given as it is
	return !(trigger >= 1.0f) ? trigger : 1.0f;
}

bool villager_needs::SatisfyOwnDesire(float foodDesire, float lifeDesire, float threshold, const std::function<bool()>& sleep,
                                      const std::function<bool()>& eat)
{
	const float food = foodDesire - threshold;
	const float life = lifeDesire - threshold;
	if (food > life && food > 0.0f)
	{
		if (eat())
		{
			return true;
		}
		return life > 0.0f && sleep();
	}
	if (life > 0.0f)
	{
		if (sleep())
		{
			return true;
		}
		return food > 0.0f && eat();
	}
	return false;
}

bool villager_needs::PausesForASecond(float draw, float life, bool poisoned, float pauseChance)
{
	const float felt = poisoned ? life * k_PoisonedLifeShare : life;
	const float missing = 1.0f - felt;
	const float weight = missing * missing * missing;
	// A life that is not a number always pauses
	return !(draw - k_PauseLifeWeight * weight >= pauseChance);
}
