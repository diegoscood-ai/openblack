/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Window.h"

namespace openblack::debug::gui
{

/// The disciples on the land, each with its work and the work it had as the hand picked it up, and for the villager in
/// the hand the things round it in the order it looks at them, with buttons that make it a trader (of a town picked
/// from the list), a craftsman, a missionary or a breeder; the towns' missionaries; each breeder's state, spot, partner
/// and pregnancy; the villagers making love out of doors and their partners. A switch draws or hides their glows and
/// symbol. Closed, it reads and changes nothing.
class Disciples final: public Window
{
public:
	Disciples() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept override {}
	void ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept override {}

private:
	/// The town a trader made here trades with, by its place in the list of towns
	int _tradeTown {0};
};

} // namespace openblack::debug::gui
