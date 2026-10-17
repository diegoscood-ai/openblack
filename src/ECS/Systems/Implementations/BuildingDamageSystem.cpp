/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "BuildingDamageSystem.h"

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Debug/TraceLog.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Physics/Hitter.h"
#include "ECS/Physics/PartialBuild.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/GameBuildingDamageWorld.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "EngineConfig.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Physics/DamageMesh.h"
#include "Physics/TurnRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;
using openblack::ecs::systems::BuildingDamageSystem;
using openblack::ecs::systems::DamageMeshDraw;
using openblack::ecs::systems::GameBuildingDamageWorld;
using DamageMesh = openblack::physics::damage::Mesh;

BuildingDamageSystem::BuildingDamageSystem()
    : BuildingDamageSystem(std::make_unique<GameBuildingDamageWorld>())
{
}

BuildingDamageSystem::BuildingDamageSystem(std::unique_ptr<openblack::ecs::building_world::World> world)
    : _world(std::move(world))
{
}

void BuildingDamageSystem::EraseMesh(entt::id_type id)
{
	if (id != 0 && _world->Resources().GetMeshes().Contains(id))
	{
		_world->Resources().GetMeshes().Erase(id);
	}
}

/// Redraws the damaged building from its damage mesh (a building with one draws it instead of its mesh).
void BuildingDamageSystem::RedrawBuilding(entt::entity building, BuildingDamage& damage)
{
	auto& registry = _world->Entities();
	const auto& transform = registry.Get<const Transform>(building);
	const auto toWorld = affine::Model(transform);
	// The damage mesh, then the intact model partly built at GetPercentForDrawBuilding (a blow once the percent is in
	// [0.2, 1) makes the damage mesh afresh from the intact model: ReactToImpact)
	// The percent is how far the building is repaired since the damage: with a damage mesh and the repair site made at the
	// hit, (life - the site's starting life) / (1 - the site's starting life)
	const float percent = ecs::abodes::GetPercentForDrawBuilding(building);
	// the morphable's melting: the land deltas stay once MorphWithTerrain is taken off below
	PartialBuildOptions options;
	options.melting = damage.morphed || registry.AllOf<MorphWithTerrain>(building);
	auto partial = percent < 1.0f ? PartialBuild::Build(building, damage.intactMesh, percent, options)
	                              : std::vector<graphics::L3DSubMesh::GeneratedPrimitive> {};
	const auto id = damage.mesh->BuildMesh(glm::inverse(toWorld), "fragmesh", std::move(partial));
	auto& meshes = _world->Resources().GetMeshes();
	if (id != 0 && meshes.Contains(damage.intactMesh))
	{
		// the building's mark on the landscape stays (a broken building can still be repaired)
		meshes.Handle(id)->SetFootprintSource(meshes.Handle(damage.intactMesh).handle());
	}
	// the Mesh stays the intact model (components::DrawMesh: sizes, map cells, the static shadow, the body); the
	// damage mesh's model is the building's DrawMesh. A damaged building draws it instead of the partly built model, so the
	// construction draw goes as abodes::RedrawConstruction takes it away for HasDestructionMesh. Removed before the
	// Assign: the on_destroy<DrawMesh> (Abodes.cpp OnDrawMeshDestroyed) erases the model it held, ours or the
	// construction draw's
	const auto old = damage.generatedMesh;
	registry.Remove<AbodeConstructionDraw, DrawMesh, NotDrawn>(building);
	damage.generatedMesh = id;
	if (id != 0)
	{
		// every sub-mesh of the generated one (a big building needs several)
		const auto submesh = meshes.Handle(id)->GetNumSubMeshes() > 1 ? static_cast<int8_t>(-1) : static_cast<int8_t>(0);
		registry.Assign<DrawMesh>(building, id, submesh, registry.Get<const Mesh>(building).bbSubmeshId);
	}
	// that sink is connected only once some building site has been drawn: the previous model is erased here as well
	// (EraseMesh checks Contains: nothing to do after OnDrawMeshDestroyed)
	EraseMesh(old);
	// the damage mesh has the landscape morph baked in
	if (registry.AllOf<MorphWithTerrain>(building))
	{
		damage.morphed = true;
		registry.Remove<MorphWithTerrain>(building);
	}
	registry.SetDirty();
}

