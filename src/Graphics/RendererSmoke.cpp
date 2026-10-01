/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The chimney smoke of the houses (LH3DSmoke, ecs/ChimneySmoke.h): one Z-sorter object per chimney, whose callback
// fn_007F8E00 advances and draws its 10 sprites in their own order

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/ChimneySmoke.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Primitive.h"
#include "Renderer.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr auto k_SmokeAlpha = entt::hashed_string("raw/smokea");

/// g_smoke_mat (fn_0080BBD0 0x80BC76): CreateMaterial(6, smoke.raw) with the alpha of smokea.raw. smoke.raw is pure
/// white in the cells the smoke uses (0-15), so the colour is the vertex colour alone and smokea.raw is enough
bool LoadAlpha()
{
	auto& textures = Locator::resources::value().GetTextures();
	if (textures.Contains(k_SmokeAlpha))
	{
		return true;
	}
	try
	{
		textures.Load(k_SmokeAlpha.value(), resources::Texture2DLoader::FromDiskTag {},
		              Locator::filesystem::value().FindPath(std::filesystem::path("Data") / "Textures" / "smokea.raw"));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Chimney smoke: cannot load Data/Textures/smokea.raw: {}", e.what());
		return false;
	}
	return textures.Contains(k_SmokeAlpha);
}

/// Whether a sphere touches the view volume of a view-projection matrix (the planes of its rows, Gribb-Hartmann), as in
/// RendererMists.cpp
bool SphereInView(const glm::mat4& viewProjection, const glm::vec3& centre, float radius)
{
	const glm::mat4 rows = glm::transpose(viewProjection);
	for (int plane = 0; plane < 6; ++plane)
	{
		const glm::vec4 p = rows[3] + (plane % 2 == 0 ? 1.0f : -1.0f) * rows[plane / 2];
		if (glm::dot(glm::vec3(p), centre) + p.w < -radius * glm::length(glm::vec3(p)))
		{
			return false;
		}
	}
	return true;
}
} // namespace

std::vector<std::pair<float, uint32_t>> Renderer::CollectChimneySmoke(const Camera& camera) const
{
	// g_game_time_inc in milliseconds: it stops while the game is paused (as CollectMists)
	static auto lastTime = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	const float speed = Game::Instance() != nullptr ? Game::Instance()->GetGameSpeed() : 1.0f;
	const bool paused = Game::Instance() == nullptr || Game::Instance()->IsPaused();
	const float milliseconds =
	    paused ? 0.0f : std::min(100.0f, std::chrono::duration<float, std::milli>(now - lastTime).count() / speed);
	lastTime = now;

	std::vector<std::pair<float, uint32_t>> order;
	_frameSmoke.clear();
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Size<ecs::components::ChimneySmoke>() == 0 || !LoadAlpha())
	{
		return order;
	}
	// GLandscape::Draw sets the hand's wind before the objects are drawn
	ecs::chimney_smoke::UpdateHandWind();

	const bool forced = ecs::chimney_smoke::ForcedByTestHook();
	const auto origin = camera.GetOrigin();
	const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Interpolation::Current);
	auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<ecs::components::ChimneySmoke, const ecs::components::Abode, const ecs::components::Mesh,
	              const ecs::components::Transform>([&](entt::entity, ecs::components::ChimneySmoke& smoke,
	                                                    const ecs::components::Abode& abode, const ecs::components::Mesh& mesh,
	                                                    const ecs::components::Transform& transform) {
		// Abode::Draw 0x516288: only when the building was on screen this frame (g_b_last_on_screen); off screen the
		// smoke does not advance. Stand-in for the object's screen test: its box's sphere against the view volume
		if (meshes.Contains(mesh.id))
		{
			const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
			const glm::vec3 centre = transform.position + transform.rotation * (box.Center() * transform.scale);
			const float scale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
			if (!SphereInView(viewProjection, centre, glm::length(box.Size()) * 0.5f * scale))
			{
				return;
			}
		}
		// PresentAtHome != 0, or IsWorkshop() && Workshop+0xC4 != 0 (the scaffold countdown, fn_007798A0 /
		// Workshop::Process 0x7797F0; openblack has no workshops making scaffolds yet)
		const bool lit = abode.presentAtHome != 0 || forced;
		if (!ecs::chimney_smoke::UpdateState(smoke, lit))
		{
			return;
		}
		// LH3DSmoke::AddDrawing 0x7F8D30: the Z-sorter key is |chimney - camera|^2; the distance sorts the same way.
		// The callback simulates while it draws, so it is advanced here, once per frame for the smoke drawn
		std::vector<ecs::chimney_smoke::DrawnPuff> drawn;
		ecs::chimney_smoke::Advance(smoke, milliseconds, drawn);
		order.emplace_back(glm::distance(smoke.position, origin), static_cast<uint32_t>(_frameSmoke.size()));
		_frameSmoke.push_back(std::move(drawn));
	});
	return order;
}

void Renderer::DrawChimneySmoke(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const
{
	if (index >= _frameSmoke.size() || _frameSmoke[index].empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	const auto& alpha = *textures.Handle(k_SmokeAlpha);
	const auto* program = _shaderManager->GetShader("Sprite");
	const auto frame = billboard::CameraFrame::From(camera);
	for (const auto& puff : _frameSmoke[index])
	{
		// LH3DSprite::Draw 0x840530, mode A (billboard::Screen): a square of half width = size in the plane of the
		// screen, turned by the angle (x_v = cos x + sin y, y_v = -sin x + cos y, i.e. by -angle), no origin offset;
		// on the GPU through vs_sprite (billboard::ScreenSpriteModel). Nothing when its depth is at or before the near
		// plane (0x840585)
		if (!billboard::InFrontOfNear(puff.position, frame))
		{
			continue;
		}
		const glm::mat4 model = billboard::ScreenSpriteModel(puff.position, glm::vec2(puff.halfWidth), puff.angle);
		// 8 cells per row (LH3DSprite +0x30): the cell's corners v0 (top left) and v2 (bottom right), 1/8 wide
		const auto uv = frame_anim::SpriteCellUv(static_cast<int>(puff.cell), 8);
		const glm::vec4 sampleRect(uv[2] - uv[0], uv[0]);
		// mode 6 (SRCALPHA / INVSRCALPHA, no light, no fog): the sprite shader's normal blend is ONE / INVSRCALPHA with
		// the tint premultiplied by its alpha
		const glm::vec4 colour = lh3d_colour::ToVec4(puff.argb);
		const glm::vec4 tint(glm::vec3(colour) * colour.a, colour.a);

		bgfx::setTransform(glm::value_ptr(model));
		program->SetUniformValue("u_sampleRect", glm::value_ptr(sampleRect));
		program->SetUniformValue("u_tint", glm::value_ptr(tint));
		program->SetTextureSampler("s_diffuse", 0, alpha);
		_plane->GetVertexBuffer().Bind();
		// depth test, no depth write (the material is two sided: no culling)
		bgfx::setState(0 | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
		               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA) |
		               BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD));
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
	}
}
