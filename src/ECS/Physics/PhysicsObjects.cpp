/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsObjects.h"

#include "3D/ObjectMatrix.h"
#include "Buildings.h"
#include "CollisionSounds.h"
#include "Audio/Audio.h"
#include "Dust.h"
#include "FragMesh.h"
#include "FromHand.h"

#include <algorithm>
#include <chrono>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/AnimalAI.h"
#include "ECS/FishShoals.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/SeaCells.h"
#include "ECS/ThingFlags.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/WaterRings.h"
#include "ECS/Fire/FireEffect.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MapShield.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;
using openblack::ecs::life::LifeOf;

namespace
{
constexpr size_t k_NumConstants = 24;
// Data\PhysicsConstants.txt of the base game, used when the file cannot be read
constexpr std::array<PhysicsData, k_NumConstants> k_DefaultConstants = {{
    {0.8f, 78.75f, 0.4f, 2.65625f, 0.2f, 0.0f},
    {0.8f, 35.0f, 0.09f, 1.2f, 0.8f, 1.0f},
    {0.3f, 80.0f, 0.3f, 1.2f, 0.8f, 1.0f},
    {2.0f, 21.5625f, 1.328125f, 1.3125f, 0.709375f, 1.0f},
    {0.603125f, 31.640625f, 0.4f, 1.9375f, 0.575f, 1.875f},
    {0.914258f, 4.0625f, 0.195313f, 1.484375f, 0.628125f, 1.078125f},
    {0.741406f, 5.625f, 0.273438f, 1.796875f, 0.728125f, 1.28125f},
    {0.994922f, 26.25f, 3.046875f, 3.0f, 0.434375f, 2.078125f},
    {0.9f, 22.1875f, 0.4f, 1.46875f, 0.503125f, 1.8125f},
    {0.3f, 8.4375f, 0.028125f, 1.375f, 0.8f, 1.9375f},
    {0.8f, 54.0625f, 0.4f, 0.0f, 0.2f, 0.0f},
    {0.8f, 17.8125f, 4.296875f, 1.3f, 0.76875f, 1.0f},
    {0.8f, 12.1875f, 1.328125f, 1.2f, 0.371875f, 1.0f},
    {0.8f, 1.0f, 0.006f, 1.2f, 0.8f, 1.0f},
    {0.096094f, 239.0625f, 0.01875f, 1.09375f, 0.925f, 1.28125f},
    {0.8f, 41.25f, 1.210938f, 1.296875f, 0.996875f, 0.0f},
    {0.8f, 8.125f, 0.39375f, 1.171875f, 0.8f, 1.0625f},
    {0.8f, 32.5f, 0.328125f, 2.125f, 0.790625f, 1.78125f},
    {0.75293f, 19.6875f, 0.820313f, 1.328125f, 0.2625f, 1.65625f},
    {0.8f, 20.0f, 0.271875f, 1.21875f, 0.63125f, 0.890625f},
    {2.043555f, 101.25f, 0.140625f, 0.5f, 0.921875f, 0.8125f},
    {0.8f, 19.6875f, 0.4f, 1.09375f, 0.69375f, 0.0f},
    {0.8f, 20.3125f, 0.4f, 1.0f, 0.55625f, 1.046875f},
    {0.8f, 18.75f, 0.28125f, 0.984375f, 0.390625f, 1.015625f},
}};

std::array<PhysicsData, k_NumConstants> g_Constants = k_DefaultConstants;
std::vector<std::unique_ptr<PhysicsObject>> g_Objects;
/// the allocated slots of the list (0xD4781C): MakeSureEndSlotIsFree 0x644C40 grows it by 16 when it is full, never
/// shrinks it, has no upper limit; DeleteAll 0x6442B0 sets it to 0
size_t g_Capacity = 0;
std::array<PhysicsObjects::ClassHandlers, static_cast<size_t>(PhysicsClass::_Count)> g_ClassHandlers;

PhysicsObject* Add(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity, entt::entity thrower, bool fromHand,
                   bool spreadReaction);

const PhysicsObjects::ClassHandlers& HandlersOf(entt::entity entity)
{
	return g_ClassHandlers.at(static_cast<size_t>(PhysicsObjects::ClassOf(entity)));
}

ImpactInfo ImpactOf(const PhysicsObject& po)
{
	ImpactInfo info;
	info.g = po.GLoad();
	info.hitBy = po.hitBy != nullptr ? po.hitBy->entity : entt::entity {entt::null};
	info.thrower = po.thrower;
	info.byPlayer = po.byPlayer;
	return info;
}

const graphics::L3DMesh* MeshOf(entt::entity entity)
{
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(entity);
	if (mesh == nullptr)
	{
		return nullptr;
	}
	// a broken building's body keeps its intact mesh
	const auto id = Buildings::BodyMesh(entity, mesh->id);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(id))
	{
		return nullptr;
	}
	return &(*meshes.Handle(id));
}

bool MeshIs(entt::entity entity, uint32_t meshIndex)
{
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(entity);
	return mesh != nullptr && mesh->id == resources::HashIdentifier(static_cast<MeshId>(meshIndex));
}

const GObjectInfo* InfoOf(entt::entity entity)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* c = registry.TryGet<const MobileStatic>(entity))
	{
		return &info.mobileStatic.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const Pot>(entity); c != nullptr && c->type != PotInfo::_COUNT)
	{
		return &info.pot.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const MobileObject>(entity))
	{
		return &info.mobileObject.at(static_cast<size_t>(c->type));
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		// OneOffSpellSeed::Create 0x72A3BE: MobileObject(pos, 0xD39F3C, ...), the GMobileObjectInfo after WHALE's
		// 0xD39E28 (stride 0x114): ONE_OFF_SPELL_SEED 25 (inferido: WHALE's address is not checked either)
		return &info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed));
	}
	if (const auto* c = registry.TryGet<const Tree>(entity))
	{
		return &info.tree.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const DeadTree>(entity))
	{
		return &info.tree.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const MapShield>(entity))
	{
		// GMapShieldInfo 0xDA05D0 / 0xDA06D8 (weight 50000: Object::GetWeight gives the physical shield a heavy body)
		return &info.mapShield.at(c->kind == MapShield::Kind::Physical ? 1 : 0);
	}
	return nullptr;
}

/// The mesh data of BuildFromVertices (0x7FBAE0): the submeshes flagged isPhysics, otherwise the LOD 0 ones; every
/// vertex and triangle of them.
bool CollectVertices(const graphics::L3DMesh& mesh, std::vector<glm::vec3>& positions, std::vector<std::array<uint32_t, 3>>& triangles)
{
	const auto& subMeshes = mesh.GetSubMeshes();
	const bool anyPhysics = std::any_of(subMeshes.begin(), subMeshes.end(), [](const auto& s) { return s->IsPhysics(); });
	const bool anyLod0 = std::any_of(subMeshes.begin(), subMeshes.end(), [](const auto& s) { return (s->GetFlags().lodMask & 1) != 0; });
	for (const auto& subMesh : subMeshes)
	{
		const bool use = anyPhysics ? subMesh->IsPhysics() : (!anyLod0 || (subMesh->GetFlags().lodMask & 1) != 0);
		if (!use)
		{
			continue;
		}
		const auto base = static_cast<uint32_t>(positions.size());
		const auto& p = subMesh->GetCollisionPositions();
		positions.insert(positions.end(), p.begin(), p.end());
		const auto& indices = subMesh->GetCollisionIndices();
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			triangles.push_back({base + indices[i], base + indices[i + 1], base + indices[i + 2]});
		}
	}
	return !positions.empty();
}

