/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Buildings.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "CollisionSounds.h"
#include "Dust.h"
#include "FragMesh.h"
#include "PartialBuild.h"
#include "Locator.h"
#include "PhysicsObjects.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
void EraseMesh(entt::id_type id)
{
	if (id != 0 && Locator::resources::value().GetMeshes().Contains(id))
	{
		Locator::resources::value().GetMeshes().Erase(id);
	}
}

/// Redraws the damaged building from its FragMesh (Abode::Draw draws the DestructionMesh instead of the mesh).
void RedrawBuilding(entt::entity building, BuildingDamage& damage)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(building);
	const auto toWorld = lh_matrix::Model(transform);
	// Abode::Draw: the FragMesh, then the intact model partly built at GetPercentForDrawBuilding (no repair yet: the
	// percent stays where the last hit left it, 1/11)
	const auto* life = registry.TryGet<const Life>(building);
	const float l = life != nullptr ? life->value : 1.0f;
	const float percent = l >= 1.0f ? 1.0f : std::clamp((l - damage.repairBase) / (1.0f - damage.repairBase), 0.0f, 1.0f);
	auto partial = percent < 1.0f ? PartialBuild::Build(building, damage.intactMesh, percent)
	                              : std::vector<graphics::L3DSubMesh::GeneratedPrimitive> {};
	const auto id = damage.mesh->BuildMesh(glm::inverse(toWorld), "fragmesh", std::move(partial));
	auto& meshes = Locator::resources::value().GetMeshes();
	if (id != 0 && meshes.Contains(damage.intactMesh))
	{
		// the building's mark on the landscape stays (a broken building can still be repaired)
		meshes.Handle(id)->SetFootprintSource(meshes.Handle(damage.intactMesh).handle());
	}
	auto& mesh = registry.Get<Mesh>(building);
	const auto old = damage.generatedMesh;
	damage.generatedMesh = id;
	mesh.id = id != 0 ? id : damage.intactMesh;
	// every sub-mesh of the generated one (a big building needs several)
	mesh.submeshId = id != 0 && meshes.Handle(id)->GetNumSubMeshes() > 1 ? static_cast<int8_t>(-1) : static_cast<int8_t>(0);
	EraseMesh(old);
	// the FragMesh has the landscape morph baked in
	if (registry.AllOf<MorphWithTerrain>(building))
	{
		damage.morphed = true;
		registry.Remove<MorphWithTerrain>(building);
	}
	registry.SetDirty();
}

/// CreateFragment (0x76EB20) + Fragment::SetUpPhysOb (0x76EC50) + InitialisePhysics: a piece of the building flies.
void CreateFragment(const FragMesh::Piece& piece, entt::entity parent)
{
	auto& registry = Locator::entitiesRegistry::value();
	const float area = piece.mesh->Area();
	// the hull is the vertices and a copy 0.45 behind each: its radius is at least the farthest vertex
	const auto id = piece.mesh->BuildMesh(glm::mat4(1.0f), "fragment");
	if (id == 0)
	{
		return;
	}
	std::vector<glm::vec3> points;
	std::vector<glm::vec3> normals;
	piece.mesh->UniqueVertices(points, normals);
	float radius = 0.0f;
	for (size_t i = 0; i < points.size(); ++i)
	{
		radius = std::max({radius, glm::length(points[i]), glm::length(points[i] - 0.45f * normals[i])});
	}
	if (0.2f * radius > area / (2.0f * radius)) // a sliver goes at once
	{
		EraseMesh(id);
		return;
	}
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, piece.centre, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, id, static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& fragment = registry.Assign<Fragment>(entity);
	fragment.mesh = piece.mesh;
	fragment.parent = parent;
	fragment.generatedMesh = id;
	fragment.turnsLeft = 100 * static_cast<int>(piece.lifeTriangles); // Fragment ctor 0x76E9FB
	fragment.area = area;
	if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: fragment {} tris area {:.1f} v ({:.1f},{:.1f},{:.1f}) w {:.2f}",
		                   piece.mesh->TriangleCount(), area, piece.velocity.x, piece.velocity.y, piece.velocity.z,
		                   glm::length(piece.angularVelocity));
	}
	// one dust puff per distinct vertex (fn_845C20: 0x80706050, size 2, +-2 units per second)
	for (const auto& p : points)
	{
		Dust::Emit(piece.centre + p, Dust::RandomVelocity(), 0x80706050u, 2.0f);
	}
	if (auto* po = PhysicsObjects::AddObject(entity, piece.velocity, piece.angularVelocity, parent))
	{
		po->flags |= PhysicsObject::NoObjectCollision;
	}
	registry.SetDirty();
}

