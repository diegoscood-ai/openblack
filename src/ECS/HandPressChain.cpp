/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandPressChain.h"

using namespace openblack::ecs;

hand_press::Branch hand_press::Choose(const Facts& facts)
{
	// a creature held or being let go keeps the hand's action: no press starts another
	if (facts.creatureLockBusy)
	{
		return Branch::None;
	}
	if (facts.screenPress)
	{
		return Branch::ScreenObject;
	}
	// the rope comes after the bubble and before every object: it took the pick from them
	if (facts.ropePress && !facts.held && !facts.lessonSkipsSelects)
	{
		return Branch::LeashPull;
	}
	if (facts.hovered && !facts.held && facts.hoveredIsField)
	{
		return Branch::Field;
	}
	if (facts.hovered && !facts.held)
	{
		return Branch::Hovered;
	}
	// a totem is the object the press collides with, like the fields and piles: whether the hand takes hold of it is
	// its own test, and one it refuses goes on to the pick-up path, where it is neither placeable nor tappable. So it
	// keeps the fish at the hand's point out too
	if (EmptyHandOverNothing(facts) && facts.cursorIsTotem && !facts.lessonSkipsSelects)
	{
		return Branch::Totem;
	}
	if (EmptyHandOverNothing(facts) && facts.tapOnlyCursorObject)
	{
		return Branch::TapOnly;
	}
	// a creature is the object the press collides with, so the fish at the hand's point are never looked for: the
	// hand takes hold of it when it may, otherwise the press goes on to the pick-up path with the creature, which is
	// neither placeable nor tappable. (not ported) with the leash in the hand, the grab start
	if (EmptyHandOverNothing(facts) && facts.creatureTakesPress && !facts.lessonSkipsSelects)
	{
		return Branch::Creature;
	}
	if (EmptyHandOverNothing(facts) && !facts.cursorIsCreature && facts.fishFarmInInfluence && !facts.lessonSkipsSelects)
	{
		return Branch::FishFarm;
	}
	if (facts.holdingSeed)
	{
		return Branch::Seed;
	}
	if (facts.held && !facts.pickPressHeld)
	{
		return Branch::Held;
	}
	return Branch::None;
}