/// Object::SetUpPhysObAsATree (0x63A230): 16 points (3 rings of 4, the top, and the bottom point three times) and 24
/// faces along the trunk; rooted trees have their centre of mass at 0.4 H, dead ones at 0.5 H.
void SetUpTreeBody(PhysOb& body, float height, float radius, float scale, bool rooted, const Transform& transform)
{
	const float h = (rooted ? 0.6f : 0.5f) * height;
	const glm::vec3 com(0.0f, (rooted ? 0.4f : 0.5f) * height / scale, 0.0f);
	std::vector<glm::vec3> v;
	for (const auto& [r, y] : {std::pair {0.65f * radius, -0.6f * h}, std::pair {radius, 0.0f}, std::pair {0.65f * radius, 0.6f * h}})
	{
		v.emplace_back(r, y, 0.0f);
		v.emplace_back(0.0f, y, -r);
		v.emplace_back(-r, y, 0.0f);
		v.emplace_back(0.0f, y, r);
	}
	v.emplace_back(0.1f * radius, -h, 0.0f);
	v.emplace_back(0.0f, h, 0.0f);
	v.emplace_back(0.1f * radius, -h, 0.0f);
	v.emplace_back(0.1f * radius, -h, 0.0f);
	std::vector<std::array<uint32_t, 3>> f;
	for (const uint32_t b : {0u, 4u})
	{
		f.push_back({b, b + 1, b + 5});
		f.push_back({b, b + 5, b + 4});
		f.push_back({b + 1, b + 2, b + 6});
		f.push_back({b + 1, b + 6, b + 5});
		f.push_back({b + 2, b + 3, b + 7});
		f.push_back({b + 2, b + 7, b + 6});
		f.push_back({b + 3, b, b + 4});
		f.push_back({b + 3, b + 4, b + 7});
	}
	for (const std::array<uint32_t, 3> t : {std::array<uint32_t, 3> {12, 1, 0}, {12, 2, 1}, {12, 3, 2}, {12, 0, 3}, {13, 8, 9},
	                                         {13, 9, 10}, {13, 10, 11}, {13, 11, 8}})
	{
		f.push_back(t);
	}
	body.BuildShape(v, f, com, std::max(h, radius), 0.3f, transform.rotation, transform.position);
}

/// Villager::SetUpPhysOb (0x5EFF40) / Animal::SetUpPhysOb (0x5F04E0): 12 points on three levels (head, waist, feet)
/// turned by pi/2 about Y, 20 faces, centre of mass at half height, drag x 2.
void SetUpLivingBody(PhysOb& body, float height, float radius, float scale, bool animal, const Transform& transform)
{
	const float y = 0.5f * height;
	const float a = (animal ? 0.25f : 0.8f) * radius;
	const float z = (animal ? 0.8f : 0.32f) * radius;
	const float b = (animal ? 0.3f : 0.9f) * radius;
	const float c = (animal ? 0.9f : 0.32f) * radius;
	const std::array<glm::vec3, 12> raw = {{{a, y, z}, {-a, y, -z}, {-a, y, z}, {a, y, -z}, {b, 0.0f, c}, {-b, 0.0f, -c},
	                                        {-b, 0.0f, c}, {b, 0.0f, -c}, {a, -y, z}, {-a, -y, -z}, {-a, -y, z}, {a, -y, -z}}};
	std::vector<glm::vec3> v;
	float farthest = 0.0f;
	for (const auto& p : raw)
	{
		v.emplace_back(-p.z, p.y, p.x); // RotY((float)pi / 2)
		farthest = std::max(farthest, glm::length(v.back()));
	}
	const std::vector<std::array<uint32_t, 3>> f = {
	    {1, 2, 0},  {0, 3, 1},  {5, 1, 3},  {7, 5, 3},  {7, 3, 0},  {4, 7, 0},   {6, 4, 0},  {2, 6, 0},  {6, 2, 1}, {5, 6, 1},
	    {9, 5, 7},  {11, 9, 7}, {11, 7, 4}, {8, 11, 4}, {10, 8, 4}, {6, 10, 4}, {10, 6, 5}, {9, 10, 5}, {11, 10, 9}, {8, 10, 11}};
	body.BuildShape(v, f, glm::vec3(0.0f, y / scale, 0.0f), farthest, 2.0f, transform.rotation, transform.position);
}

/// PhysOb::Initialise + Object::SetUpPhysOb from the entity's transform and mesh.
bool SetUpBody(entt::entity entity, PhysOb& body, bool dynamic)
{
	const auto* mesh = MeshOf(entity);
	if (mesh == nullptr)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(entity);
	// a shield's body has its Object::GetScale, which lags the drawn one (Magic/Objects/MapShield)
	const float scale = registry.AllOf<MapShield>(entity) ? magic::map_shield::CollisionScale(entity) : transform.scale.x;
	const auto size = mesh->GetBoundingBox().Size();
	const float height = size.y * scale;                         // Object::GetHeight
	const float radius = 0.5f * std::max(size.x, size.z) * scale; // Object::Get2DRadius
	if (const auto* fragment = registry.TryGet<const Fragment>(entity); fragment != nullptr && fragment->mesh)
	{
		// Fragment::SetUpPhysOb (0x76EC50): the distinct vertices and a copy of each 0.45 behind along its normal (a
		// slab), no faces (nothing is hit by a fragment), about the fragment's origin; mass 30 x area; drag x 2
		// (0x76F2DB doubles PhysOb+0x14C, the drag)
		std::vector<glm::vec3> points;
		std::vector<glm::vec3> normals;
		fragment->mesh->UniqueVertices(points, normals);
		std::vector<glm::vec3> local;
		float r = 0.0f;
		for (size_t i = 0; i < points.size(); ++i)
		{
			local.push_back(points[i]);
			local.push_back(points[i] - 0.45f * normals[i]);
			r = std::max({r, glm::length(local[local.size() - 2]), glm::length(local.back())});
		}
		// Initialise takes the half height of the Rock info's mesh (GMobileStaticInfo[2], 0x76E9E4), not the piece's
		float rockHalfHeight = 0.5f * size.y;
		if (Locator::infoConstants::has_value())
		{
			const auto rockMesh = resources::HashIdentifier(Locator::infoConstants::value().mobileStatic.at(2).meshId);
			if (auto& meshes = Locator::resources::value().GetMeshes(); meshes.Contains(rockMesh))
			{
				rockHalfHeight = 0.5f * meshes.Handle(rockMesh)->GetBoundingBox().Size().y;
			}
		}
		body.Initialise(scale, rockHalfHeight);
		body.SetUpConstants(PhysicsObjects::Weight(entity), PhysicsObjects::Constants(11), true);
		body.BuildShape(local, {}, glm::vec3(0.0f), r, 2.0f, transform.rotation, transform.position);
		return true;
	}
	const bool tree = registry.AllOf<Tree>(entity) || (registry.AllOf<DeadTree>(entity) && !MeshIs(entity, 406));
	const bool living = registry.AnyOf<Villager, Animal>(entity);
	if (tree || living)
	{
		body.Initialise(scale, 0.5f * size.y);
		body.SetUpConstants(PhysicsObjects::Weight(entity), PhysicsObjects::Constants(PhysicsObjects::ConstantsType(entity)), true);
		if (tree)
		{
			SetUpTreeBody(body, height, radius, scale, registry.AllOf<Tree>(entity), transform);
		}
		else
		{
			SetUpLivingBody(body, height, radius, scale, registry.AllOf<Animal>(entity), transform);
		}
		return true;
	}
	std::vector<glm::vec3> positions;
	std::vector<std::array<uint32_t, 3>> triangles;
	if (!CollectVertices(*mesh, positions, triangles))
	{
		return false;
	}
	body.Initialise(scale, 0.5f * size.y);
	body.SetUpConstants(PhysicsObjects::Weight(entity), PhysicsObjects::Constants(PhysicsObjects::ConstantsType(entity)), dynamic);
	body.Build(positions, triangles, transform.rotation, transform.position);
	return true;
}

