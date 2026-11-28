/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSkin.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/ScreenPick.h"
#include "Creature/CreatureEffectCut.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/BloodTrails.h"
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

namespace
{
/// A creature's mesh as the game's cut test poses it: under the bones of the pose the game reads, shaped as the body is
struct TurnPosed
{
	const graphics::L3DMesh* mesh {nullptr};
	std::span<const glm::mat4> bones;
	std::array<entt::id_type, 3> shapeMeshes {};
	bool shaped {false};
	creature_morph::Morph morph {};
};

std::optional<TurnPosed> PoseForCut(const ecs::Registry& registry, entt::entity creature)
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
	const graphics::L3DMesh* mesh = &*meshes.Handle(meshComponent->id);
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
	return TurnPosed {.mesh = mesh,
	                  .bones = bones,
	                  .shapeMeshes = {targets.evilGood, targets.thinFat, targets.weakStrong},
	                  .shaped = shape != nullptr,
	                  .morph = morph};
}

/// A sub-mesh's corners as the cut test poses them: every corner of a boned mesh goes with its own bone
std::vector<glm::vec3> CornersOf(const TurnPosed& posed, size_t s)
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& subMesh = *posed.mesh->GetSubMeshes()[s];
	std::array<std::span<const glm::vec3>, 3> shapes {};
	for (size_t axis = 0; axis < posed.shapeMeshes.size(); ++axis)
	{
		if (posed.shaped && meshes.Contains(posed.shapeMeshes.at(axis)))
		{
			const auto target = meshes.Handle(posed.shapeMeshes.at(axis));
			if (s < target->GetSubMeshes().size())
			{
				shapes.at(axis) = target->GetSubMeshes()[s]->GetSkinLocalPositions();
			}
		}
	}
	return cave_copy_pick::PoseCorners(glm::mat4(1.0f), posed.bones, true, subMesh.GetSkinBones(),
	                                   subMesh.GetSkinLocalPositions(), shapes, posed.morph);
}
} // namespace

std::optional<ecs::creature_skin::MeshCut> ecs::creature_skin::NearestCut(const Registry& registry, entt::entity creature,
                                                                          const glm::vec3& from, const glm::vec3& line)
{
	const auto posed = PoseForCut(registry, creature);
	if (!posed.has_value())
	{
		return std::nullopt;
	}
	struct Found
	{
		screen_pick::MeshHit hit;
		size_t subMesh;
		size_t primitive;
	};
	std::optional<Found> nearest;
	const auto& subMeshes = posed->mesh->GetSubMeshes();
	for (size_t s = 0; s < subMeshes.size(); ++s)
	{
		const auto& subMesh = *subMeshes[s];
		if (!hand_mesh_ray::Tested(subMesh.GetFlags()))
		{
			continue;
		}
		const auto corners = CornersOf(*posed, s);
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

	// The corners met, as numbers within their primitive
	const auto& hitMesh = *subMeshes[nearest->subMesh];
	const auto& hitVertices = hitMesh.GetCollisionVertexRanges();
	const auto p = nearest->primitive;
	if (p >= hitVertices.size())
	{
		return std::nullopt; // (port guard)
	}
	const auto& ranges = hitMesh.GetCollisionRanges();
	const auto& indices = hitMesh.GetCollisionIndices();
	MeshCut cut {.subMesh = static_cast<uint32_t>(nearest->subMesh),
	             .spot = {.primitive = static_cast<uint32_t>(p), .corners = {}, .u = nearest->hit.s, .v = nearest->hit.t}};
	for (size_t k = 0; k < cut.spot.corners.size(); ++k)
	{
		const uint32_t index = indices[ranges[p].first + nearest->hit.firstIndex + k];
		if (index < hitVertices[p].first)
		{
			return std::nullopt; // (port guard)
		}
		cut.spot.corners.at(k) = index - hitVertices[p].first;
	}
	return cut;
}

std::optional<ecs::creature_skin::SkinPlace> ecs::creature_skin::CutPlace(const Registry& registry, entt::entity creature,
                                                                          const glm::vec3& from, const glm::vec3& direction)
{
	const auto cut = NearestCut(registry, creature, from, glm::normalize(direction));
	return cut.has_value() ? PlaceOf(registry, creature, cut->spot) : std::nullopt;
}

std::optional<ecs::creature_skin::SpotFrame> ecs::creature_skin::FrameOf(const Registry& registry, entt::entity creature,
                                                                         const MeshCut& cut)
{
	const auto posed = PoseForCut(registry, creature);
	if (!posed.has_value() || cut.subMesh >= posed->mesh->GetSubMeshes().size())
	{
		return std::nullopt;
	}
	const auto& vertices = posed->mesh->GetSubMeshes()[cut.subMesh]->GetCollisionVertexRanges();
	if (cut.spot.primitive >= vertices.size())
	{
		return std::nullopt; // (port guard)
	}
	const auto corners = CornersOf(*posed, cut.subMesh);
	std::array<glm::vec3, 3> at {};
	for (size_t k = 0; k < at.size(); ++k)
	{
		const uint32_t local = cut.spot.corners.at(k);
		const size_t index = size_t {vertices[cut.spot.primitive].first} + local;
		if (local >= vertices[cut.spot.primitive].second || index >= corners.size())
		{
			return std::nullopt; // (port guard)
		}
		at.at(k) = corners[index];
	}
	const auto ab = at[1] - at[0];
	const auto ac = at[2] - at[0];
	return SpotFrame {.point = at[0] + ab * cut.spot.u + ac * cut.spot.v, .inward = glm::cross(ac, ab)};
}

std::optional<ecs::creature_skin::MeshSpot> ecs::creature_skin::SpotOnMesh(const Registry& registry, entt::entity creature,
                                                                           const creature_skin_ray::Spot& spot)
{
	const auto* meshComponent = registry.TryGet<const Mesh>(creature);
	if (meshComponent == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshComponent->id))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	const auto& subMeshes = mesh->GetSubMeshes();
	if (spot.part >= subMeshes.size())
	{
		return std::nullopt; // (port guard)
	}
	const auto& vertices = subMeshes[spot.part]->GetCollisionVertexRanges();
	if (spot.primitive >= vertices.size())
	{
		return std::nullopt; // (port guard)
	}
	MeshSpot found {.primitive = spot.primitive, .corners = {}, .u = spot.u, .v = spot.v};
	for (size_t k = 0; k < found.corners.size(); ++k)
	{
		if (spot.corners.at(k) < vertices[spot.primitive].first)
		{
			return std::nullopt; // (port guard)
		}
		found.corners.at(k) = spot.corners.at(k) - vertices[spot.primitive].first;
	}
	return found;
}