/// A piece of the building flies: the fragment is made, its body added, then its dust shed; a sliver goes after both.
void BuildingDamageSystem::CreateFragment(const DamageMesh::Piece& piece, entt::entity parent)
{
	auto& registry = _world->Entities();
	const float area = piece.mesh->Area();
	const auto id = piece.mesh->BuildMesh(glm::mat4(1.0f), "fragment");
	if (id == 0)
	{
		return;
	}
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, piece.centre, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, id, static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& fragment = registry.Assign<Fragment>(entity);
	fragment.mesh = piece.mesh;
	fragment.parent = parent;
	fragment.generatedMesh = id;
	fragment.turnsLeft = 100 * static_cast<int>(piece.lifeTriangles);
	fragment.area = area;
	if (debug_env::TraceLogs(std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr, "game"))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: fragment {} tris area {:.1f} v ({:.1f},{:.1f},{:.1f}) w {:.2f}",
		                   piece.mesh->TriangleCount(), area, piece.velocity.x, piece.velocity.y, piece.velocity.z,
		                   glm::length(piece.angularVelocity));
	}
	// its body first
	if (auto* po = _world->Dynamics().AddObject(entity, piece.velocity, piece.angularVelocity, parent))
	{
		po->flags |= ecs::PhysicsEntry::k_NoObjectCollision;
	}
	// then a dust puff at each distinct vertex but the first (colour 0x80706050, size 2, +-2 units per second)
	for (const auto& p : piece.mesh->DustCorners())
	{
		_world->Dynamics().AddPuff(piece.centre + p, _world->Dynamics().SyncedPuffVelocity(), 2.0f, 0x80706050u);
	}
	// a sliver goes only now, with its body and its dust made
	if (piece.mesh->IsSliver())
	{
		DestroyFragment(entity);
		return;
	}
	registry.SetDirty();
}

bool BuildingDamageSystem::ReactToImpact(entt::entity building, PhysicsEntry& po)
{
	auto& registry = _world->Entities();
	auto* hit = ecs::physics::hitter::HitterEntry(_world->Dynamics().GetState().objects, po);
	if (hit == nullptr || !registry.Valid(hit->entity))
	{
		return true;
	}
	// the player of the hand that threw it, inherited by what it hit (openblack's byPlayer, the local player's hand)
	const std::optional<PlayerNames> player = hit->byPlayer ? std::optional(PlayerNames::PLAYER_ONE) : std::nullopt;
	// Before the test of what hit it: with a player, and the building's own body marked as thrown from the hand, the
	// player's creature may learn to copy the damage by throwing. The test is on the building's body, as the original's
	// is; a building's body is a resting obstacle that nothing marks so, so in practice it does not happen
	if (ecs::creature_mimic::ShouldMimicBuildingHit(player, po.flags))
	{
		ecs::creature_mimic::Consider(*player, creature_watching::Deed::DamageByThrowingAt, building);
	}
	if (!_world->Dynamics().PhysicallyDestroysAbodes(hit->entity))
	{
		return true;
	}
	// (not ported, no creature) byCreature when the thrower (the proxy's, else the hitter's) is a creature
	constexpr bool byCreature = false;
	const float p = glm::length(hit->body.velocity) * hit->body.Mass();
	if (debug_env::TraceLogs(std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr, "game"))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} hit by {} p {:.0f} (v {:.1f}, m {:.0f})",
		                   static_cast<uint32_t>(building), static_cast<uint32_t>(hit->entity), p,
		                   glm::length(hit->body.velocity), hit->body.Mass());
	}
	if (p > 2000.0f && !ecs::abode_queries::IsBuilt(building))
	{
		// not built (a building site): no damage mesh, the damage goes straight to the life (the crush effect preset times
		// a crush defence of 0.2)
		return ecs::abodes::OnPhysicalDamage(building, {std::nullopt, hit->entity, player, byCreature});
	}
	if (p > 2000.0f)
	{
		auto* damage = registry.TryGet<BuildingDamage>(building);
		if (damage == nullptr)
		{
			damage = &registry.Assign<BuildingDamage>(building);
			damage->intactMesh = registry.Get<const Mesh>(building).id;
		}
		// The damage mesh the blow strikes: the intact model's on the first blow; made afresh from the intact model when the
		// building has been repaired enough since it was last broken (drawn at least a fifth repaired, but not wholly);
		// otherwise a copy of the building's. (openblack, for the draw snapshot) the copy takes the old one's place once
		// broken, so a DamageMeshDraw taken before keeps the old one. A town's building is drawn a little repaired right
		// after a blow (its repair site starts just below its life), so it is made afresh only once its villagers have
		// raised its life; a building with no town has no repair site and is made afresh by every later blow
		const bool followsLand = damage->morphed || registry.AllOf<MorphWithTerrain>(building);
		auto model = openblack::physics::damage::ModelForBlow(
		    damage->mesh, ecs::abodes::GetPercentForDrawBuilding(building),
		    [building, followsLand]() { return DamageMesh::FromEntity(building, followsLand); });
		if (!model.mesh)
		{
			return true;
		}
		if (model.rebuilt)
		{
			// the new damage mesh has no last hitter, and this blow does not become it
			damage->lastHitter = entt::null;
		}
		else if (damage->mesh)
		{
			if (damage->lastHitter == hit->entity)
			{
				// the same rock again: they stop colliding, so it goes through
				po.thrower = hit->entity;
				hit->thrower = building;
			}
			else
			{
				damage->lastHitter = hit->entity;
			}
		}
		auto broken = std::move(model.mesh);
		// Each piece's fragment is made as the impact makes the piece, before the remaining part is read
		size_t pieces = 0;
		broken->Impact(hit->body.Centre(), hit->body.velocity * 0.3f, hit->body.Radius() + 0.7f,
		               [this, building, &pieces](const DamageMesh::Piece& piece) {
			               ++pieces;
			               CreateFragment(piece, building);
		               });
		const float remaining = broken->GetRemaining();
		damage->mesh = std::move(broken);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: impact p {:.0f}, {} pieces, {:.2f} left", p, pieces, remaining);
		if (remaining >= 1.0f)
		{
			// nothing counted as lost (the broken part was only halved triangles): the damage mesh goes and the building
			// draws whole again, with no damage and no sound
			RemoveDamage(building);
			return true;
		}
		// the life (and the repair baseline) first: the redraw's partly built percent comes from them.
		if (!ecs::abodes::OnPhysicalDamage(building, {remaining, hit->entity, player, byCreature}))
		{
			return false;
		}
		RedrawBuilding(building, registry.Get<BuildingDamage>(building));
		return true;
	}
	const auto at = registry.Get<const Transform>(building).position;
	// p > 1000 level 2, p > 300 level 3: a rock on the ground sound from editor.sad's table (G_Rock_V_Ground_M / _S)
	if (p > 1000.0f)
	{
		_world->Dynamics().PlayAnimEffect({2, 0, 0x16, 0x10, 75}, building, at, false);
	}
	else if (p > 300.0f)
	{
		_world->Dynamics().PlayAnimEffect({3, 0, 0x16, 0x10, 75}, building, at, false);
	}
	return true;
}

