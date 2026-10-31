/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The chain ribbons of the particle effects (the lightning bolt and the storm's forks, the gesture trail, the creature
// beam): ParticleChainCreator's collection draw - a camera-facing strip through the collection's joints, each joint
// widened by its scale along normalize(cross(view direction, segment direction)), the texture frame across it and
// NumTexturesForWholeChain repeats along it. See Particles/Creators/Chain.h.
//
// Where a ribbon is drawn depends on its effect's draw path (particles::DrawPath): Sorted gives it its own Z object at the
// joint n / 2 (manager::SortedChain::key); Queued and Immediate draw it at once, at its place in its effect's items
// (manager::OrderedEffect). Renderer.cpp does both; the ribbon is drawn the same way either way.

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "Graphics/ChainRibbon.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Particles/Creators/Chain.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawParticleChain(RenderPass viewId, const Camera& camera, const particles::Effect::DrawChain& chain) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldTextured");
	const glm::vec3 eye = camera.GetOrigin();

	{
		const auto* creator = dynamic_cast<const particles::ChainCreator*>(chain.creator);
		if (creator == nullptr)
		{
			return;
		}
		const auto [texture, alphaTexture] = RawTextureIds(creator->texture);
		if (!textures.Contains(texture))
		{
			return;
		}
		const auto segments = static_cast<int>(chain.joints.size()) - 1;
		// the corners, four per segment (chain_ribbon::Corners), seen from the camera
		std::vector<chain_ribbon::Joint> joints;
		joints.reserve(chain.joints.size());
		for (const auto& joint : chain.joints)
		{
			joints.push_back({.position = joint.position, .scale = joint.scale});
		}
		const auto corners = chain_ribbon::Corners(joints, eye);
		// Not ported: UseDynamicLighting (colour x clamp(0.6 + 0.4 n.L)) and the joints' jitter. The V scroll is the
		// collection's chain scroll, advanced by chain_atoms::AdvanceScroll (frame_anim::ChainScroll; its rate is 0 but
		// for UR_SimpleBeam and UR_Plasma, the latter not ported)
		const float scroll = chain.collection != nullptr ? chain.collection->chainScroll : 0.0f;
		const frame_anim::UvOffset offset(0.0f, scroll);
		std::vector<Vertex> vertices;
		for (int i = 0; i < segments; ++i)
		{
			const auto& head = chain.joints[static_cast<size_t>(i)];
			const auto& tail = chain.joints[static_cast<size_t>(i) + 1];
			const auto* c = &corners[static_cast<size_t>(i) * 4];
			// ChainCreator::SegmentUv: (u0, v0) (u1, v0) (u0, v1) (u1, v1) on the four vertices in that order (uv0 on
			// head + side): U across, V along the segment, the v-scroll on all four. The original also puts the same
			// scroll in the texture offset of the draw, which adds it once more: the chain's material does not turn that
			// off, so the scroll is applied twice
			const auto segmentUv =
			    creator->SegmentUv(i, segments, scroll, chain.collection != nullptr ? chain.collection->chainTextures : -1);
			const std::array<glm::vec2, 4> uvs = {
			    frame_anim::OffsetUv(segmentUv[0], offset, false), frame_anim::OffsetUv(segmentUv[1], offset, false),
			    frame_anim::OffsetUv(segmentUv[2], offset, false), frame_anim::OffsetUv(segmentUv[3], offset, false)};
			const std::array<const particles::Effect::DrawAtom*, 4> ends = {&head, &head, &tail, &tail};
			// the index list: (0, 1, 2) and (1, 3, 2) per segment
			for (const int k : {0, 1, 2, 1, 3, 2})
			{
				const auto& end = *ends[static_cast<size_t>(k)];
				const auto alpha = static_cast<uint32_t>(std::clamp(end.alpha, 0.0f, 255.0f));
				const uint32_t abgr = (alpha << 24) | (static_cast<uint32_t>(end.colour[2]) << 16) |
				                      (static_cast<uint32_t>(end.colour[1]) << 8) | end.colour[0];
				vertices.push_back(
				    {c[k].x, c[k].y, c[k].z, uvs[static_cast<size_t>(k)].x, uvs[static_cast<size_t>(k)].y, abgr});
			}
		}
		if (vertices.empty())
		{
			return;
		}
		const auto& layout = ParticleQuadLayout();
		const auto count = static_cast<uint32_t>(vertices.size());
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
		program->SetTextureSampler(
		    "s_alpha", 1, textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture));
		bgfx::setVertexBuffer(0, &buffer);
		// the material (inferred): as the particles', modes 13 / 6 (12 / 5 with writeDepth), Z test on; no cull state,
		// so MaterialSetDoubleSided is ignored and every chain draws two-sided
		const auto mode =
		    render_modes::ModeFromProperties(render_modes::Mode::AlphaTexturedAlphaNoZWrite,
		                                     {.additive = creator->additive, .zWrite = creator->writeDepth, .alpha = true});
		bgfx::setState(render_modes::State(mode));
		program->Submit(static_cast<bgfx::ViewId>(viewId));
	}
}
