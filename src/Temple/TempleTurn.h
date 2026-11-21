/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The temple's own turn while the player is inside: what it does and in which order
namespace openblack::temple_turn
{

/// The work of a temple turn, as the game does it
class Steps
{
public:
	virtual ~Steps() = default;
	/// The temple's help and special scripts take their turn, and no other scripts
	virtual void LookInTempleScripts() = 0;
	/// The audio's game turn: the citadel's music
	virtual void AudioTurn() = 0;
	/// The room's tooltip is submitted
	virtual void RoomToolTip() = 0;
	/// The help system's turn
	virtual void HelpTurn() = 0;
};

/// One temple turn. Only while the world is paused: the temple's scripts, then the audio, then the room's tooltip and
/// the help system's turn
void Process(Steps& steps, bool paused);

} // namespace openblack::temple_turn
