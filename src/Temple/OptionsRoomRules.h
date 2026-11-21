/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The temple's options room, which opens the game's options as the player arrives and goes back once they close
namespace openblack::temple::options_room
{

/// What the room does this frame
enum class Step
{
	None,
	OpenOptions,
	GoBackToMainRoom,
};

/// What the room looks at while the player is in it
struct Facts
{
	/// Some dialog is up over the temple
	bool dialogUp {false};
	/// The room's camera has come in and the player has it
	bool arrived {false};
	/// The player's page says Restart: leaving it starts the game again, and the room waits for that
	bool restartPending {false};
};

/// Steps the room by one frame. `opened` is the room's mark that it has opened the options, which lasts until it goes
/// back to the main room or the player leaves the temple. Nothing happens while a dialog is up or until the camera has
/// come in; then the room opens the options once, and once they are closed it goes back to the main room, unless the
/// game is waiting to start again.
[[nodiscard]] constexpr Step Update(bool& opened, const Facts& facts)
{
	if (facts.dialogUp || !facts.arrived)
	{
		return Step::None;
	}
	if (!opened)
	{
		opened = true;
		return Step::OpenOptions;
	}
	if (facts.restartPending)
	{
		return Step::None;
	}
	opened = false;
	return Step::GoBackToMainRoom;
}

/// Where the player has gone from the options room
enum class Away
{
	/// Another room of the temple
	AnotherRoom,
	/// Out of the temple
	OutOfTheTemple,
};

/// Whether the options the room opened are put away now that the player is no longer in the room. In another room of
/// the temple they are, as going to a room puts every dialog away; out of the temple they stay up, as leaving it puts
/// none away. Either way the room's mark is cleared, so the next visit opens them again.
[[nodiscard]] constexpr bool PutsOptionsAway(bool& opened, Away away)
{
	const bool wasOpened = opened;
	opened = false;
	return wasOpened && away == Away::AnotherRoom;
}

} // namespace openblack::temple::options_room
