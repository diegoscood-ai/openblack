/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DynamicsSystem.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Events/DrawListEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishShoals.h"
#include "ECS/HeldApply.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/Hitter.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/PhysicsGround.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/Implementations/GameDynamicsWorld.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/Texture2D.h"
#include "InfoConstants.h"
#include "Magic/Objects/MapShield.h"
#include "Physics/BodyShapes.h"
#include "Physics/DamageMesh.h"
#include "Physics/LivingRules.h"
#include "Physics/ObjectRules.h"
#include "Physics/PairRules.h"
#include "Physics/TurnRules.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;
using openblack::ecs::life::LifeOf;
using openblack::ecs::systems::DynamicsSystem;
using openblack::ecs::systems::DynamicsSystemInterface;
using openblack::ecs::systems::GameDynamicsWorld;
using openblack::ecs::systems::PhysicsClassHooks;

namespace
{

using openblack::ecs::physics::k_DefaultConstants;
using openblack::ecs::physics::k_NumConstants;
using openblack::ecs::physics::Prediction;
} // namespace

DynamicsSystem::DynamicsSystem()
    : DynamicsSystem(std::make_unique<GameDynamicsWorld>())
{
}

DynamicsSystem::DynamicsSystem(std::unique_ptr<openblack::ecs::dynamics::World> world)
    : _world(std::move(world))
{
}

/// This system, through its interface
DynamicsSystemInterface& DynamicsSystem::Dynamics()
{
	return *this;
}

/// This system's state
openblack::ecs::physics::State& DynamicsSystem::PhysicsState()
{
	return _state;
}

// ---- the collision sounds and the dust puffs ----

namespace
{
constexpr int k_Ground = 27;   // SOUND_COLLISION_TYPE_GROUND
constexpr int k_Water = 29;    // SOUND_COLLISION_TYPE_WATER
constexpr int k_Fragment = 31; // SOUND_COLLISION_TYPE_FRAGMENT
constexpr int k_Grain = 11;
constexpr int k_Bush = 2;
constexpr int k_HollowWood = 18;
constexpr int k_BrickBuilding = 10;

// the A (hitter) and B (hit) codes of each SOUND_COLLISION_TYPE
constexpr std::array<int, 33> k_TabA = {21, 24, 20, 20, 20, 20, 20, 19, 19, 19, 19, 20, 25, 22, 22, 22, 22,
                                        22, 30, 25, 35, 31, 24, 23, 24, 25, 25, 21, 21, 21, 33, 34, 42};
constexpr std::array<int, 33> k_TabB = {11, 14, 10, 10, 10, 10, 10, 9,  9,  9,  9,  10, 15, 12, 12, 12, 12,
                                        12, 20, 15, 23, 19, 14, 13, 14, 15, 15, 16, 18, 17, 21, 22, 24};

constexpr size_t k_MaxDustPuffs = 1024;
} // namespace

/// The shape of the puffs: blobsa.raw, the alpha of data\blobs.raw (white RGB).
std::optional<graphics::TextureHandle> DynamicsSystem::DustTexture()
{
	auto& textures = _world->Resources().GetTextures();
	const auto id = entt::hashed_string("raw/blobsa").value();
	if (!textures.Contains(id))
	{
		try
		{
			auto& fileSystem = _world->FileSystem();
			textures.Load(id, resources::Texture2DLoader::FromDiskTag {},
			              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "blobsa.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Dust: cannot load Data/blobsa.raw: {}", e.what());
			return std::nullopt;
		}
	}
	return textures.Handle(id)->GetNativeHandle();
}

namespace
{
glm::vec2 DustCellUv(uint32_t cell)
{
	return graphics::frame_anim::SpriteCellUv(static_cast<int>(cell), 8)[0];
}

/// (r - 100) x 0.02, the int converted to float first
float DustAxis(uint32_t r)
{
	const auto centred = static_cast<int32_t>(r) - 100;
	return static_cast<float>(centred) * 0.02f;
}
} // namespace

/// SOUND_COLLISION_TYPE of an object (info collideSound; a Fragment is always BUSH, DeadTree mesh 406 HOLLOW_WOOD).
int DynamicsSystem::CollideSoundType(entt::entity entity)
{
	const auto& registry = _world->Entities();
	if (registry.AllOf<Fragment>(entity))
	{
		return k_Bush;
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		if (const auto* mesh = registry.TryGet<const Mesh>(entity);
		    mesh != nullptr && mesh->id == resources::HashIdentifier(static_cast<MeshId>(406)))
		{
			return k_HollowWood;
		}
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return k_BrickBuilding;
	}
	if (const auto* info = Dynamics().ObjectInfo(entity))
	{
		return static_cast<int>(info->collideSound);
	}
	return 0;
}

namespace
{
/// The velocity of a ground impact's puffs: (LocalRand(201) - 100) x 0.02 on each axis, +-2 units per second, drawn
/// z, y, x.
glm::vec3 PuffVelocity()
{
	// z, y, x in that order
	const float z = DustAxis(game_random::LocalRand(201));
	const float y = DustAxis(game_random::LocalRand(201));
	const float x = DustAxis(game_random::LocalRand(201));
	return {x, y, z};
}
} // namespace

/// The collision sound of a body at the end of a turn (its impact and what hit it are set), plus the ground dust and
/// the water splash of a landing.
void DynamicsSystem::AttemptCollisionSound(const PhysicsEntry& po)
{
	const auto& registry = _world->Entities();
	const auto obj = po.entity;
	const auto* hitBy = hitter::HitterEntry(PhysicsState().objects, po);
	const auto hitObj = hitBy != nullptr ? hitBy->entity : entt::entity {entt::null};
	// a rock hitting a building: the building plays its own sound
	if (hitObj != entt::null && registry.AnyOf<Abode, StoragePit>(hitObj) && Dynamics().PhysicallyDestroysAbodes(obj))
	{
		return;
	}
	if (!PhysicsState().soundPairs.MaySound(obj, hitObj))
	{
		return;
	}
	const int typeA = CollideSoundType(obj);
	int typeB = k_Ground;
	const auto at = po.body.Centre();
	if (hitObj != entt::null)
	{
		typeB = CollideSoundType(hitObj);
	}
	else if (_world->Land() != nullptr)
	{
		// not IsDryLand (altitude < 4, the water bit is not read) -> ring; and no cell (off the map, no block) or an
		// altitude under 3 at the cell rounded to the nearest -> WATER; altitude 3: ring + dust
		const auto& terrain = *_world->Land();
		const bool land = ecs::sea_cells::IsDryLand(terrain, ecs::sea_cells::CellOf(at));
		const auto* c = land ? nullptr : ecs::sea_cells::CellAt(terrain, ecs::sea_cells::RoundedCellOf(at));
		const bool deep = !land && (c == nullptr || terrain.GetCellAltitude(*c) < 3);
		const float ground = terrain.GetHeightAt(glm::vec2(at.x, at.z));
		const float size = std::min(2.0f * po.body.Radius(), 5.0f);
		const auto dustAt = glm::vec3(at.x, ground, at.z);
		if (deep)
		{
			typeB = k_Water;
			ecs::SplashWater(at);
		}
		if (!land)
		{
			// the body radius (max |vertex - CoM| x scale); rate 1 / r and growth 2 r with no guard
			const float radius = po.body.Radius();
			const ecs::WaterRing ring {
			    .position = glm::vec3(at.x, 0.1f, at.z), .growth = 2.0f * radius, .rate = 1.0f / radius, .cell = 0x3F};
			ecs::AddWaterRing(ring);
		}
		// 6 liquid particles (1 s, no gravity) of the foam / dust colour, which the snow lying at the point turns
		// towards the land's light by its depth; without snow it stays
		const auto snow = _world->SnowAt(dustAt);
		const uint32_t colour = deep ? 0x28C8F0F4u : 0x50806040u;
		const uint32_t tinted = snow == 0 ? colour : openblack::physics::turn::BlendColour(colour, _world->LandColour(), snow);
		for (int i = 0; i < 6; ++i)
		{
			Dynamics().AddPuff(dustAt, PuffVelocity(), size, tinted);
		}
	}
	if (typeA == k_Fragment || typeB == k_Fragment)
	{
		return;
	}
	PhysicsState().soundPairs.Add(obj, hitObj);
	// level from the unscaled info weight: 3 soft (g < 1.25), 2, 1 hard (g > 3); GRAIN never at 1
	const auto* info = Dynamics().ObjectInfo(obj);
	const float weight = info != nullptr ? info->weight : 0.0f;
	const float g = weight > 0.0f ? po.impact / (weight * 9.81f) : 1000.0f;
	int level = g < 1.25f ? 3 : g > 3.0f ? 1 : 2;
	if (typeA == k_Grain && level == 1)
	{
		level = 2;
	}
	const int a = k_TabA.at(static_cast<size_t>(std::clamp(typeA, 0, 32)));
	const int b = k_TabB.at(static_cast<size_t>(std::clamp(typeB, 0, 32)));
	// the key {level, 0, A, B, 75 COLLIDE}
	const std::array<int32_t, 5> key = {level, 0, a, b, 75};
	if (debug_env::TraceLogs(debug_env::PhysicsTrace(), "game"))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Collision sound: types {}/{} level {} -> editor.sad key {{{}, 0, {}, {}, 75}}",
		                   typeA, typeB, level, level, a, b);
	}
	// the channel follows the object unless its A code is 0x16
	Dynamics().PlayAnimEffect(key, obj, at, a != 0x16);
}

/// Every frame: the puffs age, move and grow or shrink; those past their life go.
void DynamicsSystem::UpdateDust(float seconds)
{
	if (PhysicsState().dustPuffs.empty() || seconds <= 0.0f)
	{
		return;
	}
	using openblack::physics::turn::PuffLife;
	for (auto& puff : PhysicsState().dustPuffs)
	{
		puff.age += seconds;
		// gone once past its kind's life, before moving
		if (puff.age > PuffLife(puff.kind))
		{
			continue;
		}
		puff.draw.position += puff.velocity * seconds;
		puff.draw.halfSize = openblack::physics::turn::DustPuffSize(puff.kind, puff.age, puff.size);
		puff.draw.uvMin =
		    DustCellUv(static_cast<uint32_t>(openblack::physics::turn::DustPuffFrame(puff.kind, puff.seed, puff.age)));
	}
	std::erase_if(PhysicsState().dustPuffs, [](const DustPuff& p) { return p.age > PuffLife(p.kind); });
}

