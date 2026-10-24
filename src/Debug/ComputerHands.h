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

/// The computer players' hands: for each player with one, whether its mind is on, paused or started, whether a script
/// holds the hand and gave it steps, where it is and where it goes, its speeds, its steps with the stage of the first,
/// what it holds and the actions queued for it; then each computer player's mind: its plan, its rests, its attitudes and
/// its tree with every node's weight, suppression, worth and choice. It reads nothing while closed, and changes nothing
/// but the "Hold this mind" switch, off by default, which keeps the mind from weighing.
class ComputerHands final: public Window
{
public:
	ComputerHands() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept override {}
	void ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept override {}
};

} // namespace openblack::debug::gui
