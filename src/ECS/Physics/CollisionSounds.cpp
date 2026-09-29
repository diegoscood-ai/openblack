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
#include "Audio/AudioManagerInterface.h"
#include "Buildings.h"
#include "Common/RandomNumberManager.h"
#include "Dust.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/FishShoals.h"
#include "ECS/Registry.h"
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

struct Row
{
	int a;
	int b;
	std::array<std::array<int, 2>, 3> levels; ///< editor.sad samples first..last for level 1..3 (0 = silent)
};
// editor.sad LHAudioAnimArrayTable resolved for the key {level, 0, A, B, 75} (tmp_dis/physics/snd/full_matrix.md)
constexpr std::array<Row, 99> k_Table = {{
    {19, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {19, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {19, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {19, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {19, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {19, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {20, 9, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {20, 11, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 12, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 13, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 14, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 15, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 16, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 17, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 18, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 19, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 20, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 21, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 23, {{{443, 447}, {448, 455}, {0, 0}}}},
    {20, 24, {{{443, 447}, {448, 455}, {0, 0}}}},
    {21, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {21, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {21, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {21, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {21, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {21, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {22, 9, {{{398, 406}, {398, 406}, {0, 0}}}},
    {22, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {22, 12, {{{431, 436}, {431, 436}, {437, 442}}}},
    {22, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {22, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {22, 16, {{{413, 422}, {423, 425}, {426, 430}}}},
    {22, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {22, 18, {{{524, 527}, {528, 531}, {532, 535}}}},
    {22, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {22, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {22, 21, {{{426, 430}, {426, 430}, {0, 0}}}},
    {23, 9, {{{464, 467}, {464, 467}, {0, 0}}}},
    {23, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {23, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {23, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {23, 16, {{{464, 467}, {464, 467}, {0, 0}}}},
    {23, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {23, 18, {{{524, 527}, {464, 467}, {464, 467}}}},
    {23, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {23, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {24, 9, {{{426, 430}, {426, 430}, {0, 0}}}},
    {24, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {24, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {24, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {24, 16, {{{426, 430}, {426, 430}, {426, 430}}}},
    {24, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {24, 18, {{{524, 527}, {528, 531}, {426, 430}}}},
    {24, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {24, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {25, 9, {{{407, 412}, {407, 412}, {0, 0}}}},
    {25, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {25, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {25, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {25, 16, {{{407, 412}, {407, 412}, {0, 0}}}},
    {25, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {25, 18, {{{524, 527}, {407, 412}, {407, 412}}}},
    {25, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {25, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {30, 9, {{{464, 467}, {464, 467}, {0, 0}}}},
    {30, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {30, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {30, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {30, 16, {{{464, 467}, {464, 467}, {0, 0}}}},
    {30, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {30, 18, {{{524, 527}, {528, 531}, {464, 467}}}},
    {30, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {30, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {31, 9, {{{398, 406}, {398, 406}, {0, 0}}}},
    {31, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {31, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {31, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {31, 16, {{{398, 406}, {398, 406}, {0, 0}}}},
    {31, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {31, 18, {{{524, 527}, {528, 531}, {398, 406}}}},
    {31, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {31, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {33, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {33, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {33, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {33, 16, {{{426, 430}, {426, 430}, {426, 430}}}},
    {33, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {33, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {33, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
    {35, 9, {{{426, 430}, {426, 430}, {0, 0}}}},
    {35, 10, {{{456, 463}, {456, 463}, {0, 0}}}},
    {35, 13, {{{0, 0}, {0, 0}, {0, 0}}}},
    {35, 15, {{{407, 412}, {407, 412}, {407, 412}}}},
    {35, 16, {{{426, 430}, {426, 430}, {426, 430}}}},
    {35, 17, {{{389, 392}, {389, 392}, {389, 392}}}},
    {35, 18, {{{524, 527}, {426, 430}, {426, 430}}}},
    {35, 19, {{{398, 406}, {398, 406}, {398, 406}}}},
    {35, 20, {{{464, 467}, {464, 467}, {464, 467}}}},
}};

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

void CollisionSounds::PlayEditorSample(int first, int last, glm::vec3 at)
{
	if (!Locator::audio::has_value() || first <= 0)
	{
		return;
	}
	const int sample = Locator::rng::value().NextValue(first, std::max(first, last));
	const auto id = entt::hashed_string(fmt::format("editor.sad/{}", sample).c_str()).value();
	if (!Locator::resources::value().GetSounds().Contains(id))
	{
		return;
	}
	auto& audio = Locator::audio::value();
	const auto& sound = audio.GetSound(id);
	const auto emitter = audio.CreateEmitter(id, audio::PlayType::Once, at, glm::vec3(0.0f), glm::vec2(0.0f), sound.volume,
	                                         audio::AudioStatus::Playing, false);
	Locator::entitiesRegistry::value().Get<Transform>(emitter).position = at;
	audio.PlayEmitter(emitter);
}

void CollisionSounds::PlaySample2D(const char* bank, int sample)
{
	const auto id = entt::hashed_string(fmt::format("{}/{}", bank, sample).c_str()).value();
	if (Locator::audio::has_value() && Locator::resources::value().GetSounds().Contains(id))
	{
		Locator::audio::value().PlaySound(id, audio::PlayType::Once);
	}
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
		// not dry land and (off the map or a cell altitude under 3): water; shallow cells keep the ground sounds
		const auto& terrain = Locator::terrainSystem::value();
		const int cells = terrain.GetCellsPerSide();
		const glm::ivec2 cell(static_cast<int>(at.x * 0.1f), static_cast<int>(at.z * 0.1f));
		const bool inMap = cell.x >= 0 && cell.y >= 0 && cell.x < cells && cell.y < cells;
		const auto* c = inMap ? &terrain.GetCell(glm::u16vec2(cell)) : nullptr;
		const bool land = c != nullptr && !c->properties.hasWater && !c->properties.fullWater;
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
	for (const auto& row : k_Table)
	{
		if (row.a == a && row.b == b)
		{
			const auto& range = row.levels.at(static_cast<size_t>(level - 1));
			if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Collision sound: types {}/{} level {} -> editor.sad {}..{}", typeA, typeB,
				                   level, range[0], range[1]);
			}
			PlayEditorSample(range[0], range[1], at);
			return;
		}
	}
}

void CollisionSounds::EndTurn()
{
	for (auto& p : g_Pairs)
	{
		--p.turns;
	}
	std::erase_if(g_Pairs, [](const Pair& p) { return p.turns <= 0; });
}
