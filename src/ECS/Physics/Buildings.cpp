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
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "CollisionSounds.h"
#include "Dust.h"
#include "FragMesh.h"
#include "GameClock.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/WorldTriangles.h"
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
	// Abode::Draw 0x515F70: the FragMesh, then the intact model partly built at GetPercentForDrawBuilding. (pending, with
	// the repairs of V11) the FragMesh made anew from the intact model when the percent is in [0.2, 1) at the next hit
	// (Abode::ReactToPhysicsImpact 0x4062FA..0x40633C)
	// GetPercentForDrawBuilding 0x52EFD0 -> GetPercentRepairedFromWhenDamaged 0x52F010 with a DestructionMesh and a
	// building site: (life - site +0x640) / (1 - site +0x640). (approximate until V11) openblack makes no site for the
	// damage yet (Abode::ReduceLife 0x405E7A), so +0x640 is BuildingDamage::repairBase and abodes::GetPercentForDrawBuilding
	// (which would take the no-site branch, life x 0.98) is not used here yet (agreed with session Edificios)
	const auto* life = registry.TryGet<const Life>(building);
	const float l = life != nullptr ? life->value : 1.0f;
	const float a = 1.0f - damage.repairBase;
	const float b = l - damage.repairBase;
	const float percent = (a == 0.0f || b == 0.0f) ? 0.0f : b / a;
	// the morphable's melting stream (vt +0x1F0, obj +0x80): the land deltas stay once MorphWithTerrain is taken off below
	PartialBuildOptions options;
	options.melting = damage.morphed || registry.AllOf<MorphWithTerrain>(building);
	auto partial = percent < 1.0f ? PartialBuild::Build(building, damage.intactMesh, percent, options)
	                              : std::vector<graphics::L3DSubMesh::GeneratedPrimitive> {};
	const auto id = damage.mesh->BuildMesh(glm::inverse(toWorld), "fragmesh", std::move(partial));
	auto& meshes = Locator::resources::value().GetMeshes();
	if (id != 0 && meshes.Contains(damage.intactMesh))
	{
		// the building's mark on the landscape stays (a broken building can still be repaired)
		meshes.Handle(id)->SetFootprintSource(meshes.Handle(damage.intactMesh).handle());
	}
	// the Mesh stays the intact model (session Edificios' components::DrawMesh: sizes, map cells, the static shadow, the
	// body); the FragMesh's model is the building's DrawMesh. With a DestructionMesh Abode::Draw 0x515F70 draws it
	// instead of DrawBuilding's partly built model, so V6's construction draw goes as abodes::RedrawConstruction takes
	// it away for HasDestructionMesh. Removed before the Assign: the on_destroy<DrawMesh> (Abodes.cpp
	// OnDrawMeshDestroyed) erases the model it held, ours or V6's
	const auto old = damage.generatedMesh;
	registry.Remove<AbodeConstructionDraw, DrawMesh, NotDrawn>(building);
	damage.generatedMesh = id;
	if (id != 0)
	{
		// every sub-mesh of the generated one (a big building needs several)
		const auto submesh = meshes.Handle(id)->GetNumSubMeshes() > 1 ? static_cast<int8_t>(-1) : static_cast<int8_t>(0);
		registry.Assign<DrawMesh>(building, id, submesh, registry.Get<const Mesh>(building).bbSubmeshId);
	}
	// that sink is connected only once V6 has drawn some building site: the previous model is erased here as well
	// (EraseMesh checks Contains: nothing to do after OnDrawMeshDestroyed)
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
		Dust::Emit(piece.centre + p, Dust::SyncedRandomVelocity(), 0x80706050u, 2.0f);
	}
	if (auto* po = PhysicsObjects::AddObject(entity, piece.velocity, piece.angularVelocity, parent))
	{
		po->flags |= PhysicsObject::NoObjectCollision;
	}
	registry.SetDirty();
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
	// 0x406261: player = hb->GetPlayer() (PhysicsObject::GetPlayer 0x647460: the GInterfaceStatus +0x24 of the hand that
	// threw it, inherited by what it hit; openblack's byPlayer, the local player's hand)
	const std::optional<PlayerNames> player = hit->byPlayer ? std::optional(PlayerNames::PLAYER_ONE) : std::nullopt;
	// (not ported, no creature) 0x406273..0x406286: with the player and the proxy's FROM_HAND flag,
	// ConsiderMakingCreatureMimicPlayer(status, DAMAGE_BY_THROWING_AT 16, this, 0)
	// (not ported, no creature) 0x4064BA..0x4064DA: the thrower (po +0x1C, else hb +0x1C) is a Creature -> byCreature
	constexpr bool byCreature = false;
	const float p = glm::length(hit->body.velocity) * hit->body.Mass();
	if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} hit by {} p {:.0f} (v {:.1f}, m {:.0f})", static_cast<uint32_t>(building),
		                   static_cast<uint32_t>(hit->entity), p, glm::length(hit->body.velocity), hit->body.Mass());
	}
	if (p > 2000.0f && !ecs::abode_queries::IsBuilt(building))
	{
		// 0x4062E8: not built (IsBuilt vt +0x890 = 0, a building site): no DestructionMesh, straight to
		// ApplyEffectsDueToPhysicalDestruction 0x406640 (EffectValues preset 3 x defence crush 0.2 -> ReduceLife 0x52F5E0)
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
		// (openblack, for the draw snapshot) the impact works on a FragMesh of its own: a new one, or a copy of the
		// building's, which takes the old one's place once broken (a FragMeshDraw taken before keeps the old one)
		std::shared_ptr<FragMesh> broken;
		if (!damage->mesh)
		{
			broken = FragMesh::FromEntity(building);
			if (!broken)
			{
				return true;
			}
		}
		else
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
			broken = std::make_shared<FragMesh>(*damage->mesh);
		}
		// (a repair would rebuild it from the intact model once the draw percent reaches 0.2; nobody repairs yet)
		auto pieces = broken->Impact(hit->body.Centre(), hit->body.velocity * 0.3f, hit->body.Radius() + 0.7f);
		const float remaining = broken->GetRemaining();
		damage->mesh = std::move(broken);
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
			Buildings::RemoveDamage(building);
			return true;
		}
		// the life (and the repair baseline) first: the redraw's partly built percent comes from them.
		// Abode::ApplyEffectsDueToPhysicalDestruction 0x406640 (Edificios)
		if (!ecs::abodes::OnPhysicalDamage(building, {remaining, hit->entity, player, byCreature}))
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
	// Fragment::EndPhysics 0x76F440..0x76F455: the parent (+0xA4) must be IsAvailable (vt +0x2C at 0x76F450)
	auto* damage = ecs::IsAvailable(f.parent) ? registry.TryGet<BuildingDamage>(f.parent) : nullptr;
	if (f.area > 9.0f && damage != nullptr && damage->mesh)
	{
		const auto& transform = registry.Get<const Transform>(fragment);
		const auto toWorld = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation);
		// (openblack, for the draw snapshot) merged into a copy, which takes the old FragMesh's place
		auto merged = std::make_shared<FragMesh>(*damage->mesh);
		merged->Merge(*f.mesh, toWorld);
		damage->mesh = std::move(merged);
		RedrawBuilding(f.parent, *damage);
		DestroyFragment(fragment);
		return entt::null;
	}
	f.parent = entt::null;
	return fragment;
}

