/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The creatures' bodies, hair and eyes. Each body is the instanced model of its row, drawn with its own pose (ECS/
// Animations.h), its shape blended from its species' meshes in the morphing program, and its painted skins; its hair
// and eyes follow it at once. What to draw comes from Graphics/CreatureDraw.h, collected once a frame. On a land
// without a creature the list is empty and none of this is reached.

#include <cstring>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "Creature/CreatureFizzLook.h"
#include "Creature/CreatureTattoo.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Haze.h"
#include "Graphics/RenderModes.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{

/// A colour's red, green and blue, 0 to 255
glm::ivec3 Rgb(uint32_t argb)
{
	return {static_cast<int>((argb >> 16u) & 0xFFu), static_cast<int>((argb >> 8u) & 0xFFu), static_cast<int>(argb & 0xFFu)};
}

/// The static a creature fizzing out of sight is drawn through: its alpha, which the game pairs with S_Static.raw
constexpr entt::id_type k_StaticAlpha = entt::hashed_string("raw/S_Statica").value();
/// The game's milliseconds to seconds, in single precision
constexpr float k_SecondsPerMs = 0.001f;
} // namespace

void Renderer::CollectCreatures() const
{
	_frameCreatures.clear();
	if (!Locator::entitiesRegistry::has_value() || !Locator::rendereringSystem::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	// through the const registry: a land without a creature gains no storage
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& meshes = Locator::resources::value().GetMeshes();
	_frameCreatures = creature_draw::Bodies(registry, Locator::rendereringSystem::value().GetContext().entityInstances,
	                                        [&meshes](entt::id_type id) -> std::optional<size_t> {
		                                        if (!meshes.Contains(id))
		                                        {
			                                        return std::nullopt;
		                                        }
		                                        return meshes.Handle(id)->GetBoneMatrices().size();
	                                        });
	UpdateCreatureSkins(registry);
	// the one static the creatures' eyes fizz through, as it has slid this frame
	_frameEyeStatic =
	    Locator::creatureFizzSystem::has_value() ? Locator::creatureFizzSystem::value().EyeStaticScroll() : glm::vec2(0.0f);
	// a creature gone takes the slide of its static with it
	std::erase_if(_creatureStatic, [&registry](const auto& entry) { return !registry.Valid(entry.first); });
}

void Renderer::UpdateCreatureSkins(const ecs::Registry& registry) const
{
	_creatureSkins.Update(
	    registry,
	    []() -> std::unique_ptr<Texture2D> {
		    auto texture = std::make_unique<Texture2D>("Creature Skin");
		    texture->Create(creature_tattoo::k_SkinSize, creature_tattoo::k_SkinSize, 1, TextureFormat::BGRA4, Wrapping::Repeat,
		                    Filter::Linear, nullptr);
		    // out of textures: the skin keeps its species' own
		    return bgfx::isValid(toBgfx(texture->GetNativeHandle())) ? std::move(texture) : nullptr;
	    },
	    [](Texture2D& texture, std::span<const uint16_t> texels) {
		    texture.Update(texels.data(), static_cast<uint32_t>(texels.size_bytes()));
	    });
}

bool Renderer::HasCreature(uint32_t offset, uint32_t count) const
{
	if (_frameCreatures.empty())
	{
		return false;
	}
	const auto first = std::ranges::lower_bound(_frameCreatures, offset, {}, &creature_draw::Body::instance);
	return first != _frameCreatures.end() && first->instance < offset + count;
}

void Renderer::UseCreatureBody(const creature_draw::Body& body, L3DMeshSubmitDesc& desc) const
{
	desc.paintedSkins = body.paintedSkins;
	if (body.morph.has_value())
	{
		desc.morphTargets = &*body.morph;
		desc.program = _shaderManager->GetShader("ObjectMorphInstanced");
	}
}

void Renderer::DrawCreatureBody(const L3DMesh& mesh, L3DMeshSubmitDesc& desc, const creature_draw::Body& body,
                                uint32_t frameGameMs) const
{
	// Right out of sight, nothing of it is drawn; in full sight it is drawn as it is
	if (!creature_fizz_look::Drawn(body.fizz))
	{
		return;
	}
	if (!creature_fizz_look::Fizzing(body.fizz))
	{
		DrawMesh(mesh, desc, std::numeric_limits<uint8_t>::max());
		return;
	}
	// Each fizzed draw in the world's view slides its static on, before the static is drawn
	auto& scroll = _creatureStatic[body.entity];
	if (desc.viewId == RenderPass::Main)
	{
		scroll = creature_fizz_look::Scroll(scroll, static_cast<float>(frameGameMs) * k_SecondsPerMs);
	}
	DrawFizzed(mesh, desc, creature_fizz_look::Level(body.fizz), scroll);
}

void Renderer::DrawFizzed(const L3DMesh& mesh, L3DMeshSubmitDesc& desc, uint8_t level, glm::vec2 scroll) const
{
	const auto* program = SpellLookVariant(desc.program);
	const auto* staticAlpha = StaticAlphaTexture();
	if (program == nullptr || staticAlpha == nullptr)
	{
		// (openblack guard) without its program or the static, it is drawn as it is
		DrawMesh(mesh, desc, std::numeric_limits<uint8_t>::max());
		return;
	}
	const auto* ownProgram = desc.program;
	const creature_draw::PassState own {
	    .mode = desc.mode,
	    .options = desc.options,
	    .table = desc.table,
	    .globalAlpha = desc.globalAlpha,
	};
	const auto backFaces = sea_pass::ForPass(desc.viewId).FaceCull(sea_pass::Surface::Model, false, desc.sea.unmirror);
	desc.program = program;
	for (const auto& pass : creature_draw::FizzPasses(own, level, scroll, backFaces))
	{
		desc.mode = pass.state.mode;
		desc.options = pass.state.options;
		desc.table = pass.state.table;
		desc.globalAlpha = pass.state.globalAlpha;
		desc.fizz = L3DMeshSubmitDesc::Fizz {.spellLook = pass.spellLook, .staticAlpha = staticAlpha};
		DrawMesh(mesh, desc, std::numeric_limits<uint8_t>::max());
	}
	desc.fizz.reset();
	desc.program = ownProgram;
	desc.mode = own.mode;
	desc.options = own.options;
	desc.table = own.table;
	desc.globalAlpha = own.globalAlpha;
}

const ShaderProgram* Renderer::SpellLookVariant(const ShaderProgram* program) const
{
	// Only the creature's own programs have a variant: the body's, morphed or not, and the eyes'. Creatures are never
	// drawn with the program that follows the land (ObjectHeightMapInstanced), so it has none, and a mesh drawn with it
	// would be drawn as it is (DrawFizzed's guard).
	if (_spellLookVariants.empty())
	{
		constexpr std::array<std::pair<const char*, const char*>, 2> k_Variants {{
		    {"ObjectMorphInstanced", "ObjectMorphInstancedSpellLook"},
		    {"ObjectInstanced", "ObjectInstancedSpellLook"},
		}};
		for (const auto& [own, variant] : k_Variants)
		{
			_spellLookVariants.emplace(_shaderManager->GetShader(own), _shaderManager->GetShader(variant));
		}
	}
	const auto found = _spellLookVariants.find(program);
	return found != _spellLookVariants.end() ? found->second : nullptr;
}

const Texture2D* Renderer::StaticAlphaTexture() const
{
	const auto& textures = Locator::resources::value().GetTextures();
	return textures.Contains(k_StaticAlpha) ? &*textures.Handle(k_StaticAlpha) : nullptr;
}

void Renderer::BindMorphTargets(const L3DMesh& mesh, const L3DSubMesh& subMesh,
                                const creature_draw::MorphTargets& targets) const
{
	if (!_morphStreamLayouts.front().IsValid())
	{
		// each variant's position and normal as the attributes vs_object_morph_instanced takes them in, the rest of the
		// vertex skipped: L3DSubMesh packs a position, texture coordinates, a normal and two 16-bit bone indices
		constexpr std::array<std::pair<bgfx::Attrib::Enum, bgfx::Attrib::Enum>, 3> k_Attributes {{
		    {bgfx::Attrib::Tangent, bgfx::Attrib::Bitangent},
		    {bgfx::Attrib::TexCoord1, bgfx::Attrib::TexCoord2},
		    {bgfx::Attrib::Color1, bgfx::Attrib::Weight},
		}};
		constexpr uint8_t k_TexCoordBytes = 2 * sizeof(float);
		constexpr uint8_t k_IndicesBytes = 2 * sizeof(int16_t);
		for (size_t axis = 0; axis < k_Attributes.size(); ++axis)
		{
			bgfx::VertexLayout layout;
			layout.begin()
			    .add(k_Attributes.at(axis).first, 3, bgfx::AttribType::Float)
			    .skip(k_TexCoordBytes)
			    .add(k_Attributes.at(axis).second, 3, bgfx::AttribType::Float)
			    .skip(k_IndicesBytes)
			    .end();
			_morphStreamLayouts.at(axis).Reset(bgfx::createVertexLayout(layout));
		}
	}
	const auto& subMeshes = mesh.GetSubMeshes();
	const auto found = std::ranges::find_if(subMeshes, [&subMesh](const auto& other) { return other.get() == &subMesh; });
	const auto index = static_cast<size_t>(std::distance(subMeshes.begin(), found));
	const auto& base = subMesh.GetMesh().GetVertexBuffer();
	const auto& meshes = Locator::resources::value().GetMeshes();
	for (size_t axis = 0; axis < targets.meshes.size(); ++axis)
	{
		const VertexBuffer* buffer = &base;
		if (meshes.Contains(targets.meshes.at(axis)))
		{
			const auto target = meshes.Handle(targets.meshes.at(axis));
			if (index < target->GetSubMeshes().size())
			{
				const auto& candidate = target->GetSubMeshes()[index]->GetMesh().GetVertexBuffer();
				if (candidate.GetCount() == base.GetCount() && candidate.GetStrideBytes() == base.GetStrideBytes())
				{
					buffer = &candidate;
				}
			}
		}
		buffer->BindStream(static_cast<uint8_t>(axis + 1), fromBgfx(_morphStreamLayouts.at(axis).Get()));
	}
}

void Renderer::BindBlendSources(const L3DMesh& mesh, const L3DSubMesh& subMesh, const creature_draw::MorphTargets& targets,
                                const ShaderProgram& program) const
{
	// the seams are blended only where the GPU can read RGBA32F textures in the vertex shader; elsewhere they are drawn
	// as before, unblended, and nothing else of the draw changes
	static const bool s_vertexFetch =
	    (bgfx::getCaps()->formats[bgfx::TextureFormat::RGBA32F] & BGFX_CAPS_FORMAT_TEXTURE_VERTEX) != 0;
	const auto* base = s_vertexFetch ? subMesh.GetBlendSource() : nullptr;
	const auto width = base != nullptr ? static_cast<float>(base->GetResolution().x) : 1.0f;
	const glm::vec4 u_vertexBlend {base != nullptr ? 1.0f : 0.0f, width, 0.0f, 0.0f};
	program.SetUniformValue("u_vertexBlend", &u_vertexBlend); // vs
	if (base == nullptr)
	{
		return;
	}
	const auto& subMeshes = mesh.GetSubMeshes();
	const auto found = std::ranges::find_if(subMeshes, [&subMesh](const auto& other) { return other.get() == &subMesh; });
	const auto index = static_cast<size_t>(std::distance(subMeshes.begin(), found));
	constexpr std::array<std::pair<const char*, uint8_t>, 3> k_Samplers {{
	    {"s_blendEvilGood", 7},
	    {"s_blendThinFat", 8},
	    {"s_blendWeakStrong", 9},
	}};
	program.SetTextureSampler("s_blendBase", 2, *base); // vs
	const auto& meshes = Locator::resources::value().GetMeshes();
	for (size_t axis = 0; axis < targets.meshes.size(); ++axis)
	{
		const Texture2D* source = base;
		if (meshes.Contains(targets.meshes.at(axis)))
		{
			const auto target = meshes.Handle(targets.meshes.at(axis));
			if (index < target->GetSubMeshes().size())
			{
				const auto* candidate = target->GetSubMeshes()[index]->GetBlendSource();
				if (candidate != nullptr && candidate->GetResolution() == base->GetResolution())
				{
					source = candidate;
				}
			}
		}
		program.SetTextureSampler(k_Samplers.at(axis).first, k_Samplers.at(axis).second, *source); // vs
	}
}

const Texture2D* Renderer::SkinTexture(const L3DMesh& mesh, uint32_t skinId, entt::entity paintedSkins) const
{
	if (paintedSkins != entt::null)
	{
		if (const auto* painted = _creatureSkins.Find(paintedSkins, skinId); painted != nullptr)
		{
			return painted;
		}
	}
	return world_triangles::PrimitiveTexture(mesh, skinId);
}

void Renderer::DrawCreatureParts(RenderPass viewId, const Camera& camera, const ecs::systems::RenderContext& context,
                                 const creature_draw::Body& body) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();

	// The hair first, at the end of the body's own draw (after the shadows over it), then the eyes, as the original
	// does. The hair: ribbons facing the camera, in the land's light under the creature with the land's colour and the
	// haze added; only until the creature has fizzed or frozen a fifth of the way
	if (const auto* hair = registry.TryGet<const ecs::components::CreatureHair>(body.entity);
	    hair != nullptr && !hair->groups.empty() && creature_fizz_look::HairShown(body.fizz, body.freeze))
	{
		const auto origin = glm::vec3(context.instanceUniforms.at(body.instance)[3]);
		glm::ivec3 light {255};
		glm::ivec3 added {0};
		if (IsLandLit())
		{
			const auto sample = land_light::At(land_light::CurrentTable(), glm::vec2(origin.x, origin.z));
			uint32_t diffuse = sample.diffuse;
			const auto specular = haze::ApplyObject(
			    _haze, haze::Depth(camera.GetViewMatrix(Camera::Interpolation::Current), origin), sample.specular, &diffuse);
			light = Rgb(diffuse);
			added = Rgb(specular);
		}
		const auto drawn = creature_draw::BuildHair(*hair, camera.GetOrigin(), light, added);
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

	// The eyes: each eyeball, then its eyelid in the colour of the skin under it times the body's colour (the land's
	// light under the body, hazed), set on the lid instead of the land's light: once for the first lid drawn and twice
	// for the second, with the body's specular. They are placed in the world but drawn with the body's row, so each is
	// first taken into the body's space. Not frozen, they fizz
	// with the body, through the one static all the creatures' eyes share, and right out of sight they are not drawn.
	// Frozen, an eyeball takes the body's row and so its frozen tint, and an eyelid its own colour times that tint.
	// (The frozen eyes' ice sheen is not drawn in openblack.)
	const bool eyesFizz = creature_fizz_look::EyesFizz(body.fizz, body.freeze);
	const bool eyesGone = body.freeze == 0.0f && body.fizz != 0.0f && !creature_fizz_look::Drawn(body.fizz);
	if (const auto* eyes = registry.TryGet<const ecs::components::CreatureEyes>(body.entity); eyes != nullptr && !eyesGone)
	{
		L3DMeshSubmitDesc submit {};
		submit.viewId = viewId;
		submit.program = _shaderManager->GetShader("ObjectInstanced");
		submit.options = render_modes::k_ModelPass;
		const auto& bodyRow = context.instanceUniforms.at(body.instance);
		uint32_t bodyColour = 0xFFFFFFFFu;
		uint32_t bodySpecular = 0xFF000000u;
		if (IsLandLit())
		{
			const auto origin = glm::vec3(bodyRow[3]);
			const auto sample = land_light::At(land_light::CurrentTable(), glm::vec2(origin.x, origin.z));
			bodyColour = sample.diffuse;
			bodySpecular = haze::ApplyObject(_haze, haze::Depth(camera.GetViewMatrix(Camera::Interpolation::Current), origin),
			                                 sample.specular, &bodyColour);
		}
		for (const auto& part : creature_draw::Eyes(*eyes, [&meshes](entt::id_type id) { return meshes.Contains(id); }))
		{
			const auto local = creature_draw::InBodySpace(bodyRow, part.model);
			submit.modelMatrices = &local;
			submit.matrixCount = 1;
			bgfx::InstanceDataBuffer row {};
			submit.transientInstance = nullptr;
			submit.instanceDesc = std::make_unique<InstanceDesc>(context.instanceUniformBuffer, body.instance, 1);
			if (part.tint.has_value())
			{
				constexpr auto k_Stride = static_cast<uint16_t>(sizeof(creature_draw::InstanceRow));
				if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
				{
					continue; // (openblack guard) no room left this frame
				}
				bgfx::allocInstanceDataBuffer(&row, 1, k_Stride);
				const auto drawn =
				    creature_draw::EyeRow(context.instanceUniforms.at(body.instance), context.instanceColours.at(body.instance),
				                          creature_draw::FrozenLidColour(
				                              creature_draw::LidColour(*part.tint, bodyColour, part.lidTimes), body.freeze),
				                          bodySpecular);
				std::memcpy(row.data, drawn.data(), sizeof(drawn));
				submit.transientInstance = &row;
			}
			if (eyesFizz)
			{
				DrawFizzed(*meshes.Handle(part.mesh), submit, creature_fizz_look::Level(body.fizz), _frameEyeStatic);
			}
			else
			{
				DrawMesh(*meshes.Handle(part.mesh), submit, std::numeric_limits<uint8_t>::max());
			}
		}
	}
}
