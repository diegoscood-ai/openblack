/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The surfaces of revolution of the particle effects (ZR_SurfRevol: the teleport pool, the dispensers' discs):
// RenderParticleGJMeshRotatingUV::DrawAt 0x67CBA0 sets the scrolled UV offset and draws the GJ mesh
// (RenderParticleGJMesh::DrawAt 0x67C150) in render mode 6: colour = texture x diffuse + specular, alpha = texture alpha x
// diffuse alpha. Here two passes with the WorldQuad program: the textured one blended, then the specular added with a
// white texture and the same alpha (alpha x specular is what the specular adds under that blend). Unlit (UseLighting is
// not ported). PSys/Rules/SurfRevol.h.
//
// The draw DrawAt 0x67C150 makes for a surface is fn_0081C780 (0x67CAEE), not Draw3DWorldTriangle 0x81C090 (0x67C9F2):
// the branch at 0x67C9CD takes it when the primitive has as many speculars (+0x30, count +0x38) as colours, and
// ZR_SurfRevol sizes both to NumU x NumV (0x685A0E..0x685A3D). fn_0081C780 is Draw3DWorldTriangle with a specular per
// vertex (colour / specular pairs, 0x81C9B9..0x81C9C4; Draw3DWorldTriangle writes 0, 0x81C2BF), always through
// g_world_to_clipping (0x81C783), the material's culling (+5 bit 0, 0x81CA0C..0x81CA1C; CULLMODE 0x81CC5E..0x81CC6E)
// and SetMaterial through the current table (0x81CB8F..0x81CBA1). So the surfaces stay here and do not go through
// graphics::world_triangles (WorldTriangles.h): that one has no specular and takes an L3D primitive's material, while a
// surface has its CreateMaterial(6) with the .raw texture and its alpha file.
//
// RenderParticleGJMeshRotatingUV::DrawAt 0x67CBA0 never reads [0xC0215D] (the manager's +0xAE): a surface has no Z
// object of its own on any path. A Sorted effect's (Draw_(t, 1): the teleport pool, MagicTeleport::Draw 0x5FCDC5; the
// dispensers' discs, SpellDispenser::Draw 0x722A13) is drawn at once when the effect is drawn, in the main view after
// the models (Spell::DrawSpells 0x7203F0 from 0x54E023, before the drain); a Queued one inside its effect's single Z
// object, at its place in the effect's items (PSysManager::AddDrawing 0x6797D0 -> fn_00679860 -> fn_006798B0 ->
// fn_00679920 -> vt+0xFC); an Immediate one inside the hand's. Renderer.cpp picks the place; this draws one surface.

#include <cstring>

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "Camera/Camera.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "PSys/Rules/SurfRevol.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// A 1 x 1 white texture for the specular pass (made once; bgfx owns it until the end)
const Texture2D& White()
{
	static Texture2D* white = [] {
		auto* texture = new Texture2D("surfrevol_white"); // NOLINT(cppcoreguidelines-owning-memory): lives as long as bgfx
		static constexpr uint8_t k_Pixel[4] = {255, 255, 255, 255};
		texture->Create(1, 1, 1, TextureFormat::RGBA8, Wrapping::Repeat, Filter::Linear, bgfx::copy(k_Pixel, sizeof(k_Pixel)));
		return texture;
	}();
	return *white;
}
} // namespace

void Renderer::DrawPSysSurface(RenderPass viewId, const psys::surf_revol::Surface& drawn) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto surfaces = std::span(&drawn, 1);
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	for (const auto& surface : surfaces)
	{
		const auto texture = entt::hashed_string(("raw/" + surface.texture).c_str());
		if (!textures.Contains(texture))
		{
			continue;
		}
		// the alpha file is <name>a.raw (S_TileLandscapeA.raw has a capital A)
		auto alphaTexture = entt::hashed_string(("raw/" + surface.texture + "a").c_str()).value();
		if (!textures.Contains(alphaTexture))
		{
			alphaTexture = entt::hashed_string(("raw/" + surface.texture + "A").c_str()).value();
		}
		const auto& alpha = textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture);
		const auto vertexCount = static_cast<uint32_t>(surface.vertices.size());
		const auto indexCount = static_cast<uint32_t>(surface.indices.size());
		for (int pass = 0; pass < 2; ++pass)
		{
			if (bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
			    bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
			{
				return;
			}
			bgfx::TransientVertexBuffer vertices;
			bgfx::TransientIndexBuffer indices;
			bgfx::allocTransientVertexBuffer(&vertices, vertexCount, layout);
			bgfx::allocTransientIndexBuffer(&indices, indexCount);
			auto* out = reinterpret_cast<Vertex*>(vertices.data);
			for (const auto& v : surface.vertices)
			{
				*out++ = {v.position.x, v.position.y, v.position.z, v.uv.x, v.uv.y, pass == 0 ? v.abgr : v.specular};
			}
			std::memcpy(indices.data, surface.indices.data(), surface.indices.size() * sizeof(uint16_t));
			if (pass == 0)
			{
				program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture), 0);
			}
			else
			{
				program->SetTextureSampler("s_diffuse", 0, White(), 0);
			}
			program->SetTextureSampler("s_alpha", 1, alpha, 0);
			bgfx::setVertexBuffer(0, &vertices);
			bgfx::setIndexBuffer(&indices);
			// CreateMaterial(6) + SetMaterialProperties (ZR_SurfRevol::ModifyAtomCollection 0x6863EC / 0x6863F9): 13 / 6,
			// 12 / 5 with MaterialUpdateZBuffer, Z test on; the specular goes on top additively (mode 13, (inferido)).
			// The table 0xC387C8 DrawAt puts with a DrawData alpha != 0xFF (0x67C9B7..0x67C9C0) leaves 5, 6, 12 and 13 as
			// they are (render_modes::k_GlobalAlphaModes), so the mode is the material's
			const auto mode =
			    pass == 1 ? render_modes::Mode::AlphaTexturedAlphaAdditiveNz
			              : render_modes::ModeFromProperties(render_modes::Mode::AlphaTexturedAlphaNz,
			                                                 {.additive = surface.additive,
			                                                  .zWrite = surface.writeDepth,
			                                                  .alpha = true});
			// CULLMODE ((~material +5) & 1) * 2 + 1 (fn_0081C780 0x81CC5E..0x81CC6E): +5 bit 0 is MaterialSetDoubleSided
			// (SetMaterialProperties 0x57E1B7..0x57E1C9), so a one-sided surface (the teleport pool's SF_TeleportVortex)
			// culls D3DCULL_CCW as the models do (render_modes::CullFor; the vertices are in the world, no mirror)
			bgfx::setState(render_modes::State(mode, {.cull = render_modes::CullFor(surface.doubleSided, false)}));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}