void Buildings::Redraw(entt::entity building)
{
	if (auto* damage = Locator::entitiesRegistry::value().TryGet<BuildingDamage>(building); damage != nullptr && damage->mesh)
	{
		RedrawBuilding(building, *damage);
	}
}

void Buildings::RemoveDamage(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* damage = registry.TryGet<BuildingDamage>(building);
	if (damage == nullptr)
	{
		return;
	}
	damage->mesh.reset();
	damage->lastHitter = entt::null; // it went with the FragMesh
	if (damage->generatedMesh != 0 || damage->morphed)
	{
		// the FragMesh's DrawMesh goes (only ours: the Mesh was never changed), then its model (a no-op when
		// OnDrawMeshDestroyed has erased it)
		if (const auto* draw = registry.TryGet<const DrawMesh>(building);
		    draw != nullptr && draw->id == damage->generatedMesh)
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
		// without the DestructionMesh a building with a site takes MultiMapFixed::Draw 0x518090's DrawBuilding
		// again (V6's partly built DrawMesh; nothing for one without a site)
		ecs::abodes::RedrawConstruction(building);
		registry.SetDirty();
	}
}

void Buildings::OnBuildingDeleted(entt::entity building)
{
	// the FragMesh's model, also when its DrawMesh is already gone; Registry::Destroy's on_destroy<DrawMesh> then finds it
	// erased (EraseMesh checks Contains)
	if (const auto* damage = Locator::entitiesRegistry::value().TryGet<const BuildingDamage>(building))
	{
		EraseMesh(damage->generatedMesh);
	}
	PhysicsObjects::RemoveObject(building);
}