void SyncTransform(const PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(po.entity))
	{
		return;
	}
	auto& transform = registry.Get<Transform>(po.entity);
	transform.position = po.body.ObjectOrigin();
	transform.rotation = po.body.Rotation();
	if (auto* fixed = registry.TryGet<Fixed>(po.entity))
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
}

void AddRipple(const PhysicsObject& po)
{
	const float radius = std::max(po.body.Radius(), 0.01f);
	WaterRing ring;
	ring.position = glm::vec3(po.body.Centre().x, 0.1f, po.body.Centre().z);
	ring.growth = 2.0f * radius;
	ring.rate = 1.0f / radius;
	ring.cell = 0x30; // 0x645A5E: the bob ripple takes cell 0x30 (the water hit's ring, 0x6466D2, takes 0x3F)
	AddWaterRing(ring);
}

/// ReactToPhysicsImpact (vt +0x7AC), per class. Returns false when the entry went away.
bool ReactToPhysicsImpact(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = po.entity;
	if (registry.AllOf<MapShield>(entity))
	{
		magic::map_shield::ReactToPhysicsImpact(entity, po); // PhysicalShield 0x72D610 (Magic/Objects/MapShield)
		return registry.Valid(entity);
	}
	if (const auto& handlers = HandlersOf(entity); handlers.reactToImpact)
	{
		return handlers.reactToImpact(entity, po, ImpactOf(po));
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return Buildings::ReactToPhysicsImpact(entity, po);
	}
	const float g = po.GLoad();
	if (Rocks::IsRock(entity))
	{
		// Rock::ReactToPhysicsImpact 0x6E7930: not from another rock
		const bool byRock = po.hitBy != nullptr && registry.Valid(po.hitBy->entity) && Rocks::IsRock(po.hitBy->entity);
		if (g > 4.0f && Rocks::Height(entity) > 0.7f && !byRock)
		{
			auto& life = registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity);
			life.value -= (g - 4.0f) * 0.005f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: rock hit G {:.1f}, life {:.3f}", g, life.value);
			if (life.value < 0.01f)
			{
				// Rock::SplitInTwo: the halves carry on with its velocity and half its angular velocity
				const bool flying = !po.body.resting;
				const auto velocity = flying ? po.body.velocity : glm::vec3(0.0f);
				const auto spin = flying ? po.body.angularMomentum * 0.5f : glm::vec3(0.0f);
				Rocks::SplitInTwo(entity, velocity, spin);
				return false;
			}
		}
	}
	return true;
}

/// EndPhysics (vt +0x790) at rest, the class's part. Returns the entity that stays as a resting proxy (entt::null:
/// none).
entt::entity EndPhysicsOfClass(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	SyncTransform(po);
	auto entity = po.entity;
	// Object::EndPhysics (0x6375A0): its own flying-object reactions go (the predators fleeing from it stop)
	ecs::animal_ai::EndReactionsOf(entity);
	if (const auto& handlers = HandlersOf(entity); handlers.endPhysics)
	{
		entity = handlers.endPhysics(entity, po);
		registry.SetDirty();
		return entity;
	}
	if (registry.AllOf<Fragment>(entity))
	{
		return Buildings::FragmentEndPhysics(entity, po);
	}
	registry.SetDirty();
	return entity;
}

/// EndPhysics (vt +0x790) at rest: the class's part, then Object::EndPhysics 0x6375A0 puts the object back in the map
/// cells (InsertMapObject vt +0x544 at 0x63762C; Fixed::EndPhysics reaches it through 0x52E054 / 0x52E0CB), a
/// fragment that stays (FragmentEndPhysics) too; nothing for one that is gone. (inferido) after the class's part:
/// where Villager / Animal::EndPhysics call it is not read; Tree::EndPhysics 0x74B830 searches the cells (0x74B9C0)
/// before its insert, so a replanted tree does not see itself.
/// 0x637613..0x63763A: with insert and the object not UNAVAILABLE (GameThing +0xA bit 0, 0x637617: here, still in the
/// registry), MapCoords::InBounds 0x6042C0 of its MapCoords (+0x14): inside the 512 x 512 cells it goes back in the map
/// cells, outside it is deleted (ToBeDeleted(0), vt +0xC at 0x63763A). Returns entt::null when the object that stays
/// is the deleted one. (inferido) a different object that stays (Tree -> DeadTree) is not tested here
entt::entity EndPhysics(PhysicsObject& po)
{
	const auto entity = po.entity;
	const auto kept = EndPhysicsOfClass(po);
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(entity))
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (transform == nullptr || map_coords::InBounds(map_coords::FromWorld(nullptr, transform->position)))
		{
			map_cells::InsertMapObject(entity);
		}
		else
		{
			ToBeDeleted(entity);
			return kept == entity ? entt::null : kept;
		}
	}
	return kept;
}

/// fn_646D60 / RemoveObject: the object stops being a hitter: buildings forget it (FragMesh lastHitter) and bodies that
/// had it as their thrower (the pass-through pair) collide with it again.
void ForgetThrower(entt::entity entity)
{
	for (auto& other : g_Objects)
	{
		if (other->thrower == entity && other->entity != entity)
		{
			other->thrower = entt::null;
		}
	}
	Buildings::ForgetHitter(entity);
}

void RemoveAt(size_t index)
{
	for (auto& other : g_Objects)
	{
		if (other->hitBy == g_Objects[index].get())
		{
			other->hitBy = nullptr;
		}
		if (other->body.lastHit == &g_Objects[index]->body)
		{
			other->body.lastHit = nullptr;
		}
	}
	// out of the physics: drawn at its Transform again
	if (auto& registry = Locator::entitiesRegistry::value();
	    registry.Valid(g_Objects[index]->entity) && registry.AllOf<PhysicsDrawPose>(g_Objects[index]->entity))
	{
		registry.Remove<PhysicsDrawPose>(g_Objects[index]->entity);
	}
	g_Objects.erase(g_Objects.begin() + static_cast<std::ptrdiff_t>(index));
}

/// A corner of the physics' cell boxes (RaiseUntilNotIntersecting 0x644877, GameTurnUpdate 0x645322): x 6553.6
/// [0x8AC400], __ftol, and off the map (MapCoords::InBounds 0x6042C0) fn_00604250: each signed high word clamped to
/// 0 .. [g_game+0x59C8] - 1 (511)
glm::ivec2 BoxCell(float x, float z)
{
	const auto clamped = [](float metres) {
		return std::clamp<int32_t>(map_coords::SignedCellOf(map_coords::ToFixed(metres)), 0,
		                           static_cast<int32_t>(map_coords::k_MapCells) - 1);
	};
	return {clamped(x), clamped(z)};
}

