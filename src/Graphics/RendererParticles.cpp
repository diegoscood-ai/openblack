/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The particle effects' sprites (PSys)

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "3D/AllMeshes.h"
#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Camera/Camera.h"
#include "Creature/CreatureHandGlow.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/Temple.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/GlowShellLook.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VolumeGlowShell.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/Rules/SurfRevol.h"
#include "Particles/VolumeGlow.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/HeartShell.h"
#include "Temple/TempleDestruction.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// Whether a creed glow is queued where it is: on the screen, and not too far from the eye for its size
bool CreedGlowShown(const creature_hand_glow::Placed& placed, const glm::mat4& viewProjection, glm::vec3 eye,
                    const AxisAlignedBoundingBox& box, float halfDiagonal)
{
	const auto point = glm::vec3(placed.model[3]);
	return !glow_shell::TooFar(zsort::Key(point, eye, zsort::SumOrder::XYZ), halfDiagonal, placed.scale) &&
	       region_on_screen::BoxInView(viewProjection, box, placed.model);
}
} // namespace

std::pair<entt::id_type, entt::id_type> Renderer::RawTextureIds(const std::string& texture) const
{
	if (const auto it = _rawTextureIds.find(texture); it != _rawTextureIds.end())
	{
		return it->second;
	}
	const std::pair<entt::id_type, entt::id_type> ids {entt::hashed_string(("raw/" + texture).c_str()).value(),
	                                                   entt::hashed_string(("raw/" + texture + "a").c_str()).value()};
	_rawTextureIds.emplace(texture, ids);
	return ids;
}

const bgfx::VertexLayout& Renderer::ParticleQuadLayout()
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

void Renderer::DrawParticleSprites(std::span<const particles::Effect::DrawAtom> atoms, const Camera& camera,
                                   RenderPass viewId) const
{
	DrawParticleSprites(atoms, billboard::CameraFrame::From(camera), viewId, {});
}

void Renderer::DrawParticleSprites(std::span<const particles::Effect::DrawAtom> atoms,
                                   const billboard::CameraFrame& cameraFrame, RenderPass viewId,
                                   const std::function<glm::vec3(const glm::vec3&)>& place) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldTextured");

	// atoms in the order given: one sprite of a Sorted effect (its own Z object), a run of such sprites next to each
	// other in the queue, or the sprites of a Queued / Immediate effect between its other items. Consecutive atoms with
	// the same material (the same sheets and blend) go in one draw call: drawn in the same order with the same states,
	// the same pixels as one call each
	const auto materialOf = [this](const particles::Creator& c) {
		return particles::draw::SpriteMaterial(RawTextureIds(c.texture), c.additive, c.writeDepth);
	};
	size_t i = 0;
	while (i < atoms.size())
	{
		const auto* creator = atoms[i].creator;
		const auto material = materialOf(*creator);
		const auto texture = material.texture;
		const auto alphaTexture = material.alphaTexture;
		std::vector<Vertex> vertices;
		vertices.reserve(6 * (atoms.size() - i));
		size_t j = i;
		for (; j < atoms.size(); ++j)
		{
			const auto& atom = atoms[j];
			const auto* c = atom.creator;
			if (c != creator && materialOf(*c) != material)
			{
				break;
			}
			// the whole frame drawn, looped within NumFrames or clamped to its last (frame_anim::ParticleFrameIndex)
			const int frame = frame_anim::ParticleFrameIndex(atom.frame, c->numFrames, c->loopAnim);
			const float alpha = std::clamp(atom.alpha * static_cast<float>(c->scaleAlpha) / 255.0f, 0.0f, 255.0f);
			const uint32_t abgr = (static_cast<uint32_t>(alpha) << 24) | (static_cast<uint32_t>(atom.colour[2]) << 16) |
			                      (static_cast<uint32_t>(atom.colour[1]) << 8) | atom.colour[0];
			// each atom fills its creator's sprite and draws it
			billboard::Sprite sprite;
			sprite.position = atom.position;
			// size = the drawn scale, at least 0.0001; the stretch is the height
			sprite.size = std::max(atom.scale, 1e-4f);
			sprite.height = atom.stretch;
			// ox = OriginX size, oy = OriginY size stretch; the sprite draw subtracts them from the local corners
			sprite.origin = glm::vec2(c->originX * sprite.size, c->originY * sprite.size * sprite.height);
			// the angle is atan2(M[0][2], M[0][0]) of the drawn matrix unless IgnoreRotation; the original's switch for
			// it is always on, so the roll is always on. With IgnoreRotation the angle keeps what it has: (inferred) the
			// 0 the sprite starts with, each creator having its own sprite
			sprite.angle = c->ignoreRotation ? 0.0f : std::atan2(atom.rotation[0][2], atom.rotation[0][0]);
			// CentreAtBase raises the position by height x size x 0.5
			if (c->centreAtBase)
			{
				sprite.position.y += sprite.height * sprite.size * 0.5f;
			}
			// the cell in the flags' low 6 bits; SetHorozontal is flag 0x40, which also takes the origin and the angle
			// (as the yaw)
			sprite.cell = frame_anim::SpriteCell(c->fileOffset + frame);
			sprite.cellsPerRow = static_cast<uint8_t>(c->spritesPerRow);
			sprite.horizontal = c->horizontal;
			const auto quad = billboard::SpriteQuad(sprite, cameraFrame);
			if (!quad.has_value())
			{
				continue;
			}
			for (const int k : billboard::k_SpriteTriangles)
			{
				const auto& corner = quad->corners.at(static_cast<size_t>(k));
				const auto p = place ? place(corner) : corner;
				const auto& uv = quad->uv.at(static_cast<size_t>(k));
				vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
			}
		}
		i = j;
		if (vertices.empty() || !textures.Contains(texture))
		{
			continue;
		}
		const auto& layout = ParticleQuadLayout();
		const auto count = static_cast<uint32_t>(vertices.size());
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
		program->SetTextureSampler(
		    "s_alpha", 1, textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture));
		bgfx::setVertexBuffer(0, &buffer);
		// the creator's material: modes 13 / 6 (12 / 5 with MaterialUpdateZBuffer), Z test on, two-sided. (inferred) the
		// alpha property is 1: with 0 they would be 8 / 3, the same states
		bgfx::setState(render_modes::State(material.mode));
		program->Submit(static_cast<bgfx::ViewId>(viewId));
	}
}