void DynamicsSystem::EndPrediction()
{
	// the prediction stops and its body is released
	if (!PhysicsState().prediction.active)
	{
		return;
	}
	// (openblack) the drawn pose goes with the prediction, unless the object already has a body of its own
	auto& registry = _world->Entities();
	if (registry.Valid(PhysicsState().prediction.object) && registry.AllOf<PhysicsDrawPose>(PhysicsState().prediction.object) &&
	    Dynamics().Find(PhysicsState().prediction.object) == nullptr)
	{
		registry.Remove<PhysicsDrawPose>(PhysicsState().prediction.object);
	}
	PhysicsState().prediction = Prediction {};
}

namespace
{
/// What hit the body, entt::null for nothing or for an object already gone: (openblack) a hitter deleted earlier in the
/// turn keeps its body in the list until the next turn start, but its object is destroyed at once, where the original's
/// is only marked for deletion
entt::entity HitterOf(const hitter::Entries& objects, const PhysicsEntry& po, const Registry& registry)
{
	const auto* hitBy = hitter::HitterEntry(objects, po);
	return hitBy != nullptr && registry.Valid(hitBy->entity) ? hitBy->entity : entt::entity {entt::null};
}

ImpactInfo ImpactOf(const hitter::Entries& objects, const PhysicsEntry& po, const Registry& registry)
{
	ImpactInfo info {
	    .g = po.GLoad(),
	    .hitBy = HitterOf(objects, po, registry),
	    .thrower = po.thrower,
	    .byPlayer = po.byPlayer,
	};
	return info;
}
} // namespace

const graphics::L3DMesh* DynamicsSystem::MeshOf(entt::entity entity)
{
	const auto* mesh = _world->Entities().TryGet<const Mesh>(entity);
	if (mesh == nullptr)
	{
		return nullptr;
	}
	// the Mesh component: a broken building's stays its intact model (its damage mesh is a components::DrawMesh)
	const auto id = mesh->id;
	auto& meshes = _world->Resources().GetMeshes();
	if (!meshes.Contains(id))
	{
		return nullptr;
	}
	return &(*meshes.Handle(id));
}

bool DynamicsSystem::MeshIs(entt::entity entity, uint32_t meshIndex)
{
	const auto* mesh = _world->Entities().TryGet<const Mesh>(entity);
	return mesh != nullptr && mesh->id == resources::HashIdentifier(static_cast<MeshId>(meshIndex));
}

const GObjectInfo* DynamicsSystem::InfoOf(entt::entity entity)
{
	if (_world->Info() == nullptr)
	{
		return nullptr;
	}
	const auto& info = *_world->Info();
	const auto& registry = _world->Entities();
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
		// a one-shot orb takes the mobile object info right after the whale's (inferred: the whale's entry is not
		// verified either)
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
	if (const auto* c = registry.TryGet<const MagicShield>(entity))
	{
		// the physical shield's info (its weight of 50000 gives it a heavy body)
		return &info.mapShield.at(c->kind == MagicShield::Kind::Physical ? 1 : 0);
	}
	return nullptr;
}