/// PhysicsObject::MakeSureEndSlotIsFree 0x644C40: count == capacity -> capacity + 16 (0x644C54)
void MakeSureEndSlotIsFree()
{
	if (g_Objects.size() >= g_Capacity)
	{
		g_Capacity += 16;
	}
}

/// fn_644DF0: a resting body for an object a moving body may hit.
void AddProxy(entt::entity entity)
{
	MakeSureEndSlotIsFree(); // 0x644E09
	auto po = std::make_unique<PhysicsObject>();
	po->entity = entity;
	po->villager = Locator::entitiesRegistry::value().AllOf<Villager>(entity);
	po->kind = po->villager ? 1 : 0; // 0x644E67..0x644EAD
	const bool dynamic = PhysicsObjects::CanBecomeAPhysicsObject(entity);
	if (!SetUpBody(entity, po->body, dynamic))
	{
		return;
	}
	po->body.resting = true;
	po->flags = PhysicsObject::Awake;
	// SetUpPos 0x7FC760: the turn-start matrix is the body's own (a proxy knocked this turn moves from where it rests)
	po->turnStartRotation = po->body.Rotation();
	po->turnStartCentre = po->body.Centre();
	po->turnStarted = true;
	if (Locator::entitiesRegistry::value().AnyOf<Abode, StoragePit>(entity))
	{
		Buildings::ForgetHitter(entt::null, entity); // Abode::SetUpPhysOb 0x402DD0 clears the FragMesh's last hitter
		po->flags |= PhysicsObject::NoObjectCollision; // Abode::ChecksVerticesVObjects is 0
	}
	g_Objects.push_back(std::move(po));
}

void BeginTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < g_Objects.size();)
	{
		auto& po = *g_Objects[i];
		if (!registry.Valid(po.entity))
		{
			RemoveAt(i);
			continue;
		}
		if (LifeOf(po.entity) < 0.01f)
		{
			po.body.density += 0.01f; // GameTurnUpdate housekeeping: GetLife() < 0.01 -> sink factor +0.01 a turn (corpses sink)
		}
		po.forceSum = glm::vec3(0.0f);
		// 0x645187..0x64519B: the end matrix (PhysOb +0x7C) becomes the turn-start one (+0xAC)
		po.turnStartRotation = po.body.Rotation();
		po.turnStartCentre = po.body.Centre();
		po.turnStarted = true;
		po.body.lastHit = nullptr;
		po.hitBy = nullptr;
		++i;
	}
	// awake flags: moving bodies are awake, resting ones only while something moving is near them
	for (auto& po : g_Objects)
	{
		if (po->body.resting)
		{
			po->flags &= ~PhysicsObject::Awake;
		}
		else
		{
			po->flags |= PhysicsObject::Awake;
		}
	}
	// PhysicsObject::GameTurnUpdate 0x6452A6..0x64556A: each moving body (+0x19C clear) wakes what is in the map cells
	// of its box C -/+ (fn_006E8160 |v.xz| x 0.1 [0x8AB22C] + R [+0x178]) (BoxCell), x outer ([esp+0x1C]) and z inner
	// ([esp+0x2C]); a cell is walked once for all the bodies (the list of walked cells, 0x64544B..0x645491, keeps at
	// most 0x200). The inline iterator (0x645495..0x6454A3: the fixed list, then the mobile one, map_cells::
	// ForEachInCell): what InteractsWithPhysicsObjects (vt +0x79C, 0x6454C7) is woken when it is in the physics
	// already (|= 1, 0x6454F7), else with a 3D object (+0x40) it gets a resting proxy (AddProxy fn_00644DF0, 0x64551D)
	// at once. The walk stops when the list's allocated slots are used up (count >= capacity [0xD4781C]: before each x
	// column 0x6453E2, each z cell 0x645411 and each object 0x6454B7): AddProxy does not grow it here, so the condition
	// stays true for the rest of the walk and for every later body of the turn
	constexpr size_t k_WalkedCells = 0x200; // 0x64547B
	std::vector<glm::ivec2> walked;
	const auto full = []() { return g_Objects.size() >= g_Capacity; };
	const size_t bodies = g_Objects.size(); // the proxies added below rest: they have no box
	for (size_t b = 0; b < bodies && !full(); ++b)
	{
		if (g_Objects[b]->body.resting)
		{
			continue;
		}
		const auto centre = g_Objects[b]->body.Centre();
		const auto v = g_Objects[b]->body.velocity;
		const float half = glm::length(glm::vec2(v.x, v.z)) * 0.1f + g_Objects[b]->body.Radius();
		const auto first = BoxCell(centre.x - half, centre.z - half);
		const auto second = BoxCell(centre.x + half, centre.z + half);
		const auto low = glm::min(first, second);
		const auto high = glm::max(first, second);
		for (int32_t x = low.x; x <= high.x && !full(); ++x)
		{
			for (int32_t z = low.y; z <= high.y && !full(); ++z)
			{
				// JustMapXZ::InBounds 0x5E1860 (0x645435), then the walked list
				const glm::ivec2 cell(x, z);
				if (!map_coords::InBounds(cell) || std::find(walked.begin(), walked.end(), cell) != walked.end())
				{
					continue;
				}
				if (walked.size() < k_WalkedCells)
				{
					walked.push_back(cell);
				}
				map_cells::ForEachInCell(cell, [&registry, &full](entt::entity entity) {
					if (full())
					{
						return false;
					}
					if (!PhysicsObjects::InteractsWithPhysicsObjects(entity))
					{
						return true;
					}
					if (auto* po = PhysicsObjects::Find(entity))
					{
						po->flags |= PhysicsObject::Awake;
					}
					else if (registry.AllOf<Mesh>(entity))
					{
						AddProxy(entity);
					}
					return true;
				});
			}
		}
	}
	// GetAlwaysRemainsInPhysicsInternalSystem (PhysicalShield 0x72CAF0: 1): the physical shields stay in the system at
	// their Object scale, whether or not something moves near them (Magic/Objects/MapShield)
	for (const auto shield : magic::map_shield::Shields())
	{
		if (!registry.Valid(shield) || !magic::map_shield::InteractsWithPhysicsObjects(shield))
		{
			continue;
		}
		if (auto* po = PhysicsObjects::Find(shield))
		{
			po->flags |= PhysicsObject::Awake;
		}
		else
		{
			AddProxy(shield);
		}
	}
	for (size_t i = 0; i < g_Objects.size();)
	{
		if ((g_Objects[i]->flags & PhysicsObject::Awake) == 0)
		{
			RemoveAt(i);
			continue;
		}
		++i;
	}
}