void Renderer::DrawParticleFragment(RenderPass viewId, const world_triangles::Frame& pieces, const particles::Atom* atom) const
{
	// the pieces were built on the CPU for the frame (world-space vertices, lit colours): each primitive's own material,
	// alpha test and culling, one transient buffer per call
	world_triangles::Submit(viewId, pieces, *_shaderManager, atom);
}

const VolumeGlowShell* Renderer::GlowShellOf(entt::id_type meshId, uint32_t subMeshIndex) const
{
	const uint64_t key = (static_cast<uint64_t>(meshId) << 8u) | static_cast<uint64_t>(subMeshIndex & 0xFFu);
	if (const auto found = _glowShells.find(key); found != _glowShells.end())
	{
		return found->second.get();
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::unique_ptr<VolumeGlowShell> shell;
	if (meshes.Contains(meshId))
	{
		const auto mesh = meshes.Handle(meshId);
		const auto& subMeshes = mesh->GetSubMeshes();
		if (subMeshIndex < subMeshes.size())
		{
			// The part's corners as the model holds them, and its triangles: each edge becomes a band
			const auto& part = *subMeshes[subMeshIndex];
			const auto& positions = part.GetCollisionPositions();
			const auto triangles = particles::volume_glow::ShellTriangles(part.GetCollisionIndices());
			if (!triangles.empty() && !positions.empty())
			{
				const auto vertices = VolumeGlowShell::Vertices(positions, part.GetCollisionNormals());
				shell = std::make_unique<VolumeGlowShell>(vertices, triangles);
			}
		}
	}
	return _glowShells.emplace(key, std::move(shell)).first->second.get();
}

void Renderer::DrawParticleShell(RenderPass viewId, const Camera& camera, const particles::draw::ShellDraw& shell) const
{
	if (!Locator::rendereringSystem::has_value() || !Locator::resources::has_value() || shell.object == entt::null)
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!textures.Contains(shell.texture))
	{
		return;
	}
	// The object as it is drawn this frame, by the model drawn (a finer one may stand in for its own): none while it
	// has no place among the drawn instances
	const auto& context = Locator::rendereringSystem::value().GetContext();
	const auto placed = context.entityInstances.find(shell.object);
	if (placed == context.entityInstances.end() || placed->second.index >= context.instanceUniforms.size() ||
	    !meshes.Contains(placed->second.meshId))
	{
		return;
	}
	const auto meshId = placed->second.meshId;
	auto model = context.instanceUniforms[placed->second.index];
	// A fading instance keeps its fade in the first column's w; the shell's matrix has none
	model[0][3] = 0.0f;
	const auto mesh = meshes.Handle(meshId);
	// The band reaches out by the model's half diagonal, scaled
	const float reach = glm::length(mesh->GetBoundingBox().Size()) * 0.5f * shell.zoom;
	const auto& texture = *textures.Handle(shell.texture);
	const auto& alpha = textures.Contains(shell.alphaTexture) ? *textures.Handle(shell.alphaTexture) : texture;
	const glm::vec4 colour {static_cast<float>((shell.argb >> 16u) & 0xFFu), static_cast<float>((shell.argb >> 8u) & 0xFFu),
	                        static_cast<float>(shell.argb & 0xFFu), static_cast<float>(shell.argb >> 24u)};
	const glm::vec4 look {shell.uvOffset, shell.textureFraction};
	const auto* program = _shaderManager->GetShader("VolumeGlow");
	// How much the projection widens the camera's frame across and up, which weighs how side on the shell is seen
	const auto& projection = camera.GetProjectionMatrix();
	// Added to what is behind with its alpha (mode 13), no depth write; each band seen from its front only, as the model
	const auto state = render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite,
	                                       {.cull = render_modes::Cull::Ccw, .zWrite = false, .msaa = true});
	const auto& subMeshes = mesh->GetSubMeshes();
	for (uint32_t i = 0; i < subMeshes.size(); ++i)
	{
		// Only the nearest detail's parts in their first state glow. A part moved by bones keeps only its bind pose
		// here, so it has no shell
		const auto flags = subMeshes[i]->GetFlags();
		if ((flags.lodMask & 1u) == 0 || flags.status != 0 || flags.hasBones != 0)
		{
			continue;
		}
		const auto* band = GlowShellOf(meshId, i);
		if (band == nullptr || band->indexCount == 0)
		{
			continue;
		}
		const glm::vec4 shape {reach, 0.0f, projection[0][0], projection[1][1]};
		program->SetUniformValue("u_glowShape", &shape);
		program->SetUniformValue("u_glowColour", &colour);
		program->SetUniformValue("u_glowTexture", &look);
		program->SetTextureSampler("s_diffuse", 0, texture);
		program->SetTextureSampler("s_alpha", 1, alpha);
		bgfx::setTransform(glm::value_ptr(model));
		band->vertices->Bind();
		band->triangles->Bind(band->indexCount);
		bgfx::setState(state);
		program->Submit(static_cast<bgfx::ViewId>(viewId));
	}
}

