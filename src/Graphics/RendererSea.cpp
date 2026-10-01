/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The sea (fn_00879930 / fn_0087A090), its reflection target and the hand's glow on the water (fn_005E3F70)

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/HandWaterGlow.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "3D/OceanInterface.h"
#include "3D/SkyInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "EngineConfig.h"
#include "Game.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/Mesh.h"
#include "Graphics/SeaRows.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::UpdateReflectionTarget() const
{
	// The original draws what is under the sea (the sky, the mirrored land, the moon's reflection...) straight into the
	// frame, so the reflection target has the size of the main view
	if (_resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	auto& ocean = Locator::oceanSystem::value();
	ocean.ResizeReflectionFramebuffer(_resolution.x, _resolution.y);
	uint16_t width = 0;
	uint16_t height = 0;
	ocean.GetReflectionFramebuffer().GetSize(width, height);
	ConfigureView(RenderPass::Reflection, {width, height}, 0x274659ff);
}

void Renderer::DrawSea(const DrawSceneDesc& desc) const
{
	const auto& ocean = Locator::oceanSystem::value();
	const auto* waterShader = _shaderManager->GetShader("Water");
	const auto& config = Locator::config::value();
	const auto& detail = GetDetailLevel(config.detailLevel);
	const bool running = Game::Instance() != nullptr && !Game::Instance()->IsPaused();
	// g_game_time_inc: the game milliseconds of this frame, 0 while paused
	const float milliseconds = running ? static_cast<float>(desc.time) : 0.0f;
	// fn_00879930: P = 2000 - 1800 * WaterTiling (0xC38228), 560 at the default detail level 4; the terrain-x2 mod
	// repeats the sea texture too (a shorter period)
	const bool level0 = detail.waterTiling == 0.0f;
	const float period = (level0 ? sea::k_Level0Period : detail.SeaPeriod()) / config.terrainTextureDensity;
	static sea::Drift drift;
	static uint32_t frame = 0; // [0xFA938C]
	// 0x879963 (before the level-0 test, with P = 2000 there)
	drift.ScrollRows(milliseconds, sea::k_AmbientWind, detail.SeaPeriod() / config.terrainTextureDensity);

	glm::vec4 u_seaMode {0.0f};
	glm::vec4 u_seaRows {0.0f};
	glm::vec2 ripple {0.0f};
	const Mesh* mesh = &ocean.GetScreenMesh();
	if (level0)
	{
		// fn_0087A090: the world quad of +-70000, its own drift
		drift.ScrollLevel0(milliseconds, sea::k_AmbientWind);
		mesh = &ocean.GetMesh();
		u_seaMode = {1.0f, 0.0f, drift.GetLevel0Offset()};
	}
	else
	{
		// [0xEA1DD4] / [0xEA1DDC]: the camera forward's x and z, scaled to 0.9 unless both are 0
		const auto forward = desc.camera->GetForward();
		const glm::vec2 forwardXZ(forward.x, forward.z);
		if (forwardXZ != glm::vec2(0.0f))
		{
			ripple = forwardXZ * (0.9f / glm::length(forwardXZ));
		}
		const auto viewProjection =
		    desc.camera->GetProjectionMatrix() * desc.camera->GetViewMatrix(Camera::Interpolation::Current);
		const auto range = sea::ComputeScreenRange(viewProjection, glm::vec2(_resolution), config.cameraNearClip);
		if (range)
		{
			drift.ScrollRows(milliseconds, sea::k_AmbientWind, period); // 0x879A69: a second time
			if (running)
			{
				frame = (frame + 1) & 15;
			}
			const auto rows = sea::MakeRows(*range);
			u_seaRows = {static_cast<float>(rows.first), static_cast<float>(rows.count), rows.inverseDepth, rows.inverseStep};
			u_seaMode = {0.0f, rows.softTop ? 1.0f : 0.0f, drift.GetRowsOffset()};
		}
		else
		{
			// no rows on screen: only what is under the sea shows below the horizon
			u_seaRows = {static_cast<float>(_resolution.y), 0.0f, 0.0f, 0.0f};
		}
	}

	// OPENBLACK_SEA_TRACE=1: the rows and the frame counter every 500 drawn frames
	static const bool k_Trace = std::getenv("OPENBLACK_SEA_TRACE") != nullptr;
	static uint32_t traceCount = 0;
	if (k_Trace && (traceCount++ % 500) == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
		                   "Sea trace: level0 {} first row {} rows {} 1/z {} step {} soft top {} frame {} offset ({}, {}) P {}",
		                   level0, u_seaRows.x, u_seaRows.y, u_seaRows.z, u_seaRows.w, u_seaMode.y, frame, u_seaMode.z,
		                   u_seaMode.w, period);
	}

	mesh->GetIndexBuffer().Bind(mesh->GetIndexBuffer().GetCount(), 0);
	mesh->GetVertexBuffer().Bind();
	// ZFUNC ALWAYS, no Z write, no culling (the main view is sequential, so the land drawn next covers it)
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
	auto diffuse = Locator::resources::value().GetTextures().Handle(ocean.GetDiffuseTexture());
	auto alpha = Locator::resources::value().GetTextures().Handle(ocean.GetAlphaTexture());
	waterShader->SetTextureSampler("s_diffuse", 0, *diffuse);
	waterShader->SetTextureSampler("s_alpha", 1, *alpha);
	waterShader->SetTextureSampler("s_reflection", 2, ocean.GetReflectionFramebuffer().GetColorAttachment());
	const glm::vec4 u_seaParams = {period, static_cast<float>(frame), ripple};
	waterShader->SetUniformValue("u_seaParams", &u_seaParams); // fs
	waterShader->SetUniformValue("u_seaRows", &u_seaRows);     // fs
	waterShader->SetUniformValue("u_seaMode", &u_seaMode);     // vs, fs
	const glm::vec4 u_seaCamera = {desc.camera->GetOrigin(), 0.0f};
	waterShader->SetUniformValue("u_seaCamera", &u_seaCamera); // fs
	// Living water mod: real time at a quarter speed (calm waves), also while paused; the shader time wraps at 1000 (every
	// scroll speed in fs_water repeats the texture a whole number of times in that period, so the loop is seamless)
	constexpr float k_WaveSpeed = 0.25f;
	static const auto k_Start = std::chrono::steady_clock::now();
	const float seconds =
	    std::fmod(std::chrono::duration<float>(std::chrono::steady_clock::now() - k_Start).count() * k_WaveSpeed, 1000.0f);
	const glm::vec4 u_waterMod = {config.livingWater ? 1.0f : 0.0f, seconds, config.terrainTextureDensity, 0.0f};
	waterShader->SetUniformValue("u_waterMod", &u_waterMod); // fs
	// The sea vertex colour, landscape light table entry 255 ([0xEDDD08]); without palette.raw (no table) openblack
	// leaves the sea unlit (white, inferido: the original always has the table)
	const glm::vec4 u_seaColour =
	    _landLight && _landLight->IsLoaded() ? glm::vec4(_landLight->GetColour(255), 1.0f) : glm::vec4(1.0f);
	waterShader->SetUniformValue("u_seaColour", &u_seaColour); // fs
	bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(waterShader->GetRawHandle()));
}

