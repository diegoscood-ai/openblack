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
// each joint widened by its scale along normalize(cross(view direction, segment direction)), the texture frame across
// it and NumTexturesForWholeChain repeats along it (fn_006C8920). Report: tmp_dis\psys\part_render.md §10;
// PSys/Creators/Chain.h.
//
// The ribbons go through the back-to-front list with everything else: the original draws them inside their effect's
// single Z object (fn_006798B0 0x6798DD takes fn_0067B370, "draw now", because PSysManager::AddDrawing 0x6797DE clears
// the +0xAE flag that [0xC0215D] carries), so CollectPSysChains keys them with the effect's origin and Renderer.cpp
// draws them from the sorted loop (hole H1 of tmp_dis\unify2\lh3d_zsorter_openblack.md).

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
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

std::vector<std::pair<float, uint32_t>> Renderer::CollectPSysChains(const Camera& camera) const
{
	_frameChains = psys::chain_atoms::Collect();
	std::vector<std::pair<float, uint32_t>> order;
	order.reserve(_frameChains.size());
	const auto eye = camera.GetOrigin();
	for (size_t i = 0; i < _frameChains.size(); ++i)
	{
		// fn_006798B0 0x6798D6: with [0xC0215D] = 0 (what PSysManager::AddDrawing 0x6797DE leaves when the manager is
		// queued) the ribbon takes the fn_0067B370 branch and is drawn inside the effect's own Z object, so its key is the
		// effect's: |origin - g_camera|. Only the direct Draw_(float, bool) path (fn_00679840 with the flag set) gives a
		// chain its own Z object through fn_0067B380, with the central joint as the point
		order.emplace_back(glm::distance(_frameChains[i].origin, eye), static_cast<uint32_t>(i));
	}
	return order;
}

