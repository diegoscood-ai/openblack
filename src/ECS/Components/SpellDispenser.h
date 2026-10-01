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

#include "Enums.h"

namespace openblack::ecs::components
{

/// SpellDispenser (an Abode, 0xDC bytes; Create 0x7228D0): the miracle dispenser, a building that makes a one-shot
/// orb of its magic every `period` turns while the last one has been taken (Process 0x722A70)
struct SpellDispenser
{
	uint32_t tick {0};                    ///< +0xC4
	uint32_t period {0};                  ///< +0xC8 in game turns (0 = inactive)
	entt::entity oneShot {entt::null};    ///< +0xCC the orb it made
	bool active {false};                  ///< +0xD0
	MagicType magicType {MagicType::None}; ///< +0xD4
	uint32_t psys {0};                    ///< +0xD8 its effect (PT 0x90, at the ground under it)
};

} // namespace openblack::ecs::components
