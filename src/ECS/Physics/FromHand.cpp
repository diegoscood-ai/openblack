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

/// openblack only: an object thrown with no physics body, flying ballistically
struct Thrown
{
	entt::entity entity;
	glm::vec3 velocity;
	float altitude;
};
std::vector<Thrown> g_Thrown;

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

/// Villager::CreateDroppedResource 0x750940, called for a villager released without landing: a villager whose
/// carrying type (+0xF1) is 2..15 and whose wood (+0xF6) is over GVillagerInfo::minWoodToShowGraphic (+0x26C) lets
/// his log fall: a DeadTree (fn_00510BB0, the mesh of table 0xC5E19C, scale 1, angle pi/2, woodValue = wood /
/// GetWoodValue) put into physics with the villager's velocity and angular velocity, flag 0x10,
/// AdjustToGroundLevel(false, true), RaiseUntilNotIntersecting, then DropWood(0).
/// TODO(villager-jobs): openblack's villagers carry no wood yet, so there is nothing to drop.
void CreateDroppedResource([[maybe_unused]] entt::entity villager, [[maybe_unused]] glm::vec3 velocity) {}
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
	// openblack only: an object PhysicsObject::AddObject cannot build a body for (no mesh) is put where it is, on the
	// ground (Living::InitialisePhysicsFromHand 0x5EFDF8 ends the physics of a Living at once when AddObject fails)
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
	// Living 0x5EFD80 / Villager 0x5EFE90 add FLYING and the disciple around it (AddObject sets FLYING in openblack).
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
			CreateDroppedResource(entity, velocity);
		}
		// Reaction::CreateReaction(this, REACTION_REACT_TO_FLYING_OBJECT 9, player, 0) 0x637412: spread once
		// (SpreadReaction 0x6E3E10) to the Livings near it; the thrower is the hand's player (PLAYER_ONE's interface)
		animal_ai::SpreadFlyingObjectReaction(entity, PlayerNames::PLAYER_ONE);
		// TODO(creature): Creature::CheckAllCreaturesForCatching 0x47CBD0.
	}
	// TODO(creature): a toy -> ConsiderMakingCreatureMimicPlayer(DETECTED_PLAYER_ACTION_PLAY_WITH_TOY) (0x637457).
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Hand: released {} at ({:.1f}, {:.1f}, {:.1f}) v ({:.1f}, {:.1f}, {:.1f}) thrown {} tilt ({:.2f}, {:.2f}): {}",
	                   static_cast<uint32_t>(entity), at.x, at.y, at.z, velocity.x, velocity.y, velocity.z, thrown, tilt.x, tilt.y,
	                   outcome);
	registry.SetDirty();
	return landed;
}

bool Throw(entt::entity entity, glm::vec3 velocity, bool dontReplant, float heldAltitude)
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
	// openblack only, no body could be built (no mesh): thrown, the old ballistic flight; put down, placed
	if (velocity.x * velocity.x + velocity.z * velocity.z > 4.0f)
	{
		g_Thrown.push_back({entity, velocity, heldAltitude});
		return false;
	}
	PlaceWithoutBody(entity);
	return true;
}

bool ForceDrop(entt::entity entity)
{
	return Throw(entity, glm::vec3(0.0f), true);
}

void UpdateThrown(float seconds)
{
	if (g_Thrown.empty() || seconds <= 0.0f)
	{
		return;
	}
	constexpr float k_Gravity = 30.0f;
	constexpr float k_AirDrag = 0.4f;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* terrain = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	for (auto& thrown : g_Thrown)
	{
		if (!registry.Valid(thrown.entity))
		{
			thrown.entity = entt::null;
			continue;
		}
		auto& transform = registry.Get<Transform>(thrown.entity);
		if (g_Hooks.updateRoots)
		{
			g_Hooks.updateRoots(thrown.entity);
		}
		thrown.velocity.y -= k_Gravity * seconds;
		thrown.velocity *= std::exp(-k_AirDrag * seconds);
		transform.position += thrown.velocity * seconds;
		const float ground = terrain != nullptr ? terrain->GetHeightAt(glm::vec2(transform.position.x, transform.position.z)) : 0.0f;
		if (transform.position.y <= ground + thrown.altitude && thrown.velocity.y < 0.0f)
		{
			transform.position.y = ground + thrown.altitude;
			// PhysicsObject::AttemptToAddSoundEvent 0x6465B7: off dry land a white ring at y 0.1 that grows 2 x the
			// object's radius, aging at 1 / radius (cell 0x3F); in deep water (no cell or altitude < 3 at the rounded
			// cell, 0x646683) fn_0074F2D0 scares the fish too
			if (!sea_cells::IsDryLand(transform.position))
			{
				const auto* island = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
				const auto* cell = island != nullptr ? sea_cells::CellAt(*island, sea_cells::RoundedCellOf(transform.position)) : nullptr;
				if (cell == nullptr || island->GetCellAltitude(*cell) < 3)
				{
					SplashWater(transform.position);
				}
				float radius = 1.0f;
				if (const auto* mesh = registry.TryGet<const Mesh>(thrown.entity);
				    mesh != nullptr && Locator::resources::value().GetMeshes().Contains(mesh->id))
				{
					radius = 0.5f * glm::length(Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox().Size()) *
					         transform.scale.x;
				}
				radius = std::max(radius, 0.01f);
				WaterRing ring;
				ring.position = glm::vec3(transform.position.x, 0.1f, transform.position.z);
				ring.growth = 2.0f * radius;
				ring.rate = 1.0f / radius;
				ring.cell = 0x3F;
				AddWaterRing(ring);
			}
			if (auto* fixed = registry.TryGet<Fixed>(thrown.entity); fixed != nullptr)
			{
				fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
			}
			if (IsHandPot(thrown.entity) && sea_cells::IsLand(transform.position))
			{
				if (g_Hooks.putDownHandPot)
				{
					g_Hooks.putDownHandPot(thrown.entity);
				}
			}
			else if (registry.AnyOf<Tree, DeadTree>(thrown.entity))
			{
				// Tree::ReactToPhysicsImpact: absorbed by a wood store it hits. Anything else: a thrown tree never
				// lands as planted (PHYSICS_OBJECT_FLAG_LANDED is only set by a gentle release) and becomes a DeadTree.
				if (const auto store = g_Hooks.findWoodStore ? g_Hooks.findWoodStore(transform.position) : std::nullopt; store)
				{
					if (g_Hooks.depositInStore)
					{
						g_Hooks.depositInStore(thrown.entity, *store);
					}
				}
				else if (registry.AllOf<Tree>(thrown.entity) && g_Hooks.makeDeadTree)
				{
					g_Hooks.makeDeadTree(thrown.entity, thrown.velocity);
				}
			}
			thrown.entity = entt::null;
		}
	}
	std::erase_if(g_Thrown, [](const Thrown& thrown) { return thrown.entity == entt::null; });
	registry.SetDirty();
}

std::vector<entt::entity> ThrownObjects()
{
	std::vector<entt::entity> entities;
	entities.reserve(g_Thrown.size());
	for (const auto& thrown : g_Thrown)
	{
		entities.push_back(thrown.entity);
	}
	return entities;
}
} // namespace openblack::ecs::physics::from_hand
