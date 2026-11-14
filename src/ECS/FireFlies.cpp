/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireFlies.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/MapCoords.h"
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/FireFlyObject.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_MaxFireFlies = 50;
constexpr float k_SearchRadius = 300.0f; // for both the lights and the perches
constexpr float k_HalfSize = 0.3f;
constexpr uint32_t k_Frame = 37; // S_SpriteSheet3, 8 x 8: column 5, row 4
constexpr float k_HomeDriftFactor = 5.0000005f;

using fire_flies::State;

/// A firefly, holding what the original's holds: where it is, its home (the tree or rock it rests at) and the light it
/// flies to, all in map coordinates, its flight between them, and its drift. The flight runs between the home and the
/// light whichever way it goes, each turn's place made into map coordinates again, as the original's turn does
struct FireFly
{
	/// Its object in the map (its cell and the object draw list) and its sprite
	entt::entity object {entt::null};
	entt::entity sprite {entt::null};
	State state {State::Asleep};
	/// Where it is: the object's own map coordinates; and where it was made, which its object keeps
	map_coords::MapCoords coords;
	map_coords::MapCoords madeAt;
	/// Its home and the light, and where it was at the last turn of a flight (drawing goes from there to where it is)
	map_coords::MapCoords home;
	map_coords::MapCoords light;
	map_coords::MapCoords last;
	/// The point its last drawing put it at, in metres (none before its first)
	glm::vec3 drawn {0.0f};
	/// The seconds its drawings have counted, and the drift of its last drawing
	float clock {0.0f};
	float drift {0.0f};
	float progress {0.0f};
	float duration {0.5f};
	float speedA {1.0f};
	float speedB {1.0f};
	/// The six phases drawn at its making: the big loop's two, one never read, the small loop's two, one never read
	std::array<float, 6> phases {};
	/// Bit 0: resting at its home, so that it can be woken
	uint8_t flags {1};
	/// Its number of the game's creation counter: a firefly is an object, which takes the next number as it is made
	uint32_t creationIndex {0};

	[[nodiscard]] bool Resting() const { return (flags & 1U) != 0; }
};

/// What this module keeps between calls (Locator::worldEffects)
struct FireFliesState
{
	std::vector<FireFly> fireFlies {}; // the game's firefly list, head first
	bool canSpawn {true};              // set again every morning
};

FireFliesState& FireFliesData()
{
	return openblack::Locator::worldEffects::value().Get<FireFliesState>();
}

/// The firefly's sprite sheet: the game's S_SpriteSheet3 colours (its cell is blue) with the sheet's alpha file, as the
/// game draws it. Without the colours (not loaded), the alpha alone, white
struct SheetTextures
{
	entt::id_type texture {};
	std::optional<entt::id_type> alpha;
};

SheetTextures SheetTexture()
{
	using namespace resources::shared_assets;
	if (LoadSpriteSheet3a("Fireflies") == LoadResult::Failed)
	{
		return {};
	}
	auto& textures = Locator::resources::value().GetTextures();
	const auto alpha = k_SpriteSheet3a.value();
	constexpr auto k_Colours = entt::hashed_string("raw/S_SpriteSheet3");
	if (!textures.Contains(k_Colours.value()))
	{
		return {.texture = alpha, .alpha = std::nullopt};
	}
	return {.texture = k_Colours.value(), .alpha = alpha};
}

/// An abode or a street light ((inferred) the street lanterns)
bool IsAbodeOrStreetLight(entt::entity object)
{
	return Locator::entitiesRegistry::value().AnyOf<Abode, StreetLantern>(object);
}

/// The firefly's map coordinates: where it is, at rest or in flight
map_coords::MapCoords CoordsOf(const FireFly& fly)
{
	return fly.coords;
}

