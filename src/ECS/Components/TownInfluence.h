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
/// The influence fields of a Town (kept apart from components::Town, whose `owner`, Town +0x2C, is the only player whose
/// influence counts it). Town::Process 0x747380 recomputes the radius every turn (influence::ProcessTowns).
struct TownInfluence
{
	float radius {0.0f};     ///< +0x5C8: base + abodes, x townInfluenceMultiplier; inside it the town gives 1
	bool noInfluence {false}; ///< +0x5F8, the Town ctor's last argument (0 for CREATE_TOWN): no base, no abodes
	/// +0xF24: the radius of the last GGame::Update3DInfluence (after 0x555354, written whether or not it made a
	/// circle), which Town::Process 0x74759E compares with +0x5C8 (influence::NoteInfluence)
	float drawnRadius {0.0f};
};

/// The citadel's influence (Citadel +0x6C), fixed when its CitadelHeart is made (ctor 0x4649B0): scale x the heart's
/// story influence for the land. Citadel::GetInfluence 0x464090 multiplies it by playerInfluenceMultiplier.
struct CitadelInfluence
{
	float value {0.0f};
	/// Citadel +0x78: Citadel::GetInfluence at the last GGame::Update3DInfluence (after 0x55530E, always written),
	/// which Citadel::Process 0x4630C6 compares with GetInfluence (influence::NoteInfluence)
	float drawnRadius {0.0f};
};
} // namespace openblack::ecs::components
