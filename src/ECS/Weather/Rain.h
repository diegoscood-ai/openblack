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
#include <vector>

#include <glm/vec3.hpp>

// The rain the engine draws where the LH3DAtmos grid says it rains (LH3DAtmos::Update3D 0x8357A0, Render3D 0x836250 and
// the rain object of fn_00833DA0..fn_00834370): one set of 128 streaks (lines from 50 m under the ground up to the
// cloud height, a dashed row of atmos.raw scrolling along them) repeated over 160 m tiles, one per land block near the
// camera, as dense and opaque as the rain of the block's four middle cells. This file keeps the streaks and picks the
// tiles; Graphics/RendererRain.cpp draws them.

namespace openblack::weather::rain
{
constexpr int32_t k_Drops = 128; ///< 0x80 streaks of 0x1C bytes (0xEDC358)

/// One streak (fn_00833D10)
struct Drop
{
	float phase {0.0f};  ///< +0x00 0..1 at 2.4 per second: fades in over the first 5 %, out over the last; a new place when it wraps
	float x {0.0f};      ///< +0x04 -80..80 from the tile centre
	float z {0.0f};      ///< +0x08
	float dx {0.0f};     ///< +0x0C -15..15: the top end's offset (the slant)
	float dz {0.0f};     ///< +0x10
	float scroll {0.0f}; ///< +0x14 0..1: the texture's u at the bottom (u + 1 at the top)
	float speed {0.0f};  ///< +0x18 0.1..0.2 texture turns per second (x the fall speed)
};

/// One tile to draw (fn_008341B0 -> Z-sorter -> fn_00833F80 -> fn_00834370)
struct Tile
{
	glm::vec3 origin {0.0f}; ///< the block's centre on its 80 m grid, at the land height there
	int32_t drops {0};       ///< 128, fewer from 100 m to 400 m away
	int32_t alpha {0};       ///< the bottom end's alpha: max rain of the four cells x 88 / 100, faded with the distance
	int32_t alphaTop {0};    ///< alpha / ((2 d / 400 + 1) x 5)
};

/// LH3DAtmos's rain object (fn_00833DA0): 128 streaks at random places
void Reset();
/// LH3DAtmos::Update3D 0x8357A0 with the frame's seconds: the nearest storm to the camera sets where the streaks
/// start (its elevation, 160 without a storm) and how fast they scroll (its fall speed, 1), each moving 0.3 of the way
/// per frame (40..640 m, 0.3..5); then the streaks step, if the rain was drawn last frame (fn_00833EA0)
void Update(float seconds, const glm::vec3& camera);
/// Render3D 0x836250 (the rain half): the tiles of the land blocks within 560 m whose four middle cells rain more than
/// 5, and fn_00834370's distance fade (nothing from 400 m)
[[nodiscard]] std::vector<Tile> CollectTiles(const glm::vec3& camera);
/// The renderer drew the streaks this frame (0xEDC300 = 1): the next Update steps them
void MarkDrawn();

[[nodiscard]] const std::array<Drop, k_Drops>& Drops();
/// 0xC38E10: the streaks' top above the tile's ground (160 at start)
[[nodiscard]] float Elevation();
/// 0xC38E14: the scroll speed factor (1 at start)
[[nodiscard]] float FallSpeed();
/// fn_00834370's per-streak fade: phase < 0.05 -> phase x 20, > 0.95 -> (1 - phase) x 20, else 1
[[nodiscard]] float PhaseFade(float phase);
} // namespace openblack::weather::rain
