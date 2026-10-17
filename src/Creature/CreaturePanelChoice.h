/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

/// Which creature the status panel (creature_panel) shows this frame, and with what reward
namespace openblack::creature_panel_choice
{

struct Choice
{
	entt::entity creature;
	/// The hand's reward, none while the panel shows the creature Creature Mode follows
	std::optional<float> reward;
};

/// As the original: the creature the hand is held to (pressed on it and held, through the let-go), with the hand's
/// reward and its row; without one, the creature Creature Mode follows, without the reward row. A creature the hand is
/// only over shows none
[[nodiscard]] inline std::optional<Choice> Choose(std::optional<entt::entity> held, std::optional<entt::entity> followed,
                                                  float lastFeedbackSum)
{
	if (held.has_value())
	{
		return Choice {.creature = *held, .reward = lastFeedbackSum};
	}
	if (followed.has_value())
	{
		return Choice {.creature = *followed, .reward = std::nullopt};
	}
	return std::nullopt;
}

} // namespace openblack::creature_panel_choice
