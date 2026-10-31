/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"
#include "ECS/Components/TownDesire.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// A flag over a town's store that flies as high as the town wants one thing (see town_desire_flags)
struct TownDesireFlag
{
	entt::entity town {entt::null};
	TownDesireInfo desire {TownDesireInfo::None};
	/// The store's special point it flies at
	uint32_t point {0};
	/// The store it was last put on, and where: it stays there after the store is gone, until its town makes another
	entt::entity store {entt::null};
	glm::vec3 place {0.0f};
	/// Where it was made: once a store has taken it elsewhere, it counts as a moving thing for good (the game never
	/// sets the place it compares with again)
	glm::vec3 madeAt {0.0f};
	/// Its object scale, its desire's (the info's worship site scale): its size for every measure of it. The Transform
	/// carries the drawn one
	float objectScale {1.0f};
	/// How high its place is over the ground, which it keeps if the ground moves
	float overGround {0.0f};
	/// The desire it flies for: its town's raw desire with the boosts, taken at the town's turn (0 until the first), and
	/// held to wholly wanted by a frame that shows it
	float storedDesire {0.0f};
	/// How far up it flies, 0 to 1, easing as the game's flags do
	Zoomer height;
	/// Whether it showed this frame
	bool shown {false};
};

/// A town's flags, by desire: none for a desire that flies no flag
struct TownDesireFlags
{
	std::array<entt::entity, k_TownDesireCount> flags {};
};

} // namespace openblack::ecs::components
