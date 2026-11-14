/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The creatures the interface shows over itself (Graphics/InterfaceCreatures.h): the Creature Stats tab of the
// statistics box draws each player's creature in its box. As the game does in the box's draw, each one is drawn as it
// was last drawn in the world (its pose, its shape, its skins and its colours there), moved for the draw to the point in
// front of the box's own camera that is under its place on the screen, after the depth is cleared. Its colours are
// those its world draw lit it with, the land's light under its place in the world with its haze, set on its row instead
// of the land's light, so that its move does not light it again.

#include <cstring>

#include <algorithm>
#include <limits>
#include <optional>
#include <span>
#include <utility>

#include <bgfx/bgfx.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandLight.h"
#include "Camera/Camera.h"
#include "Creature/CreatureFizzLook.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/CreatureDraw.h"
#include "Graphics/Haze.h"
#include "Graphics/InterfaceCreatures.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/RenderModes.h"
#include "Visual/RenderPass.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr auto k_Stride = static_cast<uint16_t>(sizeof(creature_draw::InstanceRow));

glm::ivec3 Rgb(uint32_t argb)
{
	return {static_cast<int>((argb >> 16u) & 0xFFu), static_cast<int>((argb >> 8u) & 0xFFu), static_cast<int>(argb & 0xFFu)};
}

/// A matrix moved across the world by an offset
glm::mat4 Moved(glm::mat4 matrix, const glm::vec3& offset)
{
	matrix[3] += glm::vec4(offset, 0.0f);
	return matrix;
}

/// Triangles moved across the world by an offset
void MoveStrip(creature_draw::Strip& strip, const glm::vec3& offset)
{
	for (auto& vertex : strip.vertices)
	{
		vertex.position += offset;
	}
}
} // namespace

