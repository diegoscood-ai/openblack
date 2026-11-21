/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AtmosGlowTargets.h"

#include <cstdint>

#include <bgfx/bgfx.h>

#include "AtmosGlow.h"
#include "GraphicsHandleBgfx.h"
#include "ScreenshotCapture.h"

namespace openblack::graphics
{

AtmosGlowTargets::~AtmosGlowTargets()
{
	ReleaseFrame();
	ReleaseCells();
}

bool AtmosGlowTargets::Supported()
{
	const auto* caps = bgfx::getCaps();
	constexpr uint16_t k_Target = BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER;
	return (caps->formats[bgfx::TextureFormat::BGRA8] & k_Target) != 0 &&
	       (caps->formats[bgfx::TextureFormat::D24S8] & k_Target) != 0 &&
	       (caps->formats[bgfx::TextureFormat::RGBA32F] & k_Target) != 0;
}

void AtmosGlowTargets::MakeCells()
{
	if (_cellsMade)
	{
		return;
	}
	constexpr auto k_Size = static_cast<uint16_t>(atmos_glow::k_Cells);
	// The cells are read at their own texels: no filtering
	constexpr uint64_t k_CellFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP;
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		const auto cells = bgfx::createTexture2D(k_Size, k_Size, false, 1, bgfx::TextureFormat::RGBA32F, k_CellFlags);
		bgfx::setName(cells, "Atmosphere Glow Cells");
		_cells.at(i) = fromBgfx(cells);
		_cellFrameBuffers.at(i) = fromBgfx(bgfx::createFrameBuffer(1, &cells, false));
	}
	// The glow is magnified linearly over the screen, from the first cell's middle to the last one's
	const auto glow =
	    bgfx::createTexture2D(k_Size, k_Size, false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | BGFX_SAMPLER_UVW_CLAMP);
	bgfx::setName(glow, "Atmosphere Glow");
	_glow = fromBgfx(glow);
	_glowFrameBuffer = fromBgfx(bgfx::createFrameBuffer(1, &glow, false));
	_next = 0;
	_stepped = false;
	_cellsMade = true;
}

void AtmosGlowTargets::Stepped()
{
	_next = 1 - _next;
	_stepped = true;
}

void AtmosGlowTargets::ReleaseCells()
{
	if (!_cellsMade)
	{
		return;
	}
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		bgfx::destroy(toBgfx(_cellFrameBuffers.at(i)));
		bgfx::destroy(toBgfx(_cells.at(i)));
	}
	bgfx::destroy(toBgfx(_glowFrameBuffer));
	bgfx::destroy(toBgfx(_glow));
	_cellsMade = false;
	_stepped = false;
}

void AtmosGlowTargets::MakeFrame(glm::u16vec2 size)
{
	if (_frameMade && _frameSize == size)
	{
		return;
	}
	ReleaseFrame();
	// Read at single texels by the step and copied to the window texel for texel: no filtering
	constexpr uint64_t k_ColourFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP;
	const auto colour = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::BGRA8, k_ColourFlags);
	const auto depth = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
	const std::array textures = {colour, depth};
	const auto frameBuffer = bgfx::createFrameBuffer(static_cast<uint8_t>(textures.size()), textures.data(), false);
	bgfx::setName(colour, "Atmosphere Glow Frame");
	bgfx::setName(frameBuffer, "Atmosphere Glow Frame");
	_frameColour = fromBgfx(colour);
	_frameDepth = fromBgfx(depth);
	_frameBuffer = fromBgfx(frameBuffer);
	_frameSize = size;
	_frameMade = true;
}

void AtmosGlowTargets::ReleaseFrame()
{
	if (!_frameMade)
	{
		return;
	}
	bgfx::destroy(toBgfx(_frameBuffer));
	bgfx::destroy(toBgfx(_frameColour));
	bgfx::destroy(toBgfx(_frameDepth));
	_frameMade = false;
	_frameSize = {0, 0};
}

void AtmosGlowTargets::Redirect()
{
	for (const auto pass : ShownFrameTargets::k_Passes)
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), toBgfx(_frameBuffer));
	}
	_redirected = true;
}

void AtmosGlowTargets::FrameEnded()
{
	if (!_redirected)
	{
		return;
	}
	for (const auto pass : ShownFrameTargets::k_Passes)
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), BGFX_INVALID_HANDLE);
	}
	_redirected = false;
}

} // namespace openblack::graphics
