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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The player camera's push scroll: in full screen, pushing the mouse on past the screen's edge moves the camera over
/// the land, and turns it, with a speed that builds up while the mouse pushes and dies away once it stops. It needs the
/// profile's push scrolling and the camera help's edge scroll feature both on.
///
/// The camera keeps a cursor of its own that runs on past the screen's edges by the mouse's movement while the pointer
/// stands at an edge, and follows the pointer elsewhere. Each frame of the keys, a mouse moving outwards while that
/// cursor is an eighth of the screen's width or more past the left or right edge adds to the speed across (x) and to
/// the turn (z), more the further from the screen's middle row the pointer is; past the top or bottom edge, to the speed
/// up and down the screen (y) and the turn, by 0.87 of that, more the further from the middle column. Each update the
/// camera moves by the speed and turns by the turn, then they die away: fast with the pointer inside the screen, slowly
/// while it stands at an edge.
namespace openblack::edge_scroll
{

/// The camera help's feature that lets the screen's edges scroll the camera
inline constexpr int32_t k_Feature = 0x100;
/// The speed's step from the camera's height over its focus, as the zoom's: three times it, at least this, at most this
/// above the focus and four times the least below it; then halved
inline constexpr float k_MinSpeed = 60.0f;
inline constexpr float k_MaxSpeed = 2000.0f;
/// How much of the step a pixel of the mouse's push adds to the speed, and to the turn
inline constexpr float k_Factor = 0.14f;
inline constexpr float k_TurnFactor = 1.0f;
/// A push up or down adds this share of what a push across does
inline constexpr float k_UpDownShare = 0.87f;
/// Below this squared speed the camera stands still
inline constexpr float k_StopSpeedSq = 0.01f;

/// Whether the push scroll works: the profile's push scrolling and the camera help's feature, in full screen only
[[nodiscard]] bool Enabled(bool pushScrolling, int32_t features, bool windowed);

/// The step the push's speed is made from, by the heights of the camera's drawn origin and focus
[[nodiscard]] float Step(float originHeight, float focusHeight);

/// The camera's own cursor this frame, and whether the pointer is inside the screen across and down (not at an edge)
struct Cursor
{
	glm::ivec2 position {0, 0};
	bool insideX {true};
	bool insideY {true};
};
/// The cursor follows the pointer on each axis where the pointer is inside the screen's edges (or the game is in a
/// window), and runs on by the mouse's movement where it stands at an edge
[[nodiscard]] Cursor FollowPointer(glm::ivec2 cursor, glm::ivec2 pointer, glm::ivec2 mouseDelta, glm::ivec2 screen,
                                   bool windowed);

/// The speed after a frame of the keys (`seconds` long), the push scroll working and the land not gripped: the speed as
/// it was without any mouse movement or with the camera's cursor inside the band; else what the push adds (see above).
/// The speed is x across, y up and down, z the turn
[[nodiscard]] glm::vec3 Push(glm::vec3 speed, glm::ivec2 cursor, glm::ivec2 pointer, glm::ivec2 mouseDelta, glm::ivec2 screen,
                             float step, float seconds);

/// The speed after an update `seconds` long: across and up and down by e^(-7 t) with the pointer inside the screen both
/// ways, else e^(-0.2 t); the turn by e^(-7 t), else e^(-2.2 t) (DecayFactor); none once its squared size is below
/// k_StopSpeedSq
[[nodiscard]] glm::vec3 Decay(glm::vec3 speed, bool insideX, bool insideY, float seconds);

/// e^(rate x seconds) as the game's FPU works it out: rate x seconds and its product with log2(e) each rounded to a
/// float, the whole part of that power of 2 taken off to the nearest, 2 to the rest less 1 kept unrounded and 1 added
/// back rounded to a float, then scaled by the whole part
[[nodiscard]] float DecayFactor(float rate, float seconds);

/// A frame's seconds as the camera's update takes them: its whole milliseconds times the float 0.001
[[nodiscard]] float FrameSeconds(int64_t milliseconds);

/// Whether the camera stands still at that speed
[[nodiscard]] bool Stopped(glm::vec3 speed);

/// The move over the land of an update: the speed's across and up and down parts times the update's seconds, as
/// (-0.16 across, 0.16 ahead over the tangent of the pitch, kept between 0.2 and 2), turned by the heading as the movement
/// keys' move is
[[nodiscard]] glm::vec3 MoveOverLand(glm::vec3 speed, float seconds, float pitch, float heading);

/// The turn of an update, in radians: the speed's turn times the update's seconds, times pi over the screen's width
[[nodiscard]] float Turn(glm::vec3 speed, float seconds, int32_t screenWidth);

} // namespace openblack::edge_scroll