std::optional<ecs::creature_skin::SkinPlace> ecs::creature_skin::PlaceOf(const Registry& registry, entt::entity creature,
                                                                         const MeshSpot& spot)
{
	const auto* meshComponent = registry.TryGet<const Mesh>(creature);
	if (meshComponent == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshComponent->id))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	const auto& subMeshes = mesh->GetSubMeshes();
	if (subMeshes.empty())
	{
		return std::nullopt; // (port guard)
	}
	// The corners, as numbers within their primitive, looked up in the first sub-mesh's primitive of the same number
	const auto& first = *subMeshes.front();
	const auto& firstVertices = first.GetCollisionVertexRanges();
	const auto& firstPrimitives = first.GetPrimitives();
	const auto& uvs = first.GetCollisionUVs();
	const auto p = spot.primitive;
	if (p >= firstVertices.size() || p >= firstPrimitives.size())
	{
		return std::nullopt; // (port guard)
	}
	std::array<glm::vec2, 3> corner {};
	for (size_t k = 0; k < corner.size(); ++k)
	{
		const uint32_t local = spot.corners.at(k);
		const uint32_t uv = firstVertices[p].first + local;
		if (local >= firstVertices[p].second || uv >= uvs.size())
		{
			return std::nullopt; // (port guard)
		}
		corner.at(k) = uvs[uv];
	}
	SkinPlace place;
	place.u = creature_effect_cut::Texel(corner[0].x, corner[1].x, corner[2].x, spot.u, spot.v);
	place.v = creature_effect_cut::Texel(corner[0].y, corner[1].y, corner[2].y, spot.u, spot.v);
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

std::vector<creature_marks::Mark> ecs::blood_trails::Step(const Registry& registry, entt::entity creature,
                                                          creature_marks::BloodTrails& trails, int32_t ms)
{
	std::vector<creature_marks::Mark> drops;
	for (auto& trail : trails)
	{
		if (!creature_marks::RunTrail(trail, ms))
		{
			continue;
		}
		// the skin met along the spot's inward normal from a little below it, under the pose the turn before left
		const auto frame = creature_skin::FrameOf(registry, creature, trail.cut);
		std::optional<creature_marks::MeshCut> next;
		if (frame.has_value())
		{
			auto from = frame->point;
			from.y -= creature_marks::k_TrailDrop;
			next = creature_skin::NearestCut(registry, creature, from, frame->inward);
		}
		if (!next.has_value())
		{
			trail.ms = creature_marks::k_TrailRunsMs;
			continue;
		}
		trail.cut = *next;
		if (const auto place = creature_skin::PlaceOf(registry, creature, next->spot); place.has_value())
		{
			drops.push_back({.u = place->u, .v = place->v, .skin = place->skin, .age = 0});
		}
	}
	return drops;
}
