/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashNeedsSign.h"

using namespace openblack;
using namespace openblack::creature_leash;

bool creature_leash::NeedsSignQualifies(const NeedsSign& sign)
{
	return sign.row != NeedsSignRow::Workshop && sign.totemHasWorshipSite;
}

float creature_leash::NeedsSignFill(float desire, float maxNeed)
{
	// a desire at or above the most, or one that can't be compared, shows the most
	if (maxNeed > desire)
	{
		return desire / maxNeed;
	}
	return maxNeed / maxNeed;
}

bool creature_leash::NeedsSignArmsAtTie(const NeedsSign& sign)
{
	return NeedsSignQualifies(sign);
}

bool creature_leash::NeedsSignArmsThisTurn(const NeedsSign& sign, float fill)
{
	// strictly more, and a fill that can't be compared doesn't
	return NeedsSignQualifies(sign) && static_cast<double>(fill) > k_NeedsSignWants;
}

std::optional<uint32_t> creature_leash::NeedsSignAction(bool knowsFoodMiracle, bool knowsFishing)
{
	if (knowsFoodMiracle)
	{
		return k_FoodForWorshipAction;
	}
	if (knowsFishing)
	{
		return k_FishForWorshipAction;
	}
	return std::nullopt;
}
