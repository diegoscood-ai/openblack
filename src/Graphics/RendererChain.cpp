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
// beam): ParticleChainCreator's collection draw fn_0067B3F0 - a camera-facing strip through the collection's joints,
// each joint widened by its scale along normalize(cross(segment direction, view direction)), textured along the chain
// (NumTexturesForWholeChain). Report: tmp_dis\psys\part_render.md §10; PSys/Creators/Chain.h.

#include <cmath>
#include <cstring>

#include <string>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "PSys/Creators/Chain.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawPSysChains(RenderPass viewId, const Camera& camera) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto chains = psys::chain_atoms::Collect();
	if (chains.empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	const glm::vec3 eye = camera.GetOrigin();

	for (const auto& chain : chains)
	{
		const auto* creator = dynamic_cast<const psys::ChainCreator*>(chain.creator);
		if (creator == nullptr)
		{
			continue;
		}
		const auto texture = entt::hashed_string(("raw/" + creator->texture).c_str());
		const auto alphaTexture = entt::hashed_string(("raw/" + creator->texture + "a").c_str());
		if (!textures.Contains(texture))
		{
			continue;
		}
		const auto segments = static_cast<int>(chain.joints.size()) - 1;
		std::vector<Vertex> vertices;
		for (int i = 0; i < segments; ++i)
		{
			const auto& head = chain.joints[static_cast<size_t>(i)];
			const auto& tail = chain.joints[static_cast<size_t>(i) + 1];
			const auto along = tail.position - head.position;
			if (glm::length(along) < 1e-4f)
			{
				continue;
			}
			// the side vector of each end: the segment direction crossed with the direction to the camera
			// (fn_0067B3F0, billboard::RibbonSide).
			// Not ported (part_render.md §10): the midpoint smoothing of the joints, UseDynamicLighting (colour x
			// clamp(0.6 + 0.4 n.L)) and the joint jitter of ChainJoint::DrawAt 0x679E80
			const auto sideHead = billboard::RibbonSide(along, head.position, eye);
			const auto sideTail = billboard::RibbonSide(along, tail.position, eye);
			if (!sideHead.has_value() || !sideTail.has_value())
			{
				continue;
			}
			// the half width (billboard::RibbonHalfWidth): side x joint +0xC / |side| (0x67B9E6..0x67BA70), the joint's
			// +0xC being the PSR scale (ChainJoint::DrawAt 0x679E9A), the same value a sprite takes as its half size
			const auto offsetHead = *sideHead * billboard::RibbonHalfWidth(head.scale);
			const auto offsetTail = *sideTail * billboard::RibbonHalfWidth(tail.scale);
			// fn_006C8920 (ChainCreator::SegmentUv): V runs along the chain, U across it over one frame column, the
			// v-scroll on all four. fn_0067B3F0 makes four vertices a segment (0x67BA82..0x67BB0D): v0 = head + side,
			// v1 = head - side, v2 = tail + side, v3 = tail - side, and fills uv0..uv3 for them (0x67BEE4..0x67BEFD);
			// fn_0081C780 copies each vertex's UV as it is into LH3DP3::Table1 +0x18 / +0x1C (0x81C9C7..0x81C9D0) and
			// draws through DrawTriangle 0x82F810 (0x81CCB2). Before that, 0x67BFAC..0x67BFEE puts the same scroll in
			// [0xECA630] ([0xECA62C] = 0, [0xECA628] = 1) and DrawTriangle 0x82F8BE adds it once more: the material of
			// fn_006AA800 has only bits 0 and 2 of +5 (SetMaterialProperties 0x57E120, 0x6AA84A), so it is not fixed
			const float scroll = chain.collection != nullptr ? chain.collection->chainScroll : 0.0f;
			const auto segmentUv = creator->SegmentUv(i, segments, scroll);
			const frame_anim::UvOffset offset(0.0f, scroll);
			const std::array<glm::vec3, 4> p = {head.position - offsetHead, tail.position - offsetTail,
			                                   tail.position + offsetTail, head.position + offsetHead};
			const std::array<glm::vec2, 4> uv = {
			    frame_anim::OffsetUv(segmentUv[1], offset, false), frame_anim::OffsetUv(segmentUv[3], offset, false),
			    frame_anim::OffsetUv(segmentUv[2], offset, false), frame_anim::OffsetUv(segmentUv[0], offset, false)};
			const std::array<const psys::Effect::DrawAtom*, 4> ends = {&head, &tail, &tail, &head};
			for (const int k : {0, 1, 2, 0, 2, 3})
			{
				const auto& end = *ends[static_cast<size_t>(k)];
				const auto alpha = static_cast<uint32_t>(std::clamp(end.alpha, 0.0f, 255.0f));
				const uint32_t abgr = (alpha << 24) | (static_cast<uint32_t>(end.colour[2]) << 16) |
				                      (static_cast<uint32_t>(end.colour[1]) << 8) | end.colour[0];
				vertices.push_back({p[k].x, p[k].y, p[k].z, uv[k].x, uv[k].y, abgr});
			}
		}
		if (vertices.empty())
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
		program->SetTextureSampler("s_alpha", 1,
		                           textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture));
		bgfx::setVertexBuffer(0, &buffer);
		// material fn_006AA860 (not read, inferido): additive when the creator says so, Z test on, Z write only with
		// writeDepth; no cull state, so MaterialSetDoubleSided (+0x4E) is ignored and every chain draws two-sided
		const uint64_t blend = creator->additive ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE)
		                                         : BGFX_STATE_BLEND_ALPHA;
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | blend |
		               (creator->writeDepth ? BGFX_STATE_WRITE_Z : 0));
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
	}
}
