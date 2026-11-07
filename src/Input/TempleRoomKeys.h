/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "3D/TempleInteriorInterface.h"
#include "Input/BindableActions.h"

namespace openblack::input
{

/// The keys for the temple's rooms (F4 to F9 by default), in the order they are read, and the room each goes to
inline constexpr std::array<std::pair<BindableActionMap, TempleRoom>, 6> k_TempleRoomKeys {{
    {BindableActionMap::ZOOM_TO_INSIDE_TEMPLE, TempleRoom::Main},
    {BindableActionMap::ZOOM_TO_CREATURE_ROOM, TempleRoom::CreatureCave},
    {BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, TempleRoom::Challenge},
    {BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, TempleRoom::SaveGame},
    {BindableActionMap::ZOOM_TO_OPTIONS_ROOM, TempleRoom::Options},
    {BindableActionMap::ZOOM_TO_LIBRARY, TempleRoom::Credits},
}};

/// The room of the first of those keys pressed this frame, in their order; none when none was. `pressed` is asked for
/// each key in turn, until one answers yes
template <typename Pressed>
[[nodiscard]] std::optional<TempleRoom> PressedTempleRoom(Pressed&& pressed)
{
	for (const auto& [action, room] : k_TempleRoomKeys)
	{
		if (pressed(action))
		{
			return room;
		}
	}
	return std::nullopt;
}

/// The rooms the keys that went down this frame go to, in the order the keys came (each key down's actions, as
/// GetKeyDownsThisFrame gives them): one for each key down bound to a room's key, a held key's repeats too
[[nodiscard]] inline std::vector<TempleRoom> TempleRoomsInOrder(std::span<const BindableActionMap> keyDowns)
{
	std::vector<TempleRoom> rooms;
	for (const auto down : keyDowns)
	{
		for (const auto& [action, room] : k_TempleRoomKeys)
		{
			if ((static_cast<uint64_t>(down) & static_cast<uint64_t>(action)) != 0)
			{
				rooms.push_back(room);
			}
		}
	}
	return rooms;
}

} // namespace openblack::input