void Renderer::DrawGlowShellAt(RenderPass viewId, const Camera& camera, entt::id_type meshId, const glm::mat4& model,
                               float reach, const particles::draw::ShellDraw& look, uint32_t addedRgb) const
{
	if (!Locator::resources::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshId) || !textures.Contains(look.texture))
	{
		return;
	}
	const auto mesh = meshes.Handle(meshId);
	const auto& texture = *textures.Handle(look.texture);
	const auto& alpha = textures.Contains(look.alphaTexture) ? *textures.Handle(look.alphaTexture) : texture;
	const glm::vec4 colour {static_cast<float>((look.argb >> 16u) & 0xFFu), static_cast<float>((look.argb >> 8u) & 0xFFu),
	                        static_cast<float>(look.argb & 0xFFu), static_cast<float>(look.argb >> 24u)};
	const glm::vec4 slide {look.uvOffset, look.textureFraction};
	const glm::vec4 added {static_cast<float>((addedRgb >> 16u) & 0xFFu), static_cast<float>((addedRgb >> 8u) & 0xFFu),
	                       static_cast<float>(addedRgb & 0xFFu), 0.0f};
	const auto* program = _shaderManager->GetShader("VolumeGlow");
	// How much the projection widens the camera's frame across and up, which weighs how side on the shell is seen
	const auto& projection = camera.GetProjectionMatrix();
	const glm::vec4 shape {reach, 0.0f, projection[0][0], projection[1][1]};
	// Added to what is behind with its alpha (mode 13), no depth write; each band seen from its front only, as the model
	const auto state = render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite,
	                                       {.cull = render_modes::Cull::Ccw, .zWrite = false, .msaa = true});
	const auto& subMeshes = mesh->GetSubMeshes();
	for (uint32_t i = 0; i < subMeshes.size(); ++i)
	{
		// The parts of the nearest detail in their first state, as for a particle effect's shell
		const auto flags = subMeshes[i]->GetFlags();
		if ((flags.lodMask & 1u) == 0 || flags.status != 0 || flags.hasBones != 0)
		{
			continue;
		}
		const auto* band = GlowShellOf(meshId, i);
		if (band == nullptr || band->indexCount == 0)
		{
			continue;
		}
		program->SetUniformValue("u_glowShape", &shape);
		program->SetUniformValue("u_glowColour", &colour);
		program->SetUniformValue("u_glowTexture", &slide);
		program->SetUniformValue("u_glowAdded", &added);
		program->SetTextureSampler("s_diffuse", 0, texture);
		program->SetTextureSampler("s_alpha", 1, alpha);
		bgfx::setTransform(glm::value_ptr(model));
		band->vertices->Bind();
		band->triangles->Bind(band->indexCount);
		bgfx::setState(state);
		program->Submit(static_cast<bgfx::ViewId>(viewId));
	}
	// A uniform keeps its last value: the other shells, which never set it, add none
	if (addedRgb != 0)
	{
		const glm::vec4 none {0.0f};
		program->SetUniformValue("u_glowAdded", &none);
	}
}

