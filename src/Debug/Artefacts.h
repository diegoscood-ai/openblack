/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

#include "Window.h"

namespace openblack::debug::gui
{

/// Lists the artefacts on the land: where each belongs, its owner, its worth, how it shows, its dance and whether its
/// aura is drawn over it, and how many each player owns. A button gives one a little more worth, so its glow and symbol
/// can be seen; a switch hides the aura (off by default). Each town shows whether a football pitch is weighed before its
/// artefacts. Closed, it reads and changes nothing.
class Artefacts final: public Window
{
public:
	Artefacts() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept override {}
	void ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept override {}

private:
	/// What became of the last action
	std::string _last;
};

} // namespace openblack::debug::gui
