/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The map's mist banks (CREATE_MIST -> Mist, an LH3DObject of type 7 drawn by LH3DMist::Draw fn_007FA300)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <LNDFile.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "3D/SkyInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Game.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// fn_00801C90: the land light under a point, table[cell luminosity] of the 4 cells around it, bilinear (like the
/// models' base colour in vs_object); the cells off the map have the full light. The luminosities are capped by the
/// cloud shadows when that cap matches the map.
glm::vec3 LandLightAt(const LandIslandInterface& island, const LandLightTable& table, const std::vector<uint8_t>& cap,
                      glm::u16vec2 capSize, glm::vec2 point)
{
	const auto extent = island.GetExtent();
	const auto size = glm::ivec2(island.GetCellMap().GetResolution());
	const glm::vec2 cellPosition = (point - extent.minimum) * 0.1f;
	const glm::ivec2 first(glm::floor(cellPosition));
	const glm::vec2 w = cellPosition - glm::vec2(first);
	const auto lightOf = [&](glm::ivec2 cell) {
		if (cell.x < 0 || cell.y < 0 || cell.x >= size.x || cell.y >= size.y)
		{
			return table.GetColour(255);
		}
		auto luminosity = island.GetCell(glm::u16vec2(cell)).luminosity;
		if (glm::ivec2(capSize) == size && cap.size() == static_cast<size_t>(size.x) * size.y)
		{
			luminosity = std::min(luminosity, cap[static_cast<size_t>(cell.y) * size.x + cell.x]);
		}
		return table.GetColour(luminosity);
	};
	const auto c00 = lightOf(first);
	const auto c10 = lightOf(first + glm::ivec2(1, 0));
	const auto c01 = lightOf(first + glm::ivec2(0, 1));
	const auto c11 = lightOf(first + glm::ivec2(1, 1));
	return glm::mix(glm::mix(c00, c01, w.y), glm::mix(c10, c11, w.y), w.x);
}
} // namespace

void Renderer::DrawMists(graphics::RenderPass viewId, const Camera& camera) const
{
	auto& registry = Locator::entitiesRegistry::value();
	// game time (g_game_time_inc): the animation stops while the game is paused
	static auto lastTime = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	const float speed = Game::Instance() != nullptr ? Game::Instance()->GetGameSpeed() : 1.0f;
	const bool paused = Game::Instance() == nullptr || Game::Instance()->IsPaused();
	const float milliseconds =
	    paused ? 0.0f : std::min(100.0f, std::chrono::duration<float, std::milli>(now - lastTime).count() / speed);
	lastTime = now;

	const auto origin = camera.GetOrigin();
	std::vector<std::pair<float, entt::entity>> order;
	registry.Each<ecs::components::Mist, const ecs::components::Transform>(
	    [&](entt::entity entity, ecs::components::Mist& mist, const ecs::components::Transform& transform) {
		    // fn_007FA300: counter += int(time_inc * 0.255), modulo 900
		    mist.counterRemainder += milliseconds * 0.255f;
		    const int step = static_cast<int>(mist.counterRemainder);
		    mist.counterRemainder -= static_cast<float>(step);
		    mist.counter = (mist.counter + step) % 900;
		    order.emplace_back(glm::distance(transform.position, origin), entity);
	    });
	if (order.empty())
	{
		return;
	}
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	const auto& textures = Locator::resources::value().GetTextures();
	static const auto k_Smoke = entt::hashed_string("raw/smoke");
	static const auto k_SmokeAlpha = entt::hashed_string("raw/smokea");
	if (mesh.GetNumSubMeshes() == 0 || !textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	const auto& smoke = *textures.Handle(k_Smoke);
	const auto& smokeAlpha = *textures.Handle(k_SmokeAlpha);
	const auto* program = _shaderManager->GetShader("Cloud");
	std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

	// the rotation of every LH3DMist is the camera-facing matrix 0xEA1C98 (UpdateWorldToCamera 0x819690): the dome's
	// axis (local +Y) towards the camera, local x = screen right, local z = screen up. The view's third basis vector
	// points away from the camera, so it is negated (a mist on the land would otherwise sink into it)
	const auto cameraBasis = glm::mat3(glm::inverse(camera.GetViewMatrix(Camera::Interpolation::Current)));
	const auto rotation = glm::mat3(cameraBasis[0], -cameraBasis[2], cameraBasis[1]);
	const bool landLight = _landLight && _landLight->IsLoaded() && Locator::terrainSystem::has_value();
	for (const auto& [distance, entity] : order)
	{
		const auto& [mist, transform] = registry.Get<const ecs::components::Mist, const ecs::components::Transform>(entity);
		const auto alpha = static_cast<float>(mist.colour >> 24u);
		if (alpha <= 0.0f)
		{
			continue;
		}
		glm::vec3 rgb(static_cast<float>((mist.colour >> 16u) & 0xFFu), static_cast<float>((mist.colour >> 8u) & 0xFFu),
		              static_cast<float>(mist.colour & 0xFFu));
		float scale = mist.size;
		if (mist.edgeShrink)
		{
			// effect branch 0x7FA3B1: full size seen from below or above, size / k edge-on; lit from above, ambient 210
			const auto toMist = transform.position - origin;
			const float length = std::max(glm::length(toMist), 1.0f);
			scale = mist.size / (1.0f + (mist.k - 1.0f) * (1.0f - std::abs(toMist.y) / length));
		}
		else if (landLight)
		{
			// 0x7FA6A4: the colour times the land light under it, byte by byte (c l / 255), then the models' light
			const auto light = glm::floor(LandLightAt(Locator::terrainSystem::value(), *_landLight, _cloudShadowCap,
			                                          _cloudShadowSize, glm::vec2(transform.position.x, transform.position.z)) *
			                                  255.0f +
			                              0.5f);
			rgb = glm::floor(rgb * light / 255.0f);
		}
		const int frame = (mist.counter / 20) & 15;
		const glm::vec4 u_cloud(static_cast<float>(frame & 7) / 8.0f, static_cast<float>(frame >> 3) / 8.0f + 0.25f,
		                        (mist.edgeShrink ? 210.0f : 90.0f) / 256.0f, mist.edgeShrink ? 0.0f : 1.0f);
		const glm::vec4 u_cloudColour(rgb / 255.0f, alpha / 255.0f);
		const auto model = glm::translate(transform.position) * glm::mat4(rotation) * glm::scale(glm::vec3(scale));
		for (const auto& subMesh : mesh.GetSubMeshes())
		{
			for (const auto& prim : subMesh->GetPrimitives())
			{
				bgfx::setTransform(&model);
				program->SetTextureSampler("s_diffuse", 0, smoke);
				program->SetTextureSampler("s_alpha", 1, smokeAlpha);
				program->SetUniformValue("u_cloud", &u_cloud);
				program->SetUniformValue("u_cloudColour", &u_cloudColour);
				if (subMesh->GetMesh().IsIndexed())
				{
					subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
				}
				subMesh->GetMesh().GetVertexBuffer().Bind();
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
				bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
			}
		}
	}
}