entt::entity BuildingDamageSystem::PieceAtRest(entt::entity fragment, const glm::mat3& rows, glm::vec3 centre)
{
	auto& registry = _world->Entities();
	const auto* found = registry.TryGet<const Fragment>(fragment);
	if (found == nullptr)
	{
		return fragment;
	}
	// the piece as it was, read first: the object's end may delete it
	const Fragment f = *found;
	// the end of any object: an available fragment goes back in the map cells, or is deleted outside them. An
	// unavailable one is neither, and stays
	const bool deleted = ecs::IsAvailable(fragment) && !_world->Dynamics().EndPhysicsAsObject(fragment);
	const auto kept = deleted ? entt::entity {entt::null} : fragment;
	// a piece with no mesh keeps its building
	if (!f.mesh)
	{
		return kept;
	}
	// a big piece sticks to its building while that is available and broken, even one the object's end deleted
	auto* damage = ecs::IsAvailable(f.parent) ? registry.TryGet<BuildingDamage>(f.parent) : nullptr;
	if (f.area > 9.0f && damage != nullptr && damage->mesh)
	{
		// (openblack, for the draw snapshot) merged into a copy, which takes the old damage mesh's place
		auto merged = std::make_shared<DamageMesh>(*damage->mesh);
		merged->Merge(*f.mesh, rows, centre);
		damage->mesh = std::move(merged);
		RedrawBuilding(f.parent, *damage);
		// then the piece goes (one the object's end deleted is gone already)
		if (ecs::IsAvailable(fragment))
		{
			DestroyFragment(fragment);
		}
		return entt::null;
	}
	if (auto* still = registry.TryGet<Fragment>(fragment); still != nullptr)
	{
		still->parent = entt::null;
	}
	return kept;
}

