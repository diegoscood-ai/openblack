/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The two object draws of the pass under the sea (graphics::sea_pass): DrawUnderWater (B: static, animated and
// complex objects) and the "cut" render mode DrawCutByPlane (C: animated and static objects). The original clips every
// triangle against the user plane on the CPU (one clipper shared by both) and lights each vertex itself (C); here the
// plane is a fragment discard in fs_object (sea_plane.sh) and the lights are vs_object's modes 2 / 3 (B) and 4 (C).

#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Animations.h"
#include "ECS/Components/CutByPlane.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/Mesh.h"
#include "Graphics/RenderModes.h"
#include "Graphics/SeaPass.h"
#include "Graphics/SeaReflection.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowList.h"
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
	// the material's own mode: the global alpha mode table is only taken when the object's global alpha flag is set.
	// (inferred) no caller here has that flag: the hand's object (a complex one) starts without it, and no call that
	// sets it on the hand's object was found; a setter through another pointer path is not ruled out. The moon does set
	// it before both its Draw and its DrawUnderWater, see Renderer::DrawMoon
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
	    !meshes.Contains(instance->second.ownMeshId))
	{
		return;
	}
	// the morphable objects' (and the citadel's) DrawUnderWater does nothing: an object that morphs with the land is not
	// drawn under the sea, as DrawCutByPlane below
	if (instance->second.morphWithTerrain)
	{
		return;
	}
	// its own mesh, not a finer one drawn in the world (components::DetailMesh)
	const auto mesh = meshes.Handle(instance->second.ownMeshId);
	static const auto k_Identity = glm::mat4(1.0f);
	// an animated object is mirrored in the frame of its animation it is drawn in, as a held or flying villager or
	// animal is (a SuperVillager's cross-faded pose while it fades), one instance with its own bones; never from the
	// bone palette, which only the main view fills
	std::span<const glm::mat4> bones;
	if (mesh->IsBoned())
	{
		const auto* animation =
		    std::as_const(Locator::entitiesRegistry::value()).TryGet<const ecs::components::SkeletalAnimation>(entity);
		bones = sea_reflection::ReflectedBones(mesh->GetBoneMatrices(), animation != nullptr && !animation->pose.empty()
		                                                                    ? std::span(ecs::DrawnPose(*animation))
		                                                                    : std::span<const glm::mat4>());
	}
	DrawUnderWater(viewId, *mesh,
	               std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1),
	               mesh->IsBoned() ? bones.data() : &k_Identity, mesh->IsBoned() ? static_cast<uint8_t>(bones.size()) : 1,
	               instance->second.morphWithTerrain, sea);
}