/// The game lists the spawn picks from: (inferred) newest first, by the creation index (the lists' order is not read)
std::vector<entt::entity> NewestFirst(std::vector<entt::entity> list)
{
	std::stable_sort(list.begin(), list.end(),
	                 [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

/// The game's tree list: the Trees (a DeadTree counts as a rock)
std::vector<entt::entity> TreeList()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Tree>(
	    [&list](entt::entity entity, const Tree& /*unused*/) { list.push_back(entity); });
	return NewestFirst(std::move(list));
}

/// The game's multi-map fixed list: the objects of that class
std::vector<entt::entity> MultiMapFixedList()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Transform>([&list](entt::entity entity, const Transform& /*unused*/) {
		if (map_cells::IsMultiCellStaticClass(entity))
		{
			list.push_back(entity);
		}
	});
	return NewestFirst(std::move(list));
}

/// The search for a light or a perch: a spiral over ceil(2r / 10)^2 cells (ceil on the double) from the start's cell.
/// Each InBounds cell is searched only when GameRand(2) != 0; in its FindType(ANY) walk an object pred accepts is taken
/// when its GetDistanceInMetres from the start is below the best (or unordered) or there is no best yet, and then
/// GameRand(3) == 0 ends that cell. The radius only sizes the spiral: no cut by distance. Null when nothing was taken
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
	// one read filter for the whole search: nothing in it takes an object in the hand or into physics
	const map_cells::ReadBatch batch;
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
				const float distance = game_math::GetDistanceInMetres(from, object::MapCoordsOf(candidate));
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

/// The fallback, 15 m along x (truncated back to map coordinates) with this altitude
map_coords::MapCoords Aside(map_coords::MapCoords coords, float altitude)
{
	coords.x = map_coords::MetresToFixedExact(map_coords::ToMetres(coords.x) + 15.0f);
	coords.altitude = altitude;
	return coords;
}

/// The firefly's sprite where it is, as its creation makes it
void MakeSprite(FireFly& fly, const SheetTextures& texture)
{
	auto& registry = Locator::entitiesRegistry::value();
	fly.sprite = registry.Create();
	registry.Assign<Sprite>(fly.sprite, texture.texture, graphics::frame_anim::SpriteCellUv(static_cast<int>(k_Frame), 8)[0],
	                        glm::vec2(1.0f / 8.0f), glm::vec4(0.0f), true, true, texture.alpha);
	registry.Assign<Transform>(fly.sprite, map_coords::ToWorld(fly.coords), glm::mat3(1.0f), glm::vec3(k_HalfSize));
}

/// The firefly's object in its cell, at its map coordinates, with its creation number, as its creation puts it in
void MakeObject(FireFly& fly)
{
	auto& registry = Locator::entitiesRegistry::value();
	fly.object = registry.Create();
	registry.Assign<Transform>(fly.object, map_coords::ToWorld(fly.coords), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ObjectCreationIndex>(fly.object, fly.creationIndex);
	registry.Assign<FireFlyObject>(fly.object);
	map_cells::InsertMapObject(fly.object);
}

/// A firefly gone: its object out of its cell, and its object and sprite destroyed
void DestroyFireFly(const FireFly& fly)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(fly.object))
	{
		map_cells::RemoveMapObject(fly.object);
		registry.Destroy(fly.object);
	}
	if (registry.Valid(fly.sprite))
	{
		registry.Destroy(fly.sprite);
	}
}

/// A new firefly: eight synced GameFloatRand in this order: speedA = GFR(0.8) + 0.6, speedB the same, then GFR(2 pi)
/// for the six phases (two of them never read, kept all the same). Its home and its last turn's place are where it is
/// made, its light none yet. The new firefly goes to the HEAD of the list
void Create(const map_coords::MapCoords& coords, const SheetTextures& texture)
{
	auto& state = FireFliesData();
	using game_random::GameFloatRand;
	FireFly fly;
	// an object of its own: it takes the creation counter's next number, before its speeds and phases are drawn
	fly.creationIndex = object_index::Peek();
	object_index::Skip(1);
	fly.coords = fly.madeAt = fly.home = fly.last = coords;
	const float speedA = GameFloatRand(0.8f);
	fly.speedA = speedA + 0.6f;
	const float speedB = GameFloatRand(0.8f);
	fly.speedB = speedB + 0.6f;
	for (auto& phase : fly.phases)
	{
		phase = GameFloatRand(glm::two_pi<float>());
	}
	MakeObject(fly);
	MakeSprite(fly, texture);
	state.fireFlies.insert(state.fireFlies.begin(), fly);
}

