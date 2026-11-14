/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePickUpRules.h"

using namespace openblack;

bool creature_pick_up::CreatureMayPickUp(const Check& check)
{
	if (check.computerPlayer && !check.creatureScripted && check.thingInScript)
	{
		return false;
	}
	if (check.ownWorshipper)
	{
		return false;
	}
	if (!check.inMap && !check.inThisHand)
	{
		return false;
	}
	if (check.leashTiedTo)
	{
		return false;
	}
	// The two radii side by side within the reach, and the weight within the share the creature lifts: written as
	// "not more than", so that equal and not-a-number pass as in the game
	if (check.thingRadius + check.creatureRadius > check.reach)
	{
		return false;
	}
	if (check.thingWeight > check.creatureWeight * k_LiftShare)
	{
		return false;
	}
	return check.thingAllows;
}

float creature_pick_up::ThingWeight(float scale, float kindWeight)
{
	return scale * scale * scale * kindWeight;
}

bool creature_pick_up::IsOwnWorshipper(bool worshippingAtSite, std::optional<PlayerNames> villagersPlayer,
                                       PlayerNames creaturesPlayer)
{
	return worshippingAtSite && villagersPlayer == creaturesPlayer;
}

bool creature_pick_up::VillagerAllows(bool insideItsHome, bool cannotBePickedUp)
{
	return !insideItsHome && !cannotBePickedUp;
}

bool creature_pick_up::PileAllows(bool cannotBePickedUp, bool canBecomePhysicsObject)
{
	return !cannotBePickedUp && canBecomePhysicsObject;
}
