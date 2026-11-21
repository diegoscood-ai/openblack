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

#include <algorithm>
#include <span>
#include <vector>

#include "Input/BindableActions.h"
#include "Input/ShortcutKeys.h"

namespace openblack::input
{

/// The flights the temple and realm keys ask for this frame, in the order the game takes them: first one for each key
/// down of the frame bound to either (each key down's actions, as GetKeyDownsThisFrame gives them), in the order the
/// keys came, a held key's repeats too, then the presses of a mouse button bound to them, which the game reads after
/// the frame's keys (the temple's before the realm's). None while a script holds the cinema bars; inside the temple
/// only the mouse's; a key the script's interface interaction has turned off asks for none. byMouse marks the presses
/// made by a mouse button, as for ShortcutActionsFor
[[nodiscard]] inline std::vector<ZoomKey> PlaceKeysInOrder(std::span<const BindableActionMap> keyDowns, ShortcutGates gates,
                                                           ShortcutPresses byMouse)
{
	std::vector<ZoomKey> zooms;
	if (gates.scriptWideScreen)
	{
		return zooms;
	}
	const auto add = [&zooms, gates](bool temple, bool realm) {
		if (temple && !gates.templeKeyBlocked)
		{
			zooms.push_back(ZoomKey::Temple);
		}
		if (realm && !gates.realmKeyBlocked)
		{
			zooms.push_back(ZoomKey::Realm);
		}
	};
	if (!gates.insideTemple)
	{
		for (const auto down : keyDowns)
		{
			const auto bits = static_cast<uint64_t>(down);
			add((bits & static_cast<uint64_t>(BindableActionMap::ZOOM_TO_TEMPLE)) != 0,
			    (bits & static_cast<uint64_t>(BindableActionMap::ZOOM_TO_REALM)) != 0);
		}
	}
	add(byMouse.temple, byMouse.realm);
	return zooms;
}

/// How many times the villager names key toggles the names this frame: once for each key down of the frame bound to it,
/// a held key's repeats too, as the game takes every buffered key, then once for a press of a mouse button bound to it.
/// None while a script holds the cinema bars; inside the temple only the mouse's. byMouse marks a press made by a
/// mouse button, as for ShortcutActionsFor
[[nodiscard]] inline int NamesKeyToggles(std::span<const BindableActionMap> keyDowns, ShortcutGates gates, bool byMouse)
{
	if (gates.scriptWideScreen)
	{
		return 0;
	}
	int toggles = byMouse ? 1 : 0;
	if (!gates.insideTemple)
	{
		toggles += static_cast<int>(std::ranges::count_if(keyDowns, [](BindableActionMap down) {
			return (static_cast<uint64_t>(down) & static_cast<uint64_t>(BindableActionMap::SHOW_VILLAGER_NAMES)) != 0;
		}));
	}
	return toggles;
}

} // namespace openblack::input