void Substep()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& po : g_Objects)
	{
		po->body.ZeroForces();
		po->body.GroundAndWater();
	}
	for (auto& a : g_Objects)
	{
		if ((a->flags & PhysicsObject::NoObjectCollision) != 0)
		{
			continue;
		}
		for (auto& b : g_Objects)
		{
			if (a == b)
			{
				continue;
			}
			if (a->body.resting && b->body.resting && !a->body.justSetUp && !b->body.justSetUp)
			{
				continue;
			}
			// 0x64583E..0x645866: a villager's body (+0x1A4 == 1) does not hit what a Living pushed (flag 2,
			// Object::PushObject 0x6396BA, Ball::KickBallAtDestination 0x435D99, FelledTree::Create 0x51186B)
			if ((a->villager && (b->flags & PhysicsObject::PushedByLiving) != 0) ||
			    (b->villager && (a->flags & PhysicsObject::PushedByLiving) != 0))
			{
				continue;
			}
			if (a->thrower == b->entity || b->thrower == a->entity)
			{
				continue;
			}
			const float reach = a->body.Radius() + b->body.Radius();
			const auto d = a->body.Centre() - b->body.Centre();
			if (glm::dot(d, d) < reach * reach)
			{
				a->body.CollideVertices(b->body);
			}
		}
	}
	for (auto& po : g_Objects)
	{
		po->body.ContactForces();
	}
	// the entry at i is still ours unless something removed it (a death removes the entry itself)
	const auto stillAt = [](size_t i, const PhysicsObject* self) { return i < g_Objects.size() && g_Objects[i].get() == self; };
	for (size_t i = 0; i < g_Objects.size();)
	{
		auto& po = *g_Objects[i];
		auto* self = &po;
		const float vyBefore = po.body.velocity.y;
		auto result = po.body.Integrate();
		if (!po.body.resting && po.body.Centre().y < po.body.Radius() * 0.5f)
		{
			// 0x645A01: HasSunk (vt +0x7B8, ECS/VillagerDrowning) stops the body and ends its physics as if at rest
			ecs::RememberLastPlayerToInteract(po.entity, po.byPlayer);
			const auto& handlers = HandlersOf(po.entity);
			if (po.body.density > 1.0f && (handlers.hasSunk ? handlers.hasSunk(po.entity, po) : ecs::HasSunk(po.entity)))
			{
				if (!stillAt(i, self)) // Living::HasSunk: the animal went (ToBeDeleted)
				{
					continue;
				}
				po.body.velocity = glm::vec3(0.0f);
				po.body.angularMomentum = glm::vec3(0.0f);
				result = PhysOb::Result::Stopped;
			}
			if (vyBefore * po.body.velocity.y < 0.0f)
			{
				AddRipple(po);
			}
		}
		if (po.body.touched)
		{
			po.forceSum += po.body.force;
		}
		// the fly-by whoosh: a body entering the 10 m sphere round the camera at more than 20 m/s (G_ROCKPAST_01..05)
		if (Locator::camera::has_value())
		{
			const auto d = po.body.Centre() - Locator::camera::value().GetOrigin();
			const float d2 = glm::dot(d, d);
			if (d2 < 100.0f && po.cameraDistance2 > 100.0f && glm::dot(po.body.velocity, po.body.velocity) > 400.0f)
			{
				// 0x645BEE..0x645C12: GAudio::PlaySoundEffect(0, 69 G_RockPast_01 + GetTickCount() % 5, mode 2, loops 0,
				// 0, 2D, InGame) 0x429D60
				audio::PlaySoundEffect(audio::Owner::None(), 69 + static_cast<int>(audio::TickCount() % 5), 2, 0, false,
				                       false, audio::SfxBank::InGame);
			}
			po.cameraDistance2 = d2;
		}
		switch (result)
		{
		case PhysOb::Result::Stopped:
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: entity {} at rest at ({:.2f}, {:.2f}, {:.2f})",
			                   static_cast<uint32_t>(po.entity), po.body.Centre().x, po.body.Centre().y, po.body.Centre().z);
			ForgetThrower(po.entity); // fn_646D60
			const auto kept = EndPhysics(po);
			if (!stillAt(i, self))
			{
				continue;
			}
			if (kept == entt::null || !registry.Valid(kept))
			{
				RemoveAt(i);
				continue;
			}
			po.entity = kept;
			po.body.resting = true;
			break;
		}
		case PhysOb::Result::Pushed:
			// a resting proxy was knocked: Object::InitialisePhysics(0, 0, NULL, false, NULL)
			if (PhysicsObjects::CanBecomeAPhysicsObject(po.entity))
			{
				po.body.resting = false;
				po.flags |= PhysicsObject::Awake;
				// Object::InitialisePhysics 0x637480: out of the map cells while it flies (IsObjectInMap vt +0x178 at
				// 0x6374AC, RemoveMapObject vt +0x548 at 0x6374BA)
				if (map_cells::IsObjectInMap(po.entity))
				{
					map_cells::RemoveMapObject(po.entity);
				}
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: knocked entity {}", static_cast<uint32_t>(po.entity));
			}
			break;
		case PhysOb::Result::Delete:
		{
			const auto entity = po.entity;
			RemoveAt(i);
			if (registry.AllOf<Fragment>(entity))
			{
				Buildings::DestroyFragment(entity);
			}
			else
			{
				ecs::ToBeDeleted(entity); // code 4 (T.y < -4R, 0x645B22): the class's ToBeDeleted(0)
			}
			continue;
		}
		default:
			break;
		}
		++i;
	}
}

void EndTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& po : g_Objects)
	{
		po->body.externalForce = glm::vec3(0.0f);
		po->body.externalTorque = glm::vec3(0.0f);
		// 0x64609F..0x6460F8: a felled tree (kind 2) that has toppled (its row 1's y, PhysOb +0x8C, < 0.98 [0x8CF3FC])
		// sounds once when taller than 10 [0x8AB414] (GetHeight vt +0x42C): SoundTag::Create(its MapCoords +0x14,
		// GetRandomSample(31, 1), track 0, mode 3, loops 0, 0, is3D 1, InGame, delay 0) 0x71EB60; then kind 3
		if (po->kind == 2 && po->body.Rotation()[1].y < 0.98f && registry.Valid(po->entity))
		{
			if (ecs::object::GetHeight(po->entity) > 10.0f)
			{
				audio::tags::CreateAtMapCoords(ecs::object::MapCoordsOf(po->entity), audio::tags::RandomSample(31, 1), false, 3,
				                               0, false, true, audio::SfxBank::InGame, 0);
			}
			po->kind = 3;
		}
		const float sum2 = glm::dot(po->forceSum, po->forceSum);
		po->impact = sum2 > 0.0001f ? std::sqrt(sum2) * 0.05f : 0.0f;
		if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr && !po->body.resting)
		{
			const auto c = po->body.Centre();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics trace: entity {} at ({:.2f}, {:.2f}, {:.2f}) v {:.2f} contacts {} G {:.2f} density {:.4f} R {:.2f}",
			                   static_cast<uint32_t>(po->entity), c.x, c.y, c.z, glm::length(po->body.velocity),
			                   po->body.numContacts, po->GLoad(), po->body.density, po->body.Radius());
		}
		po->hitBy = nullptr;
		if (po->body.lastHit != nullptr)
		{
			for (auto& other : g_Objects)
			{
				if (&other->body == po->body.lastHit)
				{
					po->hitBy = other.get();
				}
			}
		}
		if (!po->byPlayer && po->hitBy != nullptr)
		{
			po->byPlayer = po->hitBy->byPlayer; // the damage goes to whoever threw what hit it
		}
	}
	// ReactToPhysicsImpact may split rocks or kill villagers, which changes the list: work on a snapshot
	std::vector<entt::entity> hit;
	for (auto& po : g_Objects)
	{
		if (po->impact <= 0.0f)
		{
			continue;
		}
		// AttemptToAddSoundEvent 0x6464F0: the collision sample, the ground dust or the splash
		if (!po->body.resting && (po->hitBy != nullptr || po->impact > po->body.Mass() * 4.905f))
		{
			CollisionSounds::AttemptToAddSoundEvent(*po);
		}
		hit.push_back(po->entity);
	}
	for (const auto entity : hit)
	{
		if (auto* po = PhysicsObjects::Find(entity); po != nullptr && registry.Valid(entity))
		{
			if (!ReactToPhysicsImpact(*po) && !registry.Valid(entity))
			{
				PhysicsObjects::RemoveObject(entity);
			}
		}
	}
	CollisionSounds::EndTurn();
}
} // namespace

