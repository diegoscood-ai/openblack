/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RenderingSystem.h"

#include <glm/gtx/transform.hpp>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/NightLights.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Fields.h"
#include "ECS/Trees.h"
#include "ECS/Components/MeshTint.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Tree.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Life.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Graphics/DebugLines.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderManager.h"
#include "Game.h"
#include "Locator.h"
#include "PSys/Creators/Mesh.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

RenderingSystem::~RenderingSystem() = default;

namespace
{
/// The mesh atoms of the particle effects this frame (PSys/Creators/Mesh.h), drawn as instances of their mesh
std::vector<openblack::psys::mesh_atoms::Instance> g_PSysMeshes;

/// The original bakes a shadow for every Fixed and MobileObject (SetShadowOnTexture in Create3DObject 0x52DE30 /
/// 0x607210), trees and forests included, except the classes that turn it off (AnimatedStatic, DeadTree, Pot, fields,
/// ...); villagers and the creature have blob / dynamic shadows instead.
bool CastsStaticShadow(const openblack::ecs::Registry& registry, entt::entity entity)
{
	if (!registry.AnyOf<Fixed, MobileStatic, MobileObject, Tree, Abode, Feature, BigForest>(entity) ||
	    registry.AnyOf<Pot, AnimatedStatic, DeadTree, Field, Villager, Creature, Hand, Alpha, TempleInteriorPart>(entity))
	{
		return false;
	}
	// The baker (fn_008721A0) takes its casters from the map cells: an object in the hand (fn_005DC330) or in physics
	// (Object::InitialisePhysics*) has left them until it lands (EndPhysics), so it casts none meanwhile. (The original
	// re-bakes the blocks only for Fixed types; the old shadow of a tree or MobileObject lingers until something else
	// re-bakes that block. Not reproduced: openblack redraws the static shadows every frame.)
	if (openblack::Locator::handSystem::has_value())
	{
		const auto held = openblack::Locator::handSystem::value().GetHeldObject();
		if (held.has_value() && *held == entity)
		{
			return false;
		}
	}
	return !openblack::ecs::physics::PhysicsObjects::IsFlying(entity);
}
/// A broken building keeps the static shadow of its intact mesh (the FragMesh casts none); fragments cast none either
/// (Fragment: SetShadowOnTexture(0)), which CastsStaticShadow already leaves out.
entt::id_type ShadowMeshOf(const openblack::ecs::Registry& registry, entt::entity entity, entt::id_type drawn)
{
	const auto* damage = registry.TryGet<const openblack::ecs::components::BuildingDamage>(entity);
	return damage != nullptr && damage->intactMesh != 0 ? damage->intactMesh : drawn;
}
/// Object::Create3DObject (0x6365F0) turns the dynamic shadow on for every game object; trees (0x749FA3), forests
/// (0x439098), flowers, magic food (0x5FAAC8), the food in the hand (pot info 12, 0x66D180) and a few others turn it off.
bool ReceivesDynamicShadow(const openblack::ecs::Registry& registry, entt::entity entity)
{
	if (registry.AnyOf<Tree, DeadTree, BigForest, Forest, Hand, TempleInteriorPart>(entity))
	{
		return false;
	}
	const auto* pot = registry.TryGet<const Pot>(entity);
	return pot == nullptr || (pot->type != openblack::PotInfo::HandFood && pot->type != openblack::PotInfo::MagicFood);
}
} // namespace


