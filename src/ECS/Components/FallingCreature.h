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

/// The falling spell film's copy of the local player's creature: a body of its own made as the film starts, at the
/// world's origin facing heading 0, which plays the film's falling action and is gone when the film ends. It is not a
/// creature (it has no Creature component), so nothing that looks for creatures finds it. Wiki:
/// docs/wiki/magic/falling-spell.md#the-falling-spell-what-fallingspell-draws
struct FallingCreature
{
	/// The creature it is a copy of
	entt::entity source {entt::null};
	CreatureType species {CreatureType::Unknown};
	/// Its size, held within the limits a body is drawn at
	float size {1.0f};
	/// The creature's alignment as the copy was made, held to -1 evil .. 1 good, which its sparkles go by
	float alignment {0.0f};
	/// Whether its time is the film's: once the film has started it, its animations move on by the film's milliseconds
	/// rather than at its size's rate
	bool filmTimed {false};
	/// Whether it is the body that sparkles: set as it is made, then by each frame the falling spell draws (not while it
	/// is hidden); while it is not, a step of its time makes no sparkles
	bool sparkling {true};
	/// The milliseconds of its last step of time, which its eyes' look moves on by each frame it is drawn
	uint32_t lastStepMs {0};
};

} // namespace openblack::ecs::components