void PhysicsObjects::LoadConstants()
{
	g_Constants = k_DefaultConstants;
	auto& fileSystem = Locator::filesystem::value();
	std::vector<uint8_t> bytes;
	try
	{
		bytes = fileSystem.ReadAll(fileSystem.FindPath(std::filesystem::path("Data") / "PhysicsConstants.txt"));
	}
	catch (const std::exception&)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Physics: Data/PhysicsConstants.txt not found, using the built-in table");
		return;
	}
	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	int version = 0;
	int rows = 0;
	in >> version >> rows;
	// every column clamped to its own range (0x8D8B10 minima, 0x8D8B28 maxima)
	constexpr std::array<float, 6> k_Min = {0.05f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f};
	constexpr std::array<float, 6> k_Max = {3.0f, 240.0f, 10.0f, 3.0f, 1.0f, 4.0f};
	for (size_t i = 0; i < k_NumConstants; ++i)
	{
		if (static_cast<int>(i) >= rows)
		{
			g_Constants.at(i) = g_Constants.at(0); // missing rows are copies of row 0
			continue;
		}
		std::array<float, 6> v {};
		for (auto& f : v)
		{
			in >> f;
		}
		for (size_t c = 0; c < v.size(); ++c)
		{
			v.at(c) = std::clamp(v.at(c), k_Min.at(c), k_Max.at(c));
		}
		g_Constants.at(i) = {v[0], v[1], v[2], v[3], v[4], v[5]};
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} constant rows (version {})", rows, version);
}

const PhysicsData& PhysicsObjects::Constants(int type)
{
	return g_Constants.at(static_cast<size_t>(std::clamp(type, 0, static_cast<int>(k_NumConstants) - 1)));
}

int PhysicsObjects::ConstantsType(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MapShield>(entity))
	{
		return magic::map_shield::k_PhysicsConstantsType; // PhysicalShield::GetPhysicsConstantsType 0x72D7E0
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		return 9; // OneOffSpellSeed::GetPhysicsConstantsType 0x72A920 (a one-shot orb thrown from the hand)
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return 0;
	}
	if (registry.AllOf<Fragment>(entity))
	{
		return 11;
	}
	if (registry.AllOf<Villager>(entity))
	{
		return 7;
	}
	if (registry.AllOf<Animal>(entity))
	{
		return 8;
	}
	if (registry.AllOf<Tree>(entity))
	{
		return 6;
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		return MeshIs(entity, 406) ? 1 : 6; // MSH_O_WOOD_INHAND
	}
	if (registry.AllOf<Pot>(entity))
	{
		return MeshIs(entity, 323) ? 5 : 4; // MSH_I_OFFERING_FOOD
	}
	if (const auto* object = registry.TryGet<const MobileObject>(entity))
	{
		// MobileObject::GetPhysicsConstantsType 0x6079F0: Champi, MagicMushroom, Toadstool
		const auto index = static_cast<int>(object->type);
		if (index >= 17 && index <= 19)
		{
			return 21 + index - 17;
		}
		return 1;
	}
	if (const auto* statics = registry.TryGet<const MobileStatic>(entity))
	{
		// MobileStatic::GetPhysicsConstantsType 0x609270
		const auto index = static_cast<int>(statics->type);
		if ((index >= 49 && index <= 52) || index == 14 || index == 15 || index == 5 || Rocks::IsRock(entity))
		{
			return 3;
		}
		// 0x609297..0x6092AA: IsFence (vt +0x3CC, MobileStatic::IsFence 0x609110) -> row 18, before the toys
		if (from_hand::IsFence(entity))
		{
			return 18;
		}
		constexpr std::array<int, 5> k_Toys = {14, 20, 16, 15, 19}; // meshes 399..403
		for (uint32_t m = 0; m < k_Toys.size(); ++m)
		{
			if (MeshIs(entity, 399 + m))
			{
				return k_Toys.at(m);
			}
		}
		return 1; // Object::GetPhysicsConstantsType 0x6376A0 with MobileStatic::CanBecomeAPhysicsObject 0x609320 = 1
	}
	// Object::GetPhysicsConstantsType 0x6376A0: CanBecomeAPhysicsObject() ? 1 : 0
	return CanBecomeAPhysicsObject(entity) ? 1 : 0;
}

bool PhysicsObjects::InteractsWithPhysicsObjects(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MapShield>(entity))
	{
		return magic::map_shield::InteractsWithPhysicsObjects(entity); // PhysicalShield 0x72D600: 1, MagicShield 0
	}
	if (registry.AnyOf<Tree, Field, BigForest, Fragment>(entity))
	{
		return false; // standing trees: thrown objects go through them; fragments only hit the landscape
	}
	if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		// piles do not interact (fn_66ED40)
		return pot->type == PotInfo::HandWood || pot->type == PotInfo::HandFood;
	}
	// fields are never hit (Field::InteractsWithPhysicsObjects 0x528020); buildings only while standing
	// (MultiMapFixed: GetPercentBuilt > 0.1 && life > 0.01)
	if (const auto* abode = registry.TryGet<const Abode>(entity); abode != nullptr && abode->type == AbodeNumber::Field)
	{
		return false;
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		if (const auto* life = registry.TryGet<const Life>(entity); life != nullptr && life->value <= 0.01f)
		{
			return false;
		}
	}
	return registry.AnyOf<MobileStatic, MobileObject, Villager, Animal, DeadTree, Abode, StoragePit>(entity);
}

bool PhysicsObjects::CanBecomeAPhysicsObject(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<Abode, StoragePit, Field, BigForest>(entity))
	{
		return false;
	}
	// a one-shot orb is a MobileObject too (0x72A3BE)
	return registry.AnyOf<MobileStatic, MobileObject, Villager, Animal, Tree, DeadTree, Pot, Fragment, OneOffSpellSeed>(entity);
}

const GObjectInfo* PhysicsObjects::ObjectInfo(entt::entity entity)
{
	if (const auto* info = InfoOf(entity))
	{
		return info;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& constants = Locator::infoConstants::value();
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		return &constants.animal.at(static_cast<size_t>(animal->type));
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		for (const auto& info : constants.villager)
		{
			if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
			{
				return &info;
			}
		}
	}
	return nullptr;
}

float PhysicsObjects::Weight(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return 2000.0f; // Abode::SetUpPhysOb
	}
	if (const auto* fragment = registry.TryGet<const Fragment>(entity))
	{
		return std::max(30.0f * fragment->area, 0.01f); // Fragment::SetUpPhysOb
	}
	// Object::GetWeight (0x638480): scale^3 x info weight
	float weight = 1.0f;
	if (const auto* info = InfoOf(entity))
	{
		weight = info->weight;
	}
	else if (const auto* animal = registry.TryGet<const Animal>(entity); animal != nullptr && Locator::infoConstants::has_value())
	{
		weight = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type)).weight;
	}
	else if (const auto* villager = registry.TryGet<const Villager>(entity); villager != nullptr && Locator::infoConstants::has_value())
	{
		weight = 82.5f; // info.dat villagers
		for (const auto& info : Locator::infoConstants::value().villager)
		{
			if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
			{
				weight = info.weight;
				break;
			}
		}
	}
	float scale = registry.AllOf<Transform>(entity) ? registry.Get<const Transform>(entity).scale.x : 1.0f;
	if (registry.AllOf<MapShield>(entity))
	{
		scale = magic::map_shield::CollisionScale(entity); // Object::GetScale, not the drawn one
	}
	return std::max(scale * scale * scale * weight, 0.01f);
}

