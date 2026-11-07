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

// The interface's active switch (bit 0 of its flags). The script's wide screen makes the interface inactive
// (SetActive(!(on && owner))), a hand demo makes it active again so its recorded messages drive the real hand and
// inactive when it ends with a script still holding the bars. Wiki:
// docs/wiki/interface/screen-and-readouts.md#interface-interaction-levels.
namespace openblack::interface_active
{

/// Flag bit 0 = (active == 0). (pending) its other effects: a help system flag copied from the interface, an interface
/// state bit cleared, the action state reset and all immersion effects stopped
void SetActive(bool active);
/// !(flags & 1). While inactive the interface's hand state is 25
[[nodiscard]] bool IsActive();
/// What the help system's cinema bars set the switch to: inactive only while the bars are in with a script owning them,
/// so the game's own bars (no owner) leave the interface active, and so does taking any bars away
[[nodiscard]] constexpr bool ActiveWithWideScreen(bool on, uint32_t owner)
{
	return !(on && owner != 0);
}
/// The flags as a whole (bit 0 inactive; bits 1 and 2 are SET_INTERFACE_INTERACTION's limits)
[[nodiscard]] uint8_t GetFlags();
void SetFlags(uint8_t flags);

/// Whether a box holds the mouse: set by every frame's interface pass, from what that pass found (the escape menu, a
/// dialog, a debug window that took a click), and read until the next frame's pass. False before the first pass
void SetBoxHasMouse(bool box);
[[nodiscard]] bool BoxHasMouse();

} // namespace openblack::interface_active