void Renderer::ShowInterfaceCreatures(interface_creatures::Request request) noexcept
{
	if (request.creatures.empty() || request.viewSize.x < 1.0f || request.viewSize.y < 1.0f ||
	    !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value() ||
	    !Locator::rendereringSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& context = Locator::rendereringSystem::value().GetContext();
	const auto& camera = Locator::camera::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();

	// The interface's space on the window, its depth cleared, seen from the box's camera
	const auto viewId = RenderPass::InterfaceCreatures;
	const auto id = static_cast<bgfx::ViewId>(viewId);
	bgfx::setViewMode(id, bgfx::ViewMode::Sequential);
	bgfx::setViewRect(id, static_cast<uint16_t>(request.viewOrigin.x), static_cast<uint16_t>(request.viewOrigin.y),
	                  static_cast<uint16_t>(request.viewSize.x), static_cast<uint16_t>(request.viewSize.y));
	bgfx::setViewClear(id, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
	const auto view = interface_creatures::View(request.turn);
	const auto projection = interface_creatures::SpaceProjection(camera.GetProjectionMatrix());
	bgfx::setViewTransform(id, &view, &projection);
	bgfx::touch(id);
	const auto eye = interface_creatures::Eye(request.turn);
	const auto worldView = camera.GetViewMatrix(Camera::Interpolation::Current);

	for (const auto& shown : request.creatures)
	{
		// As the world last drew it: a creature with no row or no body there is not shown
		const auto found = std::ranges::find(_frameCreatures, shown.creature, &creature_draw::Body::entity);
		if (found == _frameCreatures.end() || found->instance >= context.instanceUniforms.size() ||
		    found->instance >= context.instanceColours.size() || !meshes.Contains(found->mesh))
		{
			continue;
		}
		const auto& body = *found;
		const auto& mesh = *meshes.Handle(body.mesh);
		const auto& model = context.instanceUniforms.at(body.instance);
		const auto bones = body.bones.empty() ? std::span<const glm::mat4>(mesh.GetBoneMatrices()) : body.bones;

		// Moved to the point under its anchor, at the depth that makes it fill its box: its bones' middle across the
		// land, and its feet, go there
		const float size = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, body.entity));
		const float depth = interface_creatures::Depth(size, shown.size, 1.0f);
		const auto point =
		    interface_creatures::PointFromScreen(view, projection, interface_creatures::k_Space, shown.anchor, depth);
		const auto offset = interface_creatures::MoveToPoint(point, model, bones, model[3].y);
		const auto moved = Moved(model, offset);

		// Its colours as its world draw lit it: the land's light under its place in the world, its tint, then the haze
		const auto origin = glm::vec3(model[3]);
		uint32_t diffuse = 0xFFFFFFFFu;
		uint32_t specular = 0xFF000000u;
		if (IsLandLit())
		{
			const auto sample = land_light::At(land_light::CurrentTable(), glm::vec2(origin.x, origin.z));
			const auto lit =
			    interface_creatures::BeforeHaze(context.instanceColours.at(body.instance), sample.diffuse, sample.specular);
			diffuse = lit.diffuse;
			specular = lit.specular;
			// as in the world, a colour set on the row is drawn without the haze
			if (!argb_colour::InstanceColour(context.instanceColours.at(body.instance)).has_value())
			{
				specular = haze::ApplyObject(_haze, haze::Depth(worldView, origin), lit.specular, &diffuse);
			}
		}
		const auto colours = interface_creatures::SetColours(context.instanceColours.at(body.instance), diffuse, specular);

		L3DMeshSubmitDesc submit {};
		submit.viewId = viewId;
		submit.program = _shaderManager->GetShader("ObjectInstanced");
		submit.options = render_modes::k_ModelPass;
		submit.noHaze = true;
		if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
		{
			break; // (openblack guard) no room left this frame
		}
		{
			submit.modelMatrices = bones.data();
			submit.matrixCount = static_cast<uint8_t>(bones.size());
			UseCreatureBody(body, submit);
			const creature_draw::InstanceRow row {moved[0], moved[1], moved[2], moved[3], colours};
			bgfx::InstanceDataBuffer buffer {};
			bgfx::allocInstanceDataBuffer(&buffer, 1, k_Stride);
			std::memcpy(buffer.data, row.data(), sizeof(row));
			submit.transientInstance = &buffer;
			DrawCreatureBody(mesh, submit, body, 0);
		}

		// Its hair, facing the box's camera, in its colours, until it has fizzed or frozen a fifth of the way
		if (const auto* hair = registry.TryGet<const ecs::components::CreatureHair>(body.entity);
		    hair != nullptr && !hair->groups.empty() && creature_fizz_look::HairShown(body.fizz, body.freeze))
		{
			// built where it is in the world, facing the eye as it stands from there, then moved with the body
			auto drawn = creature_draw::BuildHair(*hair, eye - offset, Rgb(diffuse), Rgb(specular));
			MoveStrip(drawn.textured, offset);
			MoveStrip(drawn.plain, offset);
			const auto& white = RevolvedSurfaceWhiteTexture();
			const bool textured = textures.Contains(ecs::components::CreatureHair::k_TextureId) &&
			                      textures.Contains(ecs::components::CreatureHair::k_AlphaTextureId);
			if (!drawn.textured.indices.empty())
			{
				world_triangles::SubmitRaw(viewId, drawn.textured.vertices, drawn.textured.indices,
				                           textured ? *textures.Handle(ecs::components::CreatureHair::k_TextureId) : white,
				                           textured ? *textures.Handle(ecs::components::CreatureHair::k_AlphaTextureId) : white,
				                           render_modes::materials::k_CreatureHair, *_shaderManager);
			}
			if (!drawn.plain.indices.empty())
			{
				world_triangles::SubmitRaw(viewId, drawn.plain.vertices, drawn.plain.indices, white, white,
				                           render_modes::materials::k_CreatureHairPlain, *_shaderManager);
			}
		}

		// Its eyes, moved with it: each eyeball in its colours, each eyelid in the skin under it times its colour
		const bool eyesFizz = creature_fizz_look::EyesFizz(body.fizz, body.freeze);
		const bool eyesGone = body.freeze == 0.0f && body.fizz != 0.0f && !creature_fizz_look::Drawn(body.fizz);
		if (const auto* eyes = registry.TryGet<const ecs::components::CreatureEyes>(body.entity); eyes != nullptr && !eyesGone)
		{
			for (const auto& part : creature_draw::Eyes(*eyes, [&meshes](entt::id_type mesh) { return meshes.Contains(mesh); }))
			{
				if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
				{
					break; // (openblack guard) no room left this frame
				}
				const auto local = creature_draw::InBodySpace(moved, Moved(part.model, offset));
				L3DMeshSubmitDesc eyeSubmit {};
				eyeSubmit.viewId = viewId;
				eyeSubmit.program = _shaderManager->GetShader("ObjectInstanced");
				eyeSubmit.options = render_modes::k_ModelPass;
				eyeSubmit.noHaze = true;
				eyeSubmit.modelMatrices = &local;
				eyeSubmit.matrixCount = 1;
				const auto drawnRow = creature_draw::EyeRow(
				    moved, colours,
				    part.tint.has_value() ? std::optional(creature_draw::FrozenLidColour(
				                                creature_draw::LidColour(*part.tint, diffuse, part.lidTimes), body.freeze))
				                          : std::nullopt,
				    specular);
				bgfx::InstanceDataBuffer buffer {};
				bgfx::allocInstanceDataBuffer(&buffer, 1, k_Stride);
				std::memcpy(buffer.data, drawnRow.data(), sizeof(drawnRow));
				eyeSubmit.transientInstance = &buffer;
				if (eyesFizz)
				{
					DrawFizzed(*meshes.Handle(part.mesh), eyeSubmit, creature_fizz_look::Level(body.fizz), _frameEyeStatic);
				}
				else
				{
					DrawMesh(*meshes.Handle(part.mesh), eyeSubmit, std::numeric_limits<uint8_t>::max());
				}
			}
		}
	}
}
