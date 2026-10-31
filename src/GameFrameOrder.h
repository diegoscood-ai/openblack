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

#include <array>
#include <optional>
#include <span>

/// The order of the steps of one frame. The game does its frame in a fixed order of stages: the input, the turns that
/// are due, the camera, the start of the draw, the hand and the draw. Each step of openblack's frame has the stage of the
/// game that it does. A step whose stage is not known keeps its place between its neighbours. The order keeps no game
/// state, so that the game and the tests use it both.
namespace openblack::game_frame
{
/// The stages of the game's frame, in the game's order
enum class Stage : uint8_t
{
	/// The keys and the mouse buttons that the player pressed
	Input,
	/// The menus and the dialogs of the interface take the input of the frame
	FrameUpdates,
	/// The turns of the game that are due, zero or one in a frame
	Turns,
	/// The game time of the frame, which is zero while the game is paused
	FrameTime,
	/// The camera moves, with the movement of the mouse in this frame, and shakes
	Camera,
	/// The draw starts. The real time of the frame is measured, a film shows its frames, and the creatures get their poses
	/// between their last two turns, before the hand moves.
	DrawStart,
	/// The hand goes to the point under the cursor, as the camera now sees it
	PreDraw,
	/// The world is drawn. The effects move on as the draw shows them, after the hand moved.
	Draw,
	/// The keys of the camera. Their steps move the camera in the next frame.
	CameraKeys,
	/// The end of the frame: the alignment of the player goes into the profile
	EndFrame,
};

/// The steps of openblack's frame
enum class Step : uint8_t
{
	/// The frame's real and fixed time, and the debug GUI's scale
	FrameClock,
	/// The window's events, the recorded input, the camera's keys, the shortcut and the leash keys
	Input,
	/// The frame ends here when the game stops or restarts
	StopCheck,
	/// The debug GUI takes its events; its Exit ends the program
	DebugGui,
	/// The in-game editor keeps its camera on what it has picked
	Editor,
	/// The turns that are due, and the temple's own turn
	Turns,
	/// The packets sent so far go to the session; the next turn applies them
	SendCommands,
	/// The frame clock and the real clock
	FrameTime,
	/// The camera's update, its shake, its near plane and the temple's frame
	Camera,
	/// The in-game menus and their test hooks
	Interface,
	/// The full screen film and the falling spell's film
	Video,
	/// The frame updaters: what moves on with the frame's game time before the draw
	FrameEffects,
	/// The screenshot after a text (a test hook)
	TextShot,
	/// A hand demo's mouse, buttons and camera
	HandDemo,
	/// The landscape's object draw list, now that the camera is as it is drawn, then the camera bookmarks' pulse
	ObjectDrawList,
	/// What no draw drew this frame is not drawn
	DrawnFrameEnd,
	/// The help system's texts
	Draw3D,
	/// The advisors' control, flight, animations and pose
	Advisors,
	/// The key and mouse icons, twice a frame
	InputPrompts,
	/// The hand's state, the interface's frame pass, the hand's point, its animation and place, the miracles' frame,
	/// the influence crossings, the leashes, the trees' bends, the hand interface and the entities' draw preparation
	Hand,
	/// The audio's frame and the music
	Audio,

	_Count
};

/// A step of the frame and the stage of the game that it does. No stage: the place of the step in the game's frame is
/// not known, and the step keeps its place between its neighbours.
struct Place
{
	Step step;
	std::optional<Stage> stage;
};

/// The frame, step by step, in openblack's order (docs/wiki/engine/loop.md#openblacks-loop)
inline constexpr std::array k_Frame = std::to_array<Place>({
    {.step = Step::FrameClock, .stage = std::nullopt},
    {.step = Step::Input, .stage = Stage::Input},
    {.step = Step::StopCheck, .stage = std::nullopt},
    {.step = Step::DebugGui, .stage = std::nullopt},
    {.step = Step::Editor, .stage = std::nullopt},
    {.step = Step::Turns, .stage = Stage::Turns},
    {.step = Step::SendCommands, .stage = Stage::Turns},
    {.step = Step::FrameTime, .stage = Stage::FrameTime},
    {.step = Step::Camera, .stage = Stage::Camera},
    // (approximate) the menus after the camera: the game's interface takes the frame's input before the turns
    {.step = Step::Interface, .stage = std::nullopt},
    {.step = Step::Video, .stage = Stage::DrawStart},
    {.step = Step::FrameEffects, .stage = Stage::DrawStart},
    {.step = Step::TextShot, .stage = std::nullopt},
    {.step = Step::HandDemo, .stage = std::nullopt},
    // (approximate) these draw steps come before the hand here; the game draws the world after the hand moved
    {.step = Step::ObjectDrawList, .stage = std::nullopt},
    {.step = Step::DrawnFrameEnd, .stage = std::nullopt},
    {.step = Step::Draw3D, .stage = std::nullopt},
    {.step = Step::Advisors, .stage = std::nullopt},
    {.step = Step::InputPrompts, .stage = std::nullopt},
    {.step = Step::Hand, .stage = Stage::PreDraw},
    {.step = Step::Audio, .stage = std::nullopt},
});

/// True when no step of the frame comes before a step of an earlier stage. The steps with no stage are not checked.
[[nodiscard]] constexpr bool InGameOrder(std::span<const Place> frame) noexcept
{
	std::optional<Stage> last;
	for (const auto& place : frame)
	{
		if (!place.stage.has_value())
		{
			continue;
		}
		if (last.has_value() && *place.stage < *last)
		{
			return false;
		}
		last = place.stage;
	}
	return true;
}

/// Does each step of the frame, in the order of the frame. A step that returns false stops the frame.
template <typename Do>
bool PlayFrame(Do&& step)
{
	for (const auto& place : k_Frame)
	{
		if (!step(place.step))
		{
			return false;
		}
	}
	return true;
}

static_assert(k_Frame.size() == static_cast<size_t>(Step::_Count), "Each step has one place in the frame");
static_assert(InGameOrder(k_Frame), "The steps of the frame follow the game's stages");

} // namespace openblack::game_frame
