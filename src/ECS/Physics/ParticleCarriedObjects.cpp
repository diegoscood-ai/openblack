/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleCarriedObjects.h"

#include <cmath>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerScript.h"
#include "Locator.h"
#include "Physics/LivingRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

bool particle_carried_objects::Take(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	// the class's part first (a villager drops its resource, then a living enters FLYING;
	// InitialisePhysicsWithoutBody), which then runs the common part below;
	// false refuses the take
	if (!Locator::dynamicsSystem::value().ClassHooks().InitialisePhysicsWithoutBody(Locator::dynamicsSystem::value(), object))
	{
		return false;
	}
	// the common part: in the physics (a moving body, or already carried) or
	// IMMOVABLE -> not started, nothing done (after the class's part: a living refused here is already FLYING)
	if (registry.AllOf<InPhysics>(object) || object_flags::IsImmovable(object))
	{
		return false;
	}
	// TODO(creature): RemoveDraggingCreatureByLeash. LOCKED_SELECT has no openblack counterpart
	if (map_cells::IsObjectInMap(object))
	{
		map_cells::RemoveMapObject(object);
	}
	// no body (no world matrix, no AddObject); the fire leaves its group (unless a game flag is set:
	// (pending) that flag)
	fire::StartedMoving(object, false);
	// started: in the physics with no body, and marked carried
	registry.AssignOrReplaceState<InPhysics>(object);
	registry.AssignOrReplace<CarriedByTornado>(object);
	registry.SetDirty();
	return true;
}

bool particle_carried_objects::IsAvailable(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return ecs::IsAvailable(object) && (!registry.AllOf<Villager>(object) || villager::IsAvailable(object));
}

bool particle_carried_objects::IsCarried(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<CarriedByTornado>(object);
}

void particle_carried_objects::Release(entt::entity object, const glm::mat3& atomRows, glm::vec3 atomPosition)
{
	auto& registry = Locator::entitiesRegistry::value();
	// not available -> the caller forgets it
	if (!IsAvailable(object))
	{
		return;
	}
	auto* transform = registry.TryGet<Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	// the angles from the atom's matrix, per class: a villager, animal or tree
	// lands upright with the atom's yaw, a static / mobile object / pot keeps the atom's tilt. The atom's matrix is the
	// object's world matrix (the original's rows, no quarter turn); openblack's atom carries a
	// living's DRAWN rotation (its Transform), so OriginalRows takes the quarter turn off first, and the result gets it
	// back (a living is drawn with it on the ground)
	const bool isLiving = registry.AnyOf<Villager, Animal>(object);
	const auto rows = Locator::dynamicsSystem::value().RotationFromRows(
	    object, isLiving ? openblack::physics::living::OriginalRows(atomRows) : atomRows);
	transform->rotation = isLiving ? Locator::dynamicsSystem::value().DrawQuarterTurn(rows) : rows;
	if (!isLiving)
	{
		// the thing keeps the angles it took from the atom (one that does not lean, its angle about y)
		placed_turn::KeepAngles(registry, object, placed_turn::AnglesOfTurn(atomRows, true));
	}
	// x / z = the atom's in fixed point (x 6553.6, truncated), altitude 0: on the ground
	transform->position = map_coords::ToWorld(
	    map_coords::MapCoords {map_coords::ToFixed(atomPosition.x), map_coords::ToFixed(atomPosition.z), 0.0f});
	if (auto* fixed = registry.TryGet<Fixed>(object); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform->position.x, transform->position.z);
	}
	// the object's own class ends its physics with no body, while the carried mark is still set (the class hooks'
	// EndPhysicsWithoutBody): a villager or an animal lands; a tree does nothing, so it is NOT put back in the map cells
	// and stays in the physics, with no body (as a hanging thing does); a hand's pile of wood or food on land becomes a
	// land pile; a dead tree tells the villagers "wood here"; a scaffold's end; anything else back in the map (InBounds)
	// or deleted
	auto& dynamics = Locator::dynamicsSystem::value();
	dynamics.ClassHooks().EndPhysicsWithoutBody(dynamics, object);
	// the carried mark cleared, and out of the physics (a carried object has no body of its own), but for a tree, whose
	// end of physics did nothing
	if (registry.Valid(object))
	{
		registry.Remove<CarriedByTornado>(object);
		if (!registry.AllOf<Tree>(object) && !dynamics.IsFlying(object))
		{
			registry.RemoveState<InPhysics>(object);
		}
	}
	registry.SetDirty();
}

particle_carried_objects::FlingMotion particle_carried_objects::FlingMotionOf(float h, float angle, float speed, float randB,
                                                                              float randC)
{
	// B = (GameFloatRand(5.0) + 8.0) x speed
	const float b = (randB + 8.0f) * speed;
	// sin / cos of the angle (unrounded), times B
	const auto vx = static_cast<float>(std::sin(static_cast<double>(angle)) * b);
	const auto vz = static_cast<float>(-(std::cos(static_cast<double>(angle)) * b));
	// C = (GameFloatRand(5.0) + 10.0) x speed
	const float c = (randC + 10.0f) * speed;
	// w = (0 - v.z, 0, v.x - 0) x (1 / h), from the stored (rounded) v.z and v.x; 1 / h first, then each product
	const float invH = 1.0f / h;
	return {glm::vec3(vx, c, vz), glm::vec3((0.0f - vz) * invH, 0.0f * invH, (vx - 0.0f) * invH)};
}

void particle_carried_objects::Fling(entt::entity object, entt::entity thrower, float angle, float speed)
{
	// h = half the object's height
	const float h = object::GetHeight(object) * 0.5f;
	// the two draws in the original's order: B's, then C's
	const float randB = game_random::GameFloatRand(5.0f);
	const float randC = game_random::GameFloatRand(5.0f);
	const auto motion = FlingMotionOf(h, angle, speed, randB, randC);
	// physics with v, w, the vortex as thrower and a body: AddObject takes w
	// in the body's axes and the original's sign
	Locator::dynamicsSystem::value().AddObject(object, motion.velocity,
	                                           ecs::systems::DynamicsSystemInterface::BodySpin {motion.spin}, thrower, false);
}