PhysicsObject* PhysicsObjects::AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
                                         entt::entity thrower, bool fromHand)
{
	return Add(entity, velocity, angularVelocity, thrower, fromHand, fromHand);
}

PhysicsObject* PhysicsObjects::AddObjectFromHand(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity)
{
	return Add(entity, velocity, angularVelocity, entt::null, true, false);
}

PhysicsObject* PhysicsObjects::AddDroppedObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
                                                std::optional<glm::vec3> angularMomentum)
{
	auto* po = AddObject(entity, velocity, angularVelocity);
	if (po == nullptr)
	{
		return nullptr;
	}
	if (angularMomentum)
	{
		po->body.angularMomentum = *angularMomentum; // 0x750A54..0x750A66: po+0x90
	}
	po->flags |= PhysicsObject::NoObjectCollision; // 0x750A6D: or [po+0x1D8], 0x10
	po->body.AdjustToGroundLevel(false, true);     // 0x750A78..0x750A7F
	RaiseUntilNotIntersecting(*po);                // 0x750A89
	return po;
}

namespace
{
PhysicsObject* Add(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity, entt::entity thrower, bool fromHand,
                   bool spreadReaction)
{
	MakeSureEndSlotIsFree(); // AddObject 0x6443C9
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !PhysicsObjects::CanBecomeAPhysicsObject(entity))
	{
		return nullptr;
	}
	// Object::InitialisePhysics 0x637480 (bw1-decomp src/Black/Object.cpp:620): an IMMOVABLE object (GameThingWithPos
	// +0x24 & 0x1000, SET_ID_MOVEABLE) gets no physics
	if (thing_flags::IsImmovable(entity))
	{
		return nullptr;
	}
	if (auto* existing = PhysicsObjects::Find(entity))
	{
		if (!existing->body.resting)
		{
			return nullptr;
		}
		PhysicsObjects::RemoveObject(entity);
	}
	auto po = std::make_unique<PhysicsObject>();
	po->entity = entity;
	po->thrower = thrower;
	po->villager = registry.AllOf<Villager>(entity);
	po->kind = po->villager ? 1 : 0; // 0x64476D..0x64479F
	if (!SetUpBody(entity, po->body, true))
	{
		return nullptr;
	}
	// SetUpPos 0x7FC760: the turn-start matrix is the body's own (drawn there until the next turn starts)
	po->turnStartRotation = po->body.Rotation();
	po->turnStartCentre = po->body.Centre();
	po->body.SetAngularVelocity(angularVelocity);
	const float speed = glm::length(velocity);
	po->body.velocity = speed > PhysOb::k_MaxSpeed ? velocity * (PhysOb::k_MaxSpeed / speed) : velocity;
	po->flags = PhysicsObject::Awake | (fromHand ? PhysicsObject::FromHand : 0);
	po->byPlayer = fromHand;
	if (const auto& handlers = HandlersOf(entity); handlers.initialisePhysics)
	{
		// Living::InitialisePhysics(FromHand) of a villager or an animal: ECS/LivingPhysics
		handlers.initialisePhysics(entity, *po, fromHand);
	}
	g_Objects.push_back(std::move(po));
	// Object::InitialisePhysics 0x637480: out of the map cells while it flies (IsObjectInMap vt +0x178 at 0x6374AC,
	// RemoveMapObject vt +0x548 at 0x6374BA)
	if (map_cells::IsObjectInMap(entity))
	{
		map_cells::RemoveMapObject(entity);
	}
	// Object::InitialisePhysics 0x637480: a burning object leaves its fire group (FireEffect::StartedMoving(0), ECS/Fire)
	fire::StartedMoving(entity, false);
	if (spreadReaction)
	{
		// Object::InitialisePhysicsFromHand (0x637412): the flying-object reaction, once (the predators flee from it)
		// the thrower: the hand's player (GInterfaceStatus::GetPlayer, 0x637405; PLAYER_ONE's interface here)
		ecs::animal_ai::SpreadFlyingObjectReaction(entity, PlayerNames::PLAYER_ONE);
	}
	return g_Objects.back().get();
}
} // namespace

void PhysicsObjects::RemoveObject(entt::entity entity)
{
	for (size_t i = 0; i < g_Objects.size(); ++i)
	{
		if (g_Objects[i]->entity == entity)
		{
			RemoveAt(i);
			ForgetThrower(entity);
			// (inferido, not read) out of the physics without EndPhysics: back in the map cells at once while it exists,
			// not at the next map_cells::Sync. Nothing for a resting proxy (it never left) or one held out (a tornado's)
			if (Locator::entitiesRegistry::value().Valid(entity))
			{
				map_cells::InsertMapObject(entity);
			}
			return;
		}
	}
}

void PhysicsObjects::RemoveObjectWithEndPhysics(entt::entity entity)
{
	for (size_t i = 0; i < g_Objects.size(); ++i)
	{
		if (g_Objects[i]->entity != entity)
		{
			continue;
		}
		auto* self = g_Objects[i].get();
		// SetXYZAngles / Pos / altitude from the body, then EndPhysics(po, insert_back_into_map = true). DropSfx
		// (vt +0x794, 0x646B48) is Object's "return 0" except Tree::DropSfx 0x74BC60 (G_PlantTree_01 + tick % 3),
		// which openblack plays where the tree is replanted (the same LANDED-on-land condition).
		EndPhysics(*self);
		for (size_t j = 0; j < g_Objects.size(); ++j)
		{
			if (g_Objects[j].get() == self)
			{
				RemoveAt(j);
				break;
			}
		}
		ForgetThrower(entity);
		return;
	}
}