/// (max - count) attempts (the max re-read each time). Each GameRand(2): nonzero, the GameRand(count) tree of the tree
/// list, an empty list ends the spawn; zero, the GameRand(count) object of the multi-map fixed list, then on to the
/// first rock, an empty list ends the spawn and no rock from there makes nothing this attempt. A firefly at its map
/// coordinates
void Spawn()
{
	auto& state = FireFliesData();
	if (state.fireFlies.size() >= k_MaxFireFlies)
	{
		return;
	}
	const auto trees = TreeList();
	const auto fixed = MultiMapFixedList();
	const auto texture = SheetTexture();
	for (size_t attempt = state.fireFlies.size(); attempt < k_MaxFireFlies; ++attempt)
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
			at = FireFlyRockFrom(fixed, game_random::GameRand(static_cast<uint32_t>(fixed.size())));
			if (at == entt::null)
			{
				continue;
			}
		}
		Create(object::MapCoordsOf(at), texture);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireflies: {} ({} trees, {} multi map fixed)", state.fireFlies.size(),
	                   trees.size(), fixed.size());
}

/// The flight's length in seconds: the distance across the land between the home and the light
float FlightDuration(const FireFly& fly)
{
	return fire_flies::FlightSeconds(game_math::GetDistanceInMetres(fly.light, fly.home), fly.speedA);
}

/// The flight out to a light: from its home, the light those map coordinates, its last turn's place where it is and
/// its drawn point its home's
void FlyToLight(FireFly& fly, const map_coords::MapCoords& light)
{
	fly.state = State::FlyingOut;
	fly.light = light;
	fly.last = fly.coords;
	fly.drawn = map_coords::ToWorld(fly.home);
	fly.progress = 0.0f;
	fly.duration = FlightDuration(fly);
}

/// The flight home: from the light, its home those map coordinates
void FlyToPerch(FireFly& fly, const map_coords::MapCoords& home)
{
	fly.state = State::FlyingHome;
	fly.home = home;
	fly.progress = 0.0f;
	fly.duration = FlightDuration(fly);
}

/// The list head to the tail
void HeadToTail()
{
	auto& state = FireFliesData();
	std::rotate(state.fireFlies.begin(), state.fireFlies.begin() + 1, state.fireFlies.end());
}

/// Only the list head, and only when it is resting. Its tree or rock must still be there: an object of the fixed list
/// of its cell that is a rock or tree and whose map coordinates equal its own (x, z and the altitude), else it is
/// deleted. (The same walk deletes it when another firefly there has its map coordinates; a firefly is a mobile
/// object, not in the fixed list, so that never happens.) Then it flies to the nearest-ish abode or lantern, raised by
/// its height + 2, none: 15 m along x, altitude 4; it is awake and goes to the tail
void WakeOne()
{
	auto& state = FireFliesData();
	if (state.fireFlies.empty() || !state.fireFlies.front().Resting())
	{
		return;
	}
	auto& fly = state.fireFlies.front();
	const auto at = CoordsOf(fly);
	bool perched = false;
	map_cells::ForEachFixed(map_coords::Cell(at), [&perched, &at](entt::entity candidate) {
		if (IsFireFlyPerch(candidate))
		{
			// map coordinates equal on x, z and the altitude
			perched = perched || object::MapCoordsOf(candidate) == at;
		}
		return true;
	});
	if (!perched)
	{
		DestroyFireFly(fly);
		state.fireFlies.erase(state.fireFlies.begin());
		return;
	}
	map_coords::MapCoords destination;
	if (const auto light = SpiralSearch(at, k_SearchRadius, IsAbodeOrStreetLight); light != entt::null)
	{
		destination = object::MapCoordsOf(light);
		const float height = object::GetHeight(light) + 2.0f;
		destination.altitude = height + destination.altitude;
	}
	else
	{
		destination = Aside(at, 4.0f);
	}
	FlyToLight(fly, destination);
	fly.flags &= static_cast<uint8_t>(~1U);
	HeadToTail();
}

