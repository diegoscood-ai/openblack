/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The falling snow: the tiles of the snowfall system, each its flakes as small squares of the atmosphere texture
// (data\textures\atmos.raw), with the atmosphere material. Each tile is one Z object of the frame's single
// transparency queue (Graphics/ZSort.h), drawn in its place among the blended models, sprites, mists, smoke and rain

#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/Snowfall.h"
#include "Camera/Camera.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
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
} // namespace

std::vector<std::pair<float, uint32_t>> Renderer::CollectSnow(const Camera& camera) const
{
	// once a frame for the main view (PreDraw), after the rain: taking the tiles lets the next update move the flakes
	std::vector<std::pair<float, uint32_t>> order;
	_frameSnow.clear();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!Locator::snowfallSystem::has_value() || !textures.Contains(k_Atmos) || !textures.Contains(k_AtmosAlpha))
	{
		return order;
	}
	_frameSnow = Locator::snowfallSystem::value().TakeTiles(camera.GetOrigin());
	order.reserve(_frameSnow.size());
	const auto eye = camera.GetOrigin();
	for (size_t i = 0; i < _frameSnow.size(); ++i)
	{
		// keyed as the rain's tiles are, at the ground under the tile's corner
		const glm::vec3 corner {_frameSnow[i].corner.x, _frameSnow[i].ground, _frameSnow[i].corner.y};
		order.emplace_back(zsort::Key(corner, eye, zsort::SumOrder::XZY), static_cast<uint32_t>(i));
	}
	return order;
}

void Renderer::DrawSnowTile(RenderPass viewId, uint32_t index) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	if (index >= _frameSnow.size() || !Locator::snowfallSystem::has_value())
	{
		return;
	}
	const auto& tile = _frameSnow[index];
	const auto flakes = Locator::snowfallSystem::value().GetFlakes();
	const auto count = static_cast<uint32_t>(std::min<size_t>(static_cast<size_t>(tile.flakes), flakes.size()));
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	if (count == 0 || bgfx::getAvailTransientVertexBuffer(count * 4, layout) < count * 4 ||
	    bgfx::getAvailTransientIndexBuffer(count * 6) < count * 6)
	{
		return;
	}
	// The flakes take the colour of the land's light, 0xAARRGGBB, with the tile's alpha
	const auto landColour = land_light::CurrentTable().GetLandColour();
	const auto bgr = ((landColour & 0xFFu) << 16u) | (landColour & 0xFF00u) | ((landColour >> 16u) & 0xFFu);
	const auto colour = (static_cast<uint32_t>(tile.alpha & 0xFF) << 24u) | bgr;
	bgfx::TransientVertexBuffer vertexBuffer;
	bgfx::TransientIndexBuffer indexBuffer;
	bgfx::allocTransientVertexBuffer(&vertexBuffer, count * 4, layout);
	bgfx::allocTransientIndexBuffer(&indexBuffer, count * 6);
	const auto vertices = std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), count * 4);
	const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), count * 6);
	constexpr std::array<uint16_t, 6> k_Triangles = {0, 1, 2, 2, 3, 0};
	const glm::vec3 corner {tile.corner.x, tile.ground, tile.corner.y};
	for (size_t i = 0; i < count; ++i)
	{
		const auto corners = snowfall::Corners(flakes[i]);
		for (size_t c = 0; c < corners.size(); ++c)
		{
			const auto position = corner + corners.at(c);
			const auto uv = snowfall::k_Uvs.at(c);
			vertices[(i * 4) + c] = {position.x, position.y, position.z, uv.x, uv.y, colour};
		}
		for (size_t t = 0; t < k_Triangles.size(); ++t)
		{
			indices[(i * 6) + t] = static_cast<uint16_t>((i * 4) + k_Triangles.at(t));
		}
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Atmos));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AtmosAlpha));
	// the atmosphere material has no alpha test
	const glm::vec4 u_alphaTest {-1.0f, 0.0f, 0.0f, 0.0f};
	program->SetUniformValue("u_alphaTest", &u_alphaTest);
	bgfx::setVertexBuffer(0, &vertexBuffer);
	bgfx::setIndexBuffer(&indexBuffer);
	// the atmosphere material: mode 6 (alpha blend), Z test, no Z write, both sides of each flake
	bgfx::setState(render_modes::State(render_modes::materials::k_Atmos));
	program->Submit(static_cast<bgfx::ViewId>(viewId));
}
