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

/// Reads out the animal nearest the hand, or the one kept: its species, its top, final and previous states and the
/// turns since the state changed, the reaction it takes, the way it moves, its prey and food, its flock and lair and
/// its needs. It only reads: closed, it reads nothing, and open it changes nothing in the game.
class Animals final: public Window
{
public:
	Animals() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	void DrawStates(entt::entity entity) const noexcept;
	void DrawReaction(entt::entity entity) const noexcept;
	void DrawTargets(entt::entity entity) const noexcept;
	void DrawFlock(entt::entity entity) const noexcept;

	/// Keep reading the same animal while the hand moves away
	bool _keep {false};
	entt::entity _kept {entt::null};
};

} // namespace openblack::debug::gui
