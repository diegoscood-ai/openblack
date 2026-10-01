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

#include <glm/vec3.hpp>

// The cloud puffs of the registered storms: GWeather::DrawClouds 0x83FC90, every frame for every storm from fn_0083F8B0
// (fn_005E5830, after the sky clouds fn_005E25C0). Each storm keeps up to 16 LH3DMist domes (+0xB8 count, +0xBC 0x30-byte
// records) drifting round its centre, darkened by its blackness and faded with it; they go to mapa's mists::Submit.
// The miracle storm has none (UR_CloudGather registers numClouds 0: its clouds are the PSys mists); the climate storms,
// the weather things and CREATE_WEATHER_STORM have the descriptor's (8 by default). Wiki: docs/bw1-notes/magic.md,
// "Tormenta" (las nubes de las tormentas registradas).

namespace openblack::weather::storm_clouds
{
/// One puff (GWeather +0xBC + 0x30 i)
struct Puff
{
	glm::vec3 offset {0.0f};   ///< +0x04 x, z in -1..1 (times (outer + inner) / 2), y the height above the land
	glm::vec3 target {0.0f};   ///< +0x10
	glm::vec3 step {0.0f};     ///< +0x1C added every frame
	int frames {0};            ///< +0x28 frames left to the target
	float size {0.0f};         ///< +0x2C x (outer + inner): the mist's size
	float k {2.5f};            ///< the mist's +0x8C, Random(2.5, 5.0)
	/// the mist's +0x84 (its atlas frame, frame_anim::MistCell). (aproximado) it starts at 0, not at the LH3DMist ctor's
	/// ftol(Random(0, 16)) & 15 (0x7F95F8): the same cell 0, a few counts of phase apart
	int counter {0};
	float counterRemainder {0.0f};
};

/// The colour of a puff (0x83FF56..0x840000): the land light table's base colour [0xFA26A4], each RGB byte x (1 - 0.5 x
/// blackness) (ftol) when the blackness is above 0, and the alpha ftol(fade x 0.75 x the base's alpha byte)
[[nodiscard]] uint32_t PuffColour(uint32_t baseArgb, float blackness, float fade);

/// Every frame (weather::UpdateFrame): DrawClouds of every storm, `milliseconds` for the mists' atlas counters
void DrawFrame(float milliseconds);
/// A land is loaded
void Clear();
/// The puffs of a storm now (tests and traces); 0 for a storm with none
[[nodiscard]] size_t PuffCount(uint32_t stormId);
} // namespace openblack::weather::storm_clouds
