/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>

#include "Window.h"

namespace openblack::debug::gui
{

/// The camera paths of the resource cache: pick one, see its points and length, and run, stop or pause it through the
/// camera path system. Above them, closed at first, what the camera help heard of the player's camera, and the player
/// camera's controls. Below, closed at first, the placed path that has the camera (a miracle's): who placed it, its
/// glide and time, its particle's last drawn frame, and a button that lets it go. Below that, also closed at first,
/// what the hand's tooltip over the land reads and offers, and the bookmarks' shared pulse
class Camera final: public Window
{
public:
	Camera() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void DrawControls();
	void DrawCameraResourceList();
	void DrawPlacedPath();
	/// Closed at first: whether the player's camera's last update heard its own frame's controls, where their input came
	/// from (the camera help's input mask), what it reported, and the game turns counted for the scripts
	static void DrawCameraHelp();
	/// Closed at first: how many frames the player's camera had no controls of its own, in which nothing read before
	/// moves it, whether its controls are held back after a drag given up or at the fence, with the zoom asked for, and
	/// how many times the end of a script's camera control made it again, its easing's and flight's times, and the turn
	/// and tilt keys held as it adds them
	static void DrawCameraControls();
	/// Closed at first: the bookmarks' shared pulse (its time, width and height, the markers' half size) and which
	/// bookmarks are on the land
	static void DrawBookmarks();
	/// Closed at first: the facts the hand's tooltip over the land reads (the hand state, the camera's hints, what the
	/// scripts allow, the player's camera's view) and the text they give
	void DrawHandToolTip();
	entt::id_type _selectedCameraPath;
};

} // namespace openblack::debug::gui
