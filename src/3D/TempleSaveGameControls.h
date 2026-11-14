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
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Temple/SaveGameRoomRules.h"

namespace openblack
{

/// The save game room's pictures and buttons on its mesh (temple::save_game_room): which of them are shown for the
/// room's list of saved games and the camera, and the presses on them. A press on a control shown takes the mouse from
/// the room's camera, and letting go over the same control, still shown, is its click. The room's scroll is the
/// scrolls' own (TempleScrolls).
class TempleSaveGameControls
{
public:
	/// Finds the controls among the room's submeshes, by their names
	void Find(std::span<const std::string> subMeshNames);

	/// The saved games the room lists, in its order
	void SetSaves(std::vector<temple::save_game_room::Entry> saves) { _saves = std::move(saves); }
	[[nodiscard]] const std::vector<temple::save_game_room::Entry>& GetSaves() const { return _saves; }
	std::vector<temple::save_game_room::Entry>& Saves() { return _saves; }
	[[nodiscard]] bool HasSave(uint32_t slot) const;

	/// A click on a control: its control and its submesh
	struct Click
	{
		temple::save_game_room::Control control;
		uint32_t subMesh;

		bool operator==(const Click&) const = default;
	};
	/// The press on the hovered control shown, and its click as it is let go over the same one. True while one has the
	/// press.
	bool Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, const temple::save_game_room::View& view,
	          std::optional<Click>& click);
	[[nodiscard]] bool IsHeld() const { return _held.has_value(); }

	/// The submeshes of the controls not shown, which are neither drawn nor picked
	[[nodiscard]] std::vector<uint32_t> GetHidden(const temple::save_game_room::View& view) const;
	/// Whether a submesh is a control shown, which the cursor makes glow
	[[nodiscard]] bool IsControl(uint32_t subMesh, const temple::save_game_room::View& view) const;
	/// The controls shown, as their parts and slots, in the mesh's order
	[[nodiscard]] std::vector<temple::save_game_room::Control> GetShown(const temple::save_game_room::View& view) const;

	/// A slot's picture on the room's mesh
	struct Picture
	{
		uint32_t subMesh;
		uint32_t slot;
	};
	/// The slots' pictures, in the mesh's order
	[[nodiscard]] std::vector<Picture> GetPictures() const;

	/// Coming into the room, the camera was not sent to a picture
	void Enter() { _lookedAt.reset(); }
	/// The camera was sent to a slot's picture
	void LookAt(uint32_t slot) { _lookedAt = slot; }
	/// The camera was sent to the room's scroll
	void LookedAtScroll() { _lookedAt.reset(); }
	/// The slot of the picture the camera was last sent to, while it has not been sent to the scroll since
	[[nodiscard]] std::optional<uint32_t> GetLookedAtSlot() const { return _lookedAt; }

	/// A control of the room's mesh: its submesh, its part and its slot
	struct Placed
	{
		uint32_t subMesh;
		temple::save_game_room::Control control;
	};
	/// Every control of the room's mesh, shown or not, in the mesh's order
	[[nodiscard]] std::span<const Placed> GetControls() const { return _controls; }

private:
	using Found = Placed;
	[[nodiscard]] bool Shown(const Found& found, const temple::save_game_room::View& view) const;
	[[nodiscard]] const Found* FindShown(uint32_t subMesh, const temple::save_game_room::View& view) const;

	std::vector<Found> _controls;
	std::vector<temple::save_game_room::Entry> _saves;
	std::optional<Found> _held;
	bool _wasPressed {false};
	std::optional<uint32_t> _lookedAt;
};

} // namespace openblack
