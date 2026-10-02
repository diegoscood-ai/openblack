/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// What FallingSpell (Magic/Objects/FallingSpell.h) draws over the film fall.bik in mode 2: the sparks (LH3DSprites with
// the smoke material [0xEA1ABC], drawn by the Z-sorter's flush fn_0082F280 at FinishFrame 0x82F480) and the light
// bursts (the finish frame callback 0x526480 -> LightBurst::Draw 0x525DF0, Draw3DWorldTriangle with
// LH3DAtmos::AdditiveMaterial [0xEDC364]). Both are screen anchored (Get3DPointFromScreen 0x81B370), so they are drawn
// here in screen pixels in the ScreenOverlay view, after the film and before the bars and the fade
// (Renderer::DrawFinishFrameOverlays).

#include <array>
#include <cstring>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "Camera/Camera.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Magic/Objects/FallingSpell.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
const auto k_Smoke = entt::hashed_string("raw/smoke");
const auto k_SmokeAlpha = entt::hashed_string("raw/smokea");
const auto k_Atmos = entt::hashed_string("raw/ATMOS");
const auto k_AtmosAlpha = entt::hashed_string("raw/ATMOSA");

struct Vertex
{
	float x, y, z, u, v;
	uint32_t abgr;
};
} // namespace

void Renderer::DrawFallingSpellOverlay() const
{
	const auto& spell = magic::falling_spell::Get();
	if (!spell.IsActive() || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const int width = _resolution.x;
	const int height = _resolution.y;
	// (inferido) the near plane [0xE839E0] of mode 2 is the one the last camera left: openblack's camera's
	const float nearZ =
	    Locator::camera::has_value() ? billboard::CameraFrame::From(Locator::camera::value()).nearZ : 1.0f;
	const auto toClipX = [width](float px) { return 2.0f * px / static_cast<float>(width) - 1.0f; };
	const auto toClipY = [height](float py) { return 1.0f - 2.0f * py / static_cast<float>(height); };
	const auto& textures = Locator::resources::value().GetTextures();
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto submit = [&](const std::vector<Vertex>& vertices, entt::hashed_string::hash_type texture,
	                        entt::hashed_string::hash_type alpha, const render_modes::Material& material) {
		const auto count = static_cast<uint32_t>(vertices.size());
		if (count == 0 || bgfx::getAvailTransientVertexBuffer(count, layout) < count || !textures.Contains(texture) ||
		    !textures.Contains(alpha))
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
		const auto* program = _shaderManager->GetShader("WorldQuad");
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(alpha));
		bgfx::setVertexBuffer(0, &buffer);
		// the Z: FinishFrame's Z reset quad (0x82F460 (d), z = 1 everywhere) of the frame before, and nothing with Z
		// drawn in mode 2 before these (the film is ZFUNC ALWAYS without Z), so LESSEQUAL always passes: ALWAYS here;
		// neither mode writes Z (6 and 13, the "Nz" variants)
		bgfx::setState(render_modes::State(material, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
		bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
	};

	// the sparks, far to near, each one Z object (LH3DSprite::AddDrawing 0x840C70); one buffer keeps that order:
	// their two triangles {0, 1, 2}, {0, 2, 3} (0x840B47 / 0x840B57) one after the other
	std::vector<Vertex> sparks;
	for (const auto& quad : spell.SparkQuads(width, height, nearZ))
	{
		for (const int i : billboard::k_SpriteTriangles)
		{
			const auto& v = quad.at(static_cast<size_t>(i));
			sparks.push_back({toClipX(v.pixel.x), toClipY(v.pixel.y), 0.5f, v.uv.x, v.uv.y, lh3d_colour::ToAbgr(v.argb)});
		}
	}
	// the smoke material [0xEA1ABC] (smoke.raw, mode 6, two-sided; LH3DSprite flag 0x80 sets it, 0x840C01..0x840C38)
	submit(sparks, k_Smoke, k_SmokeAlpha, render_modes::materials::k_Smoke);

	// the four bursts in their order, Draw3DWorldTriangle(0x80, ..., 0x40 triangles, ...) each
	std::vector<Vertex> bursts;
	const auto indices = magic::falling_spell::BurstIndices();
	for (const auto& fan : spell.Bursts())
	{
		for (const int i : indices)
		{
			const auto& v = fan.vertices.at(static_cast<size_t>(i));
			bursts.push_back({toClipX(v.pixel.x), toClipY(v.pixel.y), 0.5f, v.uv.x, v.uv.y, lh3d_colour::ToAbgr(v.argb)});
		}
	}
	// LH3DAtmos::AdditiveMaterial [0xEDC364]: atmos.raw, mode 13 (SRCALPHA / ONE), two-sided (0x835C61)
	submit(bursts, k_Atmos, k_AtmosAlpha, render_modes::materials::k_AtmosAdditive);
}
