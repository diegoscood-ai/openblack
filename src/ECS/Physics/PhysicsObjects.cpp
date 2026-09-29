/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsObjects.h"

#include "Buildings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/FishShoals.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

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
PhysicsObjects::Handlers g_Handlers;
float g_Accumulator = 0.0f;
int g_Substep = 0;

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
	if (const auto* c = registry.TryGet<const Tree>(entity))
	{
		return &info.tree.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const DeadTree>(entity))
	{
		return &info.tree.at(static_cast<size_t>(c->type));
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
	const float scale = transform.scale.x;
	const auto size = mesh->GetBoundingBox().Size();
	const float height = size.y * scale;                         // Object::GetHeight
	const float radius = 0.5f * std::max(size.x, size.z) * scale; // Object::Get2DRadius
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

float LifeOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		return static_cast<float>(villager->health) / 100.0f;
	}
	if (const auto* life = registry.TryGet<const Life>(entity))
	{
		return life->value;
	}
	return 1.0f;
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
	ring.cell = 0x3F;
	AddWaterRing(ring);
}

/// Villager::VillagerDead / Animal::SetDying. TODO(physics): the corpse and the death states; the object goes.
void Kill(entt::entity entity, const char* reason)
{
	auto& registry = Locator::entitiesRegistry::value();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} died ({})", registry.AllOf<Villager>(entity) ? "villager" : "animal",
	                   reason);
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		if (auto* abode = registry.TryGet<Abode>(villager->abode))
		{
			abode->inhabitants.erase(entity);
		}
		if (auto* town = registry.TryGet<Town>(villager->town))
		{
			town->homelessVillagers.erase(entity);
		}
	}
	PhysicsObjects::RemoveObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}

/// Object::ApplyEffect with the crush preset g_EffectInfo[3] (crush 1.0) x the object's defenceMultiplierCrush.
void ReduceLife(entt::entity entity, float damage)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* villager = registry.TryGet<Villager>(entity))
	{
		const float life = std::max(0.0f, static_cast<float>(villager->health) / 100.0f - damage);
		villager->health = static_cast<uint32_t>(std::lround(life * 100.0f));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: villager hurt {:.3f}, life {:.2f}", damage, life);
		if (villager->health == 0)
		{
			Kill(entity, "impact");
		}
		return;
	}
	auto& life = registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity);
	life.value = std::max(0.0f, life.value - damage);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: animal hurt {:.3f}, life {:.2f}", damage, life.value);
	if (life.value <= 0.0f)
	{
		Kill(entity, "impact");
	}
}

/// ReactToPhysicsImpact (vt +0x7AC), per class. Returns false when the entry went away.
bool ReactToPhysicsImpact(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = po.entity;
	if (g_Handlers.reactToImpact && g_Handlers.reactToImpact(entity, po))
	{
		return false;
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return Buildings::ReactToPhysicsImpact(entity, po);
	}
	const float g = po.GLoad();
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		// Living::ReactToPhysicsImpact 0x5ED3E0
		if (g > 2.0f)
		{
			float multiplier = 1.0f;
			if (const auto* animal = registry.TryGet<const Animal>(entity); animal != nullptr && Locator::infoConstants::has_value())
			{
				multiplier = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type)).defenceMultiplierCrush;
			}
			ReduceLife(entity, (g - 2.0f) * 0.03f * multiplier);
			return registry.Valid(entity);
		}
		return true;
	}
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

/// EndPhysics (vt +0x790) at rest. Returns the entity that stays as a resting proxy (entt::null: none).
entt::entity EndPhysics(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	SyncTransform(po);
	auto entity = po.entity;
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		// Villager/Animal::EndPhysics: stands up where it landed (the three landing poses are not done yet)
		auto& transform = registry.Get<Transform>(entity);
		const auto forward = transform.rotation[2];
		const float yaw = std::atan2(forward.x, forward.z);
		transform.rotation = glm::mat3(glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw)), glm::vec3(0.0f, 1.0f, 0.0f),
		                               glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw)));
		if (Locator::terrainSystem::has_value())
		{
			transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
		}
		if (po.body.inWater)
		{
			Kill(entity, "drowned");
			return entt::null;
		}
		if (LifeOf(entity) <= 0.0f)
		{
			Kill(entity, "landed dead");
			return entt::null;
		}
	}
	if (registry.AllOf<Fragment>(entity))
	{
		return Buildings::FragmentEndPhysics(entity, po);
	}
	if (g_Handlers.endPhysics)
	{
		entity = g_Handlers.endPhysics(entity, po);
	}
	registry.SetDirty();
	return entity;
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
	g_Objects.erase(g_Objects.begin() + static_cast<std::ptrdiff_t>(index));
}