void PhysicsObjects::RaiseUntilNotIntersecting(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// 0x644877..0x644919: the cells of the two corners (C.x - R, C.z - R) and (C.x + R, C.z + R) (BoxCell: x 6553.6,
	// __ftol, clamped into the map by fn_00604250 when off it), the low and high words of the two
	const auto centre = po.body.Centre();
	const float radius = po.body.Radius();
	const auto first = BoxCell(centre.x - radius, centre.z - radius);
	const auto second = BoxCell(centre.x + radius, centre.z + radius);
	const auto low = glm::min(first, second);
	const auto high = glm::max(first, second);
	// x outer, z inner (0x64491F..0x64493D), JustMapXZ::InBounds 0x644951, ToMap 0x644962 and the inline iterator: the
	// fixed list, then the mobile one (0x644967..0x644975, map_cells::ForEachInCell; a MultiMapFixed in every cell of
	// its NewCollideDescriptor). AddProxy (fn_00644DF0, 0x6449F4) puts each in the physics list at once, so a later
	// cell of the same object skips it ("not in the physics list yet"): here the candidates list does that
	std::vector<entt::entity> candidates;
	for (int32_t x = low.x; x <= high.x; ++x)
	{
		for (int32_t z = low.y; z <= high.y; ++z)
		{
			const glm::ivec2 cell(x, z);
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			map_cells::ForEachInCell(cell, [&](entt::entity entity) {
				// obj != po->thrower (+0x1C, 0x644987) && ShouldPhysicsRaiseObjectUntilNotIntersectingThis (vt +0x7A4,
				// = InteractsWithPhysicsObjects, 0 for LandscapeVortexIn / MapShield) && not in the physics list yet && it
				// has a Game3dObject (+0x40, 0x6449D5)
				if (entity != po.entity && entity != po.thrower && InteractsWithPhysicsObjects(entity) &&
				    Find(entity) == nullptr && registry.AllOf<Mesh>(entity) &&
				    std::find(candidates.begin(), candidates.end(), entity) == candidates.end())
				{
					candidates.push_back(entity);
				}
				return true;
			});
		}
	}
	for (const auto entity : candidates)
	{
		AddProxy(entity); // fn_00644DF0
	}
	// 0x644AB4: go up by the first push over 0.001 and start again, until nothing pushes. The original has no limit;
	// the 1000 rounds only guard openblack against a body that could never leave another.
	for (int round = 0; round < 1000; ++round)
	{
		float best = 0.0f;
		bool raised = false;
		for (const auto& other : g_Objects)
		{
			if (other.get() == &po)
			{
				continue;
			}
			// a villager is not raised over what a Living pushed, nor that over a villager (+0x1A4, flag 2)
			if ((po.villager && (other->flags & PhysicsObject::PushedByLiving) != 0) ||
			    (other->villager && (po.flags & PhysicsObject::PushedByLiving) != 0))
			{
				continue;
			}
			const float reach = other->body.Radius() + po.body.Radius();
			const auto d = po.body.Centre() - other->body.Centre();
			if (!(reach * reach > glm::dot(d, d)))
			{
				continue;
			}
			const float down = po.body.PenetrationAlong(other->body, glm::vec3(0.0f, -1.0f, 0.0f));
			const float up = other->body.PenetrationAlong(po.body, glm::vec3(0.0f, 1.0f, 0.0f));
			best = std::max(best, std::max(down, up));
			if (best > 0.001f)
			{
				// C.y += best, fn_007FD140 (the object's origin) and PhysOb::SetUpPos 0x7FC760
				po.body.SetUpPos(po.body.Rotation(), po.body.ObjectOrigin() + glm::vec3(0.0f, best, 0.0f));
				raised = true;
				break;
			}
		}
		if (!raised)
		{
			break;
		}
	}
}

PhysicsObject* PhysicsObjects::Find(entt::entity entity)
{
	for (auto& po : g_Objects)
	{
		if (po->entity == entity)
		{
			return po.get();
		}
	}
	return nullptr;
}

bool PhysicsObjects::IsFlying(entt::entity entity)
{
	const auto* po = Find(entity);
	return po != nullptr && !po->body.resting;
}

void PhysicsObjects::GameTurnUpdate()
{
	// the fragments' timers (Fragment::ProcessTimer 0x76EAF0) run from GGame::ProcessTurn 0x54E768: Game::GameLogicLoop
	BeginTurn();
	for (int substep = 0; substep < PhysOb::k_SubstepsPerTurn; ++substep) // cmp eax, 0x14 at 0x646046
	{
		Substep();
	}
	EndTurn();
	// the object's Pos and angles are set every substep (the game logic sees the end of the turn)
	auto& registry = Locator::entitiesRegistry::value();
	bool moved = false;
	for (const auto& po : g_Objects)
	{
		if (!po->body.resting && registry.Valid(po->entity))
		{
			moved = true;
			SyncTransform(*po);
			if (const auto& handlers = HandlersOf(po->entity); handlers.moved)
			{
				handlers.moved(po->entity);
			}
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

void PhysicsObjects::UpdateFrame(float turnFraction, float seconds)
{
	Dust::Update(std::min(seconds, 0.25f));
	// fn_00646FE0 (GLandscape::Draw 0x5E49DC), every frame for each awake entry: fn_007FCE80(PhysOb, g3d, the turn
	// fraction g_game +0x205D64) lerps the 12 floats of the turn-start matrix (+0xAC) to the end one (+0x7C) cell by cell
	// (0x7FCED2..0x7FCFFD), normalises each row (fn_007FB5C0 0x7FD000), scales them and takes T - R s com as the origin.
	// A body at rest has no entry of its own pose: it is drawn at its Transform (the end of the turn).
	// (pending) the -Radius < T.y filter (0x647017), the turn of animated meshes by pi/2 (vt +0x1AC, 0x7FD009) and vt +0x184
	auto& registry = Locator::entitiesRegistry::value();
	const float f = turnFraction;
	bool moving = false;
	for (const auto& po : g_Objects)
	{
		if (!registry.Valid(po->entity))
		{
			continue;
		}
		if (po->body.resting)
		{
			if (registry.AllOf<PhysicsDrawPose>(po->entity))
			{
				registry.Remove<PhysicsDrawPose>(po->entity);
			}
			continue;
		}
		moving = true;
		const auto& r1 = po->body.Rotation();
		const auto& r0 = po->turnStarted ? po->turnStartRotation : r1;
		const auto c1 = po->body.Centre();
		const auto c0 = po->turnStarted ? po->turnStartCentre : c1;
		glm::mat3 rotation;
		for (int row = 0; row < 3; ++row)
		{
			rotation[row] = r0[row] + (r1[row] - r0[row]) * f; // fld M; fsub M0; fmul f; fadd M0
		}
		lh_matrix::NormaliseRows(rotation);
		const auto centre = c0 + (c1 - c0) * f;
		auto& pose = registry.AllOf<PhysicsDrawPose>(po->entity) ? registry.Get<PhysicsDrawPose>(po->entity)
		                                                           : registry.Assign<PhysicsDrawPose>(po->entity);
		pose.rotation = rotation;
		pose.position = po->body.ObjectOrigin(rotation, centre);
	}
	if (moving)
	{
		registry.SetDirty(); // the drawn instances change every frame while something flies
	}
}

void PhysicsObjects::Clear()
{
	g_Objects.clear();
	g_Capacity = 0; // DeleteAll 0x6442B0
}

PhysicsClass PhysicsObjects::ClassOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MapShield>(entity))
	{
		return PhysicsClass::Shield;
	}
	if (registry.AllOf<Villager>(entity))
	{
		return PhysicsClass::Villager;
	}
	if (registry.AllOf<Animal>(entity))
	{
		return PhysicsClass::Animal;
	}
	if (registry.AllOf<Tree>(entity))
	{
		return PhysicsClass::Tree;
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		return PhysicsClass::DeadTree;
	}
	if (registry.AllOf<Pot>(entity))
	{
		return PhysicsClass::Pot;
	}
	if (registry.AllOf<Fragment>(entity))
	{
		return PhysicsClass::Fragment;
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return PhysicsClass::Building;
	}
	if (Rocks::IsRock(entity))
	{
		return PhysicsClass::Rock;
	}
	return PhysicsClass::Other;
}

void PhysicsObjects::SetClassHandlers(PhysicsClass type, ClassHandlers handlers)
{
	g_ClassHandlers.at(static_cast<size_t>(type)) = std::move(handlers);
}

void PhysicsObjects::ForEach(const std::function<void(const PhysicsObject&)>& func)
{
	for (const auto& object : g_Objects)
	{
		func(*object);
	}
}
