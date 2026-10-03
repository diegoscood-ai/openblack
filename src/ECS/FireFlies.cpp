/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireFlies.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "GameClock.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_MaxFireFlies = 50;    // game+0x205D34, GGame ctor 0x54B806
constexpr float k_SearchRadius = 300.0f; // fn_0052A5D0 0x52A5D6 / fn_0052A8D0 0x52A8D5
constexpr float k_HalfSize = 0.3f;
constexpr uint32_t k_Frame = 37; // S_SpriteSheet3, 8 x 8: column 5, row 4

enum class State
{
	Asleep,     // 0: invisible at its tree or rock
	Hovering,   // 1: at the house
	FlyingHome, // 2
	FlyingOut,  // 3
};

struct FireFly
{
	entt::entity sprite {entt::null};
	State state {State::Asleep};
	bool resting {true};    // +0xC0 bit 0: sleeping at a tree or rock, can be woken
	glm::vec3 from {0.0f};  // +0x60
	glm::vec3 to {0.0f};    // +0x6C
	glm::vec3 position {0.0f};
	glm::vec3 previous {0.0f}; // +0x78, the position of the last turn (drawing interpolates)
	map_coords::MapCoords coords; // +0x14 at rest: where it was made or where its last flight went
	float progress {0.0f};     // +0x98
	float duration {0.5f};     // +0x9C
	float speedA {1.0f};       // +0xA0
	float speedB {1.0f};       // +0xA4
	float phaseA1 {0.0f};      // +0xA8
	float phaseA2 {0.0f};      // +0xAC
	float phaseB1 {0.0f};      // +0xB4
	float phaseB2 {0.0f};      // +0xB8
	float clock {0.0f};        // accumulated seconds
};

std::vector<FireFly> g_fireFlies; // the list g_game +0x205D0C (head) / +0x205D10 (count)
bool g_canSpawn = true; // [0xBE9DA0], set again every morning

graphics::TextureHandle SheetTexture()
{
	auto& textures = Locator::resources::value().GetTextures();
	const auto id = entt::hashed_string("raw/S_SpriteSheet3a");
	if (!textures.Contains(id))
	{
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			textures.Load(id, resources::Texture2DLoader::FromDiskTag {},
			              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "S_SpriteSheet3a.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Fireflies: cannot load S_SpriteSheet3a.raw: {}", e.what());
			return graphics::TextureHandle {};
		}
	}
	return textures.Handle(id)->GetNativeHandle();
}

/// fn_0052B1A0: IsRock (vt +0x1F0) or IsAnyKindOfTree (vt +0x478)
bool IsRockOrTree(entt::entity object)
{
	return Rocks::IsRock(object) || Locator::entitiesRegistry::value().AnyOf<Tree, DeadTree>(object);
}

/// fn_0052B1D0: IsAbode (vt +0x208) or IsStreetLight (vt +0x200; (inferido) the GStreetLanterns)
bool IsAbodeOrStreetLight(entt::entity object)
{
	return Locator::entitiesRegistry::value().AnyOf<Abode, StreetLantern>(object);
}

/// The firefly's +0x14: at rest (asleep or hovering) the MapCoords its flight went to; (aproximado) in flight the
/// interpolated position (the port keeps it in metres)
map_coords::MapCoords CoordsOf(const FireFly& fly)
{
	if (fly.state == State::Asleep || fly.state == State::Hovering)
	{
		return fly.coords;
	}
	return map_coords::FromWorld(fly.position);
}

