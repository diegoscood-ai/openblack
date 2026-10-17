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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "3D/TempleChallengeRoom.h"

namespace openblack
{

/// The challenge room's pictures, frames and buttons on its mesh (temple_challenge_room): which of them are shown for
/// the challenges found so far, and the presses on them. A press on a picture, a frame or a button shown takes the mouse
/// from the room's camera, and letting go over the same picture or button is its click (a frame's does nothing). The
/// room's scroll is the scrolls' own (TempleScrolls).
class TempleChallengeControls
{
public:
	/// Finds the controls among the challenge room's submeshes, by their names
	void Find(std::span<const std::string> subMeshNames);

	/// The challenges of the slots, the first found first; past the room's slots they have none
	void SetChallenges(std::span<const temple_challenge_room::Challenge> challenges);

	/// A click on a picture or a button: its slot and its submesh
	struct Click
	{
		temple_challenge_room::Control control;
		uint32_t slot;
		uint32_t subMesh;

		bool operator==(const Click&) const = default;
	};
	/// The press on the hovered picture, frame or button shown, and a picture's or button's click as it is let go over
	/// the same one. True while one has the press.
	bool Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, std::optional<Click>& click);
	[[nodiscard]] bool IsHeld() const { return _held.has_value(); }

	/// The submeshes of the controls not shown, which are neither drawn nor picked
	[[nodiscard]] std::vector<uint32_t> GetHidden() const;
	/// Whether a submesh is a picture, frame or button shown, a control the cursor makes glow
	[[nodiscard]] bool IsControl(uint32_t subMesh) const;
	/// The challenge of a slot, none for a slot without one yet
	[[nodiscard]] std::optional<temple_challenge_room::Challenge> ChallengeOf(uint32_t slot) const;

private:
	struct Found
	{
		uint32_t subMesh;
		temple_challenge_room::ControlSlot control;
	};
	[[nodiscard]] const Found* FindShown(uint32_t subMesh) const;
	[[nodiscard]] bool Shown(const Found& found) const;

	std::vector<Found> _controls;
	std::array<std::optional<temple_challenge_room::Challenge>, temple_challenge_room::k_Slots> _challenges {};
	std::optional<Found> _held;
	bool _wasPressed {false};
};

} // namespace openblack
