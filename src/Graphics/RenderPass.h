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
	Reflection,
	Main,
	/// Blended (fading) models, drawn over the finished main pass so the water cannot be sorted over them.
	MainBlended,
	/// Screen-space like ScreenOverlay, before FinishFrame3D: the first InputPromptIcon update (at the end of the help system's
	/// draw): the input prompt icons under the end of frame's overlays
	FinishFrameIcons,
	/// The end of frame's queue in mode 2 (the falling spell's film): the falling creature's hand glows and the film's
	/// sparks, far to near, after the input prompt icons' first draw and over the depth the copy of the creature left,
	/// with the fall's camera (Graphics/RendererFallingSpell.cpp)
	FinishFrameQueue,
	/// The 3D of the end of frame's "after" callbacks, over the Z reset quad (its depth cleared): the advisor spirits'
	/// overlay (priority 100; Graphics/RendererSpirits.cpp)
	FinishFrame3D,
	/// Screen-space quads at the end of the frame: cinema bars and the screen fade
	ScreenOverlay,
	/// The game's interface: the in-game menus (Gui/Canvas, the original's 800 x 600 space)
	Interface,
	/// (openblack engine) while the frames are kept for a challenge's picture, the passes above draw into a target aside,
	/// which this pass copies to the window pixel for pixel, before the debug windows (Graphics/ScreenshotCapture.h)
	ShownFrameCopy,
	ImGui,
	/// The game's pointer, over the debug windows too
	Cursor,
	MeshViewer,
	/// (openblack engine) the last view of the backbuffer: its alpha set to 1 over the whole screen (alpha writes only),
	/// so a window composited with its alpha (Vulkan on some drivers) is opaque. Renderer::Frame
	OpaqueAlpha,
	/// The land seen from above, which the map in the temple's pool is textured with, into its own target once a visit
	/// to the temple. After every other view, so that none of theirs moves; the map shows from the next frame.
	TempleMap,

	_count
};

static constexpr std::array<std::string_view, static_cast<uint8_t>(RenderPass::_count)> k_RenderPassNames {
    "Reflection Pass",         //
    "Main Pass",               //
    "Main Blended Pass",       //
    "Finish Frame Icons Pass", //
    "Finish Frame Queue Pass", //
    "Finish Frame 3D Pass",    //
    "Screen Overlay Pass",     //
    "Interface Pass",          //
    "Shown Frame Copy Pass",   //
    "ImGui Pass",              //
    "Cursor Pass",             //
    "Mesh Viewer Pass",        //
    "Opaque Alpha Pass",       //
    "Temple Map Pass",         //
};

} // namespace openblack::graphics
