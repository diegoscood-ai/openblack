/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Casting from the hand, the per-frame and per-turn parts MagicLoop.cpp calls: the gesture sampling and
// ProcessPowerUpSystem, the in-hand effect, the hand effects and the utility effects. Wiki:
// docs/wiki/magic/hand-casting.md#casting-from-the-hand-gestures-and-hand-effects.

namespace openblack::magic::hand_casting
{
/// The game time the turn's power-up pass is given: the last frame's, and none once a saving banner was drawn after
/// it, as the game's banner clears the frame's time while it draws, until the next frame's time is taken
struct TurnTime
{
	float seconds {0.0f};
	/// (debug readout) the time was cleared by a saving banner since the last frame's was taken
	bool clearedByBanner {false};

	/// A frame's game time, taken after the hand's update
	void FrameDone(float frameSeconds)
	{
		seconds = frameSeconds;
		clearedByBanner = false;
	}
	/// A saving banner was drawn
	void SavingBannerDrawn()
	{
		seconds = 0.0f;
		clearedByBanner = true;
	}
};

/// A land is loaded
void OnLoadMap();
/// At the start of the game turn (the interface's turn: ProcessPowerUpSystem again, with the last frame's game time)
void ProcessTurn();
/// Every frame after the hand's update (the interface's frame updates, the hand's draw): seconds of game time (0 while
/// paused)
void Update(float seconds);
/// A save's banner is drawn: the rest of the turn's power-up pass has no frame time until the next frame's
void OnSavingBanner();
} // namespace openblack::magic::hand_casting
