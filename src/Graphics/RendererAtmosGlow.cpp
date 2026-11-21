/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The atmosphere's screen glow (Graphics/AtmosGlow.h): its quad, added over the frame before the screen fade, and its
// step from the finished frame. Wiki: docs/wiki/world/weather.md#the-screen-glow.

#include <cstdint>
#include <cstring>

#include <array>
#include <optional>
#include <string>
#include <utility>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "Common/ProcessLogOnce.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/AtmosGlow.h"
#include "Graphics/AtmosGlowTargets.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/ScreenshotCapture.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Renderer.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{

/// A quad over a whole target, in clip space, with its texture coordinates
struct TargetVertex
{
	float x;
	float y;
	float u;
	float v;
	uint32_t colour;
};

/// Draws a quad over the whole of a view's target with a program. Whether it was submitted
bool SubmitTargetQuad(bgfx::ViewId viewId, const ShaderProgram& program)
{
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	constexpr uint32_t k_White = 0xFFFFFFFFu;
	const std::array<TargetVertex, 6> quad {{{-1.0f, 1.0f, 0.0f, 0.0f, k_White},
	                                         {1.0f, 1.0f, 1.0f, 0.0f, k_White},
	                                         {1.0f, -1.0f, 1.0f, 1.0f, k_White},
	                                         {-1.0f, 1.0f, 0.0f, 0.0f, k_White},
	                                         {1.0f, -1.0f, 1.0f, 1.0f, k_White},
	                                         {-1.0f, -1.0f, 0.0f, 1.0f, k_White}}};
	const auto count = static_cast<uint32_t>(quad.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return false;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, quad.data(), sizeof(quad));
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	program.Submit(viewId);
	return true;
}

/// A view drawing into one of the glow's 16 x 16 targets
void SetUpCellView(RenderPass pass, FrameBufferHandle frameBuffer)
{
	const auto viewId = static_cast<bgfx::ViewId>(pass);
	constexpr auto k_Size = static_cast<uint16_t>(atmos_glow::k_Cells);
	bgfx::setViewFrameBuffer(viewId, toBgfx(frameBuffer));
	bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);
	bgfx::setViewRect(viewId, 0, 0, k_Size, k_Size);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
}

} // namespace

