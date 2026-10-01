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
	int counter; ///< +0x84, 0..900: the atlas frame is frame_anim::MistCell, (c / 20) & 15; the caller advances it
	             ///< (fn_007FA300 adds ftol(g_game_time_inc * 0.255) only while the object is on screen: InView)
	/// +0x50 the specular, D3DCOLOR ARGB (SetColour 0x7F9770): fn_0080DB30 0x80DEF5 puts it in [0xE9FE2C], the vertices'
	/// specular, added to texture x diffuse (D3DRS_SPECULARENABLE, fn_0082C8F0 0x82CC54). Only the effect branch keeps it
	/// (the normal branch replaces it with the haze at 0x7FA6DD); the storm clouds' lightning glow (UR_CloudGather)
	uint32_t specular {0};
};

/// Draws this mist in the current frame only (call it every frame, before the scene is drawn)
void Submit(const MistDesc& mist);
/// The test of LH3DMist::AddDrawing 0x7FA7F0 that Submit's mists get when they are drawn: the sphere at the position,
/// of radius the mesh's +0x30 (0x7FA7FE) x size x 0.55 ([0x8D3E80], 0x7FA814), touches the camera's view. Only such a
/// mist goes to the Z-sorter, so only its Draw fn_007FA300 advances its animation counter (frame_anim::MistClock): a
/// caller keeping that counter advances it when this is true. True without a camera or a sky (tests). (aproximado) the
/// game camera at update time, not the camera of the pass that draws it (the same one in a frame); +0x30 taken as the
/// mesh's box half diagonal (ComputeBoundingBox 0x8081B0)
[[nodiscard]] bool InView(const glm::vec3& position, float size);
} // namespace openblack::mists
