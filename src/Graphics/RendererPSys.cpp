/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The particle effects' sprites (PSys, fn_00679920 -> Particle3DSprite::DrawAt 0x67AE80 -> LH3DSprite::Draw 0x840530)

#include <cmath>
#include <cstring>

#include <array>
#include <string>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "Camera/Camera.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawPSysEffect(const psys::manager::Drawable& effect, const Camera& camera, RenderPass viewId) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	const glm::vec3 right = camera.GetRight();
	const glm::vec3 up = camera.GetUp();

	// atoms in list order (one Z-sorter object per effect, AddDrawing); consecutive atoms with the same material batched
	size_t i = 0;
	const auto& atoms = effect.atoms;
	while (i < atoms.size())
	{
		const auto* creator = atoms[i].creator;
		const auto texture = entt::hashed_string(("raw/" + creator->texture).c_str());
		const auto alphaTexture = entt::hashed_string(("raw/" + creator->texture + "a").c_str());
		std::vector<Vertex> vertices;
		size_t j = i;
		for (; j < atoms.size(); ++j)
		{
			const auto& atom = atoms[j];
			const auto* c = atom.creator;
			if (c->texture != creator->texture || c->additive != creator->additive || c->writeDepth != creator->writeDepth)
			{
				break;
			}
			// cell = (FileOffset + frame) & 63; looped frames wrap within NumFrames
			const float frames = static_cast<float>(c->numFrames);
			float frame = atom.frame;
			frame = c->loopAnim ? std::fmod(std::fmod(frame, frames) + frames, frames) : std::clamp(frame, 0.0f, frames - 1.0f);
			const int cell = (c->fileOffset + static_cast<int>(frame)) & 63;
			const float n = static_cast<float>(c->spritesPerRow);
			const glm::vec2 uv0(static_cast<float>(cell % c->spritesPerRow) / n, static_cast<float>(cell / c->spritesPerRow) / n);
			const glm::vec2 uv1 = uv0 + glm::vec2(1.0f / n);
			const float size = std::max(atom.scale, 1e-4f);
			const float height = size * atom.stretch;
			const float alpha = std::clamp(atom.alpha * static_cast<float>(c->scaleAlpha) / 255.0f, 0.0f, 255.0f);
			const uint32_t abgr = (static_cast<uint32_t>(alpha) << 24) | (static_cast<uint32_t>(atom.colour[2]) << 16) |
			                      (static_cast<uint32_t>(atom.colour[1]) << 8) | atom.colour[0];
			glm::vec3 centre = atom.position;
			glm::vec3 across;
			glm::vec3 along;
			if (c->horizontal)
			{
				// flag 0x40: a flat XZ quad turned about Y
				across = glm::normalize(glm::vec3(atom.rotation[0].x, 0.0f, atom.rotation[0].z) + glm::vec3(1e-6f, 0.0f, 0.0f)) * size;
				along = glm::vec3(-across.z, 0.0f, across.x) * (height / size);
			}
			else
			{
				// a screen-aligned quad, rolled by atan2(M[0][2], M[0][0]) unless IgnoreRotation
				const float roll = c->ignoreRotation ? 0.0f : std::atan2(atom.rotation[0][2], atom.rotation[0][0]);
				const float cr = std::cos(roll);
				const float sr = std::sin(roll);
				across = (right * cr + up * sr) * size;
				along = (up * cr - right * sr) * height;
				if (c->centreAtBase)
				{
					centre += glm::vec3(0.0f, height, 0.0f);
				}
				centre += across * c->originX + along * c->originY;
			}
			const std::array<glm::vec3, 4> p = {centre - across + along, centre + across + along, centre + across - along,
			                                    centre - across - along};
			const std::array<glm::vec2, 4> uv = {glm::vec2(uv0.x, uv0.y), glm::vec2(uv1.x, uv0.y), glm::vec2(uv1.x, uv1.y),
			                                     glm::vec2(uv0.x, uv1.y)};
			for (const int k : {0, 1, 2, 0, 2, 3})
			{
				vertices.push_back({p[k].x, p[k].y, p[k].z, uv[k].x, uv[k].y, abgr});
			}
		}
		i = j;
		if (vertices.empty() || !textures.Contains(texture))
		{
			continue;
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		const auto count = static_cast<uint32_t>(vertices.size());
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
		program->SetTextureSampler("s_alpha", 1, textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture));
		bgfx::setVertexBuffer(0, &buffer);
		// modes 13 / 6 (12 / 5 with MaterialUpdateZBuffer), Z test on, two-sided
		const uint64_t blend = creator->additive ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE)
		                                         : BGFX_STATE_BLEND_ALPHA;
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | blend |
		               (creator->writeDepth ? BGFX_STATE_WRITE_Z : 0));
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
	}
}
