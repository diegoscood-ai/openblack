/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The two LH3DObject draws of the pass under the sea (graphics::sea_pass): vt+0x118 DrawUnderWater (B: static
// fn_00811010, animated fn_00810E20, complex fn_00813300) and the "cut" render mode vt+0x11C DrawCutByPlane (C:
// animated fn_00811C70, static fn_0080C050). The original clips every triangle against the user plane on the CPU
// (fn_0081D2C0, shared by both) and lights each vertex with fn_00858BA0 (C); here the plane is a fragment discard in
// fs_object (sea_plane.sh) and the lights are vs_object's modes 2 / 3 (B) and 4 (C).

#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "ECS/Components/CutByPlane.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/Mesh.h"
#include "Graphics/RenderModes.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawUnderWater(RenderPass viewId, const L3DMesh& mesh, std::unique_ptr<const InstanceDesc> instances,
                              const glm::mat4* matrices, uint8_t matrixCount, bool morphWithTerrain,
                              const sea_pass::SeaDraw& sea) const
{
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	// the material's own mode (fn_00811010 0x8110CF: the table 0xC387C8 only with Flags1 & 0x80, which no caller here
	// sets: the hand's LH3DObject, type 3 0x9A3068, starts with +4 = 0x10009 (0x816537) and nothing calls its vt+0x48)
	submitDesc.options = render_modes::k_ModelPass;
	submitDesc.sea = sea;
	submitDesc.instanceDesc = std::move(instances);
	submitDesc.modelMatrices = matrices;
	submitDesc.matrixCount = matrixCount;
	submitDesc.morphWithTerrain = morphWithTerrain;
	submitDesc.program = land_morph::ObjectProgram(*_shaderManager, morphWithTerrain);
	DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawUnderWater(RenderPass viewId, entt::entity entity, const sea_pass::SeaDraw& sea) const
{
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto instance = renderCtx.entityInstances.find(entity);
	if (!Locator::entitiesRegistry::value().Valid(entity) || instance == renderCtx.entityInstances.end() ||
	    !meshes.Contains(instance->second.meshId))
	{
		return;
	}
	// (openblack) a morphing instance keeps openblack's draw with the height map, although the morphable vtables'
	// DrawUnderWater is a bare ret (0x80BA40, vt+0x118 of 0x9A2E34 / 0x9A2BFC)
	const auto mesh = meshes.Handle(instance->second.meshId);
	static const auto k_Identity = glm::mat4(1.0f);
	DrawUnderWater(viewId, *mesh,
	               std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1),
	               mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &k_Identity,
	               mesh->IsBoned() ? static_cast<uint8_t>(mesh->GetBoneMatrices().size()) : 1,
	               instance->second.morphWithTerrain, sea);
}

void Renderer::DrawCutByPlane(RenderPass viewId, entt::entity entity, sea_pass::SeaPlane plane, uint32_t argb,
                              uint32_t specular) const
{
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto instance = renderCtx.entityInstances.find(entity);
	if (!Locator::entitiesRegistry::value().Valid(entity) || instance == renderCtx.entityInstances.end() ||
	    !meshes.Contains(instance->second.meshId))
	{
		return;
	}
	// the morphable objects' DrawCutByPlane (vt+0x11C of the vtables 0x9A2E34 and 0x9A2BFC) is 0x80BA50, a bare ret:
	// cut, they are not drawn at all (none of them is cut today)
	if (instance->second.morphWithTerrain)
	{
		return;
	}
	const auto mesh = meshes.Handle(instance->second.meshId);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	// "the material mode normal" (cut_notes.txt): opaque with Z, the blended materials as DrawMesh blends them
	submitDesc.options = render_modes::k_ModelPass;
	// mirrored back inside openblack's mirrored Reflection pass: fn_00858BA0 never mirrors (0x858C5D..0x858CAE)
	submitDesc.sea = sea_pass::Cut(plane, argb, specular, viewId);
	submitDesc.instanceDesc =
	    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1);
	static const auto k_Identity = glm::mat4(1.0f);
	// the animated objects skin their bones first (fn_00811C70 0x839980 / 0x839BC0), with the clip's pose of this frame
	// (ecs/Animations.h), the static ones draw as they are
	submitDesc.modelMatrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &k_Identity;
	submitDesc.matrixCount = mesh->IsBoned() ? static_cast<uint8_t>(mesh->GetBoneMatrices().size()) : 1;
	if (const auto* animation = Locator::entitiesRegistry::value().TryGet<const ecs::components::SkeletalAnimation>(entity);
	    animation != nullptr && mesh->IsBoned() && animation->pose.size() == mesh->GetBoneMatrices().size())
	{
		submitDesc.modelMatrices = animation->pose.data();
		submitDesc.matrixCount = static_cast<uint8_t>(animation->pose.size());
	}
	submitDesc.morphWithTerrain = false;
	submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
	DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawCutBelowWater(RenderPass viewId) const
{
	// GLandscape::Draw 4d-4e (0x5E4B26..): SetClipPlane(0, -1, 0, 0) (fn_00822560; sea_pass::k_SwimPlane), keep y <= 0,
	// the object's colour, DrawCutByPlane, SetClipPlane(0, 1, 0, 0)
	const auto plane = sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_SwimPlane);
	std::vector<std::pair<entt::entity, uint32_t>> cut;
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&cut](entt::entity entity, const ecs::components::CutByPlane& component) {
		    cut.emplace_back(entity, component.belowColour);
	    });
	for (const auto& [entity, colour] : cut)
	{
		// the specular obj+0x50: 0, the shark's SetColorSpecular(0xFF303070, 0) under the water (`push 0` 0x775027,
		// vt+0x2C 0x775030, before vt+0x11C 0x775037)
		DrawCutByPlane(viewId, entity, plane, colour, 0u);
	}
}

void Renderer::DrawCutAboveWater(RenderPass viewId) const
{
	// Whale::Draw 0x774E10 (vt+0x610, in the normal object list): SetColorSpecular([0xEDDD08], 0), the land light
	// table[255] of this frame with its alpha, then DrawCutByPlane with the default plane (0, 1, 0, 0): keep y >= 0
	std::vector<entt::entity> cut;
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&cut](entt::entity entity, const ecs::components::CutByPlane& component) {
		    if (component.drawAbove)
		    {
			    cut.push_back(entity);
		    }
	    });
	const uint32_t colour = _landLight && _landLight->IsLoaded() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	for (const auto entity : cut)
	{
		DrawCutByPlane(viewId, entity, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_DefaultPlane), colour,
		               0u);
	}
}

std::unordered_set<uint32_t> Renderer::CutAboveInstances() const
{
	std::unordered_set<uint32_t> instances;
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&](entt::entity entity, const ecs::components::CutByPlane& component) {
		    if (const auto instance = renderCtx.entityInstances.find(entity);
		        component.drawAbove && instance != renderCtx.entityInstances.end())
		    {
			    instances.insert(instance->second.index);
		    }
	    });
	return instances;
}