void RenderingSystem::PrepareDrawDescs(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Count number of instances
	uint32_t instanceCount = 0;
	std::unordered_map<entt::id_type, std::pair<uint32_t, bool>> meshIds;
	std::unordered_map<entt::id_type, uint32_t> translucentIds;
	// fading meshes that follow the land (fields, piles) keep doing it while they fade (per mesh: any of its entities)
	std::unordered_map<entt::id_type, bool> translucentMorph;

	auto prep = [&meshIds, &instanceCount](const Mesh& mesh, bool morphWithTerrain) {
		auto count = meshIds.insert(std::make_pair(mesh.id, std::make_pair(mesh.submeshId, morphWithTerrain)));
		count.first->second.first++;
		instanceCount++;
	};

	registry.Each<const Mesh, const Transform>([&prep](const Mesh& mesh, const Transform& /*unused*/) { prep(mesh, false); },
	                                           entt::exclude<MorphWithTerrain, TempleInteriorPart, Alpha>);
	registry.Each<const Mesh, const Transform, const MorphWithTerrain>(
	    [&prep](const Mesh& mesh, const Transform& /*unused*/, const MorphWithTerrain& /*unused*/) { prep(mesh, true); },
	    entt::exclude<Alpha>);
	registry.Each<const Mesh, const Transform, const Alpha>(
	    [&registry, &translucentIds, &translucentMorph, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                    const Transform& /*unused*/, const Alpha& /*unused*/) {
		    ++translucentIds[mesh.id];
		    translucentMorph[mesh.id] = translucentMorph[mesh.id] || registry.AllOf<MorphWithTerrain>(entity);
		    ++instanceCount;
	    },
	    entt::exclude<TempleInteriorPart>);

	// ParticleMeshCreator atoms (Particle3DObj::DrawAt 0x679FD0): opaque ones with the meshes, translucent ones with the
	// fading meshes
	g_PSysMeshes = openblack::psys::mesh_atoms::Collect();
	std::erase_if(g_PSysMeshes, [](const auto& atom) {
		return !openblack::Locator::resources::value().GetMeshes().Contains(atom.meshId);
	});
	for (const auto& atom : g_PSysMeshes)
	{
		if (atom.translucent)
		{
			++translucentIds[atom.meshId];
			translucentMorph.try_emplace(atom.meshId, false);
		}
		else
		{
			auto count = meshIds.insert(std::make_pair(atom.meshId, std::make_pair(0u, false)));
			count.first->second.first++;
		}
		++instanceCount;
	}

	std::unordered_map<entt::id_type, uint32_t> shadowCasterIds;
	registry.Each<const Mesh, const Transform>([&registry, &shadowCasterIds, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                                        const Transform& /*unused*/) {
		if (CastsStaticShadow(registry, entity))
		{
			++shadowCasterIds[ShadowMeshOf(registry, entity, mesh.id)];
			++instanceCount;
		}
	});

	if (drawBoundingBox)
	{
		instanceCount *= 2;
	}

	// Recreate instancing uniform buffer if it is too small
	if (_renderContext.instanceUniforms.size() < instanceCount)
	{
		if (bgfx::isValid(toBgfx(_renderContext.instanceUniformBuffer)))
		{
			bgfx::destroy(toBgfx(_renderContext.instanceUniformBuffer));
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .end();
		// Grow with headroom: particles change the instance count every frame, and each resize reallocates.
		const auto capacity = instanceCount + instanceCount / 2 + 256;
		_renderContext.instanceUniformBuffer = graphics::fromBgfx(bgfx::createDynamicVertexBuffer(capacity, layout));
		_renderContext.instanceUniforms.resize(capacity);
	}

	// Determine uniform buffer offsets and instance count for draw
	uint32_t offset = 0;
	_renderContext.instancedDrawDescs.clear();
	for (const auto& [meshId, desc] : meshIds)
	{
		_renderContext.instancedDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                          std::forward_as_tuple(offset, desc.first, desc.second));
		offset += desc.first;
	}
	_renderContext.translucentDrawDescs.clear();
	_renderContext.additiveInstances.clear();
	for (const auto& [meshId, count] : translucentIds)
	{
		_renderContext.translucentDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                            std::forward_as_tuple(offset, count, translucentMorph[meshId]));
		offset += count;
	}
	_renderContext.shadowCasterDrawDescs.clear();
	for (const auto& [meshId, count] : shadowCasterIds)
	{
		_renderContext.shadowCasterDrawDescs.emplace(std::piecewise_construct, std::forward_as_tuple(meshId),
		                                             std::forward_as_tuple(offset, count, false));
		offset += count;
	}
}