void Renderer::CollectCreedGlows(const Camera& camera) const
{
	_frameCreedGlows.clear();
	if (_frameCreatures.empty() || !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto creedMesh = resources::HashIdentifier(MeshId::Creed);
	if (!meshes.Contains(creedMesh))
	{
		return;
	}
	const auto box = meshes.Handle(creedMesh)->GetBoundingBox();
	const float halfDiagonal = ecs::object::MeshHalfDiagonal(creedMesh);
	const auto viewProjection = camera.GetViewProjectionMatrix();
	const auto cameraOrigin = camera.GetOrigin();
	for (const auto& body : _frameCreatures)
	{
		const auto* glows = registry.TryGet<const ecs::components::CreatureHandGlows>(body.entity);
		if (glows == nullptr)
		{
			continue;
		}
		for (const auto& placed : glows->placed)
		{
			// Each glow queued where it is, unless it is off the screen or too far away for its size
			if (placed.has_value() && CreedGlowShown(*placed, viewProjection, cameraOrigin, box, halfDiagonal))
			{
				_frameCreedGlows.push_back(*placed);
			}
		}
	}
}

std::vector<std::pair<float, creature_hand_glow::Placed>> Renderer::FallingCreatureGlows(const Camera& camera,
                                                                                         entt::entity copy, glm::vec3 eye) const
{
	std::vector<std::pair<float, creature_hand_glow::Placed>> shown;
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return shown;
	}
	const auto* glows =
	    std::as_const(Locator::entitiesRegistry::value()).TryGet<const ecs::components::CreatureHandGlows>(copy);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto creedMesh = resources::HashIdentifier(MeshId::Creed);
	if (glows == nullptr || !meshes.Contains(creedMesh))
	{
		return shown;
	}
	// Queued as the world's glows are: on the screen and near enough
	const auto box = meshes.Handle(creedMesh)->GetBoundingBox();
	const float halfDiagonal = ecs::object::MeshHalfDiagonal(creedMesh);
	const auto viewProjection = camera.GetViewProjectionMatrix();
	for (const auto& placed : glows->placed)
	{
		if (placed.has_value() && CreedGlowShown(*placed, viewProjection, eye, box, halfDiagonal))
		{
			shown.emplace_back(zsort::Key(glm::vec3(placed->model[3]), eye, zsort::SumOrder::XYZ), *placed);
		}
	}
	return shown;
}

void Renderer::DrawCreedGlow(RenderPass viewId, const Camera& camera, uint32_t index) const
{
	if (index >= _frameCreedGlows.size())
	{
		return;
	}
	DrawCreedGlowAt(viewId, camera, _frameCreedGlows[index]);
}

void Renderer::DrawCreedGlowAt(RenderPass viewId, const Camera& camera, const creature_hand_glow::Placed& glow) const
{
	const auto creedMesh = resources::HashIdentifier(MeshId::Creed);
	// White, in the glows' texture, its slide the glow's own; it reaches out by the model's half diagonal times the zoom
	const auto [texture, alphaTexture] = RawTextureIds(particles::TextureBaseName(
	    std::string(particles::volume_glow::k_Textures.at(static_cast<size_t>(creature_hand_glow::k_Texture)))));
	const particles::draw::ShellDraw look {
	    .zoom = glow.zoom,
	    .argb = creature_hand_glow::k_Argb,
	    .texture = texture,
	    .alphaTexture = alphaTexture,
	    .uvOffset = glow.slide,
	    .textureFraction = creature_hand_glow::k_TextureFraction,
	};
	DrawGlowShellAt(viewId, camera, creedMesh, glow.model, ecs::object::MeshHalfDiagonal(creedMesh) * glow.zoom, look);
}