void StopBeingFunctional(entt::entity building)
{
	// TODO: villagers leave, stores' piles come loose, the town's emergency, a repair site (Abode::ReduceLife 0x405D90)
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} no longer works", static_cast<uint32_t>(building));
}

/// Abode::DestroyedByEffect (0x403F80): the villagers become homeless, a store loses its piles, the building goes.
void DestroyBuilding(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} destroyed", static_cast<uint32_t>(building));
	if (const auto* abode = registry.TryGet<const Abode>(building))
	{
		for (const auto villager : abode->inhabitants)
		{
			if (auto* v = registry.TryGet<Villager>(villager))
			{
				v->abode = entt::null;
				if (auto* town = registry.TryGet<Town>(v->town))
				{
					town->homelessVillagers.insert(villager);
				}
			}
		}
	}
	if (const auto* pit = registry.TryGet<const StoragePit>(building))
	{
		for (const auto pile : pit->woodPiles)
		{
			if (registry.Valid(pile))
			{
				ecs::map_cells::RemoveMapObject(pile); // CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548
				registry.Destroy(pile);
			}
		}
		if (registry.Valid(pit->foodPile))
		{
			ecs::map_cells::RemoveMapObject(pit->foodPile); // CleanupWhenDeleted 0x6377F0, vt +0x548
			registry.Destroy(pit->foodPile);
		}
	}
	if (const auto* damage = registry.TryGet<const BuildingDamage>(building))
	{
		EraseMesh(damage->generatedMesh);
	}
	PhysicsObjects::RemoveObject(building);
	// CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548 (MultiMapFixed 0x52E7B0), out of all its cells
	ecs::map_cells::RemoveMapObject(building);
	registry.Destroy(building);
	registry.SetDirty();
}

/// ApplyEffectsDueToPhysicalDestruction (0x406640): the crash, and the life becomes what is left standing.
bool ApplyEffectsDueToPhysicalDestruction(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	// 0x406640..0x40671D: SamplePlayAnimEffect(this, the camera's distance, {1, 0, 0x16, 9, 75}, 0, editor.sad, track 0)
	// (G_Crash_Abode_01..09 in editor.sad's table)
	CollisionSounds::PlayAnimEffect({1, 0, 0x16, 9, 75}, building, registry.Get<const Transform>(building).position, false);
	auto& life = registry.AllOf<Life>(building) ? registry.Get<Life>(building) : registry.Assign<Life>(building);
	const float before = life.value;
	if (auto* damage = registry.TryGet<BuildingDamage>(building); damage != nullptr && damage->mesh)
	{
		life.value = std::min(life.value, damage->mesh->Remaining());
		// Abode::ReduceLife 0x405D90: a repair site whose baseline is 1.1 x life - 0.1
		damage->repairBase = 1.1f * life.value - 0.1f;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} hit, life {:.2f} -> {:.2f}", static_cast<uint32_t>(building), before,
	                   life.value);
	// TODO: GAlignment::Update (an evil act), Town::UpdateAggressor, GPlayer::DamageFromPlayer, creature mimic
	if (before >= 0.75f && life.value < 0.75f) // info.dat ThresholdForStopBeingFunctional
	{
		StopBeingFunctional(building);
	}
	if (life.value <= 0.0f)
	{
		DestroyBuilding(building);
		return false;
	}
	return true;
}
} // namespace

bool Buildings::PhysicallyDestroysAbodes(entt::entity entity)
{
	const auto type = PhysicsObjects::ConstantsType(entity);
	return !Locator::entitiesRegistry::value().AllOf<Fragment>(entity) && (type == 3 || type == 20);
}

