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
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/DebugLines.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

RenderingSystem::~RenderingSystem() = default;

void RenderingSystem::PrepareDrawDescs(bool drawBoundingBox)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Count number of instances
	uint32_t instanceCount = 0;
	std::unordered_map<entt::id_type, std::pair<uint32_t, bool>> meshIds;
	std::unordered_map<entt::id_type, uint32_t> translucentIds;

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
	    [&translucentIds, &instanceCount](const Mesh& mesh, const Transform& /*unused*/, const Alpha& /*unused*/) {
		    ++translucentIds[mesh.id];
		    ++instanceCount;
	    },
	    entt::exclude<TempleInteriorPart>);

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

	// Set transforms for instanced draw at offsets
	registry.Each<const Mesh, const Transform>(
	    [this, &registry, &uniformOffsets, &translucentOffsets, drawBoundingBox](entt::entity entity, const Mesh& mesh,
	                                                                               const Transform& transform) {
		    const auto* alpha = registry.TryGet<const Alpha>(entity);
		    auto offset = (alpha != nullptr ? translucentOffsets : uniformOffsets).insert(std::make_pair(mesh.id, 0));
		    auto desc = (alpha != nullptr ? _renderContext.translucentDrawDescs : _renderContext.instancedDrawDescs).find(mesh.id);

		    auto modelMatrix = glm::mat4(transform.rotation);
		    modelMatrix = glm::translate(modelMatrix, transform.position * transform.rotation);
		    modelMatrix = glm::scale(modelMatrix, transform.scale);

		    const uint32_t idx = desc->second.offset + offset.first->second;
		    _renderContext.instanceUniforms[idx] = modelMatrix;
		    if (alpha != nullptr)
		    {
			    _renderContext.instanceUniforms[idx][0][3] = 1.0f - glm::clamp(alpha->value, 0.0f, 1.0f);
		    }
		    // The w of the second column carries the texture V offset (components::UvScroll).
		    if (const auto* scroll = registry.TryGet<const UvScroll>(entity); scroll != nullptr)
		    {
			    _renderContext.instanceUniforms[idx][1][3] = scroll->v;
		    }
		    // The w of the third column: the pile sink offset, which the height-map shader would otherwise undo.
		    if (const auto* sink = registry.TryGet<const PileSink>(entity);
		        sink != nullptr && registry.AllOf<MorphWithTerrain>(entity))
		    {
			    _renderContext.instanceUniforms[idx][2][3] = sink->offset;
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
