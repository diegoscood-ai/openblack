/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "Common/Zoomer.h"
#include "Enums.h"

namespace openblack::ecs::components
{

struct Pot
{
	uint16_t amount;
	uint16_t maxAmount;
	PotInfo type = PotInfo::_COUNT;
	bool poisoned = false; ///< +0x74 bit 0 (Pot::IsPoisoned 0x55D4E0 / SetPoisoned 0x55D510)
	bool speedUp = false;  ///< +0x74 bit 4 (Pot::IsSpeedUp 0x55D4F0 / PileFood::SetSpeedUp 0x66E220)
	entt::entity speedUpVisual = entt::null; ///< PileFood +0xB8: the PILEFOOD_SPEEDUP spot visual container
	/// MagicFood +0xBC / MagicWood +0xB4: the player whose miracle made the pile (NULL -> the local player)
	PlayerNames owner = PlayerNames::PLAYER_ONE;
};

// PileResource sink offset (+0x84..+0xB0): piles rise out of / sink into the ground instead of scaling. A change of
// amount moves the offset to its new target in 1 s (the same quartic as the LH3DLib Zoomer).
struct PileSink
{
	float baseY;
	float height = 1.0f;
	openblack::Zoomer offset {};
};

// Texture offset of the object (LH3DObject vfunc 0xE8 (u, v)). PileFood::Draw scrolls the grain of the storage pit and
// magic food piles by v = 0.25 * sink / height, so that the grain stays put in the world and the pile seems to shrink;
// OneOffSpellSeed::UpdateFrame 0x72A570 steps the orbs' 4x4 texture (u and v in quarters).
struct UvScroll
{
	float v = 0.0f;
	float u = 0.0f; ///< 0, 0.25, 0.5 or 0.75 (the renderer packs it with v)
};

} // namespace openblack::ecs::components
