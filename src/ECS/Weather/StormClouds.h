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

// The cloud puffs of the registered storms, drawn every frame for every storm after the sky clouds. Each storm keeps up
// to 16 mist domes drifting round its centre, darkened by its blackness and faded with it; they go to mists::Submit.
// The miracle storm has none (UR_CloudGather registers numClouds 0: its clouds are the PSys mists); the climate storms,
// the weather things and CREATE_WEATHER_STORM have the descriptor's (8 by default). Wiki:
// docs/wiki/magic/storm-and-tornado.md#the-flash-and-the-clouds-of-the-registered-storms.

namespace openblack::weather::storm_clouds
{
/// One puff
struct Puff
{
	glm::vec3 offset {0.0f}; ///< x, z in -1..1 (times (outer + inner) / 2), y the height above the land
	glm::vec3 target {0.0f};
	glm::vec3 step {0.0f}; ///< added every frame
	int frames {0};        ///< frames left to the target
	float size {0.0f};     ///< x (outer + inner): the mist's size
	float k {2.5f};        ///< the mist's k, Random(2.5, 5.0)
	/// the mist's counter (its atlas frame, frame_anim::MistCell). (approximate) it starts at 0, not at the original
	/// mist's (Random(0, 16) truncated toward zero) & 15: the same cell 0, a few counts of phase apart
	int counter {0};
};

/// The colour of a puff: the land light table's base colour, each RGB byte x (1 - 0.5 x
/// blackness) when the blackness is above 0, and the alpha fade x 0.75 x the base's alpha byte, each truncated
/// toward zero
[[nodiscard]] uint32_t PuffColour(uint32_t baseArgb, float blackness, float fade);

/// A storm keeps at most this many puffs
inline constexpr int k_MaxClouds = 16;
/// A storm's count of clouds, capped at k_MaxClouds
[[nodiscard]] int CappedCloudCount(int numClouds);
/// Whether a storm with `puffs` puffs takes a new one this frame: one a frame until it has its count
[[nodiscard]] bool AddsPuff(size_t puffs, int numClouds);
/// Where a puff stands across the land, x and z: its offset (-1 to 1 each way) times half the storm's spread (its outer
/// plus its inner radius) about the storm's middle
[[nodiscard]] glm::vec2 PuffGround(float spread, const glm::vec3& offset, const glm::vec3& centre);

/// Every frame (weather::UpdateFrame): the clouds of every storm, `milliseconds` (the frame's whole game ms) for the
/// mists' atlas counters
void DrawFrame(uint32_t milliseconds);
/// A land is loaded
void Clear();
/// The puffs of a storm now (tests and traces); 0 for a storm with none
[[nodiscard]] size_t PuffCount(uint32_t stormId);
} // namespace openblack::weather::storm_clouds