/// Only the list head, and only when it is awake. It flies to the tree or rock the search finds, none: 15 m along x,
/// altitude 0; it is resting from now on and goes to the tail
void SleepOne()
{
	auto& state = FireFliesData();
	if (state.fireFlies.empty() || state.fireFlies.front().Resting())
	{
		return;
	}
	auto& fly = state.fireFlies.front();
	const auto at = CoordsOf(fly);
	const auto perch = SpiralSearch(at, k_SearchRadius, IsFireFlyPerch);
	FlyToPerch(fly, perch != entt::null ? object::MapCoordsOf(perch) : Aside(at, 0.0f));
	fly.flags |= 1U;
	HeadToTail();
}

/// The point that far between two map coordinates, (b - a) t + a in metres on each axis, made into map coordinates
map_coords::MapCoords Between(const map_coords::MapCoords& from, const map_coords::MapCoords& to, float t)
{
	const auto a = map_coords::ToWorld(from);
	const auto b = map_coords::ToWorld(to);
	return map_coords::FromWorld(glm::vec3((b.x - a.x) * t + a.x, (b.y - a.y) * t + a.y, (b.z - a.z) * t + a.z));
}
} // namespace

void ecs::ProcessFireFliesTurn(const DayNightClock& clock)
{
	auto& fireFliesState = FireFliesData();
	const float visual = clock.GetVisualTime();
	const float skyType = clock.SkyType();
	if (visual > 12.0f && skyType > 1.0f)
	{
		if (fireFliesState.canSpawn)
		{
			Spawn();
		}
		WakeOne();
		fireFliesState.canSpawn = false;
	}
	else if (visual < 12.0f && skyType < 1.0f)
	{
		SleepOne();
		fireFliesState.canSpawn = true;
	}

	// the turn's length in seconds, read every turn
	const float turnSeconds = static_cast<float>(game_clock::MsPerTurn()) * game_clock::k_SecondsPerMs;
	// at rest the progress is 0 or 1; in flight the turn's seconds over the flight's are added, up to 1, and the place
	// moves along the flight (out from the home to the light, home from the light to the home), its last turn's place
	// kept; at the end it is at the flight's end and at rest there
	for (auto& fly : fireFliesState.fireFlies)
	{
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
		{
			const bool out = fly.state == State::FlyingOut;
			fly.progress = std::min(1.0f, turnSeconds / fly.duration + fly.progress);
			fly.last = fly.coords;
			if (fly.progress == 1.0f)
			{
				fly.coords = out ? fly.light : fly.home;
				fly.state = out ? State::Hovering : State::Asleep;
			}
			else
			{
				fly.coords = out ? Between(fly.home, fly.light, fire_flies::Ease(fly.progress))
				                 : Between(fly.light, fly.home, fire_flies::Ease(fly.progress));
			}
			// its object moves in the map, into the new cell when it has left its own
			map_cells::MoveMapObject(fly.object, map_coords::ToWorld(fly.coords));
			break;
		}
		}
	}
}

void ecs::HideFireFlies()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& fly : FireFliesData().fireFlies)
	{
		if (registry.Valid(fly.sprite))
		{
			registry.Get<Sprite>(fly.sprite).tint = glm::vec4(0.0f);
		}
	}
}

