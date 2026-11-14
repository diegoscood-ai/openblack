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

#include <glm/geometric.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/ScreenPick.h"
#include "Creature/CreatureEffectCut.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureTurnPose.h"
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

std::optional<ecs::creature_skin::SkinPlace> ecs::creature_skin::CutPlace(const Registry& registry, entt::entity creature,
                                                                          const glm::vec3& from, const glm::vec3& direction)
{
	const auto* meshComponent = registry.TryGet<const Mesh>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* turnPose = registry.TryGet<const CreatureTurnPose>(creature);
	if (meshComponent == nullptr || body == nullptr || turnPose == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshComponent->id))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	// The pose the game reads is already placed in the world. (port guard) a body not posed yet, or posed with another
	// count of bones than its mesh's, is not cut
	const auto bones = creature_turn_pose::Safe(*turnPose);
	if (!mesh->IsBoned() || bones.empty() || bones.size() != mesh->GetBoneMatrices().size())
	{
		return std::nullopt;
	}
	const auto* shape = registry.TryGet<const CreatureMorph>(creature);
	const auto morph = shape != nullptr ? shape->drawn : creature_morph::Morph {};
	const auto targets =
	    creature_morph::MeshesOf(body->species, morph, [&meshes](entt::id_type id) { return meshes.Contains(id); });
	const std::array<entt::id_type, 3> shapeMeshes {targets.evilGood, targets.thinFat, targets.weakStrong};
	const auto line = glm::normalize(direction);

	struct Found
	{
		screen_pick::MeshHit hit;
		size_t subMesh;
		size_t primitive;
	};
	std::optional<Found> nearest;
	const auto& subMeshes = mesh->GetSubMeshes();
	for (size_t s = 0; s < subMeshes.size(); ++s)
	{
		const auto& subMesh = *subMeshes[s];
		if (!hand_mesh_ray::Tested(subMesh.GetFlags()))
		{
			continue;
		}
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
		// Every corner of a boned mesh goes with its own bone, the first for one without
		const auto corners = cave_copy_pick::PoseCorners(glm::mat4(1.0f), bones, true, subMesh.GetSkinBones(),
		                                                 subMesh.GetSkinLocalPositions(), shapes, morph);
		const std::span<const uint16_t> indices = subMesh.GetCollisionIndices();
		const auto& ranges = subMesh.GetCollisionRanges();
		for (size_t p = 0; p < ranges.size(); ++p)
		{
			if (size_t {ranges[p].first} + ranges[p].second > indices.size())
			{
				continue; // (port guard)
			}
			const auto hit = screen_pick::NearestSkinIntersection(corners, indices.subspan(ranges[p].first, ranges[p].second),
			                                                      from, line, true);
			if (hit.has_value() && (!nearest.has_value() || hit->distance < nearest->hit.distance))
			{
				nearest = Found {.hit = *hit, .subMesh = s, .primitive = p};
			}
		}
	}
	if (!nearest.has_value())
	{
		return std::nullopt;
	}

	// The corners met, as numbers within their primitive, looked up in the first sub-mesh's primitive of the same number
	const auto& hitMesh = *subMeshes[nearest->subMesh];
	const auto& first = *subMeshes.front();
	const auto& hitVertices = hitMesh.GetCollisionVertexRanges();
	const auto& firstVertices = first.GetCollisionVertexRanges();
	const auto& firstPrimitives = first.GetPrimitives();
	const auto& uvs = first.GetCollisionUVs();
	const auto p = nearest->primitive;
	if (p >= hitVertices.size() || p >= firstVertices.size() || p >= firstPrimitives.size())
	{
		return std::nullopt; // (port guard)
	}
	const auto& ranges = hitMesh.GetCollisionRanges();
	const auto& indices = hitMesh.GetCollisionIndices();
	std::array<glm::vec2, 3> corner {};
	for (size_t k = 0; k < corner.size(); ++k)
	{
		const uint32_t index = indices[ranges[p].first + nearest->hit.firstIndex + k];
		const uint32_t local = index - hitVertices[p].first;
		const uint32_t uv = firstVertices[p].first + local;
		if (index < hitVertices[p].first || local >= firstVertices[p].second || uv >= uvs.size())
		{
			return std::nullopt; // (port guard)
		}
		corner.at(k) = uvs[uv];
	}
	SkinPlace place;
	place.u = creature_effect_cut::Texel(corner[0].x, corner[1].x, corner[2].x, nearest->hit.s, nearest->hit.t);
	place.v = creature_effect_cut::Texel(corner[0].y, corner[1].y, corner[2].y, nearest->hit.s, nearest->hit.t);
	// Which of the painted skins the primitive's texture is, the first when none
	if (const auto* skins = registry.TryGet<const CreatureSkin>(creature); skins != nullptr)
	{
		for (size_t i = 0; i < skins->skins.size(); ++i)
		{
			if (skins->skins[i].id == firstPrimitives[p].skinID)
			{
				place.skin = static_cast<uint8_t>(i);
			}
		}
	}
	return place;
}