namespace
{
/// The mesh data of a body: the submeshes flagged isPhysics, otherwise the LOD 0 ones; every vertex and triangle of
/// them.
bool CollectVertices(const graphics::L3DMesh& mesh, std::vector<glm::vec3>& positions,
                     std::vector<std::array<uint32_t, 3>>& triangles)
{
	const auto& subMeshes = mesh.GetSubMeshes();
	const bool anyPhysics = std::any_of(subMeshes.begin(), subMeshes.end(), [](const auto& s) { return s->IsPhysics(); });
	const bool anyLod0 =
	    std::any_of(subMeshes.begin(), subMeshes.end(), [](const auto& s) { return (s->GetFlags().lodMask & 1) != 0; });
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

/// The draw's quarter turn of an animated (boned) mesh, after the rows are normalised: c, s are the cosine and sine
/// of the float pi / 2 kept in double precision, each product and sum rounded to float: r0' = c r0 + s r2,
/// r2' = c r2 - s r0, row 1 unchanged. Villager / Animal bodies are in the original's axes (no turn); their Transform
/// is the drawn rotation, so it is the body's rows turned.
constexpr double k_QuarterTurnCos = -4.371139000186241e-08; // cosine of the float pi / 2
constexpr double k_QuarterTurnSin = 0.9999999999999990;     // sine of the float pi / 2
glm::mat3 DrawQuarterTurn(const glm::mat3& rows)
{
	glm::mat3 out = rows;
	for (int k = 0; k < 3; ++k)
	{
		const auto cr0 = static_cast<float>(k_QuarterTurnCos * rows[0][k]);
		const auto sr2 = static_cast<float>(k_QuarterTurnSin * rows[2][k]);
		const auto cr2 = static_cast<float>(k_QuarterTurnCos * rows[2][k]);
		const auto sr0 = static_cast<float>(k_QuarterTurnSin * rows[0][k]);
		out[0][k] = cr0 + sr2;
		out[2][k] = cr2 - sr0;
	}
	return out;
}

/// The world matrix rows for a yaw a and a scale S, no quarter turn: a != 0 and S != 1: (fl(c S), 0, fl(s S)),
/// (0, S, 0), (-fl(s S), 0, fl(c S)) with c, s the cosine and sine of a unrounded; a != 0 and S == 1: c unrounded,
/// s stored as a float; a == 0: diag(S) or the identity. The "0 +" forms make a -0 a +0. glm: column k = the
/// original's row k.
glm::mat3 WorldMatrixRows(float a, float S)
{
	glm::mat3 m(0.0f);
	if (a != 0.0f && S != 1.0f)
	{
		const double ca = std::cos(static_cast<double>(a));
		const double sa = std::sin(static_cast<double>(a));
		const auto cS = static_cast<float>(ca * S);
		const auto sS = static_cast<float>(sa * S);
		m[0] = glm::vec3(0.0f + cS, 0.0f, 0.0f + sS);
		m[1] = glm::vec3(0.0f, S, 0.0f);
		m[2] = glm::vec3(0.0f - sS, 0.0f, cS - 0.0f);
	}
	else if (a != 0.0f)
	{
		const double ca = std::cos(static_cast<double>(a));
		const auto sa = static_cast<float>(std::sin(static_cast<double>(a)));
		m[0] = glm::vec3(static_cast<float>(0.0 + ca), 0.0f, 0.0f + sa);
		m[1] = glm::vec3(0.0f, 1.0f, 0.0f);
		m[2] = glm::vec3(0.0f - sa, 0.0f, static_cast<float>(ca - 0.0));
	}
	else
	{
		m = glm::mat3(S);
	}
	return m;
}
} // namespace

/// Body::Initialise and the class's body setup from the entity's transform and mesh.
bool DynamicsSystem::SetUpBodyOf(entt::entity entity, openblack::physics::Body& body, bool dynamic, bool handPose)
{
	if (Dynamics().ClassOf(entity) == PhysicsClass::Creature)
	{
		// a creature builds its own body, from its skeleton (pending)
		return Dynamics().ClassHooks().SetUpBody(Dynamics(), entity, body);
	}
	const auto* mesh = MeshOf(entity);
	if (mesh == nullptr)
	{
		return false;
	}
	auto& registry = _world->Entities();
	const auto& transform = registry.Get<const Transform>(entity);
	// a shield's body has its collision scale, which lags the drawn one (Magic/Objects/MapShield)
	const float scale = registry.AllOf<MagicShield>(entity) ? magic::map_shield::CollisionScale(entity) : transform.scale.x;
	const auto size = mesh->GetBoundingBox().Size();
	const float height = size.y * scale;                          // the object's height
	const float radius = 0.5f * std::max(size.x, size.z) * scale; // the object's 2D radius
	if (const auto* fragment = registry.TryGet<const Fragment>(entity); fragment != nullptr && fragment->mesh)
	{
		// a fragment: the distinct vertices and a copy of each 0.45 behind along its normal (a slab), no faces (nothing is
		// hit by a fragment), about the fragment's origin; mass 30 x area; drag x 2
		std::vector<glm::vec3> points;
		std::vector<glm::vec3> normals;
		fragment->mesh->UniqueVertices(points, normals);
		const auto shape = openblack::physics::shapes::Fragment(points, normals);
		// Initialise takes the half height of the rock mesh the fragment is built as (the third mobile static, scale 1).
		// openblack guard: the piece's own half height without info.dat
		float rockHalfHeight = 0.5f * size.y;
		if (_world->Info() != nullptr)
		{
			const auto rockMesh = resources::HashIdentifier(_world->Info()->mobileStatic.at(2).meshId);
			if (const auto half = ecs::object::MeshHalfExtents(rockMesh))
			{
				rockHalfHeight = half->y;
			}
		}
		body.Initialise(scale, rockHalfHeight);
		body.SetUpConstants(Dynamics().BodyMassOf(entity), Dynamics().Constants(11), true);
		body.BuildShape(shape.points, shape.faces, shape.centreOfMass, shape.radius, shape.dragFactor, transform.rotation,
		                transform.position);
		return true;
	}
	const bool tree = registry.AllOf<Tree>(entity) || (registry.AllOf<DeadTree>(entity) && !MeshIs(entity, 406));
	const bool living = registry.AnyOf<Villager, Animal>(entity);
	if (tree || living)
	{
		body.Initialise(scale, 0.5f * size.y);
		body.SetUpConstants(Dynamics().BodyMassOf(entity), Dynamics().Constants(Dynamics().ConstantsType(entity)), true);
		if (tree)
		{
			const auto shape = openblack::physics::shapes::Tree(height, radius, scale, registry.AllOf<Tree>(entity));
			body.BuildShape(shape.points, shape.faces, shape.centreOfMass, shape.radius, shape.dragFactor, transform.rotation,
			                transform.position);
		}
		else
		{
			// the body in the original's axes, no quarter turn: in the map the start of physics writes the world matrix
			// (S R(yaw)) into the 3D matrix; from the hand SetUpPos takes the held matrix (the hand's angles at a throw, or
			// what the hand left for a put-down), which the hand's Transform already is (the hand's drawn pose, the
			// prediction's too: SetUpPos copies the 3D object's matrix, which the hand set). SetUpPos normalises the rows.
			// The world matrix rows carry S already, so SetUpPos takes them as they are; the Transform's are unit rows
			const auto rows =
			    handPose ? transform.rotation : WorldMatrixRows(ecs::villager_physics::ObjectYAngle(entity), scale);
			const auto kind = registry.AllOf<Animal>(entity) ? openblack::physics::shapes::Living::Animal
			                                                 : openblack::physics::shapes::Living::Villager;
			const auto shape = openblack::physics::shapes::LivingBody(kind, height, radius, scale);
			body.BuildShape(shape.points, shape.faces, shape.centreOfMass, shape.radius, shape.dragFactor, rows,
			                transform.position, 1.0f, !handPose);
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
	body.SetUpConstants(Dynamics().BodyMassOf(entity), Dynamics().Constants(Dynamics().ConstantsType(entity)), dynamic);
	body.Build(openblack::physics::shapes::FromModel(positions, triangles, scale), transform.rotation, transform.position);
	return true;
}

void DynamicsSystem::SyncTransform(const PhysicsEntry& po)
{
	auto& registry = _world->Entities();
	if (!registry.Valid(po.entity))
	{
		return;
	}
	auto& transform = registry.Get<Transform>(po.entity);
	transform.position = po.body.ObjectOrigin();
	// the object's angles from the body's matrix: a Living stays upright with its yaw, and so does a Tree (a tree
	// replanted from the hand stands straight); the others: the rows, (approximate) RotationYXZ of their angles equals them
	// only to rounding. A villager's or an animal's Transform is the drawn rotation: the yaw-only original rows (the body's
	// rows are the original's) with the draw's quarter turn (in flight and on the ground)
	if (registry.AnyOf<Villager, Animal>(po.entity))
	{
		transform.rotation = DrawQuarterTurn(Dynamics().RotationFromRows(po.entity, po.body.Axes()));
	}
	else if (registry.AllOf<Tree>(po.entity))
	{
		transform.rotation = Dynamics().RotationFromRows(po.entity, po.body.Axes());
	}
	else
	{
		transform.rotation = po.body.Axes();
	}
	if (auto* fixed = registry.TryGet<Fixed>(po.entity))
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
}

namespace
{
void AddRipple(const PhysicsEntry& po)
{
	const float radius = std::max(po.body.Radius(), 0.01f);
	const WaterRing ring {
	    .position = glm::vec3(po.body.Centre().x, 0.1f, po.body.Centre().z),
	    .growth = 2.0f * radius,
	    .rate = 1.0f / radius,
	    .cell = 0x30 // the bob ripple's cell (the water hit's ring takes 0x3F)
	};
	AddWaterRing(ring);
}
} // namespace

/// The impact reaction, per class. Returns false when the entry went away.
bool DynamicsSystem::ReactToPhysicsImpact(PhysicsEntry& po)
{
	auto& registry = _world->Entities();
	const auto entity = po.entity;
	if (registry.AllOf<MagicShield>(entity))
	{
		magic::map_shield::ReactToPhysicsImpact(entity, po); // Magic/Objects/MapShield
		return registry.Valid(entity);
	}
	if (const auto reacted =
	        Dynamics().ClassHooks().ReactToImpact(Dynamics(), po, ImpactOf(PhysicsState().objects, po, registry));
	    reacted.has_value())
	{
		return *reacted;
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return _world->BuildingDamage().ReactToImpact(entity, po);
	}
	if (from_hand::IsFence(entity))
	{
		// a thrown fence (not indestructible) that hits a store of wood goes into it, with the thrower's interface
		const auto hit = HitterOf(PhysicsState().objects, po, registry);
		auto& stores = ecs::resource_stores::Service();
		if (held_apply::FenceImpactTakes(true, registry.AllOf<Indestructible>(entity),
		                                 stores.IsStore(hit, ResourceType::Wood)) &&
		    stores.TakeObject(hit, entity, held_apply::ThrowerOf(po.byPlayer)))
		{
			return false;
		}
		return true;
	}
	const float g = po.GLoad();
	if (object_physics::IsRock(entity))
	{
		// a rock: worn by a hard knock, split once worn out
		return object_physics::KnockRock(entity, po, g);
	}
	return true;
}

/// The end of physics at rest, the class's part. Returns the entity that stays as a resting proxy (entt::null:
/// none). The object takes the body's pose first, and stands at its origin on the map's grid; at the turn's stop a
/// villager stands at the body's centre instead
entt::entity DynamicsSystem::EndPhysicsOfClass(PhysicsEntry& po, bool turnStop)
{
	auto& registry = _world->Entities();
	SyncTransform(po);
	if (registry.Valid(po.entity))
	{
		if (auto* transform = registry.TryGet<Transform>(po.entity))
		{
			const bool villagerAtStop = turnStop && registry.AllOf<Villager>(po.entity);
			transform->position = openblack::physics::objects::PlacedOnTheMap(
			    _world->Land(), villagerAtStop ? po.body.Centre() : transform->position);
			if (auto* fixed = registry.TryGet<Fixed>(po.entity))
			{
				fixed->boundingCenter = glm::vec2(transform->position.x, transform->position.z);
			}
		}
	}
	auto entity = po.entity;
	// its own flying-object reactions go (the predators fleeing from it stop)
	ecs::animal_ai::EndReactionsOf(entity);
	if (const auto kept = Dynamics().ClassHooks().EndPhysics(Dynamics(), po); kept.has_value())
	{
		entity = *kept;
		registry.SetDirty();
		return entity;
	}
	registry.SetDirty();
	return entity;
}

/// The end of physics at rest: the class's part, then the object goes back in the map cells; nothing for one that is
/// gone. After the class's part, but Villager, Animal, Scaffold, DeadTree and Fragment call it themselves in the
/// middle of theirs (EndPhysicsAsObject, the class hooks' CallsBackInMap), and a tree that becomes a DeadTree never calls
/// it (the DeadTree is inserted with no bounds test and returned). A tree searches the cells before its insert, so a
/// replanted tree does not see itself. The object's own part always returns the object, never null.
/// With insert and the object available (ecs::IsAvailable): inside the 512 x 512 cells it goes back in the map
/// cells, outside it is deleted (ToBeDeleted). Returns entt::null when the object that stays is the deleted one;
/// ProcessTurn makes the returned object the resting proxy. turnStop: the body stopped in the turn, rather than being
/// taken out of the physics
entt::entity DynamicsSystem::EndPhysics(PhysicsEntry& po, bool turnStop)
{
	const auto entity = po.entity;
	auto& registry = _world->Entities();
	// Villager / Animal (VillagerPhysics) and Scaffold put themselves back in the map, between their own work
	const bool classCallsBackInMap = Dynamics().ClassHooks().CallsBackInMap(Dynamics(), entity);
	const bool wasTree = registry.AllOf<Tree>(entity);
	const auto kept = EndPhysicsOfClass(po, turnStop);
	if (wasTree && registry.Valid(kept) && registry.AllOf<DeadTree>(kept))
	{
		// a tree's DeadTree goes in the cells with no bounds test and is returned; the object's own part is not called
		map_cells::InsertMapObject(kept);
		return kept;
	}
	// an UNAVAILABLE (marked) object is neither put back in the cells nor deleted again
	if (!classCallsBackInMap && ecs::IsAvailable(entity) && !Dynamics().EndPhysicsAsObject(entity))
	{
		return kept == entity ? entt::null : kept;
	}
	return kept;
}

bool DynamicsSystem::BreaksBuildings(entt::entity entity)
{
	return _world->Entities().Valid(entity) && PhysicallyDestroysAbodes(entity);
}

/// When a thing that breaks buildings stops or is removed from the physics, each building listed in the physics forgets
/// it: as the last thing that struck its damage mesh, and as its pass-through partner, so that the two collide again.
/// Nothing for anything else: the other bodies keep their partner, and a building that is not listed keeps its last
/// hitter. A building is tested whatever its state, even one marked for deletion
void DynamicsSystem::ForgetAsBuildingHitter(entt::entity object, bool breaksBuildings)
{
	if (!breaksBuildings)
	{
		return;
	}
	const auto& registry = _world->Entities();
	for (auto& other : PhysicsState().objects)
	{
		// (openblack) an entry whose object is already gone is no building
		if (other->entity == object || !registry.Valid(other->entity) || !registry.AnyOf<Abode, StoragePit>(other->entity))
		{
			continue;
		}
		_world->BuildingDamage().ForgetHitter(object, other->entity);
		if (other->thrower == object)
		{
			other->thrower = entt::null;
		}
	}
}

void DynamicsSystem::RemoveAt(size_t index)
{
	// the others' hitters are places in the list, which the original does not fix up either: a hitter's place names
	// whatever entry moves into it
	for (auto& other : PhysicsState().objects)
	{
		if (other->body.lastHit == &PhysicsState().objects[index]->body)
		{
			other->body.lastHit = nullptr;
		}
	}
	// out of the physics: drawn at its Transform again
	if (auto& registry = _world->Entities(); registry.Valid(PhysicsState().objects[index]->entity) &&
	                                         registry.AllOf<PhysicsDrawPose>(PhysicsState().objects[index]->entity))
	{
		registry.Remove<PhysicsDrawPose>(PhysicsState().objects[index]->entity);
	}
	// the last entry moves into its place
	const auto entity = PhysicsState().objects[index]->entity;
	openblack::physics::turn::RemoveByMovingLast(PhysicsState().objects, index);
	SyncInPhysics(entity);
}

void DynamicsSystem::SyncInPhysics(entt::entity entity)
{
	auto& registry = _world->Entities();
	if (!registry.Valid(entity))
	{
		return;
	}
	const bool inPhysics = IsFlying(entity) || registry.AllOf<CarriedByTornado>(entity);
	if (inPhysics == registry.AllOf<InPhysics>(entity))
	{
		return;
	}
	// a simulation-only mark: the drawing never reads it
	if (inPhysics)
	{
		registry.AssignOrReplaceState<InPhysics>(entity);
	}
	else
	{
		registry.RemoveState<InPhysics>(entity);
	}
}

namespace
{
/// A corner of the physics' cell boxes (RaiseClearOfWhatIsUnder, BeginTurn): metres to fixed point, truncated,
/// and off the map each signed cell clamped to 0 .. 511
glm::ivec2 BoxCell(float x, float z)
{
	const auto clamped = [](float metres) {
		return std::clamp<int32_t>(map_coords::SignedCellOf(map_coords::ToFixed(metres)), 0,
		                           static_cast<int32_t>(map_coords::k_MapCells) - 1);
	};
	return {clamped(x), clamped(z)};
}
} // namespace

/// Grows the list's capacity by 16 when it is full
void DynamicsSystem::MakeSureEndSlotIsFree()
{
	if (PhysicsState().objects.size() >= PhysicsState().capacity)
	{
		PhysicsState().capacity += 16;
		++PhysicsState().growths;
	}
}

/// A resting body for an object a moving body may hit.
void DynamicsSystem::AddProxy(entt::entity entity)
{
	MakeSureEndSlotIsFree();
	auto po = std::make_unique<PhysicsEntry>();
	po->entity = entity;
	po->villager = _world->Entities().AllOf<Villager>(entity);
	po->kind = po->villager ? 1 : 0;
	const bool dynamic = Dynamics().CanBecomeAPhysicsObject(entity);
	if (!SetUpBodyOf(entity, po->body, dynamic))
	{
		return;
	}
	po->body.resting = true;
	po->flags = PhysicsEntry::k_Awake;
	// SetUpPos: the turn-start matrix is the body's own (a proxy knocked this turn moves from where it rests)
	Dynamics().SyncTurnStart(*po);
	po->turnStarted = true;
	if (_world->Entities().AnyOf<Abode, StoragePit>(entity))
	{
		// a building's body setup clears the damage mesh's last hitter
		_world->BuildingDamage().ForgetHitter(entt::null, entity);
		po->flags |= PhysicsEntry::k_NoObjectCollision; // buildings do not check their vertices against objects
	}
	else if (_world->Entities().AllOf<CitadelHeart>(entity))
	{
		po->flags |= PhysicsEntry::k_NoObjectCollision; // nor does a temple's heart
	}
	PhysicsState().objects.push_back(std::move(po));
}

void DynamicsSystem::BeginTurn()
{
	auto& registry = _world->Entities();
	for (size_t i = 0; i < PhysicsState().objects.size();)
	{
		auto& po = *PhysicsState().objects[i];
		// an object no longer available: its body is released and the entry goes (the last one moves into its slot)
		if (!ecs::IsAvailable(po.entity))
		{
			RemoveAt(i);
			continue;
		}
		if (LifeOf(po.entity) < 0.01f)
		{
			po.body.density += 0.01f; // dead: sink factor +0.01 a turn (corpses sink)
		}
		// the summed force and the body last touched; the hitter stays as it was
		hitter::ClearTurnContacts(po);
		// the end matrix becomes the turn-start one
		Dynamics().SyncTurnStart(po);
		po.turnStarted = true;
		++i;
	}
	// after the loop over the list: a prediction whose object is no longer available ends; else the original copies
	// its matrix and clears a counter, not ported: the prediction's body is never stepped again and its drawing uses
	// the history
	if (PhysicsState().prediction.active && !ecs::IsAvailable(PhysicsState().prediction.object))
	{
		EndPrediction();
	}
	// awake flags: moving bodies are awake, resting ones only while something moving is near them
	for (auto& po : PhysicsState().objects)
	{
		if (!openblack::physics::turn::AwakeAtTurnStart(po->body.resting))
		{
			po->flags &= ~PhysicsEntry::k_Awake;
		}
		else
		{
			po->flags |= PhysicsEntry::k_Awake;
		}
	}
	// each moving body wakes what is in the map cells of its box C -/+ (|v.xz| x 0.1 + R) (BoxCell), x outer and z
	// inner; a cell is walked once for all the bodies (the list of walked cells keeps at most 512). The fixed list, then
	// the mobile one (map_cells::ForEachInCell): what InteractsWithPhysicsObjects is woken when it is in the physics
	// already, else with a 3D object it gets a resting proxy (AddProxy) at once. The walk stops when the list's
	// allocated slots are used up (count >= capacity: before each x column, each z cell and each object): AddProxy does
	// not grow it here, so the condition stays true for the rest of the walk and for every later body of the turn
	constexpr size_t k_WalkedCells = 0x200;
	std::vector<glm::ivec2> walked;
	const auto full = [this]() { return PhysicsState().objects.size() >= PhysicsState().capacity; };
	const size_t bodies = PhysicsState().objects.size(); // the proxies added below rest: they have no box
	for (size_t b = 0; b < bodies && !full(); ++b)
	{
		if (PhysicsState().objects[b]->body.resting)
		{
			continue;
		}
		const auto centre = PhysicsState().objects[b]->body.Centre();
		const auto v = PhysicsState().objects[b]->body.velocity;
		const float half = glm::length(glm::vec2(v.x, v.z)) * 0.1f + PhysicsState().objects[b]->body.Radius();
		const auto first = BoxCell(centre.x - half, centre.z - half);
		const auto second = BoxCell(centre.x + half, centre.z + half);
		const auto low = glm::min(first, second);
		const auto high = glm::max(first, second);
		for (int32_t x = low.x; x <= high.x && !full(); ++x)
		{
			for (int32_t z = low.y; z <= high.y && !full(); ++z)
			{
				// in the map, then the walked list
				const glm::ivec2 cell(x, z);
				if (!map_coords::InBounds(cell) || std::find(walked.begin(), walked.end(), cell) != walked.end())
				{
					continue;
				}
				if (walked.size() < k_WalkedCells)
				{
					walked.push_back(cell);
				}
				map_cells::ForEachInCell(cell, [this, &registry, &full](entt::entity entity) {
					if (full())
					{
						return false;
					}
					if (!Dynamics().InteractsWithPhysicsObjects(entity))
					{
						return true;
					}
					if (auto* po = Dynamics().Find(entity))
					{
						po->flags |= PhysicsEntry::k_Awake;
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
	// the physical shields always stay in the system at their collision scale, whether or not something moves near
	// them (Magic/Objects/MapShield)
	for (const auto shield : magic::map_shield::Shields())
	{
		if (!registry.Valid(shield) || !magic::map_shield::InteractsWithPhysicsObjects(shield))
		{
			continue;
		}
		if (auto* po = Dynamics().Find(shield))
		{
			po->flags |= PhysicsEntry::k_Awake;
		}
		else
		{
			AddProxy(shield);
		}
	}
	for (size_t i = 0; i < PhysicsState().objects.size();)
	{
		if ((PhysicsState().objects[i]->flags & PhysicsEntry::k_Awake) == 0)
		{
			RemoveAt(i);
			continue;
		}
		++i;
	}
}

void DynamicsSystem::Substep()
{
	auto& registry = _world->Entities();
	for (auto& po : PhysicsState().objects)
	{
		po->body.ClearForces();
		po->body.TouchGround(ecs::GameGround());
	}
	// what decides whether an entry's points are tested against another's faces (a villager's body does not hit what a
	// Living pushed: a push, a kicked ball, a felled tree)
	const auto member = [](const PhysicsEntry& po) {
		return openblack::physics::PairMember {
		    .centre = po.body.Centre(),
		    .radius = po.body.Radius(),
		    .resting = po.body.resting,
		    .justSetUp = po.body.justSetUp,
		    .noObjectCollision = (po.flags & PhysicsEntry::k_NoObjectCollision) != 0,
		    .isVillager = po.villager,
		    .pushedByLiving = (po.flags & PhysicsEntry::k_PushedByLiving) != 0,
		    .object = po.entity,
		    .thrower = po.thrower,
		};
	};
	for (auto& a : PhysicsState().objects)
	{
		for (auto& b : PhysicsState().objects)
		{
			if (a == b)
			{
				continue;
			}
			if (openblack::physics::TestsPoints(member(*a), member(*b)))
			{
				a->body.TouchFaces(b->body);
			}
		}
	}
	for (auto& po : PhysicsState().objects)
	{
		po->body.ApplyContacts();
	}
	// the entry at i is still ours unless something took it out: during the turn only AddObject does, when it replaces a
	// resting body with a moving one
	const auto stillAt = [this](size_t i, const PhysicsEntry* self) {
		return i < PhysicsState().objects.size() && PhysicsState().objects[i].get() == self;
	};
	for (size_t i = 0; i < PhysicsState().objects.size();)
	{
		auto& po = *PhysicsState().objects[i];
		auto* self = &po;
		const float vyBefore = po.body.velocity.y;
		auto result = po.body.Integrate();
		if (!po.body.resting && po.body.Centre().y < po.body.Radius() * 0.5f)
		{
			// below half its radius: HasSunk (ECS/VillagerDrowning) stops the body and ends its physics as if at rest
			ecs::RememberLastPlayerToInteract(po.entity, po.byPlayer);
			// (openblack) an object already gone is not asked: the original's is only marked for deletion
			if (po.body.density > 1.0f && registry.Valid(po.entity) && Dynamics().ClassHooks().HasSunk(Dynamics(), po))
			{
				// a sunk animal is deleted (ToBeDeleted), but its entry stays in the list until the next turn start
				if (!stillAt(i, self))
				{
					continue;
				}
				po.body.velocity = glm::vec3(0.0f);
				po.body.angularMomentum = glm::vec3(0.0f);
				result = openblack::physics::StepResult::Stopped;
			}
			if (vyBefore * po.body.velocity.y < 0.0f)
			{
				AddRipple(po);
			}
		}
		if (po.body.touched)
		{
			po.forceSum += po.body.Force();
		}
		// the fly-by whoosh: a body entering the 10 m sphere round the camera at more than 20 m/s (G_ROCKPAST_01..05)
		if (const auto camera = _world->CameraOrigin())
		{
			const auto d = po.body.Centre() - *camera;
			const float d2 = glm::dot(d, d);
			if (openblack::physics::turn::PassesCamera(d2, po.cameraDistance2, po.body.velocity))
			{
				// a 2D in-game sound, one of G_RockPast_01..05 picked by the tick count
				audio::PlaySoundEffect(audio::Owner::None(), 69 + static_cast<int>(audio::TickCount() % 5), 2, 0, false, false,
				                       audio::SfxBank::InGame);
			}
			po.cameraDistance2 = d2;
		}
		switch (result)
		{
		case openblack::physics::StepResult::Stopped:
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: entity {} at rest at ({:.2f}, {:.2f}, {:.2f})",
			                   static_cast<uint32_t>(po.entity), po.body.Centre().x, po.body.Centre().y, po.body.Centre().z);
			// Whatever happens to its object, the entry stays in the list as a resting body until the next turn start. An
			// object deleted by its end of physics (outside the map) keeps the entry, which goes then. (openblack) An
			// object already gone has no end of physics; the original's is only marked for deletion and still has one
			const auto stopped = po.entity;
			if (registry.Valid(po.entity))
			{
				ForgetAsBuildingHitter(po.entity, BreaksBuildings(po.entity));
				const auto kept = EndPhysics(po, true);
				// an object come to rest: the draw list is rebuilt on the next drawn frame
				ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
				if (!stillAt(i, self))
				{
					continue;
				}
				if (kept != entt::null && registry.Valid(kept))
				{
					// ProcessTurn's stop only (RemoveObjectWithEndPhysics does not do it again): the object EndPhysics
					// returned takes the body's angles. A DeadTree a tree's end of physics returned is a MobileStatic: it keeps
					// the body's x, y, z. (pending) the same call on any other returned object (a Living's Transform has the
					// drawn quarter turn; the others equal their rows to rounding)
					if (registry.AllOf<DeadTree, Transform>(kept))
					{
						registry.Get<Transform>(kept).rotation = Dynamics().RotationFromRows(kept, po.body.Axes());
					}
					po.entity = kept;
				}
			}
			po.body.resting = true;
			// out of the physics: the stopped object, and the one that stays as the resting body if it is another
			SyncInPhysics(stopped);
			if (po.entity != stopped)
			{
				SyncInPhysics(po.entity);
			}
			break;
		}
		case openblack::physics::StepResult::Knocked:
		{
			// a knocked resting proxy: the class's InitialisePhysics (a Villager / Animal flies, FLYING and the hit clip),
			// woken only when it starts; without a class hook, the object's own (refused for what a particle system carries)
			const bool started = registry.Valid(po.entity) && Dynamics().ClassHooks().InitialisePhysicsKnocked(Dynamics(), po);
			if (started)
			{
				po.body.resting = false;
				SyncInPhysics(po.entity);
				po.flags |= PhysicsEntry::k_Awake;
				// a knocked object flies again: the draw list is rebuilt on the next drawn frame
				ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
				// out of the map cells while it flies
				if (map_cells::IsObjectInMap(po.entity))
				{
					map_cells::RemoveMapObject(po.entity);
				}
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: knocked entity {}", static_cast<uint32_t>(po.entity));
			}
			break;
		}
		case openblack::physics::StepResult::Delete:
		{
			// fallen below -4 R: only an object still available is deleted. Its body stays in the list, stepped and hit,
			// until the next turn start
			const auto entity = po.entity;
			if (!ecs::IsAvailable(entity))
			{
				break;
			}
			if (registry.AllOf<Fragment>(entity))
			{
				_world->BuildingDamage().DestroyFragment(entity);
			}
			else
			{
				ecs::ToBeDeleted(entity); // the class's deletion
			}
			if (!stillAt(i, self))
			{
				continue;
			}
			break;
		}
		default:
			break;
		}
		++i;
	}
}

/// The end of a turn's work on these bodies, which physics::turn::EndOfTurn runs in the original's order
class DynamicsSystem::EndTurnSteps final: public openblack::physics::turn::EndOfTurnSteps
{
public:
	/// growths: the list's growths when the walk starts
	EndTurnSteps(DynamicsSystem& system, uint32_t growths)
	    : _system(system)
	    , _growthsAtStart(growths)
	{
	}

	[[nodiscard]] size_t Count() const override { return _system.PhysicsState().objects.size(); }

	void ClearPushes(size_t i) override
	{
		auto& po = Entry(i);
		po.body.externalForce = glm::vec3(0.0f);
		po.body.externalTorque = glm::vec3(0.0f);
	}

	void ToppleFelledTree(size_t i) override
	{
		auto& po = Entry(i);
		// a felled tree (kind 2) that has toppled (its row 1's y < 0.98) sounds once when taller than 10: a 3D in-game
		// sound tag at its MapCoords, a random sample of 31; then kind 3
		if (po.kind == 2 && po.body.Axes()[1].y < 0.98f && Registry().Valid(po.entity))
		{
			if (ecs::object::GetHeight(po.entity) > 10.0f)
			{
				audio::tags::CreateAtMapCoords(ecs::object::MapCoordsOf(po.entity), audio::tags::RandomSample(31, 1), false, 3,
				                               0, false, true, audio::SfxBank::InGame, 0);
			}
			po.kind = 3;
		}
	}

	void FollowObject(size_t i) override
	{
		auto& po = Entry(i);
		// a resting proxy follows its object: villagers and animals walk while their proxy waits to be hit
		if (po.body.resting && Registry().Valid(po.entity))
		{
			po.body.FollowObject(map_coords::ToWorld(object::MapCoordsOf(po.entity)));
		}
	}

	bool TakeImpact(size_t i) override
	{
		auto& po = Entry(i);
		const bool counts = hitter::TakeImpact(po);
		static const bool k_PhysicsTrace = debug_env::PhysicsTrace(); // read once: it is asked for each body each turn
		if (debug_env::TraceLogs(k_PhysicsTrace, "game") && !po.body.resting)
		{
			const auto c = po.body.Centre();
			SPDLOG_LOGGER_INFO(
			    spdlog::get("game"),
			    "Physics trace: entity {} at ({:.2f}, {:.2f}, {:.2f}) v {:.2f} contacts {} G {:.2f} density {:.4f} R {:.2f}",
			    static_cast<uint32_t>(po.entity), c.x, c.y, c.z, glm::length(po.body.velocity), po.body.Contacts(),
			    openblack::physics::turn::GLoad(counts ? po.impact : 0.0f, po.body.Mass()), po.body.density, po.body.Radius());
		}
		return counts;
	}

	void FindHitter(size_t i) override
	{
		hitter::FindHitter(_system.PhysicsState().objects, i, _system.PhysicsState().growths != _growthsAtStart);
	}

	void CollisionSound(size_t i) override
	{
		auto& po = Entry(i);
		// the collision sample, the ground dust or the splash
		const bool hit = hitter::HitterEntry(_system.PhysicsState().objects, po) != nullptr;
		if (openblack::physics::turn::WantsCollisionSound(po.body.resting, hit, po.impact, po.body.Mass()))
		{
			_system.AttemptCollisionSound(po);
		}
	}

	void React(size_t i) override
	{
		auto& po = Entry(i);
		const auto entity = po.entity;
		// What the steps after the reaction read, taken before it, as the reaction changes none of them. Only AddObject
		// takes an entry out during a turn, when a reaction puts an object resting in the list into flight, and that may
		// be this one
		const auto* hitBy = hitter::HitterEntry(_system.PhysicsState().objects, po);
		_reacted = Reacted {
		    .entity = entity,
		    .hitter = hitBy != nullptr ? hitBy->entity : entt::entity {entt::null},
		    .hit = hitBy != nullptr,
		    .fromHand = (po.flags & PhysicsEntry::k_FromHand) != 0,
		    .byPlayer = po.byPlayer,
		};
		auto& registry = Registry();
		// an object its reaction deletes (a rock split, a villager killed) keeps its entry until the next turn start; the
		// entry is not read after the reaction
		if (registry.Valid(entity))
		{
			_system.ReactToPhysicsImpact(po);
		}
	}

	void ConsiderDamageByThrowing(bool atWhatItHit) override
	{
		const auto& registry = std::as_const(Registry());
		const auto entity = _reacted.entity;
		// thrown from the hand, with a player's credit; (openblack) the object may be gone already, where the original's
		// is only marked for deletion
		if (!_reacted.fromHand || !_reacted.byPlayer || !registry.Valid(entity))
		{
			return;
		}
		const bool living = registry.AnyOf<Villager, Animal, Creature>(entity);
		const bool rock = !living && object_physics::IsRock(entity);
		if (!openblack::physics::turn::IsDamageByThrowing(_reacted.fromHand, _reacted.byPlayer, living, rock,
		                                                  registry.AllOf<Tree>(entity)))
		{
			return;
		}
		// the player of the hand that threw it, inherited by what it hit (openblack's byPlayer, the local player's hand)
		ecs::creature_mimic::Consider(
		    PlayerNames::PLAYER_ONE,
		    atWhatItHit ? creature_watching::Deed::DamageByThrowingAt : creature_watching::Deed::DamageByThrowing, entity);
	}

	[[nodiscard]] bool WasHit() const override { return _reacted.hit; }

	void RecordHit() override { _system.RecordHit(_reacted.entity, _reacted.hitter); }

	void AgeSoundPairs() override
	{
		// the pair list ages one turn (pairs stay listed for the turn they were added and the next)
		_system.PhysicsState().soundPairs.EndTurn();
	}

private:
	/// The body just reacted, and what hit it
	struct Reacted
	{
		entt::entity entity {entt::null};
		entt::entity hitter {entt::null};
		bool hit {false};
		bool fromHand {false};
		bool byPlayer {false};
	};

	[[nodiscard]] PhysicsEntry& Entry(size_t i) { return *_system.PhysicsState().objects.at(i); }
	[[nodiscard]] ecs::Registry& Registry() { return _system._world->Entities(); }

	DynamicsSystem& _system;
	const uint32_t _growthsAtStart;
	Reacted _reacted;
};

void DynamicsSystem::EndTurn()
{
	// nothing entered or left the list during the steps: the bodies last touched are taken as places in it
	hitter::CaptureLastHits(PhysicsState().objects);
	EndTurnSteps steps(*this, PhysicsState().growths);
	openblack::physics::turn::EndOfTurn(steps);
}

void DynamicsSystem::RecordHit(entt::entity hit, entt::entity hitter)
{
	// each is kept only when it is still available (a valid object not marked for deletion) as it is written
	const auto& registry = std::as_const(_world->Entities());
	const auto available = [&registry](entt::entity entity) {
		return registry.Valid(entity) && !registry.AllOf<Unavailable>(entity);
	};
	_hitObject = available(hit) ? hit : entt::entity {entt::null};
	_objectWhichHit = available(hitter) ? hitter : entt::entity {entt::null};
}

entt::entity DynamicsSystem::GetHitObject() const
{
	// read back as kept: the original does not test it again
	return _hitObject;
}

entt::entity DynamicsSystem::GetObjectWhichHit() const
{
	return _objectWhichHit;
}

void DynamicsSystem::LoadConstants()
{
	PhysicsState().constants = k_DefaultConstants;
	// read once into the resources, over the built-in table
	auto& materials = _world->Resources().GetPhysicsMaterials();
	if (!materials.Contains(openblack::physics::k_MaterialsId.value()))
	{
		auto& fileSystem = _world->FileSystem();
		std::vector<uint8_t> bytes;
		try
		{
			bytes =
			    resources::LoadBlob(_world->Resources().GetBlobs(),
			                        fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "PhysicsConstants.txt"));
		}
		catch (const std::exception&)
		{
			// without the file every material is zero, as in the game
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Physics: Data/PhysicsConstants.txt not found, every material is zero");
			PhysicsState().constants = openblack::physics::k_NoMaterials;
			return;
		}
		materials.Load(openblack::physics::k_MaterialsId.value(), resources::PhysicsMaterialsLoader::FromBufferTag {}, bytes,
		               k_DefaultConstants);
	}
	PhysicsState().constants = *materials.Handle(openblack::physics::k_MaterialsId.value());
}

const openblack::physics::PhysicsData& DynamicsSystem::Constants(int type)
{
	return PhysicsState().constants.at(static_cast<size_t>(std::clamp(type, 0, static_cast<int>(k_NumConstants) - 1)));
}

int DynamicsSystem::ConstantsType(entt::entity entity)
{
	const auto& registry = _world->Entities();
	if (registry.AllOf<MagicShield>(entity))
	{
		return magic::map_shield::k_PhysicsConstantsType;
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		return 9; // a one-shot orb thrown from the hand
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return 0;
	}
	if (registry.AllOf<CitadelHeart>(entity))
	{
		return 0; // a temple's heart
	}
	if (registry.AllOf<Creature>(entity))
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
	if (registry.AllOf<Scaffold>(entity))
	{
		return 17;
	}
	if (const auto* object = registry.TryGet<const MobileObject>(entity))
	{
		// Champi, MagicMushroom, Toadstool have their own rows
		const auto index = static_cast<int>(object->type);
		if (index >= 17 && index <= 19)
		{
			return 21 + index - 17;
		}
		return 1;
	}
	if (const auto* statics = registry.TryGet<const MobileStatic>(entity))
	{
		const auto index = static_cast<int>(statics->type);
		if ((index >= 49 && index <= 52) || index == 14 || index == 15 || index == 5 || object_physics::IsRock(entity))
		{
			return 3;
		}
		// a fence: row 18, before the toys
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
		return 1; // a mobile static can always become a physics object
	}
	// otherwise 1 when it can become a physics object, else 0
	return CanBecomeAPhysicsObject(entity) ? 1 : 0;
}

bool DynamicsSystem::PhysicallyDestroysAbodes(entt::entity entity)
{
	const auto type = ConstantsType(entity);
	return !_world->Entities().AllOf<Fragment>(entity) && (type == 3 || type == 20);
}

void DynamicsSystem::AddPuff(glm::vec3 at, glm::vec3 velocity, float size, uint32_t argb)
{
	AddPuff(at, velocity, size, argb, openblack::physics::turn::PuffKind::Dust);
}

void DynamicsSystem::AddPuff(glm::vec3 at, glm::vec3 velocity, float size, uint32_t argb,
                             openblack::physics::turn::PuffKind kind)
{
	if (PhysicsState().dustPuffs.size() >= k_MaxDustPuffs)
	{
		return;
	}
	// every kind of puff takes CRT rand() % 16 (signed; rand is never negative), after the 1024 particle limit test
	const auto seed = static_cast<uint32_t>(game_random::crt::Rand() % 16);
	const auto texture = DustTexture();
	if (!texture)
	{
		return;
	}
	const glm::vec4 colour = argb_colour::ToVec4(argb);
	const float a = colour.a;
	const glm::vec3 rgb(colour);
	// the tint premultiplied by its alpha, blended over or added; size 0 until the first Update
	const auto cell = static_cast<uint32_t>(openblack::physics::turn::DustPuffFrame(kind, seed, 0.0f));
	const DustParticleDraw draw {*texture,
	                             at,
	                             0.0f,
	                             DustCellUv(cell),
	                             glm::vec2(1.0f / 8.0f),
	                             glm::vec4(rgb * a, a),
	                             openblack::physics::turn::IsAdditive(kind)};
	PhysicsState().dustPuffs.push_back({velocity, size, 0.0f, seed, draw, kind});
}

glm::vec3 DynamicsSystem::SyncedPuffVelocity()
{
	// z, y, x in that order
	const float z = DustAxis(game_random::GameRand(201));
	const float y = DustAxis(game_random::GameRand(201));
	const float x = DustAxis(game_random::GameRand(201));
	return {x, y, z};
}

void DynamicsSystem::ClearDust()
{
	PhysicsState().dustPuffs.clear();
}

void DynamicsSystem::SnapshotDust(std::vector<DustParticleDraw>& out)
{
	out.clear();
	out.reserve(PhysicsState().dustPuffs.size());
	for (const auto& puff : PhysicsState().dustPuffs)
	{
		out.push_back(puff.draw);
	}
}

void DynamicsSystem::PlayAnimEffect(const std::array<int32_t, 5>& key, entt::entity owner, glm::vec3 at, bool track)
{
	// the distance from the camera to the point
	const auto camera = audio::ListenerPoint();
	const float distance = camera ? glm::distance(*camera, at) : 0.0f;
	audio::PlayAnimationEffect(owner != entt::null ? audio::Owner::Thing(owner) : audio::Owner::None(), distance, key,
	                           audio::AnimAction::Play, audio::Bank(audio::SfxBank::Editor), track, 0.0f, 0.0f);
}

bool DynamicsSystem::InteractsWithPhysicsObjects(entt::entity entity)
{
	const auto& registry = _world->Entities();
	if (registry.AllOf<MagicShield>(entity))
	{
		return magic::map_shield::InteractsWithPhysicsObjects(entity); // physical shield: yes, magic shield: no
	}
	if (registry.AnyOf<Tree, Field, BigForest, Fragment>(entity))
	{
		return false; // standing trees: thrown objects go through them; fragments only hit the landscape
	}
	if (registry.AllOf<Creature>(entity))
	{
		return true;
	}
	if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		// piles do not interact
		return pot->type == PotInfo::HandWood || pot->type == PotInfo::HandFood;
	}
	// fields are never hit; buildings only while standing (more than 10% built and life > 0.01)
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
	// a temple's heart once more than a tenth built, whatever its life
	if (const auto* part = registry.TryGet<const CitadelPartBuild>(entity);
	    part != nullptr && registry.AllOf<CitadelHeart>(entity))
	{
		constexpr double k_HeartStands = 0.1;
		return static_cast<double>(part->percentBuilt) > k_HeartStands;
	}
	return registry.AnyOf<MobileStatic, MobileObject, Villager, Animal, DeadTree, Abode, StoragePit>(entity);
}

bool DynamicsSystem::SetUpBody(entt::entity entity, openblack::physics::Body& body, bool dynamic)
{
	// the prediction's body is the held object's: the hand's pose (a Living's Transform as it is)
	return SetUpBodyOf(entity, body, dynamic, true);
}

bool DynamicsSystem::CanWakeKnockedProxy(entt::entity entity)
{
	// the object's InitialisePhysics refuses one in the physics: the proxy's own body rests, so only a carried one is
	return CanBecomeAPhysicsObject(entity) && !_world->Entities().AllOf<InPhysics>(entity);
}

bool DynamicsSystem::CanBecomeAPhysicsObject(entt::entity entity)
{
	const auto& registry = _world->Entities();
	if (registry.AnyOf<Abode, StoragePit, Field, BigForest>(entity))
	{
		return false;
	}
	if (registry.AllOf<Scaffold>(entity))
	{
		// the class's hook (not in a workshop slot, no site), else a mobile object's answer: yes
		return ClassHooks().CanBecomePhysicsObject(*this, entity);
	}
	// a one-shot orb is a MobileObject too
	return registry.AnyOf<MobileStatic, MobileObject, Villager, Animal, Tree, DeadTree, Pot, Fragment, OneOffSpellSeed>(entity);
}

const GObjectInfo* DynamicsSystem::ObjectInfo(entt::entity entity)
{
	if (const auto* info = InfoOf(entity))
	{
		return info;
	}
	const auto& registry = _world->Entities();
	if (_world->Info() == nullptr)
	{
		return nullptr;
	}
	const auto& constants = *_world->Info();
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
	// a creature's is its species' row of the creature tables
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const auto row = openblack::creature::InfoRow(creature->species);
		return row < constants.creature.size() ? &constants.creature.at(row) : nullptr;
	}
	return nullptr;
}

float DynamicsSystem::BodyMassOf(entt::entity entity)
{
	const auto& registry = _world->Entities();
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return 2000.0f; // a building's body
	}
	if (const auto* fragment = registry.TryGet<const Fragment>(entity))
	{
		return std::max(30.0f * fragment->area, 0.01f); // 30 x area, at least 0.01
	}
	if (const auto classMass = ClassHooks().BodyMassOf(*this, entity); classMass.has_value())
	{
		return *classMass; // the class's own body mass (a creature's), unclamped
	}
	return WeightOf(entity);
}

float DynamicsSystem::WeightOf(entt::entity entity)
{
	const auto& registry = _world->Entities();
	if (const auto classWeight = ClassHooks().ObjectWeightOf(*this, entity); classWeight.has_value())
	{
		return std::max(*classWeight, 0.01f); // the class's own weight (a building's by its info, a creature's)
	}
	// scale^3 x info weight; a fragment's info is the rock it was made as (the third mobile static)
	float weight = 1.0f;
	if (registry.AllOf<Fragment>(entity) && _world->Info() != nullptr)
	{
		weight = _world->Info()->mobileStatic.at(2).weight;
	}
	else if (const auto* info = InfoOf(entity))
	{
		weight = info->weight;
	}
	else if (const auto* animal = registry.TryGet<const Animal>(entity); animal != nullptr && _world->Info() != nullptr)
	{
		weight = _world->Info()->animal.at(static_cast<size_t>(animal->type)).weight;
	}
	else if (const auto* villager = registry.TryGet<const Villager>(entity); villager != nullptr && _world->Info() != nullptr)
	{
		weight = 82.5f; // info.dat villagers
		for (const auto& info : _world->Info()->villager)
		{
			if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
			{
				weight = info.weight;
				break;
			}
		}
	}
	float scale = registry.AllOf<Transform>(entity) ? registry.Get<const Transform>(entity).scale.x : 1.0f;
	if (registry.AllOf<MagicShield>(entity))
	{
		scale = magic::map_shield::CollisionScale(entity); // the collision scale, not the drawn one
	}
	return std::max(scale * scale * scale * weight, 0.01f);
}

PhysicsEntry* DynamicsSystem::AddObject(entt::entity entity, glm::vec3 velocity, BodySpin spin, entt::entity thrower,
                                        bool fromHand)
{
	// (order) AddObject below already runs the class's StartPhysics and CheckAllCreaturesForCatching, and L is set
	// after them; the original sets L before the creature check. The same while those hooks do not read L (none does)
	auto* po = AddObject(entity, velocity, glm::vec3(0.0f), thrower, fromHand);
	if (po != nullptr)
	{
		po->body.SetAngularVelocityFromBody(spin.w);
	}
	return po;
}

PhysicsEntry* DynamicsSystem::AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
                                        entt::entity thrower, bool fromHand)
{
	return Add(entity, velocity, angularVelocity, thrower, fromHand, fromHand, true);
}

PhysicsEntry* DynamicsSystem::InitialisePhysics(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity)
{
	// the class's StartPhysics comes after the landing test: from_hand calls InitialisePhysicsOfClass
	return Add(entity, velocity, angularVelocity, entt::null, true, false, false);
}

void DynamicsSystem::ThrowFromHandOfClass(entt::entity entity, bool dontReplant)
{
	ClassHooks().ThrowFromHand(*this, entity, dontReplant);
}

PhysicsClassHooks& DynamicsSystem::ClassHooks()
{
	return *PhysicsState().classHooks;
}

void DynamicsSystem::InitialisePhysicsOfClass(PhysicsEntry& po, bool fromHand)
{
	ClassHooks().StartPhysics(*this, po, fromHand);
}

void DynamicsSystem::SyncTurnStart(PhysicsEntry& po)
{
	// the three rows and the translation, as SetUpPos, SettleOnLand and the turn start copy them
	po.turnStartRotation = po.body.Axes();
	po.turnStartCentre = po.body.Centre();
}

PhysicsEntry* DynamicsSystem::AddDroppedObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
                                               std::optional<glm::vec3> angularMomentum)
{
	auto* po = AddObject(entity, velocity, angularVelocity);
	if (po == nullptr)
	{
		return nullptr;
	}
	return FinishDroppedObject(*po, angularMomentum);
}

PhysicsEntry* DynamicsSystem::AddDroppedObject(entt::entity entity, glm::vec3 velocity, BodySpin spin,
                                               std::optional<glm::vec3> angularMomentum)
{
	auto* po = AddObject(entity, velocity, spin);
	if (po == nullptr)
	{
		return nullptr;
	}
	return FinishDroppedObject(*po, angularMomentum);
}

PhysicsEntry* DynamicsSystem::FinishDroppedObject(PhysicsEntry& body, std::optional<glm::vec3> angularMomentum)
{
	auto* po = &body;
	if (angularMomentum)
	{
		po->body.angularMomentum = *angularMomentum;
	}
	po->flags |= PhysicsEntry::k_NoObjectCollision;
	po->body.SettleOnLand(ecs::GameGround(), false, true);
	SyncTurnStart(*po); // the copy at the end of SettleOnLand
	RaiseClearOfWhatIsUnder(*po);
	return po;
}

PhysicsEntry* DynamicsSystem::Add(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity, entt::entity thrower,
                                  bool fromHand, bool spreadReaction, bool initialiseClass)
{
	MakeSureEndSlotIsFree();
	auto& registry = _world->Entities();
	if (!registry.Valid(entity) || !Dynamics().CanBecomeAPhysicsObject(entity))
	{
		return nullptr;
	}
	// an IMMOVABLE object (SET_ID_MOVEABLE) gets no physics
	if (object_flags::IsImmovable(entity))
	{
		return nullptr;
	}
	// already in physics: an object a particle system carries is in physics with no body
	if (particle_carried_objects::IsCarried(entity))
	{
		return nullptr;
	}
	// a resting body of the object makes way for the moving one at once, also during a turn: the original's AddObject
	// takes it out itself, not through RemoveObject
	for (size_t i = 0; i < PhysicsState().objects.size(); ++i)
	{
		if (PhysicsState().objects[i]->entity != entity)
		{
			continue;
		}
		if (!PhysicsState().objects[i]->body.resting)
		{
			return nullptr;
		}
		TakeOutAt(i);
		break;
	}
	auto po = std::make_unique<PhysicsEntry>();
	po->entity = entity;
	po->thrower = thrower;
	po->villager = registry.AllOf<Villager>(entity);
	po->kind = po->villager ? 1 : 0;
	if (!SetUpBodyOf(entity, po->body, true, fromHand))
	{
		return nullptr;
	}
	// SetUpPos: the turn-start matrix is the body's own (drawn there until the next turn starts)
	Dynamics().SyncTurnStart(*po);
	po->body.SetAngularVelocity(angularVelocity);
	const float speed = glm::length(velocity);
	po->body.velocity = speed > openblack::physics::k_MaxSpeed ? velocity * (openblack::physics::k_MaxSpeed / speed) : velocity;
	po->flags = PhysicsEntry::k_Awake | (fromHand ? PhysicsEntry::k_FromHand : 0);
	po->byPlayer = fromHand;
	if (initialiseClass)
	{
		// the start of physics of a villager or an animal (VillagerPhysics). Not for InitialisePhysics: the hand's start
		// calls it after its landing test
		Dynamics().InitialisePhysicsOfClass(*po, fromHand);
	}
	PhysicsState().objects.push_back(std::move(po));
	SyncInPhysics(entity);
	// out of the map cells while it flies
	if (map_cells::IsObjectInMap(entity))
	{
		map_cells::RemoveMapObject(entity);
	}
	// before the fire's StartedMoving (AddObject's path; the hand's start calls it itself, ecs::physics::from_hand)
	if (auto* added = Dynamics().Find(entity); initialiseClass && added != nullptr)
	{
		Dynamics().CheckAllCreaturesForCatching(entity, *added);
	}
	// a burning object leaves its fire group (ECS/Fire)
	fire::StartedMoving(entity, false);
	if (spreadReaction)
	{
		// the flying-object reaction, once (the predators flee from it); the thrower: the hand's player (PLAYER_ONE's
		// interface here)
		ecs::animal_ai::SpreadFlyingObjectReaction(entity, PlayerNames::PLAYER_ONE);
	}
	return PhysicsState().objects.back().get();
}

void DynamicsSystem::TakeOutAt(size_t index)
{
	const auto entity = PhysicsState().objects[index]->entity;
	RemoveAt(index);
	// (inferred, not read) out of the physics without EndPhysics: back in the map cells at once while it exists, not at
	// the next map_cells::Sync. Nothing for a resting proxy (it never left) or one held out (a tornado's). The
	// original's only insert is at the end of physics: never for an UNAVAILABLE (marked) object
	if (ecs::IsAvailable(entity))
	{
		map_cells::InsertMapObject(entity);
	}
}

void DynamicsSystem::RemoveObject(entt::entity entity)
{
	// nothing while a turn's steps and its end run: the entry stays, and goes at the next turn start once its object is
	// no longer available
	if (_inTurnUpdate)
	{
		return;
	}
	for (size_t i = 0; i < PhysicsState().objects.size(); ++i)
	{
		if (PhysicsState().objects[i]->entity == entity)
		{
			const bool breaksBuildings = BreaksBuildings(entity);
			TakeOutAt(i);
			ForgetAsBuildingHitter(entity, breaksBuildings);
			return;
		}
	}
}

void DynamicsSystem::RemoveObjectWithEndPhysics(entt::entity entity)
{
	// the original's RemoveObject with its end of physics: nothing during a turn either
	if (_inTurnUpdate)
	{
		return;
	}
	for (size_t i = 0; i < PhysicsState().objects.size(); ++i)
	{
		if (PhysicsState().objects[i]->entity != entity)
		{
			continue;
		}
		auto* self = PhysicsState().objects[i].get();
		// what the object is, before its end of physics may delete it
		const bool breaksBuildings = BreaksBuildings(entity);
		// the angles, position and altitude from the body, then EndPhysics(po, insert_back_into_map = true)
		const bool landed = (self->flags & PhysicsEntry::k_Landed) != 0;
		const auto kept = EndPhysics(*self, false);
		// LANDED and the returned object on land -> its landing sound; the ProcessTurn stop never plays it
		if (auto& registry = _world->Entities(); landed && registry.Valid(kept))
		{
			ClassHooks().DropSound(*this, kept);
		}
		for (size_t j = 0; j < PhysicsState().objects.size(); ++j)
		{
			if (PhysicsState().objects[j].get() == self)
			{
				RemoveAt(j);
				break;
			}
		}
		ForgetAsBuildingHitter(entity, breaksBuildings);
		return;
	}
}

void DynamicsSystem::RaiseClearOfWhatIsUnder(PhysicsEntry& po)
{
	auto& registry = _world->Entities();
	// the cells of the two corners (C.x - R, C.z - R) and (C.x + R, C.z + R) (BoxCell: clamped into the map when off
	// it), the low and high of the two
	const auto centre = po.body.Centre();
	const float radius = po.body.Radius();
	const auto first = BoxCell(centre.x - radius, centre.z - radius);
	const auto second = BoxCell(centre.x + radius, centre.z + radius);
	const auto low = glm::min(first, second);
	const auto high = glm::max(first, second);
	// x outer, z inner, in the map, the fixed list then the mobile one (map_cells::ForEachInCell; a multi-cell building
	// is in every cell it covers). AddProxy puts each in the physics list at once, so a later cell of the same object
	// skips it ("not in the physics list yet"): here the candidates list does that
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
				// not the thrower && InteractsWithPhysicsObjects (false for a vortex or a map shield) && not in the physics
				// list yet && it has a 3D object
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
		AddProxy(entity);
	}
	// go up by the first push over 0.001 and start again, until nothing pushes. The original has no limit; the 1000
	// rounds only guard openblack against a body that could never leave another.
	for (int round = 0; round < 1000; ++round)
	{
		float best = 0.0f;
		bool raised = false;
		for (const auto& other : PhysicsState().objects)
		{
			if (other.get() == &po)
			{
				continue;
			}
			// a villager is not raised over what a Living pushed, nor that over a villager
			if ((po.villager && (other->flags & PhysicsEntry::k_PushedByLiving) != 0) ||
			    (other->villager && (po.flags & PhysicsEntry::k_PushedByLiving) != 0))
			{
				continue;
			}
			const float reach = other->body.Radius() + po.body.Radius();
			const auto d = po.body.Centre() - other->body.Centre();
			if (!(reach * reach > glm::dot(d, d)))
			{
				continue;
			}
			const float down = po.body.OverlapDepth(other->body, glm::vec3(0.0f, -1.0f, 0.0f));
			const float up = other->body.OverlapDepth(po.body, glm::vec3(0.0f, 1.0f, 0.0f));
			best = std::max(best, std::max(down, up));
			if (best > 0.001f)
			{
				// C.y += best: the object's origin goes up, then Body::SetUpPos
				po.body.SetUpPos(po.body.Axes(), po.body.ObjectOrigin() + glm::vec3(0.0f, best, 0.0f));
				SyncTurnStart(po); // SetUpPos copies the matrix into the turn-start one
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

bool DynamicsSystem::EndPhysicsAsObject(entt::entity entity)
{
	auto& registry = _world->Entities();
	// an UNAVAILABLE object (a zombie of the dead list) is neither put back in the cells nor deleted; for every caller
	// of the end of physics, the villager's and the animal's too
	if (!ecs::IsAvailable(entity))
	{
		return false;
	}
	// with insert and the object available: inside the map it goes back in the cells, outside it is deleted
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr || map_coords::InBounds(map_coords::FromWorld(nullptr, transform->position)))
	{
		map_cells::InsertMapObject(entity);
		return true;
	}
	ToBeDeleted(entity);
	return false;
}

PhysicsEntry* DynamicsSystem::Find(entt::entity entity)
{
	// an entry whose object is gone waits only for the next turn start: it is no object's any more
	if (!_world->Entities().Valid(entity))
	{
		return nullptr;
	}
	for (auto& po : PhysicsState().objects)
	{
		if (po->entity == entity)
		{
			return po.get();
		}
	}
	return nullptr;
}

glm::mat3 DynamicsSystem::DrawQuarterTurn(const glm::mat3& rows)
{
	return ::DrawQuarterTurn(rows);
}

glm::mat3 DynamicsSystem::RotationFromRows(entt::entity entity, const glm::mat3& rows)
{
	const auto& registry = _world->Entities();
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	if (registry.AnyOf<Villager, Animal, Tree, Feature>(entity))
	{
		affine::DecomposeYXZ(rows, y, x, z);
		return affine::AngleY(y); // the yaw only
	}
	if (registry.AnyOf<MobileStatic, MobileObject, Pot, DeadTree, Fragment, Scaffold, OneOffSpellSeed>(entity))
	{
		affine::DecomposeYXZ(rows, y, x, z);
		return affine::RotationYXZ(y, x, z); // all three angles
	}
	return rows;
}

bool DynamicsSystem::IsFlying(entt::entity entity)
{
	const auto* po = Find(entity);
	return po != nullptr && !po->body.resting;
}

float DynamicsSystem::PushObject(entt::entity object, entt::entity pusher)
{
	(void)pusher; // the force does not depend on the pusher
	// the force is weight x 9.81. (not ported) the dead tree's own weight
	const float force = WeightOf(object) * 9.81f;
	PhysicsEntry* po = nullptr;
	if (auto& registry = _world->Entities(); registry.Valid(object) && registry.AllOf<InPhysics>(object))
	{
		// in the physics: its body takes the push. One a particle system carries has none, and the start of physics gives
		// no body to an object already in physics: (inferred) nothing then, so no force
		po = registry.AllOf<CarriedByTornado>(object) ? nullptr : Find(object);
	}
	else
	{
		po = AddObject(object, glm::vec3(0.0f), glm::vec3(0.0f)); // no velocity, no spin
	}
	if (po == nullptr)
	{
		return 0.0f;
	}
	po->flags |= PhysicsEntry::k_PushedByLiving;
	// the object's MapCoords as a point (with its altitude); d = T - it
	const auto at = map_coords::ToWorld(object::MapCoordsOf(object));
	auto d = po->body.Centre() - at;
	// all three exactly 0 adds nothing; else d x F / sqrt((dz dz + dy dy) + dx dx)
	if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
	{
		const float s = force / std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);
		d = glm::vec3(d.x * s, d.y * s, d.z * s);
	}
	po->body.externalForce.x += d.x;
	po->body.externalForce.y += d.y;
	po->body.externalForce.z += d.z;
	return 0.0f;
}

void DynamicsSystem::ProcessTurn()
{
	// the fragments' timers run from Game::GameLogicLoop, not here
	BeginTurn();
	// from the steps to the end of the turn nothing leaves the list (RemoveObject does nothing)
	_inTurnUpdate = true;
	for (int substep = 0; substep < openblack::physics::k_StepsPerTurn; ++substep)
	{
		Substep();
	}
	// the prediction counts its turns while active
	if (PhysicsState().prediction.active)
	{
		++PhysicsState().prediction.turns;
	}
	EndTurn();
	_inTurnUpdate = false;
	// the object's Pos and angles are set every substep (the game logic sees the end of the turn)
	auto& registry = _world->Entities();
	bool moved = false;
	for (const auto& po : PhysicsState().objects)
	{
		if (!po->body.resting && registry.Valid(po->entity))
		{
			moved = true;
			SyncTransform(*po);
			ClassHooks().Moved(*this, po->entity);
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

void DynamicsSystem::UpdateFrame(float turnFraction, float seconds)
{
	UpdateDust(std::min(seconds, 0.25f));
	// every frame for each awake entry, the turn-start matrix is lerped to the end one cell by cell by the turn
	// fraction, each row normalised and scaled, and T - R s com taken as the origin.
	// A body at rest has no entry of its own pose: it is drawn at its Transform (the end of the turn).
	// (pending) an animated object's frame, and the quarter turn of animated meshes other than villagers and animals
	auto& registry = _world->Entities();
	const float f = turnFraction;
	bool moving = false;
	for (const auto& po : PhysicsState().objects)
	{
		// (dead list plan) an UNAVAILABLE (marked) object is not posed: the original's pose pass has no availability test,
		// but a marked object's body goes at the next turn start and the object is out of the cells, so it is not drawn
		if (!ecs::IsAvailable(po->entity))
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
		// posed only when -Radius < T.y (the current centre), so a body sunk deeper than its radius keeps the pose it was
		// last drawn at
		if (openblack::physics::turn::SunkOutOfSight(po->body.Centre().y, po->body.Radius()))
		{
			continue;
		}
		moving = true;
		const auto& r1 = po->body.Axes();
		const auto& r0 = po->turnStarted ? po->turnStartRotation : r1;
		const auto c1 = po->body.Centre();
		const auto c0 = po->turnStarted ? po->turnStartCentre : c1;
		glm::mat3 rotation;
		for (int row = 0; row < 3; ++row)
		{
			rotation[row] = r0[row] + (r1[row] - r0[row]) * f; // in the original's operation order
		}
		affine::NormaliseRows(rotation);
		// an animated (boned) mesh takes the quarter turn before the scale and the origin.
		// (pending) the other boned objects (the reward chest, the dolphin, the shark, the ballista, some gates) keep
		// openblack's Transform convention; only Villager / Animal bodies are in the original's axes
		if (registry.AnyOf<Villager, Animal>(po->entity))
		{
			rotation = DrawQuarterTurn(rotation);
		}
		const auto centre = c0 + (c1 - c0) * f;
		auto& pose = registry.AllOf<PhysicsDrawPose>(po->entity) ? registry.Get<PhysicsDrawPose>(po->entity)
		                                                         : registry.Assign<PhysicsDrawPose>(po->entity);
		pose.rotation = rotation;
		pose.position = po->body.ObjectOrigin(rotation, centre);
	}
	// while the prediction is active and its object has a 3D object, its matrix = history[k] lerped to history[k + 1]
	// by the turn fraction, all 12 floats ((h1 - h0) f + h0) with k = the turns since < N, else history[N]; the rows
	// normalised, then Body::DrawOrigin.
	// (pending) an animated mesh's frame; the draw itself is the renderer's, from PhysicsDrawPose. (pending) the hand
	// skips drawing the held object while it is the prediction's. (openblack guards) Valid, a mesh and a history stand
	// for the 3D object test
	if (PhysicsState().prediction.active && registry.Valid(PhysicsState().prediction.object) &&
	    MeshOf(PhysicsState().prediction.object) != nullptr && !PhysicsState().prediction.history.empty())
	{
		const auto& history = PhysicsState().prediction.history;
		const size_t n = history.size() - 1;
		glm::mat3 rotation;
		glm::vec3 centre;
		if (PhysicsState().prediction.turns < n)
		{
			const auto& [r0, c0] = history[PhysicsState().prediction.turns];
			const auto& [r1, c1] = history[PhysicsState().prediction.turns + 1];
			for (int row = 0; row < 3; ++row)
			{
				rotation[row] = (r1[row] - r0[row]) * f + r0[row];
			}
			centre = (c1 - c0) * f + c0;
		}
		else
		{
			rotation = history[n].first;
			centre = history[n].second;
		}
		affine::NormaliseRows(rotation);
		const auto object = PhysicsState().prediction.object;
		auto& pose = registry.AllOf<PhysicsDrawPose>(object) ? registry.Get<PhysicsDrawPose>(object)
		                                                     : registry.Assign<PhysicsDrawPose>(object);
		pose.rotation = rotation;
		pose.position = PhysicsState().prediction.body.DrawOrigin(rotation, centre);
		moving = true;
	}
	if (moving)
	{
		registry.SetDirty(); // the drawn instances change every frame while something flies
	}
}

void DynamicsSystem::BeginPrediction(entt::entity object)
{
	EndPrediction();
	PhysicsState().prediction.active = true;
	PhysicsState().prediction.object = object;
}

void DynamicsSystem::SetPrediction(entt::entity object, openblack::physics::Body body,
                                   std::vector<std::pair<glm::mat3, glm::vec3>> history)
{
	if (!PhysicsState().prediction.active || PhysicsState().prediction.object != object)
	{
		BeginPrediction(object);
	}
	PhysicsState().prediction.body = std::move(body);
	PhysicsState().prediction.history = std::move(history);
	PhysicsState().prediction.turns = 0;
}

bool DynamicsSystem::IsPredictionObject(entt::entity object)
{
	return PhysicsState().prediction.active && PhysicsState().prediction.object == object;
}

void DynamicsSystem::EndPredictionOf(entt::entity object)
{
	if (PhysicsState().prediction.active && PhysicsState().prediction.object == object)
	{
		EndPrediction();
	}
}

void DynamicsSystem::ResetSimulation()
{
	EndPrediction();
	std::vector<entt::entity> entities;
	entities.reserve(PhysicsState().objects.size());
	for (const auto& po : PhysicsState().objects)
	{
		entities.push_back(po->entity);
	}
	PhysicsState().objects.clear();
	for (const auto entity : entities)
	{
		SyncInPhysics(entity);
	}
	PhysicsState().capacity = 0;
	_inTurnUpdate = false;
	_hitObject = entt::null;
	_objectWhichHit = entt::null;
}

PhysicsClass DynamicsSystem::ClassOf(entt::entity entity)
{
	const auto& registry = _world->Entities();
	if (registry.AllOf<MagicShield>(entity))
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
	if (registry.AllOf<Scaffold>(entity))
	{
		return PhysicsClass::Scaffold;
	}
	if (object_physics::IsRock(entity))
	{
		return PhysicsClass::Rock;
	}
	if (registry.AllOf<Creature>(entity))
	{
		return PhysicsClass::Creature;
	}
	return PhysicsClass::Other;
}

void DynamicsSystem::CheckAllCreaturesForCatching(entt::entity object, PhysicsEntry& po)
{
	Dynamics().ClassHooks().OfferToCatchingCreatures(Dynamics(), object, po);
}

void DynamicsSystem::SetClassHooks(std::unique_ptr<PhysicsClassHooks> hooks)
{
	PhysicsState().classHooks = std::move(hooks);
}

void DynamicsSystem::ForEachEntry(const std::function<void(const PhysicsEntry&)>& func)
{
	const auto& registry = _world->Entities();
	for (const auto& object : PhysicsState().objects)
	{
		// not an entry whose object is gone, which only waits for the next turn start
		if (registry.Valid(object->entity))
		{
			func(*object);
		}
	}
}