void Renderer::DrawHandWaterGlow(RenderPass viewId) const
{
	if (!_landLight || !_landLight->IsLoaded() || !Locator::handSystem::has_value() ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto hands = Locator::handSystem::value().GetPlayerHands();
	const auto& registry = Locator::entitiesRegistry::value();
	if (hands.empty() || !registry.Valid(hands[0]))
	{
		return;
	}
	const auto& position = registry.Get<const ecs::components::Transform>(hands[0]).position;
	const auto glow = hand_water_glow::Compute(Locator::terrainSystem::value(), _landLight->GetBaseColour(),
	                                           _landLight->GetRow6Colour(), position);
	if (!glow)
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Atmos = entt::hashed_string("raw/ATMOS");
	static const auto k_AtmosAlpha = entt::hashed_string("raw/ATMOSA");
	if (!textures.Contains(k_Atmos) || !textures.Contains(k_AtmosAlpha))
	{
		return;
	}
	struct Vertex
	{
		float x, y, z, u, v;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	if (bgfx::getAvailTransientVertexBuffer(6, layout) != 6)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, 6, layout);
	auto* vertices = reinterpret_cast<Vertex*>(buffer.data);
	// fn_005E3F70: (x - 60, z - 60), (x + 60, z - 60), (x + 60, z + 60), (x - 60, z + 60) on y = 0 with the UVs of
	// 0x92B2F8 in the same order, indices 0x92B2E0 = {0, 1, 2, 2, 3, 0}
	const float h = hand_water_glow::k_HalfSize;
	const auto uvMin = hand_water_glow::k_UvMinimum;
	const auto uvMax = hand_water_glow::k_UvMaximum;
	const std::array<Vertex, 4> corners = {{
	    {glow->centre.x - h, 0.0f, glow->centre.y - h, uvMin.x, uvMin.y},
	    {glow->centre.x + h, 0.0f, glow->centre.y - h, uvMax.x, uvMin.y},
	    {glow->centre.x + h, 0.0f, glow->centre.y + h, uvMax.x, uvMax.y},
	    {glow->centre.x - h, 0.0f, glow->centre.y + h, uvMin.x, uvMax.y},
	}};
	constexpr std::array<size_t, 6> k_Indices = {0, 1, 2, 2, 3, 0};
	for (size_t i = 0; i < k_Indices.size(); ++i)
	{
		vertices[i] = corners[k_Indices[i]];
	}
	const auto argb = glow->argb;
	const glm::vec4 colour = lh3d_colour::ToVec4(argb);
	const glm::vec4 celestial(0.0f, 0.0f, 0.0f, 1.0f);
	const glm::mat4 identity(1.0f);
	const auto* program = _shaderManager->GetShader("Celestial");
	bgfx::setTransform(&identity);
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Atmos));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AtmosAlpha));
	program->SetUniformValue("u_colour", &colour);
	program->SetUniformValue("u_celestial", &celestial);
	bgfx::setVertexBuffer(0, &buffer);
	// LH3DAtmos::AdditiveMaterial (0xEDC364, mode 13: SRCALPHA / ONE), ZFUNC ALWAYS (0x5E4281), no Z write, no culling
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}
