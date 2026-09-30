/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

namespace openblack::ecs::components
{
/// The influence fields of a Town (kept apart from components::Town). Town::Process 0x747380 recomputes the radius
/// every turn (influence::ProcessTowns).
struct TownInfluence
{
	PlayerNames owner;       ///< Town +0x2C (GetPlayer): only the owner's influence counts it
	float radius {0.0f};     ///< +0x5C8: base + abodes, x townInfluenceMultiplier; inside it the town gives 1
	bool noInfluence {false}; ///< +0x5F8, the Town ctor's last argument (0 for CREATE_TOWN): no base, no abodes
};

/// The citadel's influence (Citadel +0x6C), fixed when its CitadelHeart is made (ctor 0x4649B0): scale x the heart's
/// story influence for the land. Citadel::GetInfluence 0x464090 multiplies it by playerInfluenceMultiplier.
struct CitadelInfluence
{
	float value {0.0f};
};
} // namespace openblack::ecs::components
