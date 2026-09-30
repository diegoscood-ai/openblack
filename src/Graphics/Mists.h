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

/// LH3DMist objects that are not map mists (e.g. the storm puffs of GWeather::DrawClouds 0x83FC90): the caller keeps
/// its own objects and submits them every frame; they are culled, sorted back to front with the map mists and the
/// blended models, and drawn by the same fn_007FA300 port (Renderer::DrawMist, RendererMists.cpp).
namespace openblack::mists
{
struct MistDesc
{
	glm::vec3 position; ///< the object's position (the dome's centre)
	float size;         ///< +0x88: the mesh scale
	uint32_t colour;    ///< ARGB; the alpha (+0x90) is colour >> 24, nothing is drawn with 0
	/// +0x80 bit 2 (+0x8C != 1): the effect branch, size shrunk to size / (1 + (k - 1)(1 - |dy| / |d|)) along local Y
	/// and Z, lit from straight above with ambient 210, atlas rows 2-3; without it the plain size, the colour times the
	/// land light under it plus the haze, the models' light, atlas rows 0-1
	bool edgeShrink;
	float k;     ///< +0x8C
	int counter; ///< +0x84, 0..900: the atlas frame is (counter / 20) & 15; the caller advances it (fn_007FA300 adds
	             ///< ftol(g_game_time_inc * 0.255) only while the object is on screen)
};

/// Draws this mist in the current frame only (call it every frame, before the scene is drawn)
void Submit(const MistDesc& mist);
} // namespace openblack::mists
