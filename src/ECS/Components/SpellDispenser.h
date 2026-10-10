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
#include "Magic/DispenserRules.h"

namespace openblack::ecs::components
{

/// The miracle dispenser (an Abode): a building that makes a one-shot orb of its magic every `period` turns while the
/// last one has been taken
struct SpellDispenser
{
	MagicType magicType {MagicType::None};
	/// The count, the period in game turns (0 = inactive) and whether it is active
	magic::DispenserTimer timer;
	/// The orb it made, while it has one
	entt::entity orb {entt::null};
	/// Its swirl on the land under it, 0 for none
	uint32_t effect {0};
};

} // namespace openblack::ecs::components
