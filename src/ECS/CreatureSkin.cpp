/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSkin.h"

#include <array>
#include <span>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Mesh.h"
#include "ECS/HandMeshRay.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::vector<cave_copy_pick::Part> ecs::creature_skin::DrawnSkin(const Registry& registry, entt::entity creature)
{
	std::vector<cave_copy_pick::Part> parts;
	const auto* meshComponent = registry.TryGet<const Mesh>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (meshComponent == nullptr || body == nullptr || !Locator::resources::has_value())
	{
		return parts;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshComponent->id))
	{
		return parts;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	const auto model = ecs::DrawnBodyModel(registry, creature);
	std::span<const glm::mat4> bones = mesh->GetBoneMatrices();
	if (const auto* animation = registry.TryGet<const CreatureAnimation>(creature); animation != nullptr)
	{
		const auto& drawn = creature_turn_pose::BonesDrawn(*animation);
		if (!drawn.empty() && drawn.size() == mesh->GetBoneMatrices().size())
		{
			bones = drawn;
		}
	}
	const auto* shape = registry.TryGet<const CreatureMorph>(creature);
	const auto morph = shape != nullptr ? shape->drawn : creature_morph::Morph {};
	const auto targets =
	    creature_morph::MeshesOf(body->species, morph, [&meshes](entt::id_type id) { return meshes.Contains(id); });
	const std::array<entt::id_type, 3> shapeMeshes {targets.evilGood, targets.thinFat, targets.weakStrong};
	const auto& subMeshes = mesh->GetSubMeshes();
	for (size_t s = 0; s < subMeshes.size(); ++s)
	{
		const auto& subMesh = *subMeshes[s];
		const auto flags = subMesh.GetFlags();
		// a sub-mesh left out still takes its place, so that a spot's part stays the same
		cave_copy_pick::Part part;
		if (!subMesh.IsPhysics() && ecs::hand_mesh_ray::Tested(flags))
		{
			std::array<std::span<const glm::vec3>, 3> shapes {};
			for (size_t axis = 0; axis < shapeMeshes.size(); ++axis)
			{
				if (shape != nullptr && meshes.Contains(shapeMeshes.at(axis)))
				{
					const auto target = meshes.Handle(shapeMeshes.at(axis));
					if (s < target->GetSubMeshes().size())
					{
						shapes.at(axis) = target->GetSubMeshes()[s]->GetSkinLocalPositions();
					}
				}
			}
			part.corners =
			    cave_copy_pick::PoseCorners(model, mesh->IsBoned() ? bones : std::span<const glm::mat4> {}, flags.hasBones,
			                                subMesh.GetSkinBones(), subMesh.GetSkinLocalPositions(), shapes, morph);
			part.indices = subMesh.GetCollisionIndices();
			const auto& primitives = subMesh.GetPrimitives();
			const auto& ranges = subMesh.GetCollisionRanges();
			for (size_t p = 0; p < primitives.size() && p < ranges.size(); ++p)
			{
				part.primitives.push_back(
				    {.first = ranges[p].first, .count = ranges[p].second, .twoSided = primitives[p].twoSided});
			}
		}
		parts.push_back(std::move(part));
	}
	return parts;
}

std::optional<ecs::hand_mesh_ray::Hit> ecs::creature_skin::HandFeel(std::span<const cave_copy_pick::Part> skin,
                                                                    const glm::vec3& origin, const glm::vec3& direction)
{
	std::optional<hand_mesh_ray::Hit> best;
	for (const auto& part : skin)
	{
		for (const auto& primitive : part.primitives)
		{
			if (size_t {primitive.first} + primitive.count > part.indices.size())
			{
				continue; // (port guard)
			}
			best = hand_mesh_ray::NearestPosed(part.corners, std::span(part.indices).subspan(primitive.first, primitive.count),
			                                   origin, direction, best);
		}
	}
	return best;
}
