/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// What FallingSpell (Magic/Objects/FallingSpell.h) draws over the film fall.bik in mode 2: the copy of the creature
// (drawn at once after the film, in the colours the spell gives it instead of the land's light), the sparks (sprites
// with the smoke material, drawn by the Z-sorter's flush at the end of the frame) and the light bursts (a finish frame
// callback, world triangles in the additive atmosphere material). The copy is drawn in the 3D frame through the fall's
// camera, over the film in the main view. The sparks and the bursts are anchored to points on the screen. The sparks
// share the end of frame's queue with the copy's hand glows, which the original drains after the help system's 3D draw
// (the input prompt icons' first draw): both are drawn far to near in the FinishFrameQueue view, after the icons' view
// and over the copy's depth, the sparks placed back in the world so that the view's camera puts them on their pixels.
// The bursts are drawn in screen pixels in the ScreenOverlay view, before the bars and the fade
// (Renderer::DrawFinishFrameOverlays). (inferred) The sparks lie in front of the copy, so they are not tested against it.

#include <cstring>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/FallingCreature.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "EngineConfig.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/CreatureDraw.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Magic/Objects/FallingSpell.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
constexpr entt::id_type k_Atmos = entt::hashed_string("raw/ATMOS").value();
constexpr entt::id_type k_AtmosAlpha = entt::hashed_string("raw/ATMOSA").value();

struct Vertex
{
	float x, y, z, u, v;
	uint32_t abgr;
};

/// The quads' layout: a position, a texture coordinate and a colour
const bgfx::VertexLayout& QuadLayout()
{
	static const bgfx::VertexLayout s_layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	return s_layout;
}

/// Quads in one material and texture, in a view, in their order. Neither mode writes Z, and ZFUNC is ALWAYS
void SubmitQuads(ShaderManager& shaders, bgfx::ViewId viewId, std::span<const Vertex> vertices, entt::id_type texture,
                 entt::id_type alpha, const render_modes::Material& material)
{
	const auto& textures = Locator::resources::value().GetTextures();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (count == 0 || bgfx::getAvailTransientVertexBuffer(count, QuadLayout()) < count || !textures.Contains(texture) ||
	    !textures.Contains(alpha))
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, QuadLayout());
	std::memcpy(buffer.data, vertices.data(), vertices.size_bytes());
	const auto* program = shaders.GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(alpha));
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(render_modes::State(material, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
	program->Submit(viewId);
}

/// A spark's two triangles {0, 1, 2}, {0, 2, 3}, in clip space at the middle depth, or placed back in the world through
/// `clipToWorld`, the inverse of a view's camera
void AppendSpark(std::vector<Vertex>& vertices, const std::array<magic::falling_spell::ScreenVertex, 4>& quad, int width,
                 int height, const std::optional<glm::dmat4>& clipToWorld)
{
	for (const int i : billboard::k_SpriteTriangles)
	{
		const auto& v = quad.at(static_cast<size_t>(i));
		glm::vec3 point {2.0f * v.pixel.x / static_cast<float>(width) - 1.0f,
		                 1.0f - 2.0f * v.pixel.y / static_cast<float>(height), 0.5f};
		if (clipToWorld.has_value())
		{
			const auto world = *clipToWorld * glm::dvec4(glm::dvec3(point), 1.0);
			point = glm::vec3(glm::dvec3(world) / world.w);
		}
		vertices.push_back({point.x, point.y, point.z, v.uv.x, v.uv.y, argb_colour::ToAbgr(v.argb)});
	}
}
} // namespace

