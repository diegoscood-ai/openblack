/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandStateRules.h"

using namespace openblack;

int32_t hand_state::Required(const RequiredStateFacts& facts) noexcept
{
	if (facts.insideCitadel)
	{
		return k_Citadel;
	}
	if (facts.holding && facts.interfaceState == 0x19)
	{
		return k_Invisible;
	}
	if (facts.holding)
	{
		// a pile in the locked select with something held: holding, before the miracle state and tugging, and without the
		// 180 ms release impulse
		if (facts.lockedSelectOnPile)
		{
			return k_Holding;
		}
		if (facts.holdsSeed)
		{
			return k_Grain;
		}
		return facts.tugging ? k_Tug : k_Holding;
	}
	if (facts.playingAnimation)
	{
		return k_PlayAnim;
	}
	// the interface's hand state
	switch (facts.interfaceState)
	{
	case 1:
	case 2:
	case 0xC:
	case 0x19:
		return k_Invisible;
	case 0xE:
		return k_Totem;
	case 0x13:
		return facts.creatureInHand ? k_Creature : k_Normal;
	default:
		// (pending) the camera state in the camera mode with the tricons
		return k_Normal;
	}
}
