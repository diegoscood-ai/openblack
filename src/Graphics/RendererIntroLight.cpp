/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The intro light (PLAY_JC_SPECIAL 0, ecs/IntroSpecial.h): one Z object of the frame's transparency queue (fn_00828300,
// key |head - g_camera|^2), whose callback 0x8283D0 draws its LH3DSprites one after the other with LH3DSprite::Draw
// 0x840530 (mode A: in the plane of the screen, nothing at or before the near plane) in the material [0xD19C8C],
// CreateMaterial(13, misc0.raw): SRCALPHA / ONE, no Z write, one-sided; ZFUNC ALWAYS around them when the light asks.
// Everything comes from the frame's graphics::OverlayFrame (filled before DrawScene): nothing here reads the game.

#include <cstdint>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawIntroLight(RenderPass viewId, const Camera& camera, const IntroLightOverlay& light) const
{
	static const auto k_Texture = entt::hashed_string("raw/misc0");
	static const auto k_Alpha = entt::hashed_string("raw/misc0a");
	if (light.sprites.empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return;
	}
	// each sprite in its order, four corners and LH3DSprite::Draw's triangles (0,1,2),(0,2,3) (0x840B47 / 0x840B57):
	// one indexed draw keeps the primitives' order (the state is the same for all)
	const auto frame = billboard::CameraFrame::From(camera);
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
	vertices.reserve(light.sprites.size() * 4);
	indices.reserve(light.sprites.size() * billboard::k_SpriteTriangles.size());
	for (const auto& source : light.sprites)
	{
		auto sprite = source;
		sprite.cell = frame_anim::SpriteCell(source.cell); // +0x28 & 0x3F of the 8 x 8 sheet (+0x30 = 8)
		const auto quad = billboard::SpriteQuad(sprite, frame);
		if (!quad.has_value())
		{
			continue; // 0x84055D..0x840585
		}
		const uint32_t abgr = lh3d_colour::ToAbgr(sprite.argb);
		const auto first = static_cast<uint16_t>(vertices.size()); // at most 20 x 4
		for (size_t corner = 0; corner < quad->corners.size(); ++corner)
		{
			vertices.push_back({quad->corners.at(corner), quad->uv.at(corner), abgr});
		}
		for (const int corner : billboard::k_SpriteTriangles)
		{
			indices.push_back(static_cast<uint16_t>(first + corner));
		}
	}
	if (vertices.empty())
	{
		return;
	}
	// mode 13 (0x82ECD0): SRCALPHA / ONE, ZWRITE 0; the cull of the material's +5 = 0 (LH3DSprite::Draw 0x840C08..
	// 0x840C27: CULLMODE 3; the screen quad is front facing, so it shows); ZFUNC 8 while depthAlways (0x828483 /
	// 0x828673), else 4. The shared upload of Draw3DWorldTriangle's .raw materials (WorldQuad, s_diffuse / s_alpha)
	render_modes::StateOptions options;
	options.zFunc = light.depthAlways ? render_modes::ZFunc::Always : render_modes::ZFunc::LessEqual;
	world_triangles::SubmitRaw(viewId, vertices, indices, *textures.Handle(k_Texture), *textures.Handle(k_Alpha),
	                           render_modes::materials::k_Misc0Additive, *_shaderManager, options);
}