void Renderer::CollectHeartShells(const DrawSceneDesc& desc) const
{
	_frameHeartShells.clear();
	if (!Locator::entitiesRegistry::has_value() || !Locator::rendereringSystem::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& context = Locator::rendereringSystem::value().GetContext();
	std::unordered_set<entt::entity> shown;
	registry.Each<const ecs::components::Temple>([&](entt::entity heart, const ecs::components::Temple& temple) {
		if (!temple.shellShown)
		{
			return;
		}
		shown.insert(heart);
		const auto placed = context.entityInstances.find(heart);
		const bool hasRow = placed != context.entityInstances.end() && placed->second.index < context.instanceUniforms.size();
		// From a little after the explosion the heart itself is no longer drawn, and has no row, but its shell still is
		const auto placement =
		    heart_shell::PlacementOf(hasRow, !temple_destruction::HeartDrawn(temple.destroying, temple.destructionClock));
		if (placement == heart_shell::Placement::None)
		{
			return;
		}
		// At the heart as it is drawn, swelling and reaching out along the destruction's clock; the reach beats with the
		// owner's heart, which only the local player's has (none for another player's)
		const float clock = temple.destructionClock;
		const float pulse = Locator::heartbeatSystem::has_value()
		                        ? Locator::heartbeatSystem::value().Pulse(temple.owner, desc.clock.turnFraction)
		                        : 0.0f;
		const auto point = placement == heart_shell::Placement::AtHeart
		                       ? glm::vec3(context.instanceUniforms[placed->second.index][3])
		                       : ecs::DrawnPosition(registry, heart);
		_frameHeartShells.push_back({.heart = heart,
		                             .model = heart_shell::Model(point, heart_shell::Size(clock)),
		                             .reach = heart_shell::Reach(clock, pulse),
		                             .argb = particles::surf_revol::PlayerColour(static_cast<int>(temple.owner)),
		                             .creationStamp = temple.creationStamp});
	});
	// The hearts are drawn newest first, and their shells with them
	std::vector<uint32_t> stamps;
	stamps.reserve(_frameHeartShells.size());
	std::ranges::transform(_frameHeartShells, std::back_inserter(stamps), &HeartShellDraw::creationStamp);
	std::vector<HeartShellDraw> ordered;
	ordered.reserve(_frameHeartShells.size());
	for (const auto index : heart_shell::DrawOrder(stamps))
	{
		ordered.push_back(_frameHeartShells.at(index));
	}
	_frameHeartShells = std::move(ordered);
	// A shell shown again starts its texture where a new one does
	std::erase_if(_heartShellSlides, [&shown](const auto& slide) { return !shown.contains(slide.first); });
}

void Renderer::DrawHeartShell(RenderPass viewId, const Camera& camera, const HeartShellDraw& shell, uint32_t frameGameMs) const
{
	// Its texture slides on by the frame's game time each time it is drawn
	auto& slide = _heartShellSlides[shell.heart];
	slide = glow_shell::Slide(slide, heart_shell::k_SlideSpeed, frameGameMs);
	if (shell.reach == 0.0f)
	{
		return;
	}
	const auto creedMesh = resources::HashIdentifier(MeshId::Creed);
	const auto [texture, alphaTexture] = RawTextureIds(particles::TextureBaseName(
	    std::string(particles::volume_glow::k_Textures.at(static_cast<size_t>(heart_shell::k_Texture)))));
	const particles::draw::ShellDraw look {
	    .zoom = shell.reach,
	    .argb = shell.argb,
	    .texture = texture,
	    .alphaTexture = alphaTexture,
	    .uvOffset = slide,
	    .textureFraction = heart_shell::k_TextureFraction,
	};
	// with the grey the original adds at the shell's inner edge
	DrawGlowShellAt(viewId, camera, creedMesh, shell.model, ecs::object::MeshHalfDiagonal(creedMesh) * shell.reach, look,
	                heart_shell::k_Added);
}
