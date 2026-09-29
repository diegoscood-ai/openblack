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
	/// Dynamic shadow silhouettes (the hand)
	DynamicShadow,
	/// The physics objects' shadow silhouettes (PhysicsShadows), 4 x 2 subsamples per texel
	PhysicsShadow,
	/// Their subsample count into the 32 x 32 shadow textures
	PhysicsShadowResolve,
	Reflection,
	Main,
	/// Blended (fading) models, drawn over the finished main pass so the water cannot be sorted over them.
	MainBlended,
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
    "Dynamic Shadow Pass", //
    "Physics Shadow Pass", //
    "Physics Shadow Resolve Pass", //
    "Reflection Pass",  //
    "Main Pass",        //
    "Main Blended Pass", //
    "Screen Overlay Pass", //
    "ImGui Pass",       //
    "Mesh Viewer Pass", //
};

} // namespace openblack::graphics
