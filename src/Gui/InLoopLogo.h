/*******************************************************************************
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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The Lionhead logo the game plays once, on the first pass of its loop, outside network games: the logo picture as
/// 4096 star sprites over the sky, which the camera flies round and into for about 33 seconds; the mouse's moves push
/// the stars about and the wheel slows it down; a key or the left or right mouse button ends it. One start in four (by
/// the machine's clock in seconds) shows a picture and a name from a file in the temporary folder instead.
///
/// The rules only: the stars' motion, the camera's path, the fades, the input's push and the end. The drawing is
/// graphics::InLoopLogoDraw's, the loop Game::PlayInLoopLogo's. Free of game state, so the tests drive it with fakes.
/// Every multiply and add is a float operation in the game's order (the game's FPU runs at 24 bits); the few that work
/// on the FPU's wider sine and exponent say so. Wiki: docs/wiki/engine/game-clock.md#the-in-loop-logo.
namespace openblack::in_loop_logo
{

/// The picture is 64 x 64, one star a pixel
inline constexpr int32_t k_PictureSize = 64;
inline constexpr size_t k_Stars = static_cast<size_t>(k_PictureSize) * k_PictureSize;
/// Its red, green and blue bytes, rows of y, each row x from 0 to 63 (data\lhlogo.raw)
inline constexpr size_t k_PictureBytes = k_Stars * 3;
using Picture = std::array<uint8_t, k_PictureBytes>;
/// The picture shown by default, in the game's Data folder
inline constexpr std::string_view k_DefaultPicture = "lhlogo.raw";
/// The file a start may show instead, in the temporary folder
inline constexpr std::string_view k_UserPictureFile = "userlogo.raw";
/// The name's longest length in the user's file, in UTF-16 characters
inline constexpr size_t k_NameLength = 128;
/// The name shown under the default picture: two blanks
inline constexpr std::u16string_view k_DefaultName = u"  ";
/// The stars' texture (gatheringtext.raw, with its alpha) and its cell, of an 8 x 8 sheet
inline constexpr uint8_t k_StarCell = 5;
/// A star whose green byte is at least this is drawn added to what is behind it; the others are blended over it
inline constexpr uint8_t k_BrightGreen = 0x80;
/// The camera the stars are placed with, looking at the origin; and the point it flies into
inline constexpr glm::vec3 k_StartEye {0.0f, 0.0f, 100.0f};
/// The near clip the logo is drawn with
inline constexpr float k_NearClip = 0.1f;
/// The sound sample played with it (the in-game bank's)
inline constexpr int k_Sample = 160;

/// Whether this start tries the user's file first: the machine's clock in whole seconds is a multiple of 4
[[nodiscard]] constexpr bool TriesUserPicture(uint32_t ticks) noexcept
{
	return ((ticks / 1000u) & 3u) == 0u;
}

/// What the user's file shows
struct UserPicture
{
	Picture picture {};
	std::u16string name;
};
/// The user's file: 4096 grey bytes, rows of y, each written into the red, green and blue of its pixel; then up to 128
/// UTF-16 characters (little-endian) of a name, which ends at its first 0. The file is shown only when its name is not
/// empty, so a file of 4096 bytes or fewer never is
[[nodiscard]] std::optional<UserPicture> ReadUserPicture(std::span<const uint8_t> bytes);

/// The C runtime's Random(a, b), drawn from its shared stream
using RandomFn = std::function<float(float, float)>;
/// The world point of a pixel at a depth along the camera's view (the camera the logo is drawn with)
using PointFn = std::function<glm::vec3(glm::ivec2 pixel, float depth)>;
/// A point's pixel and its depth, when it projects onto the screen (none when it is behind the near clip or off it)
struct OnScreen
{
	glm::ivec2 pixel {0};
	float depth {0.0f};
};
using ProjectFn = std::function<std::optional<OnScreen>(const glm::vec3& point)>;

/// One star
struct Star
{
	glm::vec3 position {0.0f};
	/// Its half width
	float size {1.0f};
	float angle {0.0f};
	uint32_t argb {0xFFFFFFFFu};
	/// Drawn added (green at least k_BrightGreen)
	bool bright {false};
};

/// The logo as it plays
struct State
{
	/// In the order they are drawn: by their blue byte, as the runtime's quicksort leaves them
	std::vector<Star> stars;
	/// Each star's own size, which the zoom scales
	std::vector<float> sizes;
	std::vector<glm::vec3> home;
	std::vector<glm::vec3> velocity;
	/// The mouse's smoothed move, in whole pixels
	glm::ivec2 mouseMove {0};
	/// 0.1 to 1: how fast the logo's time runs
	float speed {1.0f};
	/// The logo's own time, 0 before its first frame
	float time {0.0f};
	bool finished {false};
	/// The default picture and its blank name, with the trademark sign
	bool defaultPicture {true};
	std::u16string name {k_DefaultName};
};

/// The logo made for a screen of `screen` pixels: for each column x of the picture, each row y, a depth Random(70, 130),
/// the star at pixel (W / 2 + (x - 32) W / 240, H / 2 + (y - 32) W / 240) at that depth (whole-number divisions), its
/// colour the pixel's and opaque, its angle Random(0, 2 pi), its size depth / Random(200, 320) (at least 0.0001); then
/// sorted by their blue byte, and each star's place its home
[[nodiscard]] State Start(const Picture& picture, bool defaultPicture, std::u16string name, glm::ivec2 screen,
                          const PointFn& pointAt, const RandomFn& random);

/// What a frame of the logo shows, from its time
struct Frame
{
	/// The black fade over everything, 0 to 255: from 255 to 0 over the first 12 of time, back to 255 from 31 to 33
	uint8_t fade {0};
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	/// 0 to 1 from 15 to 26: the fly into the logo, eased
	float zoom {0.0f};
	/// 1 until 30, then down to -2 at 36
	float flash {1.0f};
	/// The sky's colour: white darkening from 22 on, its alpha 160 rising with it
	uint32_t skyArgb {0};
	/// The stars' alpha, 255 until 31, 0 from 33
	uint8_t starAlpha {255};
	/// What the stars' sizes are scaled by: 1 + 1.54 zoom^16
	float sizeScale {1.0f};
	/// The name's (and the trademark's box) fade in, 0 to 1 from 22 to 23, back to 0 from 33 to 34
	float nameFade {0.0f};
	/// The flash is over: the logo ends after this frame
	bool ends {false};
};

/// The time after a frame of `deltaMs`: 0.001 for the first frame, then time + deltaMs x speed x 0.0011
[[nodiscard]] float NextTime(float time, int32_t deltaMs, float speed) noexcept;
/// The camera's path and the fades at a time
[[nodiscard]] Frame FrameAt(float time) noexcept;
/// The time moves on by the frame, and the frame it shows; `finished` is set when it ends
Frame Advance(State& state, int32_t deltaMs);

/// The input of one frame
struct Input
{
	/// The mouse's move since the last frame, in pixels
	glm::ivec2 move {0};
	/// The mouse's place
	glm::ivec2 mouse {0};
	/// The wheel's turn since the last frame, 120 a notch, forwards positive
	int32_t wheel {0};
	int32_t deltaMs {0};
	int32_t screenWidth {0};
};

/// The game's exponential, as the FPU works it inline: x times its log2(e) rounded to a float's mantissa, then two to
/// that power with its fraction and the 1 added each rounded to a float's mantissa
[[nodiscard]] double FpuExp(float x) noexcept;

/// One frame of the stars after Advance, with the camera of that frame: the mouse's move smoothed by a tenth; the wheel
/// (within +-240) scales the speed by exp(wheel / 1500), kept within 0.1 and 1; when the smoothed move is not nothing,
/// each star on the screen within a tenth of the screen's width of the mouse (both ways) is pushed along it by
/// 4 (1 - distance / that width), at its own depth, and takes the push into its velocity; then each star springs back
/// home (velocity - (place - home) dt 10.01), slows by exp(-dt), moves, takes the frame's alpha and size, and a new
/// angle Random(0, 2 pi)
void Step(State& state, const Frame& frame, const Input& input, const ProjectFn& project, const PointFn& pointAt,
          const RandomFn& random);

/// The C runtime's draws a logo of `updates` frames takes from its stream: three a star as it starts, one a star each
/// frame
[[nodiscard]] constexpr uint64_t RandomDraws(uint64_t updates) noexcept
{
	return k_Stars * 3u + k_Stars * updates;
}

/// What ends the logo, checked before each frame: a key held, the left or the right mouse button held, or its end
[[nodiscard]] constexpr bool GoesOn(bool keyHeld, bool buttonHeld, bool finished) noexcept
{
	return !keyHeld && !buttonHeld && !finished;
}

/// Where the trademark sign goes and its size, on a screen of `screen` pixels: (W 0.64, H 0.32) cut to whole pixels,
/// its size W / 70 in whole pixels
struct TextPlace
{
	glm::vec2 position {0.0f};
	float size {0.0f};
};
[[nodiscard]] TextPlace TrademarkPlace(glm::ivec2 screen) noexcept;
/// Where the name goes: centred on the screen's middle (by its width at that size) at 0.71428573 of the height, its
/// size H / 20
[[nodiscard]] TextPlace NamePlace(glm::ivec2 screen, float nameWidth) noexcept;
/// The size the name is measured and drawn at
[[nodiscard]] float NameSize(glm::ivec2 screen) noexcept;

} // namespace openblack::in_loop_logo
