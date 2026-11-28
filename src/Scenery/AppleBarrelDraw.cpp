/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AppleBarrelDraw.h"

#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/SkeletalPose.h"
#include "ECS/Animations.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;

std::array<particles::mesh_atoms::Instance, 2> apple_barrel::DrawnMeshes(entt::id_type barrelMesh, entt::id_type wormMesh,
                                                                         const glm::vec3& barrel, const glm::vec3& worm,
                                                                         std::vector<glm::mat4> wormPose)
{
	const auto drawn = [&barrel](entt::id_type mesh, const glm::vec3& place) {
		return particles::mesh_atoms::Instance {
		    .meshId = mesh,
		    .model = glm::translate(glm::mat4(1.0f), place),
		    .alpha = 1.0f,
		    .uv = {0.0f, 0.0f},
		    .translucent = false,
		    .additive = false,
		    .colour = {0xFF, 0xFF, 0xFF},
		    .landscapeColour = true,
		    .globalAlpha = false,
		    .landLightAlone = true,
		    .lightPoint = barrel,
		};
	};
	std::array<particles::mesh_atoms::Instance, 2> meshes {drawn(barrelMesh, barrel), drawn(wormMesh, worm)};
	meshes[1].pose = std::move(wormPose);
	return meshes;
}

std::vector<particles::mesh_atoms::Instance> apple_barrel::DrawnThisFrame()
{
	if (!Locator::appleBarrelSystem::has_value() || !Locator::resources::has_value())
	{
		return {};
	}
	const auto& drawn = Locator::appleBarrelSystem::value().GetDrawn();
	if (!drawn.has_value())
	{
		return {};
	}
	const auto barrelMesh = resources::HashIdentifier(MeshId::Z_AppleBarrel);
	const auto wormMesh = resources::HashIdentifier(MeshId::AnimalWorm);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto clip = ecs::ClipId(static_cast<uint32_t>(AnimId::AWorm));
	if (!meshes.Contains(barrelMesh) || !meshes.Contains(wormMesh) || !animations.Contains(clip))
	{
		return {};
	}
	// the worm posed at its place in its clip
	std::vector<glm::mat4> pose;
	if (const auto mesh = meshes.Handle(wormMesh); mesh->IsBoned())
	{
		graphics::ComputePose(*mesh, *animations.Handle(clip), static_cast<float>(drawn->wormPlace), pose);
	}
	const auto both = DrawnMeshes(barrelMesh, wormMesh, drawn->barrel, drawn->worm, std::move(pose));
	return {both.begin(), both.end()};
}
