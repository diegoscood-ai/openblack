/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a released object does when the hand lets it go: Object::ThrowObjectFromHand 0x6385E0 and
// Object::InitialisePhysicsFromHand 0x636F00 (bw1-decomp src/Black/Object.cpp:447)

#include "FromHand.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/WaterRings.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PhysicsObjects.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace openblack::ecs::physics::from_hand
{
namespace
{
HandHooks g_Hooks;

bool IsHandPot(entt::entity entity)
{
	return g_Hooks.isHandPot && g_Hooks.isHandPot(entity);
}

/// LHMatrix::GetYXZ 0x7FAB30 (confirmed by emulation, dev\documentacion\agua\re\emu_getyxz.py) with the rows = the body
/// axes (openblack's columns): x = asin(r2.y), z = atan2(-r0.y, r1.y); a uniform scale does not change them.
glm::vec2 TiltXZ(const glm::mat3& rotation)
{
	const auto& r0 = rotation[0];
	const auto& r1 = rotation[1];
	const auto& r2 = rotation[2];
	return glm::vec2(std::atan2(r2.y, std::sqrt(r2.x * r2.x + r2.z * r2.z)), std::atan2(-r0.y, r1.y));
}

} // namespace

void SetHandHooks(HandHooks hooks)
{
	g_Hooks = std::move(hooks);
}

bool IsFence(entt::entity entity)
{
	const auto* statics = Locator::entitiesRegistry::value().TryGet<const MobileStatic>(entity);
	if (statics == nullptr || !Locator::infoConstants::has_value() || statics->type == MobileStaticInfo::None)
	{
		return false;
	}
	const auto mesh = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics->type)).meshId;
	return mesh == MeshId::BuildingAmericanFence || mesh == MeshId::BuildingCelticFenceShort ||
	       mesh == MeshId::BuildingCelticFenceTall;
}

void PlaceWithoutBody(entt::entity entity)
{
	// (approximate) openblack only: an object PhysicsObject::AddObject cannot build a body for (no mesh) is put where it
	// is, on the ground (Living::InitialisePhysicsFromHand 0x5EFDF8 ends the physics of a Living at once when AddObject
	// fails; an Object stays IN_PHYSICS where the hand left it, step2_throw.md)
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(entity);
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
	                         : 0.0f;
	transform.position.y = ground;
	if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	// its EndPhysics at once (Living::InitialisePhysicsFromHand 0x5EFDF8): Object::EndPhysics 0x6375A0 puts it back in
	// the map cells (InsertMapObject vt +0x544 at 0x63762C)
	map_cells::InsertMapObject(entity);
	if (registry.AllOf<Villager>(entity) && sea_cells::IsWater(transform.position))
	{
		VillagerEndPhysicsInWater(entity);
	}
	else
	{
		SetVillagerState(entity, VillagerStates::Landed);
	}
	animal_ai::PutDown(entity);
	registry.SetDirty();
}

