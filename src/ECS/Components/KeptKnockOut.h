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

#include <optional>

#include <entt/entity/entity.hpp>

// Kept apart from the fight's component so that the saved world's load can name it without the fight's headers
namespace openblack::ecs::components
{

/// A knock-out as a saved game keeps it in its creature's Die action: the step the action is at, the turns its lie and
/// its wait at home have left, whether it rests once home, and the thing the action is done to (none: the creature)
struct KeptKnockOut
{
	enum class Step : uint8_t
	{
		Faint,
		Lie,
		CarryHome,
		HomeWait,
		WaitForSpells,
		Rest,
	};
	Step step {Step::Faint};
	int32_t lieTurns {0};
	int32_t homeWaitTurns {0};
	bool rests {false};
	std::optional<entt::entity> actedOn;

	bool operator==(const KeptKnockOut&) const = default;
};

} // namespace openblack::ecs::components
