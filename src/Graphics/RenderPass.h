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
#include <string_view>

namespace openblack::graphics
{

enum class RenderPass : uint8_t
{
	Footprint,
	/// Static object shadows baked into an island-wide texture (the original bakes them into the block textures)
	StaticShadow,
	/// Land alpha lowered by the river channels (the original's river footprints lower the block textures' alpha)
	LandAlpha,
	Reflection,
	Main,
	/// Blended (fading) models, drawn over the finished main pass so the water cannot be sorted over them.
	MainBlended,
	/// The 3D of LH3DRender::FinishFrame's "after" callbacks, over the Z reset quad (0x82F460 (d): its depth cleared):
	/// the advisor spirits' overlay (HelpDude 0x5C2E10, priority 100; Graphics/RendererSpirits.cpp)
	FinishFrame3D,
	/// Screen-space quads at the end of the frame (LH3DRender::FinishFrame): cinema bars and the screen fade
	ScreenOverlay,
	ImGui,
	MeshViewer,

	_count
};

static constexpr std::array<std::string_view, static_cast<uint8_t>(RenderPass::_count)> k_RenderPassNames {
    "Footprint Pass",   //
    "Static Shadow Pass", //
    "Land Alpha Pass",    //
    "Reflection Pass",  //
    "Main Pass",        //
    "Main Blended Pass", //
    "Finish Frame 3D Pass", //
    "Screen Overlay Pass", //
    "ImGui Pass",       //
    "Mesh Viewer Pass", //
};

} // namespace openblack::graphics