void Renderer::DrawPSysChain(RenderPass viewId, const Camera& camera, uint32_t index) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	if (index >= _frameChains.size())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	const glm::vec3 eye = camera.GetOrigin();

	{
		const auto& chain = _frameChains[index];
		const auto* creator = dynamic_cast<const psys::ChainCreator*>(chain.creator);
		if (creator == nullptr)
		{
			return;
		}
		const auto texture = entt::hashed_string(("raw/" + creator->texture).c_str());
		const auto alphaTexture = entt::hashed_string(("raw/" + creator->texture + "a").c_str());
		if (!textures.Contains(texture))
		{
			return;
		}
		const auto segments = static_cast<int>(chain.joints.size()) - 1;
		// fn_0067B3F0: four vertices per segment, (head + side, head - side, tail + side, tail - side), each end widened
		// by its own side vector: normalize(cross(eye - joint, segment)) (0x67B89E..0x67B976, the sign of
		// billboard::RibbonSide) x the joint's PSR scale (0x67BA3B..0x67BA70: side x scale / |side|; a zero side stays
		// zero), so the half width is the scale itself (0x67BA82..0x67BB0D writes joint +- that to the strip
		// fn_0081C780)
		std::vector<glm::vec3> corners(static_cast<size_t>(segments) * 4);
		for (int i = 0; i < segments; ++i)
		{
			const auto& head = chain.joints[static_cast<size_t>(i)];
			const auto& tail = chain.joints[static_cast<size_t>(i) + 1];
			const auto along = tail.position - head.position;
			// (eye - joint) x segment, unnormalised as in 0x67B8D9..0x67B924, then side / |side| x scale; only an exactly
			// zero side is left as it is (0x67BA04..0x67BA39), so a zero-length segment (two joints at one point, the
			// gesture trail of a still hand) collapses to its joint instead of the NaN billboard::RibbonSide's
			// normalize(segment) would give
			const auto sideOf = [&eye, &along](const glm::vec3& joint, float scale) {
				const auto side = glm::cross(eye - joint, along);
				const float length = glm::length(side);
				// the half width (billboard::RibbonHalfWidth): side x joint +0xC / |side| (0x67B9E6..0x67BA70), the
				// joint's +0xC being the PSR scale (ChainJoint::DrawAt 0x679E9A), the same value a sprite takes as its
				// half size
				return length > 0.0f ? side * (billboard::RibbonHalfWidth(scale) / length) : glm::vec3(0.0f);
			};
			const auto sideHead = sideOf(head.position, head.scale);
			const auto sideTail = sideOf(tail.position, tail.scale);
			auto* c = &corners[static_cast<size_t>(i) * 4];
			c[0] = head.position + sideHead;
			c[1] = head.position - sideHead;
			c[2] = tail.position + sideTail;
			c[3] = tail.position - sideTail;
		}
		// 0x67BD2D..0x67BE82: where two segments meet, both pairs of vertices move to their midpoints, so the strip has
		// no gaps or steps at the joints ([0xD4EC14], 0 here, would copy the previous tail instead)
		for (int i = 1; i < segments; ++i)
		{
			auto* previous = &corners[static_cast<size_t>(i - 1) * 4];
			auto* c = &corners[static_cast<size_t>(i) * 4];
			c[0] = previous[2] = (previous[2] + c[0]) * 0.5f;
			c[1] = previous[3] = (previous[3] + c[1]) * 0.5f;
		}
		// Not ported (part_render.md §10): UseDynamicLighting (colour x clamp(0.6 + 0.4 n.L), 0x67BB10..0x67BCCF) and
		// the joint jitter of ChainJoint::DrawAt 0x679E80. The V scroll is the collection's chain +0x3C, advanced by
		// chain_atoms::AdvanceScroll (0x67BE88..0x67BED5, frame_anim::ChainScroll; its rate +0x4C is 0 but for
		// UR_SimpleBeam / UR_Plasma, not ported)
		const float scroll = chain.collection != nullptr ? chain.collection->chainScroll : 0.0f;
		const frame_anim::UvOffset offset(0.0f, scroll);
		std::vector<Vertex> vertices;
		for (int i = 0; i < segments; ++i)
		{
			const auto& head = chain.joints[static_cast<size_t>(i)];
			const auto& tail = chain.joints[static_cast<size_t>(i) + 1];
			const auto* c = &corners[static_cast<size_t>(i) * 4];
			// fn_006C8920 (ChainCreator::SegmentUv, 0x67BEE4..0x67BEFD): (u0, v0) (u1, v0) (u0, v1) (u1, v1) on the four
			// vertices in that order (uv0 on head + side): U across, V along the segment, the v-scroll on all four.
			// fn_0081C780 copies each vertex's UV as it is into LH3DP3::Table1 +0x18 / +0x1C (0x81C9C7..0x81C9D0) and
			// draws through DrawTriangle 0x82F810 (0x81CCB2). Before that, 0x67BFAC..0x67BFEE puts the same scroll in
			// [0xECA630] ([0xECA62C] = 0, [0xECA628] = 1) and DrawTriangle 0x82F8BE adds it once more: the material of
			// fn_006AA800 has only bits 0 and 2 of +5 (SetMaterialProperties 0x57E120, 0x6AA84A), so it is not fixed
			const auto segmentUv = creator->SegmentUv(i, segments, scroll);
			const std::array<glm::vec2, 4> uvs = {
			    frame_anim::OffsetUv(segmentUv[0], offset, false), frame_anim::OffsetUv(segmentUv[1], offset, false),
			    frame_anim::OffsetUv(segmentUv[2], offset, false), frame_anim::OffsetUv(segmentUv[3], offset, false)};
			const std::array<const psys::Effect::DrawAtom*, 4> ends = {&head, &head, &tail, &tail};
			// the index list 0x67B77A..0x67B7D8: (0, 1, 2) and (1, 3, 2) per segment
			for (const int k : {0, 1, 2, 1, 3, 2})
			{
				const auto& end = *ends[static_cast<size_t>(k)];
				const auto alpha = static_cast<uint32_t>(std::clamp(end.alpha, 0.0f, 255.0f));
				const uint32_t abgr = (alpha << 24) | (static_cast<uint32_t>(end.colour[2]) << 16) |
				                      (static_cast<uint32_t>(end.colour[1]) << 8) | end.colour[0];
				vertices.push_back({c[k].x, c[k].y, c[k].z, uvs[static_cast<size_t>(k)].x, uvs[static_cast<size_t>(k)].y,
				                    abgr});
			}
		}
		if (vertices.empty())
		{
			return;
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
