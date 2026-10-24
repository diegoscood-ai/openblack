/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLostThing.h"

#include <algorithm>
#include <optional>

using namespace openblack;
using namespace openblack::creature_lost_thing;

std::vector<uint32_t> creature_lost_thing::PlanThings(const creature_planner::Plan& plan)
{
	std::vector<uint32_t> things;
	for (const auto& thing : {plan.activityObject, plan.object, plan.instrument})
	{
		if (thing.has_value())
		{
			things.push_back(*thing);
		}
	}
	return things;
}

std::vector<uint32_t> creature_lost_thing::StepThings(std::span<const creature_mind::Step> agenda, size_t from)
{
	std::vector<uint32_t> things;
	for (size_t i = from; i < agenda.size(); ++i)
	{
		const auto& step = agenda[i];
		for (const auto& thing : {step.object, step.order.object, step.movement.object, step.cast.object})
		{
			if (thing.has_value())
			{
				things.push_back(*thing);
			}
		}
	}
	return things;
}

bool creature_lost_thing::Lost(const InUse& inUse, const std::function<bool(uint32_t)>& gone)
{
	const auto goneNotOwn = [&](uint32_t thing) {
		return std::ranges::find(inUse.usedByItself, thing) == inUse.usedByItself.end() && gone(thing);
	};
	return std::ranges::any_of(inUse.planThings, goneNotOwn) || std::ranges::any_of(inUse.stepThings, goneNotOwn);
}
