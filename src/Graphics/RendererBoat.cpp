/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The missionaries' boat (PetitNavire, ecs/PetitNavire.h): its reflection (PreDraw 0x5DFF20 -> DrawUnderWater in
// 0xFF303070) and the LH3DSprites of the boat's frame: the flat smoke of its wake (PostDraw 0x5E0785) and the
// SmokyStuff puffs (fn_00824140), all with the smoke material [0xEA1ABC] (smoke.raw / smokea.raw, render mode 6). Each
// sprite is one Z object of the frame's single transparency queue (LH3DSprite::AddDrawing 0x840C70; Graphics/ZSorter.h).

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/PetitNavire.h"
#include "ECS/Registry.h"
#include "ECS/SmokyStuff.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/Mesh.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ZSorter.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
const auto k_Texture = entt::hashed_string("raw/smoke");
const auto k_Alpha = entt::hashed_string("raw/smokea");
} // namespace

void Renderer::DrawBoatReflection(RenderPass viewId) const
{
	const auto entity = ecs::petit_navire::GetReflectedHull();
	if (entity == entt::null || !Locator::rendereringSystem::has_value())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto instance = renderCtx.entityInstances.find(entity);
	if (!Locator::entitiesRegistry::value().Valid(entity) || instance == renderCtx.entityInstances.end() ||
	    !meshes.Contains(instance->second.meshId))
	{
		return;
	}
	const auto mesh = meshes.Handle(instance->second.meshId);
	// DrawUnderWater (static 0x811010 -> fn_00850FC0): mirrored in y = 0 (the reflection camera here), what had y < 0
	// clipped away, unlit in the diffuse obj+0x4C = 0xFF303070 (vs_object mode 2 with the packed rgb)
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	submitDesc.options = render_modes::k_ModelPass;
	submitDesc.clipBelowSea = true;
	submitDesc.unlitColour = static_cast<float>(ecs::petit_navire::k_ReflectionColour & 0x00FFFFFFu);
	submitDesc.instanceDesc =
	    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1);
	static const auto k_Identity = glm::mat4(1.0f);
	submitDesc.modelMatrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &k_Identity;
	submitDesc.matrixCount = mesh->IsBoned() ? static_cast<uint8_t>(mesh->GetBoneMatrices().size()) : 1;
	submitDesc.program = _shaderManager->GetShader("ObjectInstanced");
	DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectBoatSprites(const Camera& camera) const
{
	std::vector<std::pair<float, uint32_t>> order;
	_frameBoatSprites.clear();
	const auto& wake = ecs::petit_navire::GetWake();
	const auto& clouds = ecs::smoky_stuff::Get();
	if (wake.empty() && clouds.empty())
	{
		return order;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return order;
	}
	const auto eye = camera.GetOrigin();
	// LH3DSprite::AddDrawing 0x840C70: key = |sprite +0/+4/+8 - g_camera|^2, (x^2 + y^2) + z^2; the sprite is queued
	// whether or not Draw then culls it at the near plane
	const auto add = [this, &order, &eye](const glm::vec3& position, std::optional<billboard::Quad> quad, uint32_t argb) {
		order.emplace_back(zsorter::Key(position, eye), static_cast<uint32_t>(_frameBoatSprites.size()));
		_frameBoatSprites.push_back({std::move(quad), argb});
	};
	// the wake: flag 0x40, the quad in the sprite's XZ turned about Y (LH3DSprite::Draw 0x8405FE, billboard::Horizontal)
	for (const auto& wakeSprite : wake)
	{
		billboard::Sprite sprite;
		sprite.position = wakeSprite.position;
		sprite.size = wakeSprite.half;
		sprite.height = wakeSprite.aspect;
		sprite.angle = wakeSprite.angle;
		sprite.argb = wakeSprite.argb;
		sprite.cell = frame_anim::SpriteCell(wakeSprite.cell);
		sprite.horizontal = true;
		add(sprite.position, billboard::Horizontal(sprite), sprite.argb);
	}
	// the puffs: mode A, in the plane of the screen and turned on it (0x84071D: view x -> (cos, -sin), view y -> (sin,
	// cos); billboard::SpriteQuad), nothing at or before the near plane (0x840585)
	const auto frame = billboard::CameraFrame::From(camera);
	for (const auto& cloud : clouds)
	{
		if (cloud.life <= 0.0f)
		{
			continue;
		}
		for (const auto& puff : cloud.puffs)
		{
			billboard::Sprite sprite;
			sprite.position = puff.position;
			sprite.size = puff.half;
			sprite.angle = puff.angle;
			sprite.argb = puff.argb;
			sprite.cell = frame_anim::SpriteCell(puff.cell);
			add(sprite.position, billboard::SpriteQuad(sprite, frame), sprite.argb);
		}
	}
	return order;
}

void Renderer::DrawBoatSprite(RenderPass viewId, uint32_t index) const
{
	if (index >= _frameBoatSprites.size() || !_frameBoatSprites[index].quad.has_value())
	{
		return;
	}
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto& [quad, argb] = _frameBoatSprites[index];
	// the cell of the 8 x 8 sheet (+0x30 = 8) is in the quad's UVs, the colour is the vertex diffuse
	const uint32_t abgr = lh3d_colour::ToAbgr(argb);
	std::array<Vertex, billboard::k_SpriteTriangles.size()> vertices {};
	for (size_t k = 0; k < vertices.size(); ++k)
	{
		const auto i = static_cast<size_t>(billboard::k_SpriteTriangles.at(k));
		const auto& p = quad->corners.at(i);
		const auto& uv = quad->uv.at(i);
		vertices.at(k) = {p.x, p.y, p.z, uv.x, uv.y, abgr};
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
	const auto& textures = Locator::resources::value().GetTextures();
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// the smoke material [0xEA1ABC] (PetitNavire 0x5E1328, SmokyStuff::Create 0x823D21), mode 6: SRCALPHA /
	// INVSRCALPHA, no Z write, both faces
	bgfx::setState(render_modes::State(render_modes::materials::k_Smoke));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}
