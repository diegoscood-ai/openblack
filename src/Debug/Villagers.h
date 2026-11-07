/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Window.h"

namespace openblack::debug::gui
{

/// Reads out the villager nearest the hand, or the one kept: the reaction it follows and the priority it would take it
/// up at now, whether its town was attacked lately enough for a shield, whether it takes an effect now (a hit, a heal)
/// and which player's interface last let go of it. It only reads: closed, it reads nothing, and open it changes nothing
/// in the game.
class Villagers final: public Window
{
public:
	Villagers() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	void DrawReactions(entt::entity villager) const noexcept;

	/// Keep reading the same villager while the hand moves away
	bool _keep {false};
	entt::entity _kept {entt::null};
};

} // namespace openblack::debug::gui