float Radius2D(entt::entity entity)
{
	const auto* mesh = MeshOf(entity);
	if (mesh == nullptr)
	{
		return 0.0f;
	}
	const auto size = mesh->GetBoundingBox().Size() * Locator::entitiesRegistry::value().Get<const Transform>(entity).scale;
	return 0.5f * std::max(size.x, size.z);
}

/// fn_644DF0: a resting body for an object a moving body may hit.
void AddProxy(entt::entity entity)
{
	auto po = std::make_unique<PhysicsObject>();
	po->entity = entity;
	po->villager = Locator::entitiesRegistry::value().AllOf<Villager>(entity);
	const bool dynamic = PhysicsObjects::CanBecomeAPhysicsObject(entity);
	if (!SetUpBody(entity, po->body, dynamic))
	{
		return;
	}
	po->body.resting = true;
	po->flags = PhysicsObject::Awake;
	if (Locator::entitiesRegistry::value().AnyOf<Abode, StoragePit>(entity))
	{
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
			po.body.density += 0.01f; // corpses sink
		}
		po.forceSum = glm::vec3(0.0f);
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
	std::vector<std::pair<glm::vec2, float>> boxes;
	for (const auto& po : g_Objects)
	{
		if (!po->body.resting)
		{
			const auto v = po->body.velocity;
			boxes.emplace_back(glm::vec2(po->body.Centre().x, po->body.Centre().z),
			                   glm::length(glm::vec2(v.x, v.z)) * 0.1f + po->body.Radius());
		}
	}
	if (!boxes.empty())
	{
		std::vector<entt::entity> candidates;
		registry.Each<const Transform, const Mesh>([&](entt::entity entity, const Transform& transform, const Mesh&) {
			if (!PhysicsObjects::InteractsWithPhysicsObjects(entity))
			{
				return;
			}
			const glm::vec2 at(transform.position.x, transform.position.z);
			const float reach = Radius2D(entity);
			for (const auto& [centre, half] : boxes)
			{
				const auto d = glm::abs(at - centre);
				if (d.x <= half + reach && d.y <= half + reach)
				{
					candidates.push_back(entity);
					return;
				}
			}
		});
		for (const auto entity : candidates)
		{
			if (auto* po = PhysicsObjects::Find(entity))
			{
				po->flags |= PhysicsObject::Awake;
			}
			else
			{
				AddProxy(entity);
			}
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
			// Living::HasSunk: a sunk villager or animal drowns
			if (po.body.density > 1.0f && registry.AnyOf<Villager, Animal>(po.entity))
			{
				Kill(po.entity, "sunk");
				continue;
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
		switch (result)
		{
		case PhysOb::Result::Stopped:
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: entity {} at rest at ({:.2f}, {:.2f}, {:.2f})",
			                   static_cast<uint32_t>(po.entity), po.body.Centre().x, po.body.Centre().y, po.body.Centre().z);
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
			else if (registry.Valid(entity))
			{
				registry.Destroy(entity);
				registry.SetDirty();
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
		if (po->soundTurns > 0)
		{
			--po->soundTurns;
		}
		const float sum2 = glm::dot(po->forceSum, po->forceSum);
		po->impact = sum2 > 0.0001f ? std::sqrt(sum2) * 0.05f : 0.0f;
		if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr && !po->body.resting)
		{
			const auto c = po->body.Centre();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics trace: entity {} at ({:.2f}, {:.2f}, {:.2f}) v {:.2f} contacts {} G {:.2f}",
			                   static_cast<uint32_t>(po->entity), c.x, c.y, c.z, glm::length(po->body.velocity),
			                   po->body.numContacts, po->GLoad());
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
		// AttemptToAddSoundEvent 0x6464F0: landing in the sea splashes (fn_74F2D0) and leaves a ring
		// TODO(physics): the collision samples (SamplePlayAnimEffect by SOUND_COLLISION_TYPE) and the 6 dust particles
		if (!po->body.resting && po->soundTurns == 0 &&
		    (po->hitBy != nullptr || po->impact > po->body.Mass() * 4.905f) && po->body.inWater)
		{
			SplashWater(po->body.Centre());
			AddRipple(*po);
			po->soundTurns = 2;
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
		constexpr std::array<int, 5> k_Toys = {14, 20, 16, 15, 19}; // meshes 399..403
		for (uint32_t m = 0; m < k_Toys.size(); ++m)
		{
			if (MeshIs(entity, 399 + m))
			{
				return k_Toys.at(m);
			}
		}
		// TODO(physics): IsFence -> 18
		return 1;
	}
	return 1;
}

bool PhysicsObjects::InteractsWithPhysicsObjects(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<Tree, Field, BigForest, Fragment>(entity))
	{
		return false; // standing trees: thrown objects go through them; fragments only hit the landscape
	}
	if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		// piles do not interact (fn_66ED40)
		return pot->type == PotInfo::HandWood || pot->type == PotInfo::HandFood;
	}
	// fields are never hit (Field::InteractsWithPhysicsObjects 0x528020)
	if (const auto* abode = registry.TryGet<const Abode>(entity); abode != nullptr && abode->type == AbodeNumber::Field)
	{
		return false;
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
	return registry.AnyOf<MobileStatic, MobileObject, Villager, Animal, Tree, DeadTree, Pot, Fragment>(entity);
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
	const float scale = registry.AllOf<Transform>(entity) ? registry.Get<const Transform>(entity).scale.x : 1.0f;
	return std::max(scale * scale * scale * weight, 0.01f);
}

PhysicsObject* PhysicsObjects::AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
                                         entt::entity thrower, bool fromHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !CanBecomeAPhysicsObject(entity))
	{
		return nullptr;
	}
	if (auto* existing = Find(entity))
	{
		if (!existing->body.resting)
		{
			return nullptr;
		}
		RemoveObject(entity);
	}
	auto po = std::make_unique<PhysicsObject>();
	po->entity = entity;
	po->thrower = thrower;
	po->villager = registry.AllOf<Villager>(entity);
	if (!SetUpBody(entity, po->body, true))
	{
		return nullptr;
	}
	po->body.SetAngularVelocity(angularVelocity);
	const float speed = glm::length(velocity);
	po->body.velocity = speed > PhysOb::k_MaxSpeed ? velocity * (PhysOb::k_MaxSpeed / speed) : velocity;
	po->flags = PhysicsObject::Awake | (fromHand ? PhysicsObject::FromHand : 0);
	po->byPlayer = fromHand;
	g_Objects.push_back(std::move(po));
	return g_Objects.back().get();
}

