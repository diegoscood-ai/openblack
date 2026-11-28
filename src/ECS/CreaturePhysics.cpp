/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePhysics.h"

#include <cmath>

#include <stdexcept>
#include <utility>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include "3D/AffineMatrix.h"
#include "3D/L3DMesh.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

creature_physics::BodyShape creature_physics::ShapeOf(float height)
{
	const float r = 0.5f * height;
	return ShapeOf(r, glm::vec3(0.0f, r, 0.0f));
}

creature_physics::BodyShape creature_physics::ShapeOf(float radius, glm::vec3 centre)
{
	// (approximate) the original's body has a point for each bone of its skeleton, met through each bone's box; here a
	// ball of the body's sphere, its six farthest points and the faces between them, facing out
	const float r = radius;
	BodyShape shape;
	shape.points = {{r, 0.0f, 0.0f}, {-r, 0.0f, 0.0f}, {0.0f, r, 0.0f}, {0.0f, -r, 0.0f}, {0.0f, 0.0f, r}, {0.0f, 0.0f, -r}};
	shape.faces = {{0, 2, 4}, {1, 4, 2}, {0, 4, 3}, {0, 5, 2}, {1, 3, 4}, {1, 2, 5}, {0, 3, 5}, {1, 5, 3}};
	shape.centre = centre;
	shape.radius = r;
	return shape;
}

std::optional<creature_physics::BoundingSphere> creature_physics::BoundingSphereOf(std::span<const glm::vec3> bones)
{
	if (bones.empty())
	{
		return std::nullopt;
	}
	BoundingSphere sphere;
	for (const auto& bone : bones)
	{
		sphere.centre.x = sphere.centre.x + bone.x;
		sphere.centre.y = sphere.centre.y + bone.y;
		sphere.centre.z = sphere.centre.z + bone.z;
	}
	const float share = 1.0f / static_cast<float>(bones.size());
	sphere.centre *= share;
	for (const auto& bone : bones)
	{
		const glm::vec3 d = bone - sphere.centre;
		const float distance = std::sqrt(((d.z * d.z) + (d.y * d.y)) + (d.x * d.x));
		if (distance > sphere.radius)
		{
			sphere.radius = distance;
		}
	}
	return sphere;
}

float creature_physics::Weight(const Registry& registry, entt::entity creature)
{
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		throw std::invalid_argument("creature_physics::Weight: the entity is not a creature");
	}
	const auto target = creature_pose::TargetMorph(registry, creature);
	return openblack::physics::living::CreatureMass(creature_pose::DrawnSize(registry, creature), target.thinFat,
	                                                target.weakStrong);
}

std::vector<openblack::physics::collide_boxes::Box> creature_physics::CollideBoxesOf(const Registry& registry,
                                                                                     entt::entity creature)
{
	const auto* mesh = registry.TryGet<const Mesh>(creature);
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	if (mesh == nullptr || body == nullptr || body->boneMatrices.empty() || !registry.AllOf<Transform>(creature) ||
	    !Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		return {};
	}
	std::vector<openblack::physics::collide_boxes::Box> boxes;
	for (const auto& bounds : Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoneBounds())
	{
		boxes.push_back({.min = bounds.min, .max = bounds.max});
	}
	const auto& transform = registry.Get<const Transform>(creature);
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	std::vector<affine::AffineMatrix> bones;
	bones.reserve(body->boneMatrices.size());
	for (const auto& bone : body->boneMatrices)
	{
		bones.push_back(affine::FromModel(placement * bone));
	}
	openblack::physics::collide_boxes::InitForCollision(boxes, bones);
	return boxes;
}

std::optional<creature_physics::BoundingSphere> creature_physics::PosedSphereOf(const Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	if (body == nullptr || body->boneMatrices.empty() || !registry.AllOf<Transform>(creature))
	{
		return std::nullopt;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	std::vector<glm::vec3> bones;
	bones.reserve(body->boneMatrices.size());
	for (const auto& bone : body->boneMatrices)
	{
		bones.emplace_back(placement * bone[3]);
	}
	return BoundingSphereOf(bones);
}

void creature_physics::FollowPose(entt::entity creature, openblack::physics::Body& body)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	if (const auto sphere = PosedSphereOf(registry, creature); sphere.has_value())
	{
		body.SetSphere(sphere->centre, sphere->radius);
	}
	if (auto boxes = CollideBoxesOf(registry, creature); !boxes.empty())
	{
		body.SetCollideBoxes(std::move(boxes));
	}
}

bool creature_physics::SetUpBody(entt::entity creature, openblack::physics::Body& body)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return false;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	// the sphere about the body as it is posed, its bones placed in the world as the body is drawn; a body not posed yet
	// stands as a ball as tall as the creature
	auto shape = ShapeOf(object::GetHeight(creature));
	if (const auto sphere = PosedSphereOf(registry, creature); sphere.has_value())
	{
		// its centre of mass in the creature's own turn, from its feet
		shape = ShapeOf(sphere->radius, glm::inverse(transform.rotation) * (sphere->centre - transform.position));
	}
	// with its mesh's boxes, a point a bone (all at the centre until the first step puts them at the first box) and no
	// faces: what is thrown meets the boxes
	auto boxes = CollideBoxesOf(registry, creature);
	if (!boxes.empty())
	{
		shape.points.assign(boxes.size(), glm::vec3(0.0f));
		shape.faces.clear();
	}
	body.Initialise(1.0f, shape.radius);
	body.SetUpConstants(shape.mass,
	                    Locator::dynamicsSystem::value().Constants(Locator::dynamicsSystem::value().ConstantsType(creature)),
	                    shape.dynamic);
	body.BuildShape(shape.points, shape.faces, shape.centre, shape.radius, 1.0f, transform.rotation, transform.position);
	body.SetCollideBoxes(std::move(boxes));
	return true;
}

creature_physics::Release creature_physics::ReleaseOf(bool atTarget, entt::entity creature)
{
	return atTarget && creature != entt::null ? Release::AtTarget : Release::LetGo;
}

std::optional<glm::vec3> creature_physics::LetGoSpin(bool spins) noexcept
{
	if (!spins)
	{
		return std::nullopt;
	}
	// one about the vertical, which this port's spin holds negated
	return glm::vec3(0.0f, -1.0f, 0.0f);
}
