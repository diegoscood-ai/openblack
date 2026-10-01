/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <unordered_map>
#include <unordered_set>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "Graphics/GraphicsHandle.h"
#include "Graphics/Mesh.h"

namespace openblack::ecs::systems
{
struct RenderContext
{
	RenderContext();
	~RenderContext();
	std::unique_ptr<graphics::Mesh> boundingBox;
	std::unique_ptr<graphics::Mesh> streams;
	std::unique_ptr<graphics::Mesh> footpaths;
	std::unique_ptr<graphics::Mesh> footprints;

	struct InstancedDrawDesc
	{
		InstancedDrawDesc(uint32_t offset, uint32_t count, bool morphWithTerrain)
		    : offset(offset)
		    , count(count)
		    , morphWithTerrain(morphWithTerrain)
		{
		}
		uint32_t offset;
		uint32_t count;
		bool morphWithTerrain;
	};

	/// A list of cpu-side uniforms which is refilled at every \ref PrepareDraw.
	/// This vector will resize to the number of instances it manages
	/// but in practice, it should only grow its reserved memory.
	/// If debug bounding boxes are enabled, it will double in size to fit all
	/// bounding boxes in the second half of the list.
	std::vector<glm::mat4> instanceUniforms;
	/// Stores information for rendering which is prepared at \ref PrepareDraw.
	std::map<entt::id_type, const InstancedDrawDesc> instancedDrawDescs;
	/// Same for entities with a components::Alpha (drawn blended after the opaque ones). Their opacity travels in the
	/// unused w of the first column of the model matrix, as 1 - alpha so that opaque instances keep 0 there.
	std::map<entt::id_type, const InstancedDrawDesc> translucentDrawDescs;
	/// The instances (indices of instanceUniforms) of those that are PSys mesh atoms with UseAdditiveAlpha: material mode
	/// 13 (GJUtils::SetMaterialProperties 0x57E120: SRCALPHA / ONE, no Z write), PSys/Creators/Mesh.h
	std::unordered_set<uint32_t> additiveInstances;
	/// Where each entity's model matrix is this frame
	struct EntityInstance
	{
		entt::id_type meshId;
		uint32_t index;
		bool morphWithTerrain;
		/// LH3DObject Flags1 0x40 (Object::Create3DObject; off for trees, forests, some pots...): the hand's shadow
		bool receivesDynamicShadow;
	};
	std::unordered_map<entt::entity, EntityInstance> entityInstances;
	/// The objects that cast a static shadow (see RenderingSystem.cpp, CastsStaticShadow), again, in their own range
	std::map<entt::id_type, const InstancedDrawDesc> shadowCasterDrawDescs;
	/// Not an actual vertex buffer, but a dynamic general purpose buffer which
	/// stores uniform data as a GPU-side copy of \ref _instanceUniforms and
	/// which is populated in \ref PrepareDraw and consumed in \ref DrawModels.
	/// This buffer will resize if the size of \ref _instanceUniforms exceeds
	/// its allocated size. It will never shrink.
	/// The values stored are a list of uniforms (model matrix) needed for both
	/// the instances of entities and their bounding boxes.
	graphics::DynamicVertexBufferHandle instanceUniformBuffer;

	bool dirty {true};
	bool hasBoundingBoxes {false};
};

class RenderingSystemInterface
{
public:
	virtual void SetDirty() = 0;
	virtual void PrepareDraw(bool drawBoundingBox, bool drawFootpaths, bool drawStreams) = 0;
	virtual const RenderContext& GetContext() = 0;
	inline ~RenderingSystemInterface() = default;
};
} // namespace openblack::ecs::systems