void PhysicsObjects::RemoveObject(entt::entity entity)
{
	for (size_t i = 0; i < g_Objects.size(); ++i)
	{
		if (g_Objects[i]->entity == entity)
		{
			RemoveAt(i);
			return;
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

void PhysicsObjects::Update(float seconds)
{
	// objects with timers of their own (fragments) count game turns even when nothing moves
	static float s_TurnClock = 0.0f;
	s_TurnClock += std::min(seconds, 0.25f);
	while (s_TurnClock >= PhysOb::k_Dt * PhysOb::k_SubstepsPerTurn)
	{
		s_TurnClock -= PhysOb::k_Dt * PhysOb::k_SubstepsPerTurn;
		Buildings::ProcessTurn();
	}
	const bool anyMoving = std::any_of(g_Objects.begin(), g_Objects.end(), [](const auto& po) { return !po->body.resting; });
	if (!anyMoving && g_Substep == 0)
	{
		g_Accumulator = 0.0f;
		return;
	}
	g_Accumulator += std::min(seconds, 0.25f);
	while (g_Accumulator >= PhysOb::k_Dt)
	{
		g_Accumulator -= PhysOb::k_Dt;
		if (g_Substep == 0)
		{
			BeginTurn();
		}
		Substep();
		if (++g_Substep == PhysOb::k_SubstepsPerTurn)
		{
			EndTurn();
			g_Substep = 0;
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& po : g_Objects)
	{
		if (!po->body.resting && registry.Valid(po->entity))
		{
			SyncTransform(*po);
			if (g_Handlers.moved)
			{
				g_Handlers.moved(po->entity);
			}
		}
	}
	registry.SetDirty();
}

void PhysicsObjects::Clear()
{
	g_Objects.clear();
	g_Accumulator = 0.0f;
	g_Substep = 0;
}

void PhysicsObjects::SetHandlers(Handlers handlers)
{
	g_Handlers = std::move(handlers);
}