bool ecs::DrawFireFly(entt::entity object, float seconds, const glm::vec3& camera)
{
	auto& fireFlies = FireFliesData().fireFlies;
	const auto it = std::ranges::find(fireFlies, object, &FireFly::object);
	if (it == fireFlies.end())
	{
		return false;
	}
	auto& fly = *it;
	auto& registry = Locator::entitiesRegistry::value();
	if (fly.state == State::Asleep || !registry.Valid(fly.sprite))
	{
		return true;
	}
	// the squared distance from the point its last drawing put it at, summed z, y, then x
	const glm::vec3 d = camera - fly.drawn;
	const float squared = (d.z * d.z + d.y * d.y) + d.x * d.x;
	if (!(squared <= 90000.0f))
	{
		return true;
	}
	// alpha 190, fading out between 100 and 300 m
	auto& sprite = registry.Get<Sprite>(fly.sprite);
	sprite.tint = glm::vec4(1.0f, 1.0f, 1.0f, fire_flies::Alpha(false, squared) / 255.0f);

	fly.clock += seconds;
	const float t1 = fly.clock * fly.speedA;
	const float t2 = fly.clock * fly.speedB;
	const float a1 = std::fmod(t1 * 0.1f + fly.phases[0], glm::two_pi<float>());
	const float a2 = std::fmod(t1 * 0.1f + fly.phases[1], glm::two_pi<float>());
	const float b1 = std::fmod(t2 + fly.phases[3], glm::two_pi<float>());
	const float b2 = std::fmod(t2 * 1.21f + fly.phases[4], glm::two_pi<float>());
	fly.drift = fire_flies::DriftAmplitude(fly.state, fly.progress);
	const float amplitude = fly.drift;
	const glm::vec3 big =
	    amplitude * 8.0f * glm::vec3(std::cos(a2) * std::cos(a1), 0.5f * std::sin(a2), std::cos(a2) * std::sin(a1));
	const glm::vec3 small =
	    amplitude * glm::vec3(std::cos(b2) * std::cos(b1), 0.5f * std::sin(b2), std::cos(b2) * std::sin(b1));
	const auto previous = map_coords::ToWorld(fly.last);
	const auto position = map_coords::ToWorld(fly.coords);
	// the two loops added first, then the turn's place between the last and this one, as the original sums them
	const glm::vec3 p = ((position - previous) * game_clock::TurnFraction() + previous) + (big + small);
	fly.drawn = p;
	registry.Get<Transform>(fly.sprite).position = p;
	return true;
}

void ecs::PlaceFireFly(const map_coords::MapCoords& coords)
{
	Create(coords, SheetTexture());
	// at run time, not compiled out of release builds, so that a run's log shows each one
	if (const auto logger = spdlog::get("game"))
	{
		logger->debug("Fireflies: a land script hides one at ({}, {}), {} in all", coords.x, coords.z,
		              FireFliesData().fireFlies.size());
	}
}

std::vector<ecs::SavedFireFly> ecs::SavedFireFlies()
{
	std::vector<SavedFireFly> saved;
	for (const auto& fly : FireFliesData().fireFlies)
	{
		saved.push_back({.state = fly.state,
		                 .coords = fly.coords,
		                 .madeAt = fly.madeAt,
		                 .home = fly.home,
		                 .light = fly.light,
		                 .last = fly.last,
		                 .drawn = fly.drawn,
		                 .clock = fly.clock,
		                 .drift = fly.drift,
		                 .progress = fly.progress,
		                 .duration = fly.duration,
		                 .speedA = fly.speedA,
		                 .speedB = fly.speedB,
		                 .phases = fly.phases,
		                 .flags = fly.flags,
		                 .creationIndex = fly.creationIndex,
		                 .object = fly.object});
	}
	return saved;
}