std::optional<bool> InitialisePhysicsFromHand(entt::entity entity, glm::vec3 velocity, bool dontReplant)
{
	// Object::InitialisePhysicsFromHand 0x636F00 (bw1-decomp src/Black/Object.cpp:447), from the hand (thrower NULL).
	// Living 0x5EFD80 / Villager 0x5EFE90 add FLYING and the disciple around it: FLYING (the class's initialisePhysics)
	// at the end, only when the object does not land (0x5EFDD7..0x5EFDEB).
	// TODO(physics): the angular velocity (GInterfaceStatus::ThrowAngularVelocity) is 0.
	auto& registry = Locator::entitiesRegistry::value();
	const bool tree = registry.AnyOf<Tree, DeadTree>(entity); // IsAnyKindOfTree (vt +0x478)
	// AddObject's own fromHand path would spread the flying-object reaction at once, but here it is only spread when
	// the object does not land (0x637412, below); PHYSICS_OBJECT_FLAG_FROM_HAND and the player are set by
	// AddObjectFromHand
	auto* po = PhysicsObjects::AddObjectFromHand(entity, velocity, glm::vec3(0.0f));
	if (po == nullptr)
	{
		return std::nullopt;
	}
	// thrown = |v.xz|^2 > 4 from the hand (> 1 from a creature); either way AdjustToGroundLevel(thrown, !tree) (thrown:
	// only raised out of the ground), ZeroForces, RaiseUntilNotIntersecting and PHYSICS_OBJECT_FLAG_FROM_HAND
	const bool thrown = velocity.x * velocity.x + velocity.z * velocity.z > 4.0f;
	po->body.AdjustToGroundLevel(thrown, !tree);
	// AdjustToGroundLevel ends copying the matrix into the turn-start one (0x7FCE6B): a put-down villager's landType
	// comes from the ground-aligned rows, not the hand's (RaiseUntilNotIntersecting does the same when it raises it)
	PhysicsObjects::SyncTurnStart(*po);
	const float oldAltitude = po->body.Centre().y;
	po->body.ZeroForces();
	PhysicsObjects::RaiseUntilNotIntersecting(*po);
	const auto at = po->body.Centre();
	// 0x637128: not thrown and not raised over something (the exception of a computer player's villager has no hand
	// here): landed on dry land, or on a cell (rounded, fistp) of altitude > 1
	bool landed = false;
	if (!thrown && oldAltitude == at.y && Locator::terrainSystem::has_value())
	{
		const auto& island = Locator::terrainSystem::value();
		landed = sea_cells::IsDryLand(island, sea_cells::CellOf(at)) ||
		         sea_cells::AltitudeAt(island, sea_cells::RoundedCellOf(at)) > 1; // 0x637231: cmp [cell + 4], 1; jbe
	}
	const bool living = registry.AnyOf<Villager, Animal>(entity); // IsLiving (vt +0x3C4)
	const bool fence = IsFence(entity);
	if (landed && (living || fence) && LandscapeNormal(at).y < 0.7f)
	{
		landed = false; // 0x6372A6: too steep to stand on
	}
	const auto tilt = TiltXZ(registry.Get<const Transform>(entity).rotation);
	const char* outcome = "in physics";
	if (landed)
	{
		// TODO(villager-jobs): GGuidance::MakeDiscipleSFX when the villager's disciple changed (0x6372E8).
		// TODO(creature): ConsiderCreatureMimickingWhenObjectLands (0x637306).
		po->flags |= PhysicsObject::Landed;
		// Living, Fence, or a Tree with no FireEffect (+0x44, ECS/Fire) on land: out of physics at once, where it is,
		// through its EndPhysics; a tree held tilted (GetYXZ: |x| or |z| > 0.2) or dont_replant stays in physics, not
		// LANDED. A hot or burning tree stays in physics, LANDED (bw1-decomp Object.cpp:567), and ends a DeadTree.
		const bool treeOnLand = registry.AllOf<Tree>(entity) && fire::Find(entity) == nullptr && sea_cells::IsLand(at);
		if (living || fence || treeOnLand)
		{
			if (treeOnLand && (dontReplant || std::abs(tilt.x) > 0.2f || std::abs(tilt.y) > 0.2f))
			{
				po->flags &= ~PhysicsObject::Landed;
				landed = false;
				outcome = "tilted tree: in physics";
			}
			else
			{
				PhysicsObjects::RemoveObjectWithEndPhysics(entity);
				outcome = "landed: out of physics";
			}
		}
		else
		{
			outcome = "landed: in physics";
		}
	}
	else
	{
		if (registry.AllOf<Villager>(entity))
		{
			// 0x6373F4..0x6373FA: Villager::CreateDroppedResource 0x750940(&velocity, NULL, &angular_velocity) (session
			// Personas, ECS/Villager); the hand's angular velocity is 0 (ThrowAngularVelocity, step2_throw.md)
			ecs::villager::CreateDroppedResource(entity, velocity, std::nullopt, glm::vec3(0.0f));
		}
		// Reaction::CreateReaction(this, REACTION_REACT_TO_FLYING_OBJECT 9, player, 0) 0x637412: spread once
		// (SpreadReaction 0x6E3E10) to the Livings near it; the thrower is the hand's player (PLAYER_ONE's interface)
		animal_ai::SpreadFlyingObjectReaction(entity, PlayerNames::PLAYER_ONE);
		// TODO(creature): Creature::CheckAllCreaturesForCatching 0x47CBD0.
	}
	// TODO(creature): a toy -> ConsiderMakingCreatureMimicPlayer(DETECTED_PLAYER_ACTION_PLAY_WITH_TOY) (0x637457).
	// Living::InitialisePhysicsFromHand, once Object's has returned: po not NULL (test edi, edi 0x5EFDD3), not LANDED
	// (test [po + 0x1D8], 8 0x5EFDD7) and po->object still this one (cmp [po + 0x18], esi 0x5EFDE0) -> SetTopState(FLYING
	// 0xA) 0x5EFDEB. Find(entity) is the "still this one" (the ones taken out of the physics are gone), so a put-down
	// villager or animal goes to LANDED through its EndPhysics without passing through FLYING.
	if (auto* now = PhysicsObjects::Find(entity); now != nullptr && (now->flags & PhysicsObject::Landed) == 0)
	{
		PhysicsObjects::InitialisePhysicsOfClass(*now, true);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Hand: released {} at ({:.1f}, {:.1f}, {:.1f}) v ({:.1f}, {:.1f}, {:.1f}) thrown {} tilt ({:.2f}, {:.2f}): {}",
	                   static_cast<uint32_t>(entity), at.x, at.y, at.z, velocity.x, velocity.y, velocity.z, thrown, tilt.x, tilt.y,
	                   outcome);
	registry.SetDirty();
	return landed;
}

bool Throw(entt::entity entity, glm::vec3 velocity, bool dontReplant)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	// Pot::InitialisePhysicsFromHand 0x66DF00: |v|^2 <= 5 (all three axes) puts the resource down at once
	// (PSysGlobal::StartMultiPutdown, Pot::AddResourceToPos, GoolooGooloo, ToBeDeleted); faster it flies as an Object
	if (IsHandPot(entity) && glm::dot(velocity, velocity) <= 5.0f)
	{
		if (g_Hooks.putDownHandPot)
		{
			g_Hooks.putDownHandPot(entity);
		}
		return true;
	}
	// TODO(villager-jobs): Villager::ThrowObjectFromHand 0x756AE0 first clears the disciple (SetVillagerDisciple(0, 0, 0))
	// when the villager is the player's or nobody's.
	if (const auto landed = InitialisePhysicsFromHand(entity, velocity, dontReplant))
	{
		return *landed;
	}
	PlaceWithoutBody(entity); // no body could be built (no mesh)
	return true;
}

bool ForceDrop(entt::entity entity)
{
	return Throw(entity, glm::vec3(0.0f), true);
}

} // namespace openblack::ecs::physics::from_hand