void BuildingDamageSystem::Redraw(entt::entity building)
{
	if (auto* damage = _world->Entities().TryGet<BuildingDamage>(building); damage != nullptr && damage->mesh)
	{
		RedrawBuilding(building, *damage);
	}
}

void BuildingDamageSystem::RemoveDamage(entt::entity building)
{
	auto& registry = _world->Entities();
	auto* damage = registry.TryGet<BuildingDamage>(building);
	if (damage == nullptr)
	{
		return;
	}
	damage->mesh.reset();
	damage->lastHitter = entt::null; // it went with the damage mesh
	if (damage->generatedMesh != 0 || damage->morphed)
	{
		// the damage mesh's DrawMesh goes (only ours: the Mesh was never changed), then its model (a no-op when
		// OnDrawMeshDestroyed has erased it)
		if (const auto* draw = registry.TryGet<const DrawMesh>(building); draw != nullptr && draw->id == damage->generatedMesh)
		{
			registry.Remove<DrawMesh>(building);
		}
		EraseMesh(damage->generatedMesh);
		damage->generatedMesh = 0;
		if (damage->morphed && !registry.AllOf<MorphWithTerrain>(building))
		{
			registry.Assign<MorphWithTerrain>(building);
		}
		damage->morphed = false;
		// without the damage mesh a building with a site is drawn partly built again (the construction DrawMesh; nothing for
		// one without a site)
		ecs::abodes::RedrawConstruction(building);
		registry.SetDirty();
	}
}

namespace
{
/// The destroy listener's erase, as BuildingDamageSystem::EraseMesh: entt calls the listener without the system, so it
/// reaches the mesh cache through the locator
void EraseDestroyedMesh(entt::id_type id)
{
	if (id != 0 && Locator::resources::value().GetMeshes().Contains(id))
	{
		Locator::resources::value().GetMeshes().Erase(id);
	}
}

/// The mesh cache only (no component, no map cell, no physics: DynamicsSystem::RemoveObject would touch the
/// entity's PhysicsDrawPose and the cells, so it must not run inside the destroy signal). A no-op when the DrawMesh's
/// listener erased the same model (EraseMesh checks Contains). The component is still there during the signal.
void OnBuildingDamageDestroyed(entt::registry& registry, entt::entity entity)
{
	// (openblack, guard) a reset after the resources have gone (shutdown)
	if (Locator::resources::has_value())
	{
		EraseDestroyedMesh(registry.get<BuildingDamage>(entity).generatedMesh);
	}
}
} // namespace

void BuildingDamageSystem::ConnectDamageListener()
{
	// entt's sink::connect disconnects the same listener first: idempotent
	_world->Entities().OnDestroy<BuildingDamage>().connect<&OnBuildingDamageDestroyed>();
}

void BuildingDamageSystem::ProcessTurn()
{
	auto& registry = _world->Entities();
	std::vector<entt::entity> expired;
	registry.Each<Fragment>([&](entt::entity entity, Fragment& f) {
		// a parent no longer available is forgotten
		if (f.parent != entt::null && !ecs::IsAvailable(f.parent))
		{
			f.parent = entt::null;
		}
		if (--f.turnsLeft <= 0)
		{
			expired.push_back(entity);
		}
	});
	for (const auto entity : expired)
	{
		DestroyFragment(entity);
	}
}

void BuildingDamageSystem::ForgetHitter(entt::entity hitter, entt::entity building)
{
	auto& registry = _world->Entities();
	if (building != entt::null)
	{
		// with a hitter given, only when it is the building's last hitter
		if (auto* damage = registry.TryGet<BuildingDamage>(building);
		    damage != nullptr && damage->mesh && (hitter == entt::null || damage->lastHitter == hitter))
		{
			damage->lastHitter = entt::null;
		}
		return;
	}
	registry.Each<BuildingDamage>([hitter](entt::entity, BuildingDamage& damage) {
		if (damage.mesh && damage.lastHitter == hitter)
		{
			damage.lastHitter = entt::null;
		}
	});
}

void BuildingDamageSystem::DestroyFragment(entt::entity fragment)
{
	auto& registry = _world->Entities();
	if (!registry.Valid(fragment))
	{
		return;
	}
	_world->Dynamics().RemoveObject(fragment);
	if (const auto* f = registry.TryGet<const Fragment>(fragment))
	{
		EraseMesh(f->generatedMesh);
	}
	ecs::map_cells::RemoveMapObject(fragment); // out of the map cells, as any deleted object
	registry.Destroy(fragment);
	registry.SetDirty();
}