/// The game lists the spawn picks from: (inferido) newest first, by the creation index (the lists' order is not read)
std::vector<entt::entity> NewestFirst(std::vector<entt::entity> list)
{
	std::stable_sort(list.begin(), list.end(),
	                 [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

/// GameLists.trees (g_game +0x205CDC head, +0x205CE0 count): the Trees (a DeadTree is a Rock subclass)
std::vector<entt::entity> TreeList()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Tree>(
	    [&list](entt::entity entity, const Tree& /*unused*/) { list.push_back(entity); });
	return NewestFirst(std::move(list));
}

/// GameLists.multi_map_fixed (g_game +0x205CB4 head, +0x205CB8 count): the MultiMapFixed-class objects
std::vector<entt::entity> MultiMapFixedList()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Transform>([&list](entt::entity entity, const Transform& /*unused*/) {
		if (map_cells::IsMultiMapFixedClass(entity))
		{
			list.push_back(entity);
		}
	});
	return NewestFirst(std::move(list));
}

/// fn_0052A670 (fn_0052B1D0's objects) / fn_0052A7A0 (fn_0052B1A0's): GUtils::Spiral over ftol(ceil(2r / 10))^2 cells
/// (0x52A673..0x52A6B4, ceil on the double) from the start's cell. Each InBounds cell is searched only when
/// GameRand(2) != 0 (0x52A6ED / 0x52A81D); in its FindType(ANY) walk (0x52A701, 0x52A75E) an object pred accepts is
/// taken when its GetDistanceInMetres from the start is below the best ("test ah, 1": or unordered) or there is no best
/// yet, and then GameRand(3) == 0 (0x52A747 / 0x52A877) ends that cell. The radius only sizes the spiral: no cut by
/// distance. Null when nothing was taken
entt::entity SpiralSearch(const map_coords::MapCoords& from, float radius, bool (*pred)(entt::entity))
{
	const float twice = radius + radius;
	const float cells = twice / 10.0f;
	const auto side = static_cast<int32_t>(std::ceil(static_cast<double>(cells)));
	int32_t count = side * side;
	map_coords::MapCoords coords = from;
	map_coords::Spiral spiral;
	entt::entity best = entt::null;
	float bestDistance = 0.0f;
	for (; count > 0; --count)
	{
		if (map_coords::InBounds(coords) && game_random::GameRand(2) != 0)
		{
			const auto cell = map_coords::Cell(coords);
			for (auto candidate = map_cells::FindType(cell, ObjectType::Any); candidate != entt::null;
			     candidate = map_cells::FindType(cell, ObjectType::Any, candidate))
			{
				if (!pred(candidate))
				{
					continue;
				}
				const float distance = gutils::GetDistanceInMetres(from, object::MapCoordsOf(candidate));
				if (!(distance >= bestDistance) || best == entt::null)
				{
					bestDistance = distance;
					best = candidate;
					if (game_random::GameRand(3) == 0)
					{
						break;
					}
				}
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	return best;
}

/// fn_0052A630 / fn_0052A917: the fallback, 15 m along x (ftol((x 10 / 65536 + 15) 65536 / 10)) with this altitude
map_coords::MapCoords Aside(map_coords::MapCoords coords, float altitude)
{
	coords.x = map_coords::ToFixedGUtils(map_coords::ToMetres(coords.x) + 15.0f);
	coords.altitude = altitude;
	return coords;
}

/// FireFly::Create 0x52A200 -> the ctor 0x52A280 -> fn_0052A380: eight synced GameFloatRand in this order (FireFly.cpp
/// lines 0x78..0x81): +0xA0 = GFR(0.8) + 0.6, +0xA4 the same, then GFR(2 pi) for +0xA8, +0xAC, +0xB0, +0xB4, +0xB8 and
/// +0xBC (+0xB0 and +0xBC are drawn and never read). The new firefly goes to the HEAD of the list (0x52A2C8..0x52A2DD)
void Create(const map_coords::MapCoords& coords, graphics::TextureHandle texture)
{
	using game_random::GameFloatRand;
	FireFly fly;
	fly.coords = coords;
	fly.position = fly.previous = fly.from = fly.to = map_coords::ToWorld(coords);
	const float speedA = GameFloatRand(0.8f);
	fly.speedA = speedA + 0.6f;
	const float speedB = GameFloatRand(0.8f);
	fly.speedB = speedB + 0.6f;
	fly.phaseA1 = GameFloatRand(glm::two_pi<float>());
	fly.phaseA2 = GameFloatRand(glm::two_pi<float>());
	static_cast<void>(GameFloatRand(glm::two_pi<float>())); // +0xB0
	fly.phaseB1 = GameFloatRand(glm::two_pi<float>());
	fly.phaseB2 = GameFloatRand(glm::two_pi<float>());
	static_cast<void>(GameFloatRand(glm::two_pi<float>())); // +0xBC
	auto& registry = Locator::entitiesRegistry::value();
	fly.sprite = registry.Create();
	registry.Assign<Sprite>(fly.sprite, texture, graphics::frame_anim::SpriteCellUv(static_cast<int>(k_Frame), 8)[0],
	                        glm::vec2(1.0f / 8.0f), glm::vec4(0.0f), true);
	registry.Assign<Transform>(fly.sprite, fly.position, glm::mat3(1.0f), glm::vec3(k_HalfSize));
	g_fireFlies.insert(g_fireFlies.begin(), fly);
}

/// fn_0052B200: (max - count) attempts (0x52B215..0x52B223, the max re-read each time). Each GameRand(2) (0x52B235):
/// nonzero, the GameRand(count) (0x52B25F) tree of GameLists.trees, an empty list ends the spawn (0x52B24E); zero, the
/// GameRand(count) (0x52B2CC) object of GameLists.multi_map_fixed, then on to the first IsRock (0x52B2FF..0x52B312), an
/// empty list ends the spawn (0x52B2BF) and no rock from there makes nothing this attempt. A firefly at its MapCoords
void Spawn()
{
	if (g_fireFlies.size() >= k_MaxFireFlies)
	{
		return;
	}
	const auto trees = TreeList();
	const auto fixed = MultiMapFixedList();
	const auto texture = SheetTexture();
	for (size_t attempt = g_fireFlies.size(); attempt < k_MaxFireFlies; ++attempt)
	{
		entt::entity at = entt::null;
		if (game_random::GameRand(2) != 0)
		{
			if (trees.empty())
			{
				break;
			}
			at = trees[game_random::GameRand(static_cast<uint32_t>(trees.size()))];
		}
		else
		{
			if (fixed.empty())
			{
				break;
			}
			for (size_t i = game_random::GameRand(static_cast<uint32_t>(fixed.size())); i < fixed.size(); ++i)
			{
				if (Rocks::IsRock(fixed[i]))
				{
					at = fixed[i];
					break;
				}
			}
			if (at == entt::null)
			{
				continue;
			}
		}
		Create(object::MapCoordsOf(at), texture);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireflies: {} ({} trees, {} multi map fixed)", g_fireFlies.size(), trees.size(),
	                   fixed.size());
}

/// fn_0052A950 (out) / fn_0052AA10 (home): the flight to that MapCoords
void StartFlight(FireFly& fly, const map_coords::MapCoords& to, State state)
{
	fly.from = fly.position;
	fly.coords = to;
	fly.to = map_coords::ToWorld(to);
	fly.progress = 0.0f;
	fly.state = state;
	fly.duration = std::max(0.5f, glm::distance(fly.from, fly.to) / (3.0f * fly.speedA));
}

/// The list head to the tail (0x52B43C..0x52B4B3 / 0x52B51C..0x52B591)
void HeadToTail()
{
	std::rotate(g_fireFlies.begin(), g_fireFlies.begin() + 1, g_fireFlies.end());
}

/// fn_0052B350: only the list head, and only when it is resting (+0xC0 bit 0). Its tree or rock must still be there: an
/// object of the fixed list of its cell (GetFirstObjectFixed 0x52B388 + GetMapChild) that is fn_0052B1A0 and whose
/// MapCoords equal its own (operator== 0x605660, x and z), else it is deleted (0x52B3FB). (The same walk deletes it
/// when another FireFly there has its MapCoords; a FireFly is type 0x2A, which DoesObjectTypeCountAsFixed puts in the
/// mobile list, so that never happens.) Then it flies to fn_0052A670's abode or lantern, raised by its GetHeight + 2
/// (fn_0052A5D0), none: 15 m along x, altitude 4; it is awake and goes to the tail
void WakeOne()
{
	if (g_fireFlies.empty() || !g_fireFlies.front().resting)
	{
		return;
	}
	auto& fly = g_fireFlies.front();
	const auto at = CoordsOf(fly);
	bool perched = false;
	map_cells::ForEachFixed(map_coords::Cell(at), [&perched, &at](entt::entity candidate) {
		if (IsRockOrTree(candidate))
		{
			const auto coords = object::MapCoordsOf(candidate);
			perched = perched || (coords.x == at.x && coords.z == at.z);
		}
		return true;
	});
	if (!perched)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(fly.sprite))
		{
			registry.Destroy(fly.sprite);
		}
		g_fireFlies.erase(g_fireFlies.begin());
		return;
	}
	map_coords::MapCoords destination;
	if (const auto light = SpiralSearch(at, k_SearchRadius, IsAbodeOrStreetLight); light != entt::null)
	{
		destination = object::MapCoordsOf(light);
		const float height = object::GetHeight(light) + 2.0f; // 0x52A605..0x52A60B
		destination.altitude = height + destination.altitude;
	}
	else
	{
		destination = Aside(at, 4.0f);
	}
	StartFlight(fly, destination, State::FlyingOut);
	fly.resting = false;
	HeadToTail();
}

/// fn_0052B4C0: only the list head, and only when it is awake. It flies to fn_0052A7A0's tree or rock (fn_0052A8D0),
/// none: 15 m along x, altitude 0; it is resting from now on and goes to the tail
void SleepOne()
{
	if (g_fireFlies.empty() || g_fireFlies.front().resting)
	{
		return;
	}
	auto& fly = g_fireFlies.front();
	const auto at = CoordsOf(fly);
	const auto perch = SpiralSearch(at, k_SearchRadius, IsRockOrTree);
	StartFlight(fly, perch != entt::null ? object::MapCoordsOf(perch) : Aside(at, 0.0f), State::FlyingHome);
	fly.resting = true;
	HeadToTail();
}

float Smooth(float p)
{
	return p * p * (3.0f - 2.0f * p);
}
} // namespace

void ecs::ProcessFireFliesTurn(const DayNightClock& clock)
{
	const float visual = clock.GetVisualTime();
	const float skyType = clock.GetSkyType();
	if (visual > 12.0f && skyType > 1.0f)
	{
		if (g_canSpawn)
		{
			Spawn();
		}
		WakeOne();
		g_canSpawn = false;
	}
	else if (visual < 12.0f && skyType < 1.0f)
	{
		SleepOne();
		g_canSpawn = true;
	}

	// FireFly::Process fn_0052AF90 0x52AF93..0x52AFB6: fild [0xD01A38]; fmul [0x8AA3B0] = 0.001, read every turn
	const float turnSeconds = static_cast<float>(game_clock::MsPerTurn()) * game_clock::k_SecondsPerMs;
	for (auto& fly : g_fireFlies)
	{
		fly.previous = fly.position;
		switch (fly.state)
		{
		case State::Asleep:
			fly.progress = 0.0f;
			break;
		case State::Hovering:
			fly.progress = 1.0f;
			break;
		case State::FlyingHome:
		case State::FlyingOut:
			fly.progress = std::min(1.0f, fly.progress + turnSeconds / fly.duration);
			fly.position = glm::mix(fly.from, fly.to, Smooth(fly.progress));
			if (fly.progress >= 1.0f)
			{
				fly.state = fly.state == State::FlyingOut ? State::Hovering : State::Asleep;
			}
			break;
		}
	}
}

void ecs::UpdateFireFlies(float seconds, const glm::vec3& camera)
{
	if (g_fireFlies.empty())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// FireFly::Draw fn_0052ABC0 0x52ADF6: g_game +0x205D64, the fraction of the turn
	const float interpolation = game_clock::TurnFraction();
	for (auto& fly : g_fireFlies)
	{
		if (!registry.Valid(fly.sprite))
		{
			continue;
		}
		auto& sprite = registry.Get<Sprite>(fly.sprite);
		fly.clock += seconds;
		const float t1 = fly.clock * fly.speedA;
		const float t2 = fly.clock * fly.speedB;
		const float a1 = std::fmod(t1 * 0.1f + fly.phaseA1, glm::two_pi<float>());
		const float a2 = std::fmod(t1 * 0.1f + fly.phaseA2, glm::two_pi<float>());
		const float b1 = std::fmod(t2 + fly.phaseB1, glm::two_pi<float>());
		const float b2 = std::fmod(t2 * 1.21f + fly.phaseB2, glm::two_pi<float>());
		float amplitude = 0.0f;
		switch (fly.state)
		{
		case State::Asleep:
			amplitude = 0.0f;
			break;
		case State::Hovering:
			amplitude = 1.0f;
			break;
		case State::FlyingHome:
			amplitude = fly.progress < 0.8f ? 1.0f : 1.0f - (fly.progress - 0.8f) * 5.0f;
			break;
		case State::FlyingOut:
			amplitude = fly.progress < 0.2f ? fly.progress * 5.0f : 1.0f;
			break;
		}
		const glm::vec3 big = amplitude * 8.0f * glm::vec3(std::cos(a2) * std::cos(a1), 0.5f * std::sin(a2), std::cos(a2) * std::sin(a1));
		const glm::vec3 small = amplitude * glm::vec3(std::cos(b2) * std::cos(b1), 0.5f * std::sin(b2), std::cos(b2) * std::sin(b1));
		const glm::vec3 p = fly.previous + (fly.position - fly.previous) * interpolation + big + small;
		registry.Get<Transform>(fly.sprite).position = p;

		// Draw 0x52AA90: nothing asleep or 300 m away; alpha 190, fading out between 100 and 300 m
		const glm::vec3 d = camera - p;
		const float d2 = glm::dot(d, d);
		float alpha = 0.0f;
		if (fly.state != State::Asleep && d2 < 90000.0f)
		{
			alpha = d2 < 10000.0f ? 190.0f : 190.0f * (1.0f - (d2 - 10000.0f) / 80000.0f);
		}
		sprite.tint = glm::vec4(1.0f, 1.0f, 1.0f, alpha / 255.0f);
	}
}

void ecs::ClearFireFlies()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& fly : g_fireFlies)
		{
			if (registry.Valid(fly.sprite))
			{
				registry.Destroy(fly.sprite);
			}
		}
	}
	g_fireFlies.clear();
	g_canSpawn = true;
}

bool ecs::TakeFireFlyAt(const glm::vec3& position)
{
	// MapCoords::operator== 0x605660 on the fixed-point x and z (1/65536 of a 10 m cell)
	constexpr float k_Tolerance = 10.0f / 65536.0f;
	const auto it = std::find_if(g_fireFlies.begin(), g_fireFlies.end(), [&](const FireFly& fly) {
		return std::abs(fly.position.x - position.x) < k_Tolerance && std::abs(fly.position.z - position.z) < k_Tolerance;
	});
	if (it == g_fireFlies.end())
	{
		return false;
	}
	if (Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(it->sprite))
	{
		Locator::entitiesRegistry::value().Destroy(it->sprite);
	}
	g_fireFlies.erase(it);
	return true;
}