void Buildings::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> expired;
	registry.Each<Fragment>([&](entt::entity entity, Fragment& f) {
		// Fragment::ProcessTimer 0x76EAF3..0x76EB06: a parent (+0xA4) no longer IsAvailable (vt +0x2C at 0x76EAFF) is
		// forgotten
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

void Buildings::ForgetHitter(entt::entity hitter, entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (building != entt::null)
	{
		if (auto* damage = registry.TryGet<BuildingDamage>(building); damage != nullptr && damage->mesh)
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

void Buildings::AppendFragMeshes(graphics::world_triangles::Frame& out, const FragMesh::FrameLight& frame)
{
	static std::vector<FragMeshDraw> s_draws; // kept for its capacity; emptied after the draw (no FragMesh held on)
	SnapshotFragMeshes(s_draws, frame);
	AppendFragMeshes(out, s_draws);
	s_draws.clear();
}

void Buildings::AppendFragMeshes(graphics::world_triangles::Frame& out, const std::vector<FragMeshDraw>& draws)
{
	for (const auto& draw : draws)
	{
		draw.mesh->AppendDraw(out, draw.hasMatrix ? &draw.world : nullptr, draw.light);
	}
}

void Buildings::SnapshotFragMeshes(std::vector<FragMeshDraw>& out, const FragMesh::FrameLight& frame)
{
	out.clear();
	auto& registry = Locator::entitiesRegistry::value();
	// Abode::Draw 0x516080..0x5160E9: the DestructionMesh +0x90 (only while it is what the building draws)
	registry.Each<const BuildingDamage, const Transform, const DrawMesh>(
	    [&](entt::entity entity, const BuildingDamage& damage, const Transform& transform, const DrawMesh& draw) {
		    if (!damage.mesh || damage.generatedMesh == 0 || draw.id != damage.generatedMesh)
		    {
			    return;
		    }
		    // 0x5160A6..0x5160D8: with a fire (+0x44) +0x10 = fn_00730570 (the charring grey) and +0x14 =
		    // GetFireEffectCharingColor 0x730480 (the glow), both with alpha 0xFF (0x7305DE, 0x73055A); else 0xFFFFFFFF / 0.
		    // The same pair as the other burning objects (RenderingSystem's Burning)
		    uint32_t tint = 0xFFFFFFFFu;
		    uint32_t tintSpecular = 0;
		    if (const auto* fire = ecs::fire::Find(entity); fire != nullptr)
		    {
			    const uint32_t grey = ecs::fire::graphic::CharringGrey(*fire);
			    const auto glow = ecs::fire::graphic::CharringGlow(
			        *fire, static_cast<float>(game_clock::Turn()) + game_clock::TurnFraction());
			    tint = lh3d_colour::Argb(grey, grey, grey, 0xFF);
			    tintSpecular = lh3d_colour::Argb(glow.r, glow.g, glow.b, 0xFF);
		    }
		    // 0x5160DB..0x5160E9: no matrix (the triangles are in the world), the position of the LH3DObject (+0x40 + 0x38)
		    out.push_back({damage.mesh, false, glm::mat4(1.0f),
		                   FragMesh::ObjectLight(transform.position, tint, tintSpecular, frame), transform.position,
		                   tint, tintSpecular});
	    });
	// Fragment::Draw 0x76EC00 (GetWorldMatrix vt+0x63C, then its translation) and PhysicsObject::DrawAll 0x646E57..
	// 0x646E77 (the LH3DObject's matrix +0x14 and translation +0x38): the same matrix; the FragMesh +0x94 keeps the
	// ctor's 0xFFFFFFFF / 0 (fn_007F6EE0 0x7F6EE5..0x7F6EEC), nothing sets it again. While it flies that matrix is the
	// pose between its last two turns (fn_00646FE0 -> fn_007FCE80, components::PhysicsDrawPose), the scale Transform's
	registry.Each<const Fragment, const Transform>(
	    [&](entt::entity entity, const Fragment& fragment, const Transform& transform) {
		    if (!fragment.mesh)
		    {
			    return;
		    }
		    const auto* flying = registry.TryGet<const PhysicsDrawPose>(entity);
		    const auto position = flying != nullptr ? flying->position : transform.position;
		    const auto matrix = flying != nullptr ? lh_matrix::Model(flying->position, flying->rotation, transform.scale)
		                                          : lh_matrix::Model(transform);
		    out.push_back({fragment.mesh, true, matrix, FragMesh::ObjectLight(position, 0xFFFFFFFFu, 0, frame), position,
		                   0xFFFFFFFFu, 0});
	    });
}
