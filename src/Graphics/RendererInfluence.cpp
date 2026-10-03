/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The influence border and the hand's ripples on it (ECS/Influence/InfluenceCircles.cpp keeps both lists).
// InfluenceCircle::Draw(1) 0x826C90, once a frame from GGame::Process3dEngine (0x54E3CB..0x54E419) in the world view:
// every circle of every player as a curtain 40 high in burn.raw / burna.raw, its middle row lit in the owner's colour,
// scrolled, in the material [0xEB9A18] (render mode 6, two-sided, tiled), through Draw3DWorldTriangle 0x81C090 at once
// (no Z object), after everything else drawn at once and before FinishFrame's Z-sorter drain (original-frame.md rows
// 23 / 24a). The ripples: one Z object each (fn_008274A0), whose callback 0x827500 draws its 7 smoke.raw sprites with
// LH3DSprite::DrawSpecial1 0x840CC0 in the plane of the border. Research: dev\_scratch\coordinador\spec_influence_circle.md.

#include <cstdint>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "ECS/Influence/Influence.h"
#include "GameClock.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"
#include "Graphics/ZSorter.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// [0xEA1A9C]: .\data\Textures\burn.raw [0xC37F90], fn_008379E0(path, 0x41, -1, 0) (0x80BD2F..0x80BD3F): ARGB4444 with
/// the alpha of burna.raw (argb4444::k_AlphaFlagStems); the loader keeps the two files as two textures
const auto k_Burn = entt::hashed_string("raw/burn");
const auto k_BurnAlpha = entt::hashed_string("raw/burna");
/// [0xEA1A98] smoke.raw and its alpha, the texture of the smoke material [0xEA1ABC] the ripples use
const auto k_Smoke = entt::hashed_string("raw/smoke");
const auto k_SmokeAlpha = entt::hashed_string("raw/smokea");
/// fn_00827250: every ripple sprite's +0x28 |= 0x3F, cell 63
constexpr int k_RippleCell = 63;
} // namespace

void Renderer::DrawInfluenceCircles(RenderPass viewId, const Camera& camera) const
{
	// Draw(1): the camera gate and the middle row's alpha from g_camera.y [0xEA1DBC] (0x826CA9..0x826CF3)
	const auto alpha = influence::CurtainAlpha(camera.GetOrigin().y);
	if (!alpha.has_value())
	{
		return;
	}
	// 0x826D0E..0x826D2A: the material [0xEB9A18] is made on the first draw, render_modes::materials::k_InfluenceCircle.
	// 0x826D2D..0x826D83: the scroll clock, [0xECA628] = 1 and the offset; it advances with or without circles
	const auto offset = frame_anim::InfluenceScroll(_influenceScrollMs, game_clock::FrameGameMs());
	// 0x826D8F..0x826E11: [0xEA9EA0] = a copy of g_world_to_clipping, the plain world space (vs_blob's u_viewProj)
	// 0x826F0D..0x826F57: the middle rows' alpha, for the players whose border is shown
	influence::SetCurtainAlpha(*alpha);
	const auto circles = influence::Circles();
	if (circles.empty() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Burn) || !textures.Contains(k_BurnAlpha))
	{
		return;
	}
	const auto& diffuse = *textures.Handle(k_Burn);
	const auto& alphaTexture = *textures.Handle(k_BurnAlpha);
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
	for (const auto& circle : circles)
	{
		// Draw3DWorldTriangle(3N + 3, positions, colours, uvs, 4N, indices, [0xEB9A18], 1) (0x826F59..0x826F80): every
		// circle is drawn, the invisible ones (alpha 0, border not shown yet) too. No fog, no haze, no specular
		// (0x81C2BF); the culling of the material, none (+5 bit 0); the UV offset added by DrawTriangle
		// (0x82F8BE..0x82F8F7, the material's bit 0x10 is clear)
		const auto& curtain = circle.curtain;
		vertices.clear();
		indices.clear();
		for (size_t k = 0; k < curtain.positions.size(); ++k)
		{
			vertices.push_back({curtain.positions[k], frame_anim::OffsetUv(curtain.uvs[k], offset, false),
			                    world_triangles::ToAbgr(curtain.colours[k])});
		}
		for (const auto index : curtain.indices)
		{
			indices.push_back(static_cast<uint16_t>(index)); // at most 3 x 250 + 2
		}
		world_triangles::SubmitRaw(viewId, vertices, indices, diffuse, alphaTexture,
		                           render_modes::materials::k_InfluenceCircle, *_shaderManager);
	}
	// 0x826F8B: [0xECA628] = 0 (the offset is only this draw's)
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectInfluenceRipples(const Camera& camera) const
{
	std::vector<std::pair<float, uint32_t>> order;
	const auto ripples = influence::Ripples();
	const auto eye = camera.GetOrigin();
	order.reserve(ripples.size());
	for (size_t i = 0; i < ripples.size(); ++i)
	{
		// fn_008274A0: NewZObject(0x827500, |ripple +0..+8 - g_camera|^2 summed (x^2 + y^2) + z^2)
		order.emplace_back(zsorter::Key(ripples[i].point, eye, zsorter::SumOrder::XYZ), static_cast<uint32_t>(i));
	}
	return order;
}

void Renderer::DrawInfluenceRipple(RenderPass viewId, uint32_t index) const
{
	const auto ripples = influence::Ripples();
	if (index >= ripples.size() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	// fn_00827500: the sprites grow and fade (influence::DrawRipple); M = the ripple's matrix x world_to_clip, here the
	// world matrix (vs_blob's u_viewProj does the rest)
	const auto matrix = ripples[index].matrix;
	const auto sprites = influence::DrawRipple(index, game_clock::FrameGameMs());
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
	vertices.reserve(sprites.size() * 4);
	indices.reserve(sprites.size() * billboard::k_SpriteTriangles.size());
	for (const auto& drawn : sprites)
	{
		// LH3DSprite::DrawSpecial1 0x840CC0: the quad in the matrix's XZ plane turned by the sprite's angle
		billboard::Sprite sprite;
		sprite.size = drawn.size;
		sprite.angle = drawn.angle;
		sprite.argb = drawn.argb;
		sprite.cell = frame_anim::SpriteCell(k_RippleCell);
		const auto quad = billboard::PlaneOfMatrix(sprite, matrix);
		const auto first = static_cast<uint16_t>(vertices.size());
		for (size_t c = 0; c < quad.corners.size(); ++c)
		{
			vertices.push_back({quad.corners.at(c), quad.uv.at(c), world_triangles::ToAbgr(sprite.argb)});
		}
		for (const int corner : billboard::k_SpriteTriangles)
		{
			indices.push_back(static_cast<uint16_t>(first + corner));
		}
	}
	// the smoke material [0xEA1ABC] (fn_00827250): mode 6, two-sided
	world_triangles::SubmitRaw(viewId, vertices, indices, *textures.Handle(k_Smoke), *textures.Handle(k_SmokeAlpha),
	                           render_modes::materials::k_Smoke, *_shaderManager);
}