void Renderer::DrawAtmosGlow() const
{
	if (!_atmosGlowUse.draws || !_atmosGlowTargets->GlowWritten() || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	// One quad over the screen, pre-transformed from (0, 0) to (w, h) with the original's pixel centres, its texture
	// from the first cell's middle to the last one's, linearly magnified
	const auto topLeft = OverlayToClip(0.0f, 0.0f);
	const auto bottomRight = OverlayToClip(static_cast<float>(_resolution.x), static_cast<float>(_resolution.y));
	constexpr float k_UvMin = 0.5f / atmos_glow::k_Cells;
	constexpr float k_UvMax = (atmos_glow::k_Cells - 0.5f) / atmos_glow::k_Cells;
	const uint32_t abgr = argb_colour::ToAbgr(atmos_glow::QuadColour(_atmosDarkness));
	const float l = topLeft.x;
	const float r = bottomRight.x;
	const float t = topLeft.y;
	const float b = bottomRight.y;
	const SpiritQuadVertex a {l, t, 0.5f, k_UvMin, k_UvMin, abgr};
	const SpiritQuadVertex bq {r, t, 0.5f, k_UvMax, k_UvMin, abgr};
	const SpiritQuadVertex c {r, b, 0.5f, k_UvMax, k_UvMax, abgr};
	const SpiritQuadVertex d {l, b, 0.5f, k_UvMin, k_UvMax, abgr};
	const std::array quad {a, bq, c, a, c, d};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(quad.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, quad.data(), sizeof(quad));
	const auto viewId = static_cast<bgfx::ViewId>(RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	bgfx::setVertexBuffer(0, &buffer);
	// The texture times the colour, added (ONE, ONE), with no depth test or write and no alpha test
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ADD);
	const auto* program = _shaderManager->GetShader("Interface");
	program->SetTextureSampler("s_texture", 0, _atmosGlowTargets->Glow());
	program->Submit(viewId);
	_atmosGlowDrawn = true;
}

void Renderer::SubmitAtmosGlowStep(std::optional<TextureHandle> keptFrame)
{
	const auto use = std::exchange(_atmosGlowUse, {});
	_atmosGlowReadout.darkness = _atmosDarkness;
	_atmosGlowReadout.drawn = std::exchange(_atmosGlowDrawn, false);
	_atmosGlowReadout.stepped = false;
	_atmosGlowReadout.refused = nullptr;
	if (!use.steps)
	{
		_atmosGlowTargets->ReleaseFrame();
		return;
	}
	// The finished frame: the kept one's target, the screenshot's drawn aside, or else the glow's own, which the window
	// then shows pixel for pixel
	std::optional<std::pair<TextureHandle, glm::u16vec2>> frame;
	if (keptFrame.has_value())
	{
		frame = std::pair {*keptFrame, _shownTargets->Size()};
	}
	else
	{
		frame = _offscreenShots->RedirectedColour();
	}
	const char* refused = nullptr;
	if (!AtmosGlowTargets::Supported())
	{
		refused = "this renderer can't draw into BGRA8, D24S8 and RGBA32F targets";
	}
	else if (!frame.has_value() && (_bgfxReset & BGFX_RESET_MSAA_MASK) != 0)
	{
		refused = "the window is multisampled";
	}
	else if (!frame.has_value() && (_backBufferSize.x == 0 || _backBufferSize.y == 0))
	{
		refused = "the window has no size";
	}
	if (refused != nullptr)
	{
		_atmosGlowReadout.refused = refused;
		_atmosGlowTargets->ReleaseFrame();
		// (debug) why the glow is not stepped, once a reason
		if (log_once::First(std::string("atmos-glow-refused: ") + refused))
		{
			spdlog::get("graphics")->debug("Atmosphere glow: frame {} not stepped: {}", _framesDrawn, refused);
		}
		return;
	}
	if (frame.has_value())
	{
		_atmosGlowTargets->ReleaseFrame();
	}
	else
	{
		_atmosGlowTargets->MakeFrame(_backBufferSize);
		_atmosGlowTargets->Redirect();
		SubmitShownFrameCopy(_atmosGlowTargets->FrameColour());
		frame = std::pair {_atmosGlowTargets->FrameColour(), _backBufferSize};
	}
	_atmosGlowTargets->MakeCells();

	// The step: each cell from the cells of the frame before and its pixel of this frame
	const auto* step = _shaderManager->GetShader("AtmosGlowStep");
	SetUpCellView(RenderPass::AtmosGlowStep, _atmosGlowTargets->NextCells());
	const glm::vec4 grid {static_cast<float>(frame->second.x), static_cast<float>(frame->second.y),
	                      bgfx::getCaps()->originBottomLeft ? 1.0f : 0.0f, _atmosGlowTargets->CellsAtZero() ? 1.0f : 0.0f};
	step->SetTextureSampler("s_cells", 0, _atmosGlowTargets->LastCells());
	step->SetTextureSampler("s_frame", 1, frame->first);
	step->SetUniformValue("u_glowGrid", &grid);
	if (!SubmitTargetQuad(static_cast<bgfx::ViewId>(RenderPass::AtmosGlowStep), *step))
	{
		return;
	}
	// The glow's texture from the cells just stepped
	const auto* pack = _shaderManager->GetShader("AtmosGlowPack");
	SetUpCellView(RenderPass::AtmosGlowPack, _atmosGlowTargets->GlowFrameBuffer());
	pack->SetTextureSampler("s_cells", 0, _atmosGlowTargets->NextCellsColour());
	if (!SubmitTargetQuad(static_cast<bgfx::ViewId>(RenderPass::AtmosGlowPack), *pack))
	{
		return;
	}
	_atmosGlowTargets->Stepped();
	_atmosGlowReadout.stepped = true;
	++_atmosGlowReadout.steps;
}
