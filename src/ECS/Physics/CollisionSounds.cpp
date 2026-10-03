/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CollisionSounds.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

#include <spdlog/spdlog.h>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Buildings.h"
#include "Dust.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/FishShoals.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/WaterRings.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PhysicsObjects.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
constexpr int k_Ground = 27;   // SOUND_COLLISION_TYPE_GROUND
constexpr int k_Water = 29;    // SOUND_COLLISION_TYPE_WATER
constexpr int k_Fragment = 31; // SOUND_COLLISION_TYPE_FRAGMENT
constexpr int k_Grain = 11;
constexpr int k_Bush = 2;
constexpr int k_HollowWood = 18;
constexpr int k_BrickBuilding = 10;

// the A (hitter) and B (hit) codes of each SOUND_COLLISION_TYPE (0xBFEE34, 0xBFEDB0)
constexpr std::array<int, 33> k_TabA = {21, 24, 20, 20, 20, 20, 20, 19, 19, 19, 19, 20, 25, 22, 22, 22, 22,
                                        22, 30, 25, 35, 31, 24, 23, 24, 25, 25, 21, 21, 21, 33, 34, 42};
constexpr std::array<int, 33> k_TabB = {11, 14, 10, 10, 10, 10, 10, 9, 9, 9, 9, 10, 15, 12, 12, 12, 12,
                                        12, 20, 15, 23, 19, 14, 13, 14, 15, 15, 16, 18, 17, 21, 22, 24};

struct Pair
{
	entt::entity a;
	entt::entity b;
	int turns;
};
std::vector<Pair> g_Pairs; // 0xD47208, at most 128

bool Listed(entt::entity a, entt::entity b)
{
	return std::ranges::any_of(g_Pairs, [&](const Pair& p) { return (p.a == a && p.b == b) || (p.a == b && p.b == a); });
}
} // namespace

int CollisionSounds::TypeOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
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
	if (const auto* info = PhysicsObjects::ObjectInfo(entity))
	{
		return static_cast<int>(info->collideSound);
	}
	return 0;
}

void CollisionSounds::PlayAnimEffect(const std::array<int32_t, 5>& key, entt::entity owner, glm::vec3 at, bool track)
{
	// 0x6468AB..0x646919: GGame::GetCamera, then |LH3DTech::g_camera - point| as the distance (fsqrt 0x64690D)
	const auto camera = audio::ListenerPoint();
	const float distance = camera ? glm::distance(*camera, at) : 0.0f;
	audio::SamplePlayAnimEffect(owner != entt::null ? audio::Owner::Thing(owner) : audio::Owner::None(), distance, key,
	                            audio::AnimAction::Play, audio::Bank(audio::SfxBank::Editor), track, 0.0f, 0.0f);
}

void CollisionSounds::AttemptToAddSoundEvent(const PhysicsObject& po)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto obj = po.entity;
	const auto hitObj = po.hitBy != nullptr ? po.hitBy->entity : entt::entity {entt::null};
	// a rock hitting a building: the building plays its own sound
	if (hitObj != entt::null && registry.AnyOf<Abode, StoragePit>(hitObj) && Buildings::PhysicallyDestroysAbodes(obj))
	{
		return;
	}
	if (Listed(obj, hitObj) || g_Pairs.size() >= 128)
	{
		return;
	}
	const int typeA = TypeOf(obj);
	int typeB = k_Ground;
	const auto at = po.body.Centre();
	if (hitObj != entt::null)
	{
		typeB = TypeOf(hitObj);
	}
	else if (Locator::terrainSystem::has_value())
	{
		// 0x6465B7: not IsDryLand (altitude < 4, the water bit is not read) -> ring; and no cell (off the map, no
		// block) or an altitude under 3 at the cell rounded to the nearest (fistp) -> WATER; altitude 3: ring + dust
		const auto& terrain = Locator::terrainSystem::value();
		const bool land = ecs::sea_cells::IsDryLand(terrain, ecs::sea_cells::CellOf(at));
		const auto* c = land ? nullptr : ecs::sea_cells::CellAt(terrain, ecs::sea_cells::RoundedCellOf(at));
		const bool deep = !land && (c == nullptr || terrain.GetCellAltitude(*c) < 3);
		const float ground = terrain.GetHeightAt(glm::vec2(at.x, at.z));
		const float size = std::min(2.0f * po.body.Radius(), 5.0f);
		const auto dustAt = glm::vec3(at.x, ground, at.z);
		if (deep)
		{
			typeB = k_Water;
			ecs::SplashWater(at); // fn_74F2D0
		}
		if (!land)
		{
			const float radius = std::max(po.body.Radius(), 0.01f);
			ecs::WaterRing ring;
			ring.position = glm::vec3(at.x, 0.1f, at.z);
			ring.growth = 2.0f * radius;
			ring.rate = 1.0f / radius;
			ring.cell = 0x3F;
			ecs::AddWaterRing(ring);
		}
		// 0x646776..0x646854: 6 liquid particles (fn_00845C20, kind 4: 1 s, no gravity) of the foam / dust colour.
		// fn_004ED180 tints that colour first: k = clamp(ftol(SnowCover(point)), 0, 255) (the 128 x 128 grid
		// [0xEDC344], 40 units a cell, fn_0086CA80) and each channel c += floor((base - c) * k / 256) towards the
		// light's base colour [0xFA26A4]. Without snow k = 0 and the colour stays; there is no SnowCover in this tree
		// (weather of the openblack-magic session), so none is applied.
		for (int i = 0; i < 6; ++i)
		{
			Dust::Emit(dustAt, Dust::RandomVelocity(), deep ? 0x28C8F0F4u : 0x50806040u, size);
		}
	}
	if (typeA == k_Fragment || typeB == k_Fragment)
	{
		return;
	}
	g_Pairs.push_back({obj, hitObj, 2});
	// level from the unscaled info weight: 3 soft (g < 1.25), 2, 1 hard (g > 3); GRAIN never at 1
	const auto* info = PhysicsObjects::ObjectInfo(obj);
	const float weight = info != nullptr ? info->weight : 0.0f;
	const float g = weight > 0.0f ? po.impact / (weight * 9.81f) : 1000.0f;
	int level = g < 1.25f ? 3 : g > 3.0f ? 1 : 2;
	if (typeA == k_Grain && level == 1)
	{
		level = 2;
	}
	const int a = k_TabA.at(static_cast<size_t>(std::clamp(typeA, 0, 32)));
	const int b = k_TabB.at(static_cast<size_t>(std::clamp(typeB, 0, 32)));
	// 0x64686C..0x64688A: the key {level, 0, A (0xBFEE34), B (0xBFEDB0), 75 COLLIDE}
	const std::array<int32_t, 5> key = {level, 0, a, b, 75};
	if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Collision sound: types {}/{} level {} -> editor.sad key {{{}, 0, {}, {}, 75}}",
		                   typeA, typeB, level, level, a, b);
	}
	// 0x64689F: the channel follows the object (+0x0C) unless its A code is 0x16
	PlayAnimEffect(key, obj, at, a != 0x16);
}

void CollisionSounds::EndTurn()
{
	for (auto& p : g_Pairs)
	{
		--p.turns;
	}
	std::erase_if(g_Pairs, [](const Pair& p) { return p.turns <= 0; });
}