void RenderingSystem::PrepareDrawUploadUniforms(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Store offsets of uniforms for descs
	std::map<entt::id_type, uint32_t> uniformOffsets;
	std::map<entt::id_type, uint32_t> translucentOffsets;
	std::map<entt::id_type, uint32_t> shadowCasterOffsets;
	_renderContext.entityInstances.clear();
	_renderContext.sortPoints.clear();

	// Set transforms for instanced draw at offsets
	registry.Each<const Mesh, const Transform>(
	    [this, &registry, &uniformOffsets, &translucentOffsets, &shadowCasterOffsets,
	     drawBoundingBox](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    const auto* alpha = registry.TryGet<const Alpha>(entity);
		    auto offset = (alpha != nullptr ? translucentOffsets : uniformOffsets).insert(std::make_pair(mesh.id, 0));
		    auto desc = (alpha != nullptr ? _renderContext.translucentDrawDescs : _renderContext.instancedDrawDescs).find(mesh.id);

		    // villagers and animals are drawn where ECS/MobileDrawing puts them this frame (between turns, turning, on the slope)
		    const auto* draw = registry.TryGet<const DrawPosition>(entity);
		    const auto& drawRotation = draw != nullptr ? draw->rotation : transform.rotation;
		    const auto& drawPosition = draw != nullptr ? draw->position : transform.position;
		    auto modelMatrix = glm::mat4(drawRotation);
		    modelMatrix = glm::translate(modelMatrix, drawPosition * drawRotation);
		    modelMatrix = glm::scale(modelMatrix, transform.scale);
		    // the one-shot orb is drawn turned to the camera (fn_00518720, Magic/Core/OneOffSpellSeed.cpp)
		    if (const auto* orb = registry.TryGet<const OneOffSpellSeed>(entity); orb != nullptr)
		    {
			    modelMatrix = glm::scale(glm::translate(transform.position + orb->facingOffset) * glm::mat4(orb->facing),
			                             transform.scale);
			    if (alpha != nullptr)
			    {
				    _renderContext.sortPoints.insert_or_assign(desc->second.offset + offset.first->second, orb->sortPoint);
			    }
		    }
		    else if (draw != nullptr)
		    {
			    modelMatrix[0] += draw->shearX * modelMatrix[1];
			    modelMatrix[2] += draw->shearZ * modelMatrix[1];
		    }

		    const uint32_t idx = desc->second.offset + offset.first->second;
		    _renderContext.instanceUniforms[idx] = modelMatrix;
		    _renderContext.entityInstances.insert_or_assign(
		        entity, RenderContext::EntityInstance {mesh.id, idx, registry.AllOf<MorphWithTerrain>(entity),
		                                               ReceivesDynamicShadow(registry, entity)});
		    if (CastsStaticShadow(registry, entity))
		    {
			    const auto casterMesh = ShadowMeshOf(registry, entity, mesh.id);
			    auto casterOffset = shadowCasterOffsets.insert(std::make_pair(casterMesh, 0));
			    const auto casterDesc = _renderContext.shadowCasterDrawDescs.find(casterMesh);
			    if (casterDesc != _renderContext.shadowCasterDrawDescs.end())
			    {
				    _renderContext.instanceUniforms[casterDesc->second.offset + casterOffset.first->second] = modelMatrix;
				    casterOffset.first->second++;
			    }
		    }
		    if (alpha != nullptr)
		    {
			    _renderContext.instanceUniforms[idx][0][3] = 1.0f - glm::clamp(alpha->value, 0.0f, 1.0f);
		    }
		    // The w of the second column carries the texture offset (components::UvScroll): v + 4 x u in 1/256 steps
		    if (const auto* scroll = registry.TryGet<const UvScroll>(entity); scroll != nullptr)
		    {
			    _renderContext.instanceUniforms[idx][1][3] = openblack::graphics::frame_anim::PackUvOffset(scroll->u, scroll->v);
		    }
		    // components::ObjectColour, SetColour 0x7F9770 (the power-up bands: DrawSpellGraphic 0x51A3BE with
		    // GetPlayerColour 0x64D800, PHandFX Band::Draw 0x68D86D..0x68D8B1): -1 - (r 65536 + g 256 + b) in the w of the
		    // third column, the PSys mesh atoms' encoding (vs_object)
		    if (const auto* colour = registry.TryGet<const ObjectColour>(entity); colour != nullptr)
		    {
			    _renderContext.instanceUniforms[idx][2][3] =
			        -1.0f - static_cast<float>(colour->rgb[0] * 65536 + colour->rgb[1] * 256 + colour->rgb[2]);
		    }
		    // The w of the third column: components::MeshTint, 1e6 (2e6 dissolving) + 5 bits each of the ground colour
		    // (r, g, b from the bottom) and of `own` (bits 15-19)
		    if (const auto* tint = registry.TryGet<const MeshTint>(entity); tint != nullptr)
		    {
			    const auto bits = [](float value) {
				    return static_cast<uint32_t>(std::clamp(value * 31.0f + 0.5f, 0.0f, 31.0f));
			    };
			    const auto packed = bits(tint->own) * 32768u + bits(tint->ground.r) * 1024u + bits(tint->ground.g) * 32u +
			                        bits(tint->ground.b);
			    _renderContext.instanceUniforms[idx][2][3] = (tint->dissolve ? 2e6f : 1e6f) + static_cast<float>(packed);
		    }
		    // Field::Draw 0x528570 (without the world.foliage tint): the object colour by growth in the w of the fourth
		    // column, negative (-1 - r 65536 - g 256 - b), and the ripe field's sway, a shear of its up axis along world
		    // z (only the drawn matrix: the original restores it after AddForDrawing)
		    if (const auto* field = registry.TryGet<const Field>(entity);
		        field != nullptr && !registry.AllOf<MeshTint>(entity))
		    {
			    const auto colour = ecs::FieldDrawColour(*field);
			    _renderContext.instanceUniforms[idx][3][3] =
			        -(1.0f + static_cast<float>(colour.r * 65536u + colour.g * 256u + colour.b));
			    if (field->growth >= Field::k_AgeRecolt)
			    {
				    // slot: bits 16-19 of the field's address in the original, any stable per-field number here
				    const auto slot = (static_cast<uint32_t>(entt::to_integral(entity)) * 2654435761u) >> 28u;
				    _renderContext.instanceUniforms[idx][1][0] = 0.0f;
				    _renderContext.instanceUniforms[idx][1][2] = transform.scale.y * 1.75f * ecs::WindSway(slot);
			    }
		    }
		    // Tree::Draw 0x74B016 (the tables of Tree::PreDraw 0x74A7C0): the wind sway, the up axis's x = scale x 0 and
		    // z = scale x the lean of the tree's slot (bits 2-5 of +0x5C), only the drawn matrix. Not while the tree is
		    // tilted (pulled or held by the hand). A tree bent away from a passing object (bits 6-9 of +0x5C, table
		    // 0xD19A48, worked out in ecs::UpdateTrees) draws that bend instead of the sway: the drawn matrix turned about
		    // its base, the crown leaning along the bend direction.
		    if (const auto* tree = registry.TryGet<const Tree>(entity);
		        tree != nullptr && tree->bendAngle != 0.0f && transform.rotation[1].x == 0.0f &&
		        transform.rotation[1].z == 0.0f)
		    {
			    const glm::vec3 away(tree->bendDirection.x, 0.0f, tree->bendDirection.y);
			    const auto bend =
			        glm::mat3(glm::rotate(glm::mat4(1.0f), tree->bendAngle, glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), away)));
			    auto& instance = _renderContext.instanceUniforms[idx];
			    for (int column = 0; column < 3; ++column)
			    {
				    const auto turned = bend * glm::vec3(instance[column]);
				    instance[column] = glm::vec4(turned, instance[column][3]);
			    }
			    if (!registry.AllOf<MeshTint>(entity))
			    {
				    const auto grey = static_cast<uint32_t>(ecs::TreeBrightness());
				    instance[3][3] = -(1.0f + static_cast<float>(grey * 65536u + grey * 256u + grey));
			    }
		    }
		    else if (const auto* swayTree = registry.TryGet<const Tree>(entity);
		        swayTree != nullptr && transform.rotation[1].x == 0.0f && transform.rotation[1].z == 0.0f)
		    {
			    // the tree's own slot, round(yAngle x 16 / 2pi) & 15 (0x74A0E7): trees facing the same way sway together
			    const auto slot = static_cast<uint32_t>(swayTree->windSlot);
			    // Tree::Draw 0x74B077: every RGB channel of the tree's colour times the frame's brightness / 256
			    // (ecs::TreeBrightness), as an own colour in the w of the fourth column like the fields' tint
			    if (!registry.AllOf<MeshTint>(entity))
			    {
				    const auto grey = static_cast<uint32_t>(ecs::TreeBrightness());
				    _renderContext.instanceUniforms[idx][3][3] = -(1.0f + static_cast<float>(grey * 65536u + grey * 256u + grey));
			    }
			    _renderContext.instanceUniforms[idx][1][0] = 0.0f;
			    _renderContext.instanceUniforms[idx][1][2] = transform.scale.y * ecs::WindSway(slot);
		    }
		    // Tree::Draw's fire part fn_0074B3A0 (a tree with a FireEffect, ECS/Fire/FireGraphic): its colour x the burnt
		    // grey (the object colour, as the field's), and below 0.2 life it shrinks to 5 x life across (the matrix rows
		    // 0 and 2, not its height)
		    if (registry.AnyOf<Tree, DeadTree>(entity))
		    {
			    if (const auto colour = ecs::fire::graphic::TreeDrawColour(entity); colour.has_value())
			    {
				    _renderContext.instanceUniforms[idx][3][3] =
				        -(1.0f + static_cast<float>(colour->r * 65536u + colour->g * 256u + colour->b));
				    const float life = ecs::life::LifeOf(entity);
				    if (life < 0.2f)
				    {
					    const float shrink = 1.0f - (0.2f - life) * 5.0f;
					    for (const int axis : {0, 2})
					    {
						    auto& column = _renderContext.instanceUniforms[idx][axis];
						    column = glm::vec4(glm::vec3(column) * shrink, column.w);
					    }
				    }
			    }
		    }
		    // The w of the fourth column: 2 + the grey of a house's lit windows at night (Abode::Draw), 1 otherwise
		    if (const auto* abode = registry.TryGet<const Abode>(entity); abode != nullptr && Game::Instance() != nullptr)
		    {
			    const float grey = night_lights::WindowGrey(Game::Instance()->GetDayNightClock(), transform.position,
			                                                !abode->inhabitants.empty());
			    if (grey >= 0.0f)
			    {
				    _renderContext.instanceUniforms[idx][3][3] = 2.0f + grey;
			    }
		    }
		    // Living +0xD0 (components::SpecularColour, the heal chakra's glow): 3e6 + 7 bits each of r, g, b in the same w,
		    // added to the land light's specular (fn_0080BF10). (aproximado) 7 bits per channel to fit the float, where
		    // fn_0080BF10 adds 8
		    if (const auto* specular = registry.TryGet<const SpecularColour>(entity); specular != nullptr)
		    {
			    const auto bits = [](uint8_t value) { return static_cast<uint32_t>(value) >> 1u; };
			    _renderContext.instanceUniforms[idx][3][3] =
			        3e6f + static_cast<float>(bits(specular->colour.r) * 16384u + bits(specular->colour.g) * 128u +
			                                  bits(specular->colour.b));
		    }
		    if (drawBoundingBox)
		    {
			    auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(mesh.id);
			    auto box = l3dMesh->GetBoundingBox();
			    auto boxMatrix = modelMatrix * glm::translate(box.Center()) * glm::scale(box.Size());
			    _renderContext.instanceUniforms[idx + _renderContext.instanceUniforms.size() / 2] = boxMatrix;
		    }
		    offset.first->second++;
	    },
	    entt::exclude<TempleInteriorPart>);

	// the particle effects' mesh atoms, after the entities of the same mesh
	for (const auto& atom : g_PSysMeshes)
	{
		auto& offsets = atom.translucent ? translucentOffsets : uniformOffsets;
		const auto& descs = atom.translucent ? _renderContext.translucentDrawDescs : _renderContext.instancedDrawDescs;
		const auto desc = descs.find(atom.meshId);
		if (desc == descs.end())
		{
			continue;
		}
		auto offset = offsets.insert(std::make_pair(atom.meshId, 0));
		const uint32_t idx = desc->second.offset + offset.first->second;
		_renderContext.instanceUniforms[idx] = atom.model;
		if (atom.translucent)
		{
			_renderContext.instanceUniforms[idx][0][3] = 1.0f - atom.alpha;
		}
		if (atom.additive)
		{
			_renderContext.additiveInstances.insert(idx);
		}
		// the DrawData colour (the creator's colour, x the player's for UsePlayerColor), packed as -1 - (r 65536 + g 256 +
		// b): with DrawWithLandscapeColor (fn_0080BEC0) in the w of the fourth column, the object colour vs_object multiplies
		// the land light by; otherwise in the w of the third column, the colour alone (SetColour vt 0x2C, vs_object)
		const float packed = -1.0f - static_cast<float>(atom.colour[0] * 65536 + atom.colour[1] * 256 + atom.colour[2]);
		_renderContext.instanceUniforms[idx][atom.landscapeColour ? 3 : 2][3] = packed;
		if (atom.uv != glm::vec2(0.0f))
		{
			_renderContext.instanceUniforms[idx][1][3] = openblack::graphics::frame_anim::PackUvOffset(atom.uv.x, atom.uv.y - std::floor(atom.uv.y));
		}
		offset.first->second++;
	}

	if (!_renderContext.instanceUniforms.empty())
	{
		const auto size = static_cast<uint32_t>(_renderContext.instanceUniforms.size() * sizeof(glm::mat4));
		// Copied, not referenced: bgfx reads the memory a frame later, after a resize may have freed it.
		bgfx::update(toBgfx(_renderContext.instanceUniformBuffer), 0, bgfx::copy(_renderContext.instanceUniforms.data(), size));
	}
}
