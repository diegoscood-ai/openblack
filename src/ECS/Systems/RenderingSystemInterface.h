/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/LandLight.h"
#include "Graphics/GraphicsHandle.h"
#include "Graphics/Mesh.h"

namespace openblack::psys
{
struct Atom;
enum class DrawPath : uint8_t;
} // namespace openblack::psys

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
	/// The fifth column of every instance (i_data4), at the same indices: the object's LH3DColor fields obj+0x4C / +0x50
	/// / +0x54 packed by lh3d_colour::PackInstanceTint / Colour / Specular / Window (src/Graphics/Lh3dColour.h), zero for
	/// the land light alone. Zeroed before every refill; instanceUniformBuffer holds the two interleaved (80 bytes each).
	std::vector<glm::vec4> instanceColours;
	/// Stores information for rendering which is prepared at \ref PrepareDraw.
	std::map<entt::id_type, const InstancedDrawDesc> instancedDrawDescs;
	/// The entities with components::NotDrawn (a building at 0 %): not drawn, only their landscape footprint
	/// (Renderer::DrawFootprintPass; SetFootPrintOnTexture 0x52EA33 stays on while it is unbuilt)
	std::map<entt::id_type, const InstancedDrawDesc> footprintOnlyDrawDescs;
	/// Same for entities with a components::Alpha (drawn blended after the opaque ones). Their opacity travels in the
	/// unused w of the first column of the model matrix, as 1 - alpha so that opaque instances keep 0 there.
	std::map<entt::id_type, const InstancedDrawDesc> translucentDrawDescs;
	/// The instances (indices of instanceUniforms) of those that are PSys mesh atoms with UseAdditiveAlpha: material mode
	/// 13 (GJUtils::SetMaterialProperties 0x57E120: SRCALPHA / ONE, no Z write), PSys/Creators/Mesh.h
	std::unordered_set<uint32_t> additiveInstances;
	/// The opaque PSys mesh atoms drawn with DrawCutByPlane (the particle's +0x24 & 4, fn_00679F20 0x679F29 -> vt+0x11C
	/// 0x679F4A): out of instancedDrawDescs, drawn in Renderer::DrawPass (the cut-atom loop after DrawCutAboveWater)
	/// with graphics::sea_pass::CutAtoms
	std::map<entt::id_type, const InstancedDrawDesc> cutAtomDrawDescs;
	/// The translucent ones (in translucentDrawDescs): their instance indices, drawn cut from the sorted list
	std::unordered_set<uint32_t> cutAtomInstances;
	/// Every PSys mesh atom of this frame (psys::mesh_atoms::Instance) with its effect's draw path (psys::DrawPath), in
	/// mesh_atoms::Collect's order, refilled at every PrepareDraw. Its instance is in the old ranges (instancedDrawDescs,
	/// translucentDrawDescs, cutAtomDrawDescs) while psys::manager::k_DrawByPath is false, and in psysAtomDrawDescs only
	/// once it is true. Sorted: its own Z object at `key` (fn_00679F60, opaque, translucent or cut alike); Queued /
	/// Immediate: drawn at its place in its effect's items (psys::manager::OrderedEffect, by psysAtomIndex[atom]).
	/// Interface: dev\tmp_dis\miracles\polish\drawpath_fix.md
	struct PSysAtomInstance
	{
		uint32_t index;          ///< its instance in instanceUniforms / instanceColours / instancePoses
		entt::id_type meshId;
		psys::DrawPath path;
		uint32_t effect;         ///< the effect's id (psys::manager)
		const psys::Atom* atom;  ///< the atom (Effect::DrawAtom::atom)
		glm::vec3 key;           ///< the object's +0x38..+0x40, its translation (fn_00679F60 0x679F7E..0x679F9F)
		bool translucent;        ///< additive or faded with the global alpha (mesh_atoms::Instance::translucent)
		bool additive;           ///< also in additiveInstances
		bool cut;                ///< DrawCutByPlane drawn cut (vt+0x11C 0x679F4A, sea_pass::CutAtoms); also in cutAtomInstances
	};
	std::vector<PSysAtomInstance> psysAtoms;
	/// atom -> its index in psysAtoms
	std::unordered_map<const psys::Atom*, uint32_t> psysAtomIndex;
	/// With psys::manager::k_DrawByPath: the ranges of every PSys mesh atom, drawn by none of the other loops
	std::map<entt::id_type, const InstancedDrawDesc> psysAtomDrawDescs;
	/// Blended instances sorted at another point than their model matrix's translation (the one-shot orb, whose sort key
	/// OneOffSpellSeed::Draw 0x518E90 pushes toward the camera by its radius): instance index -> the point
	std::unordered_map<uint32_t, glm::vec3> sortPoints;
	/// How the models of a mesh take the land light (land_light::ObjectMode): the plain models fn_00801C90 +
	/// fn_007FEB30, trees, worship sites, spell icons, the Dove class; one per mesh (RenderingSystem LandLightOf),
	/// refilled at every PrepareDraw
	std::unordered_map<entt::id_type, land_light::ObjectLight> meshLandLight;
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
	/// The bones of the instances that are not entities: the PSys mesh atoms of a ParticleAnimCreator (Particle3DAnim::
	/// DrawAt 0x67A8E0, PSys/Creators/Mesh.h), instance index -> the bones' model matrices, refilled at every PrepareDraw
	std::unordered_map<uint32_t, std::vector<glm::mat4>> instancePoses;
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
