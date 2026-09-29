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
	/// Dynamic shadow silhouettes (the hand)
	DynamicShadow,
	Reflection,
	Main,
	/// Blended (fading) models, drawn over the finished main pass so the water cannot be sorted over them.
	MainBlended,
	ImGui,
	MeshViewer,

	_count
};

static constexpr std::array<std::string_view, static_cast<uint8_t>(RenderPass::_count)> k_RenderPassNames {
    "Footprint Pass",   //
    "Static Shadow Pass", //
    "Dynamic Shadow Pass", //
    "Reflection Pass",  //
    "Main Pass",        //
    "Main Blended Pass", //
    "ImGui Pass",       //
    "Mesh Viewer Pass", //
};

} // namespace openblack::graphics
