/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The rain streaks: the tiles of the rain system, each its streaks as lines with the atmosphere material (render mode
// 6, data\textures\atmos.raw). Each tile is one Z object of the frame's single transparency queue (Graphics/ZSort.h),
// drawn in its place among the blended models, sprites, mists and smoke

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_Atmos = entt::hashed_string("raw/ATMOS").value();
constexpr entt::id_type k_AtmosAlpha = entt::hashed_string("raw/ATMOSA").value();

/// OPENBLACK_WEATHER_TRACE's frame count for the rain, in the debug hooks' store (Locator::debugHooks)
struct RendererRainDebugHooksState
{
	uint32_t traceFrame {0};
};

RendererRainDebugHooksState& RendererRainDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("renderer rain: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<RendererRainDebugHooksState>();
}
} // namespace

std::vector<std::pair<float, uint32_t>> Renderer::CollectRain(const Camera& camera) const
{
	// once a frame for the main view (PreDraw): taking tiles lets the next update step the streaks, so they are taken
	// only when they can be drawn
	std::vector<std::pair<float, uint32_t>> order;
	auto& rainSystem = Locator::rainSystem::value();
	// OPENBLACK_WEATHER_TRACE: one line a second with what the rain draws
	static const bool k_Trace = debug_env::WeatherTrace();
	const auto& textures = Locator::resources::value().GetTextures();
	const bool textured = textures.Contains(k_Atmos) && textures.Contains(k_AtmosAlpha);
	_frameRain = textured ? rainSystem.TakeTiles(camera.GetOrigin()) : std::vector<rain::Tile> {};
	if (k_Trace && ++RendererRainDebugHooksData().traceFrame % 60 == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Rain: {} tiles, textures {}, elevation {:.1f}, first tile alpha {}",
		                   _frameRain.size(), textured, rainSystem.GetHeight(),
		                   _frameRain.empty() ? -1 : _frameRain.front().alpha);
	}
	if (_frameRain.empty())
	{
		return order;
	}
	order.reserve(_frameRain.size());
	const auto eye = camera.GetOrigin();
	for (size_t i = 0; i < _frameRain.size(); ++i)
	{
		// the key is |(x, land altitude at (x, z), z) - camera|^2 at the tile's point, the block's map position + 80,
		// summed (x^2 + z^2) + y^2. (openblack) the tile travels by its index in _frameRain, not in the user data K
		// (zsort::PackRainUser): TakeTiles has already faded the alpha with the distance (the rain's own fade), so
		// a K made from it would not be the original's. (approximate) the height is the land's height at
		// the tile's centre, not the altitude of the map cell the original reads
		const auto& tile = _frameRain[i];
		const glm::vec3 origin(tile.centre.x, tile.ground, tile.centre.y);
		order.emplace_back(zsort::Key(origin, eye, zsort::SumOrder::XZY), static_cast<uint32_t>(i));
	}
	return order;
}

void Renderer::DrawRainTile(RenderPass viewId, uint32_t index) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	if (index >= _frameRain.size())
	{
		return;
	}
	const auto& tile = _frameRain[index];
	const auto& textures = Locator::resources::value().GetTextures();

	// each streak from (x, -50, z) to (x + dx, elevation, z + dz) around the tile's origin, u from its scroll to
	// scroll + 1 along row 129 of the texture (v = 129 / 256); the colours are white with the tile's alphas: `alpha` at
	// the bottom, `alphaTop` at the top, x the streak's phase fade
	const auto& rainSystem = Locator::rainSystem::value();
	const auto streaks = rainSystem.GetStreaks();
	const float elevation = rainSystem.GetHeight();
	constexpr float k_V = rain::k_TextureRow;
	const glm::vec3 origin(tile.centre.x, tile.ground, tile.centre.y);
	std::vector<Vertex> vertices;
	for (int32_t i = 0; i < tile.streaks; ++i)
	{
		const auto& drop = streaks[static_cast<size_t>(i)];
		const float fade = rain::PhaseFade(drop.phase);
		const auto bottomAlpha =
		    static_cast<uint32_t>(fade < 1.0f ? static_cast<int32_t>(static_cast<float>(tile.alpha) * fade) : tile.alpha) &
		    0xFF;
		const auto topAlpha = static_cast<uint32_t>(fade < 1.0f ? static_cast<int32_t>(static_cast<float>(tile.alphaTop) * fade)
		                                                        : tile.alphaTop) &
		                      0xFF;
		const auto ends = rain::Ends(drop, elevation);
		const glm::vec3 bottom = origin + ends[0];
		const glm::vec3 top = origin + ends[1];
		vertices.push_back({bottom.x, bottom.y, bottom.z, drop.scroll, k_V, (bottomAlpha << 24) | 0xFFFFFFu});
		vertices.push_back({top.x, top.y, top.z, drop.scroll + 1.0f, k_V, (topAlpha << 24) | 0xFFFFFFu});
	}

	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (count == 0 || bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Atmos));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AtmosAlpha));
	bgfx::setVertexBuffer(0, &buffer);
	// the atmosphere material: mode 6 (alpha blend), Z test, no Z write (two-sided, tiling); one-pixel lines as the
	// original's
	bgfx::setState(render_modes::State(render_modes::materials::k_Atmos, {.extra = BGFX_STATE_PT_LINES}));
	program->Submit(static_cast<bgfx::ViewId>(viewId));
}
