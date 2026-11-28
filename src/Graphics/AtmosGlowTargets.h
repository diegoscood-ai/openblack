/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>

#include <glm/vec2.hpp>

#include "GraphicsHandle.h"

namespace openblack::graphics
{

/// The targets of the atmosphere's screen glow (Graphics/AtmosGlow.h): the frame the window would show, drawn aside so
/// it can be read when nothing else draws it aside; the cells' values, 16 x 16 in floats, in two targets in turn (a
/// frame's step reads one and writes the other); and the glow's 16 x 16 texture, its 4-bit channels widened to 8, which
/// the next frame adds over itself.
class AtmosGlowTargets
{
public:
	AtmosGlowTargets() = default;
	AtmosGlowTargets(const AtmosGlowTargets&) = delete;
	AtmosGlowTargets& operator=(const AtmosGlowTargets&) = delete;
	~AtmosGlowTargets();

	/// Whether this renderer can draw into the targets' formats: 8-bit colour with a 24-bit depth and stencil, and
	/// 32-bit float colour
	[[nodiscard]] static bool Supported();

	/// Makes the cells' targets and the glow's texture unless made, the cells at 0 and the texture not yet written
	void MakeCells();
	/// The cells' target the next step writes, its colour, and the one it reads, which the first step takes as all 0
	/// (CellsAtZero) whatever it holds
	[[nodiscard]] FrameBufferHandle NextCells() const { return _cellFrameBuffers.at(_next); }
	[[nodiscard]] TextureHandle NextCellsColour() const { return _cells.at(_next); }
	[[nodiscard]] TextureHandle LastCells() const { return _cells.at(1 - _next); }
	/// Whether no step has written the cells since they were made or reset
	[[nodiscard]] bool CellsAtZero() const { return !_stepped; }
	[[nodiscard]] FrameBufferHandle GlowFrameBuffer() const { return _glowFrameBuffer; }
	[[nodiscard]] TextureHandle Glow() const { return _glow; }
	/// Whether a step has written the glow's texture
	[[nodiscard]] bool GlowWritten() const { return _stepped; }
	/// Call once a step is submitted: the targets change places
	void Stepped();
	/// The cells back to 0 and the texture not written, as at the start
	void ResetCells() { _stepped = false; }

	/// Makes the frame's target at a size unless it is of it
	void MakeFrame(glm::u16vec2 size);
	/// Lets the frame's target go
	void ReleaseFrame();
	[[nodiscard]] TextureHandle FrameColour() const { return _frameColour; }
	/// Draws the frame being made's screen passes (ShownFrameTargets::k_Passes) into the frame's target
	void Redirect();
	/// Call when bgfx has the frame: it gives the passes back to the window
	void FrameEnded();

private:
	void ReleaseCells();

	bool _cellsMade {false};
	std::array<TextureHandle, 2> _cells {};
	std::array<FrameBufferHandle, 2> _cellFrameBuffers {};
	TextureHandle _glow {};
	FrameBufferHandle _glowFrameBuffer {};
	size_t _next {0};
	bool _stepped {false};

	bool _frameMade {false};
	glm::u16vec2 _frameSize {0, 0};
	TextureHandle _frameColour {};
	TextureHandle _frameDepth {};
	FrameBufferHandle _frameBuffer {};
	bool _redirected {false};
};

} // namespace openblack::graphics
