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

#include "3D/L3DMesh.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/MeshTint.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/DebugLines.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "Graphics/ShaderManager.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

RenderingSystem::~RenderingSystem() = default;

namespace
{
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

	std::unordered_map<entt::id_type, uint32_t> shadowCasterIds;
	registry.Each<const Mesh, const Transform>([&registry, &shadowCasterIds, &instanceCount](entt::entity entity, const Mesh& mesh,
	                                                                                        const Transform& /*unused*/) {
		if (CastsStaticShadow(registry, entity))
		{
			++shadowCasterIds[mesh.id];
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

	// Set transforms for instanced draw at offsets
	registry.Each<const Mesh, const Transform>(
	    [this, &registry, &uniformOffsets, &translucentOffsets, &shadowCasterOffsets,
	     drawBoundingBox](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		    const auto* alpha = registry.TryGet<const Alpha>(entity);
		    auto offset = (alpha != nullptr ? translucentOffsets : uniformOffsets).insert(std::make_pair(mesh.id, 0));
		    auto desc = (alpha != nullptr ? _renderContext.translucentDrawDescs : _renderContext.instancedDrawDescs).find(mesh.id);

		    auto modelMatrix = glm::mat4(transform.rotation);
		    modelMatrix = glm::translate(modelMatrix, transform.position * transform.rotation);
		    modelMatrix = glm::scale(modelMatrix, transform.scale);

		    const uint32_t idx = desc->second.offset + offset.first->second;
		    _renderContext.instanceUniforms[idx] = modelMatrix;
		    _renderContext.entityInstances.insert_or_assign(
		        entity, RenderContext::EntityInstance {mesh.id, idx, registry.AllOf<MorphWithTerrain>(entity),
		                                               ReceivesDynamicShadow(registry, entity)});
		    if (CastsStaticShadow(registry, entity))
		    {
			    auto casterOffset = shadowCasterOffsets.insert(std::make_pair(mesh.id, 0));
			    const auto casterDesc = _renderContext.shadowCasterDrawDescs.find(mesh.id);
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
		    // The w of the second column carries the texture V offset (components::UvScroll).
		    if (const auto* scroll = registry.TryGet<const UvScroll>(entity); scroll != nullptr)
		    {
			    _renderContext.instanceUniforms[idx][1][3] = scroll->v;
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

	if (!_renderContext.instanceUniforms.empty())
	{
		const auto size = static_cast<uint32_t>(_renderContext.instanceUniforms.size() * sizeof(glm::mat4));
		// Copied, not referenced: bgfx reads the memory a frame later, after a resize may have freed it.
		bgfx::update(toBgfx(_renderContext.instanceUniformBuffer), 0, bgfx::copy(_renderContext.instanceUniforms.data(), size));
	}
}