void BuildingDamageSystem::AppendDamageMeshes(graphics::world_triangles::Frame& out, const DamageMesh::FrameLight& frame,
                                              float turnTime)
{
	std::vector<DamageMeshDraw> draws; // this frame's (no damage mesh held on)
	SnapshotDamageMeshes(draws, frame, turnTime);
	AppendDamageMeshes(out, draws);
}

void BuildingDamageSystem::AppendDamageMeshes(graphics::world_triangles::Frame& out, const std::vector<DamageMeshDraw>& draws)
{
	for (const auto& draw : draws)
	{
		draw.mesh->AppendDraw(out, draw.world ? &*draw.world : nullptr, draw.light);
	}
}

void BuildingDamageSystem::SnapshotDamageMeshes(std::vector<DamageMeshDraw>& out, const DamageMesh::FrameLight& frame,
                                                float turnTime)
{
	out.clear();
	auto& registry = _world->Entities();
	// a broken building's damage mesh (only while it is what the building draws)
	registry.Each<const BuildingDamage, const Transform, const DrawMesh>(
	    [&](entt::entity entity, const BuildingDamage& damage, const Transform& transform, const DrawMesh& draw) {
		    if (!damage.mesh || damage.generatedMesh == 0 || draw.id != damage.generatedMesh)
		    {
			    return;
		    }
		    // with a fire the tint is the charring grey and the specular tint the glow, both with alpha 0xFF; else
		    // 0xFFFFFFFF / 0. The same pair as the other burning objects (RenderingSystem's Burning)
		    uint32_t tint = 0xFFFFFFFFu;
		    uint32_t tintSpecular = 0;
		    if (const auto* fire = ecs::fire::Find(entity); fire != nullptr)
		    {
			    const uint32_t grey = ecs::fire::graphic::CharringGrey(*fire);
			    // turnTime = the frame's DrawSceneDesc::clock: the same value as the live read
			    const auto glow = ecs::fire::graphic::CharringGlow(*fire, turnTime);
			    tint = argb_colour::Argb(grey, grey, grey, 0xFF);
			    tintSpecular = argb_colour::Argb(glow.r, glow.g, glow.b, 0xFF);
		    }
		    // no matrix (the triangles are in the world), the building's position
		    out.push_back({damage.mesh, std::nullopt, DamageMesh::ObjectLight(transform.position, tint, tintSpecular, frame),
		                   transform.position, tint, tintSpecular});
	    });
	// a fragment's world matrix and its translation; its tint stays the damage mesh's default 0xFFFFFFFF / 0, nothing sets
	// it again. While it flies that matrix is the pose between its last two turns (components::PhysicsDrawPose), the
	// scale Transform's
	// a flying one wholly under the water is drawn by nothing, as the original's physics list skips it (a resting one is
	// in the map and drawn)
	// the awake physics entries wholly under the water, once for the snapshot (by entity, sorted)
	std::vector<entt::entity> sunk;
	if (!(Locator::config::has_value() && Locator::config::value().drawSunkPhysicsObjects) &&
	    Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().ForEachEntry([&sunk](const ecs::PhysicsEntry& entry) {
			if (!entry.body.resting && openblack::physics::turn::SunkOutOfSight(entry.body.Centre().y, entry.body.Radius()))
			{
				sunk.push_back(entry.entity);
			}
		});
		std::ranges::sort(sunk);
	}
	registry.Each<const Fragment, const Transform>(
	    [&](entt::entity entity, const Fragment& fragment, const Transform& transform) {
		    if (!fragment.mesh)
		    {
			    return;
		    }
		    const auto* flying = registry.TryGet<const PhysicsDrawPose>(entity);
		    // by its physics entry: one already sunk on its first frame in the physics has no draw pose yet
		    if (std::ranges::binary_search(sunk, entity))
		    {
			    return;
		    }
		    const auto position = flying != nullptr ? flying->position : transform.position;
		    const auto matrix = flying != nullptr ? affine::Model(flying->position, flying->rotation, transform.scale)
		                                          : affine::Model(transform);
		    out.push_back(
		        {fragment.mesh, matrix, DamageMesh::ObjectLight(position, 0xFFFFFFFFu, 0, frame), position, 0xFFFFFFFFu, 0});
	    });
}
