/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleTurn.h"

namespace openblack::temple_turn
{

void Process(Steps& steps, bool paused)
{
	// The game's temple turn does nothing unless the world is paused
	if (!paused)
	{
		return;
	}
	steps.LookInTempleScripts();
	steps.AudioTurn();
	steps.RoomToolTip();
	steps.HelpTurn();
}

} // namespace openblack::temple_turn
