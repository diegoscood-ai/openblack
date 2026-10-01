/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The "cut" render mode of the models (LH3DObject vt+0x11C DrawCutByPlane): animated fn_00811C70, static fn_0080C050.
// The original clips every triangle against the user plane on the CPU (fn_0081D2C0) and lights each vertex with
// fn_00858BA0; here the plane is a fragment discard in fs_object and the light is vs_object's mode 4.

#include <limits>
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
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawCutByPlane(RenderPass viewId, entt::entity entity, int8_t keep, uint32_t argb, bool mirrored) const
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
	submitDesc.state = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
	submitDesc.cutByPlane = keep;
	submitDesc.cutColour = argb;
	submitDesc.mirrorInSea = mirrored;
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
	// GLandscape::Draw 4d-4e (0x5E4B26..): SetClipPlane(0, -1, 0, 0) (fn_00822560), keep y <= 0, the object's colour,
	// DrawCutByPlane, SetClipPlane(0, 1, 0, 0)
	std::vector<std::pair<entt::entity, uint32_t>> cut;
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&cut](entt::entity entity, const ecs::components::CutByPlane& component) {
		    cut.emplace_back(entity, component.belowColour);
	    });
	for (const auto& [entity, colour] : cut)
	{
		DrawCutByPlane(viewId, entity, -1, colour, true);
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
		DrawCutByPlane(viewId, entity, 1, colour, false);
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