void Renderer::DrawCreatureReflection(RenderPass viewId) const
{
	if (!Locator::leashSystem::has_value() || _frameCreatures.empty())
	{
		return;
	}
	const auto local = Locator::leashSystem::value().PlayersCreature(creature::LocalPlayer());
	if (!local.has_value())
	{
		return;
	}
	const auto body = std::ranges::find(_frameCreatures, *local, &creature_draw::Body::entity);
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (body == _frameCreatures.end() || body->instance >= renderCtx.instanceUniforms.size() || !meshes.Contains(body->mesh))
	{
		return;
	}
	// where its body is drawn, its row's translation
	const auto position = glm::vec3(renderCtx.instanceUniforms[body->instance][3]);
	const shadow_math::BlockAt blocks = [this](int x, int z) { return _shadows->BlockAt(x, z); };
	if (!sea_pass::ReflectsCreature(position, body->fizz, blocks))
	{
		return;
	}
	// its pose of this frame, one instance with its own bones (the palette is only filled for the main view)
	const auto mesh = meshes.Handle(body->mesh);
	const auto bones = sea_reflection::ReflectedBones(mesh->GetBoneMatrices(), body->bones);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	submitDesc.options = render_modes::k_ModelPass;
	submitDesc.sea = sea_pass::UnderWater(sea_pass::k_CreatureColour, sea_pass::k_CreatureSpecular);
	submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, body->instance, 1);
	submitDesc.modelMatrices = bones.data();
	submitDesc.matrixCount = static_cast<uint8_t>(bones.size());
	submitDesc.morphWithTerrain = false;
	submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
	// its shape and painted skins, as the main draw; no fizz or freeze look
	UseCreatureBody(*body, submitDesc);
	DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawCutByPlane(RenderPass viewId, entt::entity entity, sea_pass::SeaPlane plane, uint32_t argb,
                              uint32_t specular) const
{
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto instance = renderCtx.entityInstances.find(entity);
	if (!Locator::entitiesRegistry::value().Valid(entity) || instance == renderCtx.entityInstances.end() ||
	    !meshes.Contains(instance->second.ownMeshId))
	{
		return;
	}
	// the morphable objects' (and the citadel's) DrawCutByPlane does nothing: cut, they are not drawn at all (none of
	// them is cut today)
	if (instance->second.morphWithTerrain)
	{
		return;
	}
	// its own mesh, not a finer one drawn in the world (components::DetailMesh)
	const auto mesh = meshes.Handle(instance->second.ownMeshId);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	// the material's normal mode: opaque with Z, the blended materials as DrawMesh blends them
	submitDesc.options = render_modes::k_ModelPass;
	// mirrored back inside openblack's mirrored Reflection pass: the cut draw never mirrors
	submitDesc.sea = sea_pass::Cut(plane, argb, specular, viewId);
	submitDesc.instanceDesc =
	    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance->second.index, 1);
	static const auto k_Identity = glm::mat4(1.0f);
	// the animated objects skin their bones first, with the clip's pose of this frame
	// (ecs/Animations.h), the static ones draw as they are
	submitDesc.modelMatrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &k_Identity;
	submitDesc.matrixCount = mesh->IsBoned() ? static_cast<uint8_t>(mesh->GetBoneMatrices().size()) : 1;
	// the pose of its main draw this frame (a fade between two clips included), as the reflection takes it
	if (const auto* animation = Locator::entitiesRegistry::value().TryGet<const ecs::components::SkeletalAnimation>(entity);
	    animation != nullptr && mesh->IsBoned())
	{
		const auto& pose = ecs::DrawnPose(*animation);
		if (pose.size() == mesh->GetBoneMatrices().size())
		{
			submitDesc.modelMatrices = pose.data();
			submitDesc.matrixCount = static_cast<uint8_t>(pose.size());
		}
	}
	submitDesc.morphWithTerrain = false;
	submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
	DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawCutBelowWater(RenderPass viewId) const
{
	// the land draw's sharks under the water: each sets its own clip plane (0, -1, 0, 0) (sea_pass::k_SharkPlane),
	// keep y <= 0, the object's colour, DrawCutByPlane, then the plane (0, 1, 0, 0) again
	const auto plane = sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_SharkPlane);
	std::vector<std::pair<entt::entity, uint32_t>> cut;
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&cut](entt::entity entity, const ecs::components::CutByPlane& component) {
		    cut.emplace_back(entity, component.belowColour);
	    });
	for (const auto& [entity, colour] : cut)
	{
		// the specular: 0, the shark's colour 0xFF303070 and specular 0 under the water, set before the cut draw
		DrawCutByPlane(viewId, entity, plane, colour, 0u);
	}
}

void Renderer::DrawCutAboveWater(RenderPass viewId) const
{
	DrawCutAboveWater(viewId, CutAboveInstances());
}

void Renderer::DrawCutAboveWater(RenderPass viewId, const std::unordered_set<uint32_t>& only) const
{
	// the shark's own Draw (in the normal object list): its colour is the land light table[255] of this frame with its
	// alpha, specular 0, then DrawCutByPlane with the default plane (0, 1, 0, 0): keep y >= 0
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	std::vector<entt::entity> cut;
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&](entt::entity entity, const ecs::components::CutByPlane& component) {
		    if (const auto instance = renderCtx.entityInstances.find(entity);
		        component.drawAbove && instance != renderCtx.entityInstances.end() && only.contains(instance->second.index))
		    {
			    cut.push_back(entity);
		    }
	    });
	const uint32_t colour = IsLandLit() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	for (const auto entity : cut)
	{
		DrawCutByPlane(viewId, entity, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_DefaultPlane), colour, 0u);
	}
}

std::unordered_set<uint32_t> Renderer::CutAboveInstances(bool drawnAboveOnly) const
{
	std::unordered_set<uint32_t> instances;
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	Locator::entitiesRegistry::value().Each<const ecs::components::CutByPlane>(
	    [&](entt::entity entity, const ecs::components::CutByPlane& component) {
		    if (const auto instance = renderCtx.entityInstances.find(entity);
		        (component.drawAbove || (component.ownerDraws && !drawnAboveOnly)) &&
		        instance != renderCtx.entityInstances.end())
		    {
			    instances.insert(instance->second.index);
		    }
	    });
	return instances;
}
