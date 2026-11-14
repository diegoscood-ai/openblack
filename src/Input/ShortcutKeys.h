/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include <glm/vec2.hpp>

#include "Camera/ZoomToPlaces.h"

namespace openblack::input
{

/// The one-press keys pressed this frame
struct ShortcutPresses
{
	bool temple;
	bool realm;
	bool villagerNames;
	bool help;
	bool villagerDetails {false};
};

/// What keeps the keys from acting
struct ShortcutGates
{
	/// A script holds the cinema bars: none of the keys acts
	bool scriptWideScreen;
	/// Inside the temple the keys are the temple's own: none of these acts from a key, but a mouse button bound to one
	/// still does
	bool insideTemple;
	/// The script's interface interaction has turned the place keys off
	bool templeKeyBlocked;
	bool realmKeyBlocked;
	/// The script's interface interaction has turned the Help key off
	bool helpKeyBlocked;
};

/// Which place a key flies the camera to
enum class ZoomKey : uint8_t
{
	Temple,
	Realm,
};

/// What the keys do this frame
struct ShortcutActions
{
	bool toggleVillagerNames;
	/// The Show Villager Details key: the details of the living thing under the hand, or of every villager
	bool toggleVillagerDetails {false};
	std::optional<ZoomKey> zoom;
	/// The advisors answer about the hand's thing or the ground (the Help key)
	bool askHelp;
};

/// The keys' actions. The temple key goes before the realm key when both are pressed in one frame. byMouse marks the
/// presses that came from a mouse button bound to the action rather than from its key: the game reads the mouse
/// buttons' bindings before it looks at the temple, and only the keys go to the temple
[[nodiscard]] constexpr ShortcutActions ShortcutActionsFor(ShortcutPresses presses, ShortcutGates gates,
                                                           ShortcutPresses byMouse = {})
{
	if (gates.scriptWideScreen)
	{
		return {.toggleVillagerNames = false, .toggleVillagerDetails = false, .zoom = std::nullopt, .askHelp = false};
	}
	if (gates.insideTemple)
	{
		presses = {.temple = presses.temple && byMouse.temple,
		           .realm = presses.realm && byMouse.realm,
		           .villagerNames = presses.villagerNames && byMouse.villagerNames,
		           .help = presses.help && byMouse.help,
		           .villagerDetails = presses.villagerDetails && byMouse.villagerDetails};
	}
	std::optional<ZoomKey> zoom;
	if (presses.temple && !gates.templeKeyBlocked)
	{
		zoom = ZoomKey::Temple;
	}
	else if (presses.realm && !gates.realmKeyBlocked)
	{
		zoom = ZoomKey::Realm;
	}
	return {.toggleVillagerNames = presses.villagerNames,
	        .toggleVillagerDetails = presses.villagerDetails,
	        .zoom = zoom,
	        .askHelp = presses.help && !gates.helpKeyBlocked};
}

/// The middle of the island the realm key flies over: halfway between the corners of the first and the last land
/// blocks along each axis. The land's extent runs on to the far side of its last block, one block further
[[nodiscard]] inline glm::vec2 RealmMiddle(glm::vec2 extentMinimum, glm::vec2 extentMaximum, float blockSize)
{
	return (extentMinimum + extentMaximum - glm::vec2(blockSize)) * 0.5f;
}

/// Whether a press of the temple key moves the camera: a single tap does not while the camera watches a fight (it only
/// starts the double tap's time)
[[nodiscard]] constexpr bool TempleKeyMovesCamera(bool doubleTap, bool watchingFight)
{
	return doubleTap || !watchingFight;
}

/// Whether the Show Villager Details key turns on the details of the thing under the hand (a living thing), rather
/// than every villager's
[[nodiscard]] constexpr bool DetailsKeyTakesTheThing(bool thingUnderHand, bool living)
{
	return thingUnderHand && living;
}

/// Acts on the options screen's one-press actions that no other part of the game takes: the temple and realm keys and
/// the villagers' names and details. The camera and hand keys are read where they are used; the temple's room keys by the game;
/// the leash and creature keys by their own work.
class ShortcutKeys
{
public:
	/// Acts on the presses of the frame
	void Update();
	/// A game starts on a land with the camera at the player's temple: the camera looks at the temple as at a start place
	/// a land's script gives, then flies to it as the temple key's double tap flies. Without a temple the camera stays
	/// where it is.
	void StartAtTemple();

private:
	/// A key's flight: the world camera's own, rising by the flight's share; the camera help hears of a flight to a place
	static void Fly(const zoom_to::Flight& flight);
	/// One press of the temple or realm key: its flight from the camera's view as it is now, its log line (the press-th
	/// of the frame's presses)
	void FlyForPlaceKey(ZoomKey zoom, glm::vec2 realm, const zoom_to::Altitude& altitude, std::size_t press,
	                    std::size_t presses);

	zoom_to::ZoomToPlaces _zoomTo;
};

} // namespace openblack::input