void Renderer::DrawFallingCreature(const Camera& camera) const
{
	const auto& spell = magic::falling_spell::Get();
	const auto& tint = spell.CreatureTintNow();
	if (!spell.IsActive() || !tint.has_value() || !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// its creature's skins as they are painted now: the land's draw, which keeps them up to date, does not run in mode 2
	UpdateCreatureSkins(registry);
	if (Locator::config::has_value() && !Locator::config::value().drawFallingCreature)
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto body = creature_draw::FallingBody(registry, [&meshes](entt::id_type id) -> std::optional<size_t> {
		if (!meshes.Contains(id))
		{
			return std::nullopt;
		}
		return meshes.Handle(id)->GetBoneMatrices().size();
	});
	constexpr auto k_Stride = static_cast<uint16_t>(sizeof(creature_draw::InstanceRow));
	if (!body.has_value() || bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
	{
		return;
	}
	_shaderManager->SetCamera(RenderPass::MainBlended, camera);
	// Its row: where its body is drawn, with the spell's colour set on it instead of the land's light (so no haze
	// either) and the spell's specular; lit by the model light the spell set
	const auto model = ecs::DrawnBodyModel(registry, body->entity);
	const auto useRow = [](L3DMeshSubmitDesc& submit, bgfx::InstanceDataBuffer& buffer, const creature_draw::InstanceRow& row) {
		bgfx::allocInstanceDataBuffer(&buffer, 1, k_Stride);
		std::memcpy(buffer.data, row.data(), sizeof(row));
		submit.transientInstance = &buffer;
	};
	const auto& mesh = *meshes.Handle(body->mesh);
	{
		L3DMeshSubmitDesc submit {};
		submit.viewId = RenderPass::MainBlended;
		submit.program = _shaderManager->GetShader("ObjectInstanced");
		submit.options = render_modes::k_ModelPass;
		submit.noHaze = true;
		const auto bones = body->bones.empty() ? std::span<const glm::mat4>(mesh.GetBoneMatrices()) : body->bones;
		submit.modelMatrices = bones.data();
		submit.matrixCount = static_cast<uint8_t>(bones.size());
		UseCreatureBody(*body, submit);
		bgfx::InstanceDataBuffer row {};
		useRow(submit, row, creature_draw::FallingRow(model, tint->colour, tint->specular));
		DrawMesh(mesh, submit, std::numeric_limits<uint8_t>::max());
	}
	// Then its hair, at the end of its body's draw: the ribbons facing the fall's eye, each tuft's colour in the body's
	// colour with its specular added, as the body's draw lights a creature's hair
	if (const auto* hair = registry.TryGet<const ecs::components::CreatureHair>(body->entity);
	    hair != nullptr && !hair->groups.empty())
	{
		const auto rgb = [](uint32_t argb) {
			return glm::ivec3 {static_cast<int>((argb >> 16u) & 0xFFu), static_cast<int>((argb >> 8u) & 0xFFu),
			                   static_cast<int>(argb & 0xFFu)};
		};
		const auto drawn = creature_draw::BuildHair(*hair, camera.GetOrigin(), rgb(tint->colour), rgb(tint->specular));
		const auto& textures = Locator::resources::value().GetTextures();
		const auto& white = RevolvedSurfaceWhiteTexture();
		const bool textured = textures.Contains(ecs::components::CreatureHair::k_TextureId) &&
		                      textures.Contains(ecs::components::CreatureHair::k_AlphaTextureId);
		if (!drawn.textured.indices.empty())
		{
			world_triangles::SubmitRaw(RenderPass::MainBlended, drawn.textured.vertices, drawn.textured.indices,
			                           textured ? *textures.Handle(ecs::components::CreatureHair::k_TextureId) : white,
			                           textured ? *textures.Handle(ecs::components::CreatureHair::k_AlphaTextureId) : white,
			                           render_modes::materials::k_CreatureHair, *_shaderManager);
		}
		if (!drawn.plain.indices.empty())
		{
			world_triangles::SubmitRaw(RenderPass::MainBlended, drawn.plain.vertices, drawn.plain.indices, white, white,
			                           render_modes::materials::k_CreatureHairPlain, *_shaderManager);
		}
	}
	// Then its eyes, placed in the world and drawn in its body's space: each eyeball in the body's colours, each eyelid
	// in the skin under it times the body's colour
	if (const auto* eyes = registry.TryGet<const ecs::components::CreatureEyes>(body->entity); eyes != nullptr)
	{
		L3DMeshSubmitDesc submit {};
		submit.viewId = RenderPass::MainBlended;
		submit.program = _shaderManager->GetShader("ObjectInstanced");
		submit.options = render_modes::k_ModelPass;
		submit.noHaze = true;
		for (const auto& part : creature_draw::Eyes(*eyes, [&meshes](entt::id_type id) { return meshes.Contains(id); }))
		{
			if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
			{
				break;
			}
			const auto local = creature_draw::InBodySpace(model, part.model);
			submit.modelMatrices = &local;
			submit.matrixCount = 1;
			const uint32_t colour =
			    part.tint.has_value() ? creature_draw::LidColour(*part.tint, tint->colour, part.lidTimes) : tint->colour;
			bgfx::InstanceDataBuffer row {};
			useRow(submit, row, creature_draw::FallingRow(model, colour, tint->specular));
			DrawMesh(*meshes.Handle(part.mesh), submit, std::numeric_limits<uint8_t>::max());
		}
	}
	// Its hand glows, which its draw queued, are drawn with the sparks from the end of frame's queue
	// (DrawFallingFrameQueue)
}

void Renderer::DrawFallingFrameQueue(const Camera& camera) const
{
	const auto& spell = magic::falling_spell::Get();
	const int width = _resolution.x;
	const int height = _resolution.y;
	const auto viewId = RenderPass::FinishFrameQueue;
	_shaderManager->SetCamera(viewId, camera);
	// The copy's glows its draw placed this frame, as its draw queued them (none while it is hidden or not drawn)
	std::vector<std::pair<float, creature_hand_glow::Placed>> glows;
	const auto& fall = spell.CameraNow();
	const bool copyDrawn = !Locator::config::has_value() || Locator::config::value().drawFallingCreature;
	if (fall.has_value() && copyDrawn && Locator::entitiesRegistry::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		registry.Each<const ecs::components::FallingCreature>([&](entt::entity copy, auto&&...) {
			const auto shown = FallingCreatureGlows(camera, copy, fall->position);
			glows.insert(glows.end(), shown.begin(), shown.end());
		});
	}
	const auto sparks = spell.KeyedSparkQuads(width, height, billboard::CameraFrame::From(camera).nearZ);
	std::vector<float> glowKeys;
	std::vector<float> sparkKeys;
	std::ranges::transform(glows, std::back_inserter(glowKeys), [](const auto& glow) { return glow.first; });
	std::ranges::transform(sparks, std::back_inserter(sparkKeys), &magic::falling_spell::KeyedSpark::key);
	// Far to near, each glow at once and each run of sparks in one draw, placed back in the world
	const auto clipToWorld = glm::inverse(glm::dmat4(camera.GetProjectionMatrix()) *
	                                      glm::dmat4(camera.GetViewMatrix(Camera::Interpolation::Current)));
	std::vector<Vertex> run;
	const auto flush = [this, &run, viewId]() {
		SubmitQuads(*_shaderManager, static_cast<bgfx::ViewId>(viewId), run, k_Smoke, k_SmokeAlpha,
		            render_modes::materials::k_Smoke);
		run.clear();
	};
	for (const auto& draw : magic::falling_spell::FrameEndOrder(glowKeys, sparkKeys))
	{
		if (draw.glow)
		{
			flush();
			DrawCreedGlowAt(viewId, camera, glows.at(draw.index).second);
		}
		else
		{
			AppendSpark(run, sparks.at(draw.index).quad, width, height, clipToWorld);
		}
	}
	flush();
}

void Renderer::DrawFallingSpellOverlay(const Camera* camera) const
{
	const auto& spell = magic::falling_spell::Get();
	if (!spell.IsActive() || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const int width = _resolution.x;
	const int height = _resolution.y;
	// (inferred) the near plane of mode 2 is the one the last camera left: openblack's camera's (the draw's
	// copy, DrawSceneDesc::camera)
	const float nearZ = camera != nullptr ? billboard::CameraFrame::From(*camera).nearZ : 1.0f;
	const auto toClipX = [width](float px) { return 2.0f * px / static_cast<float>(width) - 1.0f; };
	const auto toClipY = [height](float py) { return 1.0f - 2.0f * py / static_cast<float>(height); };
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	// the Z: the end of frame's Z reset quad (z = 1 everywhere) of the frame before, and nothing with Z drawn in mode 2
	// before these but the copy (the film is ZFUNC ALWAYS without Z), so LESSEQUAL always passes: ALWAYS here; neither
	// mode writes Z (6 and 13, the "NoZWrite" variants)
	const auto submit = [this, viewId](const std::vector<Vertex>& vertices, entt::id_type texture, entt::id_type alpha,
	                                   const render_modes::Material& material) {
		SubmitQuads(*_shaderManager, viewId, vertices, texture, alpha, material);
	};

	// the sparks, far to near, each one Z object, with the copy's glows: the end of frame's queue in its own view, after
	// the input prompt icons' first draw. (openblack guard) without the draw's camera, the sparks alone here, one buffer
	// keeping their order: their two triangles {0, 1, 2}, {0, 2, 3} one after the other
	if (camera != nullptr)
	{
		DrawFallingFrameQueue(*camera);
	}
	else
	{
		std::vector<Vertex> sparks;
		for (const auto& quad : spell.SparkQuads(width, height, nearZ))
		{
			AppendSpark(sparks, quad, width, height, std::nullopt);
		}
		// the smoke material (smoke.raw, mode 6, two-sided; the sprite's flag 0x80 selects it)
		submit(sparks, k_Smoke, k_SmokeAlpha, render_modes::materials::k_Smoke);
	}

	// the four bursts in their order, 64 world triangles each
	std::vector<Vertex> bursts;
	const auto indices = magic::falling_spell::BurstIndices();
	for (const auto& fan : spell.Bursts())
	{
		for (const int i : indices)
		{
			const auto& v = fan.vertices.at(static_cast<size_t>(i));
			bursts.push_back({toClipX(v.pixel.x), toClipY(v.pixel.y), 0.5f, v.uv.x, v.uv.y, argb_colour::ToAbgr(v.argb)});
		}
	}
	// the additive atmosphere material: atmos.raw, mode 13 (SRCALPHA / ONE), two-sided
	submit(bursts, k_Atmos, k_AtmosAlpha, render_modes::materials::k_AtmosAdditive);
}
