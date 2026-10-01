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
// The surface is one atom of its effect, so the original draws it inside the effect's single Z object
// (PSysManager::AddDrawing 0x6797D0 -> fn_00679860 -> fn_006798B0 -> fn_00679920 -> vt+0xFC 0x67CBA0) and not after the
// sorted list: CollectPSysSurfaces keys each one with the effect's origin and Renderer.cpp draws it from the sorted loop
// (hole H2 of tmp_dis\unify2\lh3d_zsorter_openblack.md). Without that, the one-shot orb's bubble - mode 12, additive and
// writing Z (0x82ECA6), queued nearer than the effect by OneOffSpellSeed::Draw 0x518E90 - hid the dispenser's disc.

#include <cstring>

#include <memory>
#include <span>
#include <string>
#include <utility>
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

std::vector<std::pair<float, uint32_t>> Renderer::CollectPSysSurfaces(const Camera& camera) const
{
	_frameSurfaces = psys::surf_revol::Collect();
	std::vector<std::pair<float, uint32_t>> order;
	order.reserve(_frameSurfaces.size());
	const auto eye = camera.GetOrigin();
	for (size_t i = 0; i < _frameSurfaces.size(); ++i)
	{
		// no Z object of its own: the key is the effect's (PSysManager::AddDrawing 0x6797D0, |origin - g_camera|)
		order.emplace_back(glm::distance(_frameSurfaces[i].origin, eye), static_cast<uint32_t>(i));
	}
	return order;
}

void Renderer::DrawPSysSurface(RenderPass viewId, uint32_t index) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	if (index >= _frameSurfaces.size())
	{
		return;
	}
	const auto surfaces = std::span(&_frameSurfaces[index], 1);
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
			// Not ported: no cull state, so Surface::doubleSided (MaterialSetDoubleSided) is ignored and every surface draws
			// two-sided
			const auto mode =
			    pass == 1 ? render_modes::Mode::AlphaTexturedAlphaAdditiveNz
			              : render_modes::ModeFromProperties(render_modes::Mode::AlphaTexturedAlphaNz,
			                                                 {.additive = surface.additive,
			                                                  .zWrite = surface.writeDepth,
			                                                  .alpha = true});
			bgfx::setState(render_modes::State(mode));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}
