/*******************************************************************************
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
#include <span>

#include "3D/CreatureCaveTargets.h"
#include "3D/TempleChallengeControls.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleScrolls.h"
#include "3D/TempleToggles.h"
#include "Gui/ToolTips.h"

namespace openblack
{

/// The tooltip the temple submits each turn: the one of the game's 170 tooltips, the action whose mouse it shows and
/// its arrows. It stays as it was in what the rooms don't set, as the game's do.
struct TempleToolTip
{
	/// None submits nothing, as an id outside the tooltips' does
	std::optional<uint32_t> index;
	gui::ToolTipAction action;
	uint32_t arrows;
};

/// What the rooms choose their tooltip by, each frame
struct TempleToolTipInput
{
	TempleRoom room;
	/// Whether the player has the room's camera, rather than its path or a walk through a door
	bool inControl;
	/// How close the camera has come to the scroll it looks at, and whether it looks at one
	float zoom;
	bool lookingAtScroll;
	/// Whether the room has a press: from the frame the button goes down in it, on one of its controls or on the room
	/// itself, until it is let go (RoomPressHeld)
	bool pressHeld;
	bool overPool;
	bool pressingPool;
	std::optional<uint32_t> hoveredDoor;
	bool overWayBack;
	/// The creature's room's target the cursor is over, and whether the camera zooms to one
	std::optional<CreatureCaveTargets::Target> caveTarget;
	bool zoomingToCaveTarget;
	/// The submesh of the room's mesh the cursor is over
	std::optional<uint32_t> hoveredSubMesh;
	std::span<const TempleScrolls::Control> scrolls;
	/// The main room's buttons drawn
	std::span<const TempleToggles::Control> toggles;
	/// The challenge room's pictures and buttons, the slot of the picture last looked at in high detail, and whether
	/// the camera was last sent to a picture rather than the scroll
	std::span<const TempleChallengeControl> challenges;
	std::optional<uint32_t> challengeHighDetail;
	bool pictureLookedAt {false};
};

/// Whether the room keeps a press this frame: one begins as the button goes down in the room, on a control or not, and
/// ends as it is let go. A press still held as the room changes, or while a dialog is up, is not the room's, nor one
/// held from before then.
[[nodiscard]] constexpr bool RoomPressHeld(bool wasHeld, bool pressed, bool wasPressed, bool roomChanged, bool dialogUp)
{
	if (!pressed || roomChanged || dialogUp)
	{
		return false;
	}
	return wasHeld || !wasPressed;
}

/// The tooltip before the temple has set one: Rotate, with the left button
constexpr TempleToolTip k_FirstTempleToolTip {.index = 4, .action = gui::ToolTipAction::Select, .arrows = 0};

/// What the hand shows in the temple for what it is over, as each room, its scrolls and then the temple as a whole
/// decide it
void UpdateTempleToolTip(TempleToolTip& toolTip, const TempleToolTipInput& input);

} // namespace openblack
