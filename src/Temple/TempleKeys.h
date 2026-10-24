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

#include "3D/TempleInteriorInterface.h"

/// What the temple's keys do inside it
namespace openblack::temple
{

/// Where Escape takes the player inside the temple
enum class EscapeResult
{
	GoToMainRoom,
	Leave
};

/// Escape inside the temple: from the main room the player leaves, from any other room they go back to the main room.
/// While the game keeps the player in a room (the save game room once the game is over), Escape in the save game room
/// leaves at once
[[nodiscard]] constexpr EscapeResult OnEscape(TempleRoom current, std::optional<TempleRoom> keptIn) noexcept
{
	if (current == TempleRoom::SaveGame && keptIn.has_value())
	{
		return EscapeResult::Leave;
	}
	return current == TempleRoom::Main ? EscapeResult::Leave : EscapeResult::GoToMainRoom;
}

} // namespace openblack::temple