bool ecs::FireFliesMaySpawn()
{
	return FireFliesData().canSpawn;
}

void ecs::RestoreFireFly(const SavedFireFly& saved)
{
	FireFly fly {.state = saved.state,
	             .coords = saved.coords,
	             .madeAt = saved.madeAt,
	             .home = saved.coords,
	             .light = saved.light,
	             .last = saved.coords,
	             .drawn = saved.drawn,
	             .clock = saved.clock,
	             .drift = saved.drift,
	             .progress = saved.progress,
	             .duration = saved.duration,
	             .speedA = saved.speedA,
	             .speedB = saved.speedB,
	             .phases = saved.phases,
	             .flags = saved.flags,
	             .creationIndex = saved.creationIndex};
	MakeObject(fly);
	MakeSprite(fly, SheetTexture());
	FireFliesData().fireFlies.push_back(fly);
}

void ecs::SetFireFliesMaySpawn(bool maySpawn)
{
	FireFliesData().canSpawn = maySpawn;
}

size_t ecs::FireFlyCount()
{
	return FireFliesData().fireFlies.size();
}

void ecs::ClearFireFlies()
{
	auto& state = FireFliesData();
	for (const auto& fly : state.fireFlies)
	{
		DestroyFireFly(fly);
	}
	state.fireFlies.clear();
	state.canSpawn = true;
}

bool ecs::TakeFireFlyAt(const map_coords::MapCoords& coords)
{
	auto& state = FireFliesData();
	// the first object of the cell's walk that is a firefly with those map coordinates (x, z and the altitude)
	auto it = state.fireFlies.end();
	const map_cells::ReadBatch batch;
	const auto cell = map_coords::Cell(coords);
	for (auto candidate = map_cells::FindType(cell, ObjectType::Any); candidate != entt::null && it == state.fireFlies.end();
	     candidate = map_cells::FindType(cell, ObjectType::Any, candidate))
	{
		it = std::ranges::find_if(state.fireFlies, [candidate, &coords](const FireFly& fly) {
			return fly.object == candidate && CoordsOf(fly) == coords;
		});
	}
	if (it == state.fireFlies.end())
	{
		return false;
	}
	DestroyFireFly(*it);
	state.fireFlies.erase(it);
	return true;
}

float fire_flies::Ease(float progress)
{
	return (3.0f - (progress + progress)) * progress * progress;
}

float fire_flies::FlightSeconds(float metres, float speed)
{
	return std::max(0.5f, metres / (3.0f * speed));
}

float fire_flies::Alpha(bool asleep, float squaredDistance)
{
	if (asleep || !(squaredDistance < 90000.0f))
	{
		return 0.0f;
	}
	return squaredDistance < 10000.0f ? 190.0f : std::trunc((1.0f - (squaredDistance - 10000.0f) / 80000.0f) * 190.0f);
}

float fire_flies::DriftAmplitude(State state, float progress)
{
	switch (state)
	{
	case State::Asleep:
		return 0.0f;
	case State::Hovering:
		return 1.0f;
	case State::FlyingHome:
		// the flight home's factor is the original's float a hair over 5
		return progress < 0.8f ? 1.0f : 1.0f - (progress - 0.8f) * k_HomeDriftFactor;
	case State::FlyingOut:
		return progress < 0.2f ? progress * 5.0f : 1.0f;
	}
	return 0.0f;
}

bool ecs::IsFireFlyPerch(entt::entity object)
{
	return object_physics::IsRock(object) || Locator::entitiesRegistry::value().AnyOf<Tree, DeadTree>(object);
}

entt::entity ecs::FireFlyRockFrom(std::span<const entt::entity> fixed, size_t start)
{
	const auto from = fixed.subspan(std::min(start, fixed.size()));
	const auto it = std::ranges::find_if(from, [](entt::entity thing) { return object_physics::IsRock(thing); });
	return it != from.end() ? *it : entt::null;
}
