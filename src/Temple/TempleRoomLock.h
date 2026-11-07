/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <utility>

#include "3D/TempleInteriorInterface.h"

namespace openblack::temple
{

/// The developer function the game over's script ends with: the player is taken into the temple's save game room and
/// kept there
inline constexpr int32_t k_DevFunctionEnterSaveGameRoom = 12;

/// The room a developer function takes the player into and keeps them in, if it is one that does
[[nodiscard]] constexpr std::optional<TempleRoom> RoomKeptBy(int32_t devFunction) noexcept
{
	if (devFunction == k_DevFunctionEnterSaveGameRoom)
	{
		return TempleRoom::SaveGame;
	}
	return std::nullopt;
}

/// The room of the temple the game keeps the player in once the game is over: the save game room, where a saved game
/// can be loaded. Going to any other room, or out of the temple, ends the game.
class RoomLock
{
public:
	/// Keeps the player in a room from now on
	void Lock(TempleRoom room) noexcept { _room = room; }
	[[nodiscard]] std::optional<TempleRoom> GetRoom() const noexcept { return _room; }
	/// Whether going to a room leaves the room the player is kept in, which takes them out of the temple
	[[nodiscard]] bool Leaves(TempleRoom room) const noexcept { return _room.has_value() && room != *_room; }
	/// The player leaves the temple: if they were kept in a room the game is over, and the room no longer holds them
	void Left() noexcept
	{
		_ended = _ended || _room.has_value();
		_room.reset();
	}
	/// A land is loaded: the room no longer holds the player, and the game goes on
	void Clear() noexcept { _room.reset(); }
	/// Whether the player left the room they were kept in since this was last asked, ending the game
	[[nodiscard]] bool TakeEnded() noexcept { return std::exchange(_ended, false); }

private:
	std::optional<TempleRoom> _room;
	bool _ended {false};
};

} // namespace openblack::temple
