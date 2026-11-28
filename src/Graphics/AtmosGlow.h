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

/// The atmosphere's screen glow: while the Weather setting is on and the sky's darkness is above 0, a 16 x 16 copy of
/// the finished frames follows the screen an eighth of the way each frame, and the frame adds it over itself, in the
/// darkness's grey, between the end of the frame's 2D and the screen fade. These are its rules; the renderer runs the
/// step and the 4-bit cut on the GPU (fs_atmos_glow_step, fs_atmos_glow_pack), the same integer steps in floats.
/// Wiki: docs/wiki/world/weather.md#the-screen-glow.
namespace openblack::graphics::atmos_glow
{

/// The cells across and down the screen
constexpr int k_Cells = 16;
/// The largest value a cell's channel keeps: what its 4-bit cut takes as 15 with the blue's rounding
constexpr int32_t k_CellLimit = 0xF7000;
/// A screen channel of 8 bits is taken times 4096
constexpr int k_ChannelShift = 12;

/// The screen pixel a cell reads, from the top left: the columns w / 16 apart from the middle of the first, the rows
/// h / 16 apart from h / 32, every division a whole one
[[nodiscard]] constexpr glm::ivec2 CellPixel(int column, int row, glm::ivec2 screen) noexcept
{
	const int step = screen.x >> 4;
	return {column * step + step / 2, row * screen.y / k_Cells + screen.y / (2 * k_Cells)};
}

/// One channel of a cell after a frame: an eighth of the way to the screen's channel times 4096, the eighth cut
/// towards zero, then held to 0..k_CellLimit
[[nodiscard]] constexpr int32_t StepChannel(int32_t value, uint8_t screen) noexcept
{
	const int32_t target = static_cast<int32_t>(screen) << k_ChannelShift;
	const int32_t stepped = value + (target - value) / 8;
	return stepped < 1 ? 0 : (stepped > k_CellLimit - 1 ? k_CellLimit : stepped);
}

/// A cell's channels as the glow's 16-bit texel, alpha 15: each the 4 bits above its value's 16th, rounded up from
/// 0x80 for the red, 0x800 for the green and 0x8000 for the blue
[[nodiscard]] constexpr uint16_t PackCell(int32_t red, int32_t green, int32_t blue) noexcept
{
	return static_cast<uint16_t>(0xF000u | ((static_cast<uint32_t>(red + 0x80) >> 8) & 0xF00u) |
	                             ((static_cast<uint32_t>(green + 0x800) >> 12) & 0xF0u) |
	                             (static_cast<uint32_t>(blue + 0x8000) >> 16));
}

/// The colour (0xAARRGGBB) the glow's quad is drawn in: opaque, grey by the darkness
[[nodiscard]] constexpr uint32_t QuadColour(uint8_t darkness) noexcept
{
	return 0xFF000000u + darkness * 0x010101u;
}

/// What a frame is drawn as, for the glow
struct FrameInputs
{
	/// The Weather detail setting
	bool weather {false};
	/// The sky's darkness, 0 to 90
	uint8_t darkness {0};
	/// The frame is drawn inside the temple
	bool temple {false};
	/// The frame is the falling spell's film
	bool fallingSpell {false};
	/// The world view drew the land this frame (not under a film that covers the screen)
	bool worldDrawn {false};
};

/// What the glow does in a frame
struct FrameUse
{
	/// The finished frame steps the cells (the world's view, also under a film that covers the screen)
	bool steps {false};
	/// The glow is added over the frame, before the screen fade (only when the world's view drew the land)
	bool draws {false};

	bool operator==(const FrameUse&) const = default;
};

[[nodiscard]] constexpr FrameUse ForFrame(const FrameInputs& in) noexcept
{
	const bool on = in.weather && in.darkness > 0;
	return {.steps = on && !in.temple && !in.fallingSpell, .draws = on && in.worldDrawn};
}

/// (debug) what the glow did in the last frame drawn
struct Readout
{
	uint8_t darkness {0};
	bool stepped {false};
	bool drawn {false};
	/// The frames that stepped the cells since the start
	uint64_t steps {0};
	/// Why a frame that should have stepped could not, or nullptr
	const char* refused {nullptr};
};

} // namespace openblack::graphics::atmos_glow