bool Buildings::ReactToPhysicsImpact(entt::entity building, PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* hit = po.hitBy;
	if (hit == nullptr || !registry.Valid(hit->entity) || !PhysicallyDestroysAbodes(hit->entity))
	{
		return true;
	}
	// TODO: ConsiderMakingCreatureMimicPlayer(DAMAGE_BY_THROWING_AT) when the hand threw it
	const float p = glm::length(hit->body.velocity) * hit->body.Mass();
	if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} hit by {} p {:.0f} (v {:.1f}, m {:.0f})", static_cast<uint32_t>(building),
		                   static_cast<uint32_t>(hit->entity), p, glm::length(hit->body.velocity), hit->body.Mass());
	}
	if (p > 2000.0f)
	{
		auto* damage = registry.TryGet<BuildingDamage>(building);
		if (damage == nullptr)
		{
			damage = &registry.Assign<BuildingDamage>(building);
			damage->intactMesh = registry.Get<const Mesh>(building).id;
		}
		if (!damage->mesh)
		{
			damage->mesh = FragMesh::FromEntity(building);
			if (!damage->mesh)
			{
				return true;
			}
		}
		else if (damage->mesh->lastHitter == hit->entity)
		{
			// the same rock again: they stop colliding, so it goes through
			po.thrower = hit->entity;
			hit->thrower = building;
		}
		else
		{
			damage->mesh->lastHitter = hit->entity;
		}
		// (a repair would rebuild it from the intact model once the draw percent reaches 0.2; nobody repairs yet)
		auto pieces = damage->mesh->Impact(hit->body.Centre(), hit->body.velocity * 0.3f, hit->body.Radius() + 0.7f);
		const float remaining = damage->mesh->GetRemaining();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: impact p {:.0f}, {} pieces, {:.2f} left", p, pieces.size(), remaining);
		// the Fragments are made inside FragMesh::Impact, before the remaining part is read
		for (const auto& piece : pieces)
		{
			CreateFragment(piece, building);
		}
		if (remaining >= 1.0f)
		{
			// nothing counted as lost (the broken part was only halved triangles): the FragMesh goes and the building
			// draws whole again, with no damage and no sound
			damage->mesh.reset();
			if (registry.Get<const Mesh>(building).id != damage->intactMesh)
			{
				auto& mesh = registry.Get<Mesh>(building);
				EraseMesh(damage->generatedMesh);
				damage->generatedMesh = 0;
				mesh.id = damage->intactMesh;
				mesh.submeshId = 0;
				if (damage->morphed && !registry.AllOf<MorphWithTerrain>(building))
				{
					registry.Assign<MorphWithTerrain>(building);
				}
				damage->morphed = false;
				registry.SetDirty();
			}
			return true;
		}
		// the life (and the repair baseline) first: the redraw's partly built percent comes from them
		if (!ApplyEffectsDueToPhysicalDestruction(building))
		{
			return false;
		}
		RedrawBuilding(building, registry.Get<BuildingDamage>(building));
		return true;
	}
	const auto at = registry.Get<const Transform>(building).position;
	// 0x406511..0x406610: p > 1000 level 2, p > 300 level 3; SamplePlayAnimEffect(this, the camera's distance, {level,
	// 0, 0x16, 0x10, 75}, 0, editor.sad, track 0) (G_Rock_V_Ground_M / _S in editor.sad's table)
	if (p > 1000.0f)
	{
		CollisionSounds::PlayAnimEffect({2, 0, 0x16, 0x10, 75}, building, at, false);
	}
	else if (p > 300.0f)
	{
		CollisionSounds::PlayAnimEffect({3, 0, 0x16, 0x10, 75}, building, at, false);
	}
	return true;
}

entt::entity Buildings::FragmentEndPhysics(entt::entity fragment, const PhysicsObject& /*po*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& f = registry.Get<Fragment>(fragment);
	auto* damage = registry.Valid(f.parent) ? registry.TryGet<BuildingDamage>(f.parent) : nullptr;
	if (f.area > 9.0f && damage != nullptr && damage->mesh)
	{
		const auto& transform = registry.Get<const Transform>(fragment);
		const auto toWorld = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation);
		damage->mesh->Merge(*f.mesh, toWorld);
		RedrawBuilding(f.parent, *damage);
		DestroyFragment(fragment);
		return entt::null;
	}
	f.parent = entt::null;
	return fragment;
}

void Buildings::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> expired;
	registry.Each<Fragment>([&](entt::entity entity, Fragment& f) {
		if (f.parent != entt::null && !registry.Valid(f.parent))
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

entt::id_type Buildings::BodyMesh(entt::entity entity, entt::id_type drawn)
{
	const auto* damage = Locator::entitiesRegistry::value().TryGet<const BuildingDamage>(entity);
	return damage != nullptr && damage->intactMesh != 0 ? damage->intactMesh : drawn;
}

void Buildings::ForgetHitter(entt::entity hitter, entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (building != entt::null)
	{
		if (auto* damage = registry.TryGet<BuildingDamage>(building); damage != nullptr && damage->mesh)
		{
			damage->mesh->lastHitter = entt::null;
		}
		return;
	}
	registry.Each<BuildingDamage>([hitter](entt::entity, BuildingDamage& damage) {
		if (damage.mesh && damage.mesh->lastHitter == hitter)
		{
			damage.mesh->lastHitter = entt::null;
		}
	});
}

void Buildings::DestroyFragment(entt::entity fragment)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(fragment))
	{
		return;
	}
	PhysicsObjects::RemoveObject(fragment);
	if (const auto* f = registry.TryGet<const Fragment>(fragment))
	{
		EraseMesh(f->generatedMesh);
	}
	ecs::map_cells::RemoveMapObject(fragment); // CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548
	registry.Destroy(fragment);
	registry.SetDirty();
}
