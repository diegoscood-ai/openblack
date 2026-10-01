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
#include <optional>
#include <random>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
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
constexpr float k_TurnSeconds = static_cast<float>(game_clock::k_MsPerTurn) * game_clock::k_SecondsPerMs; // [0xD01A38] * 0.001
constexpr float k_SearchRadius = 300.0f; // fn_0052A5D0 / fn_0052A7A0
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
	glm::vec3 perch {0.0f};    // the tree or rock it sleeps on
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

std::vector<FireFly> g_fireFlies;
bool g_canSpawn = true; // [0xBE9DA0], set again every morning
std::mt19937 g_random {0x46495245u};

float Random(float from, float to)
{
	return std::uniform_real_distribution<float>(from, to)(g_random);
}

float Ground(const glm::vec3& p)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(p.x, p.z)) : p.y;
}

float MeshHeight(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * registry.Get<const Transform>(entity).scale.y;
}

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

/// The trees and rocks the fireflies sleep on (GameLists.trees, the rocks of GameLists.multi_map_fixed)
void Perches(std::vector<glm::vec3>& trees, std::vector<glm::vec3>& rocks)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Tree, const Transform>(
	    [&trees](entt::entity /*unused*/, const Tree& /*unused*/, const Transform& transform) { trees.push_back(transform.position); });
	registry.Each<const Transform>([&rocks](entt::entity entity, const Transform& transform) {
		if (Rocks::IsRock(entity))
		{
			rocks.push_back(transform.position);
		}
	});
}

/// The spiral search of fn_0052A5D0 / fn_0052A7A0 over 300 m: each cell is tested with a 50 % chance and the search
/// may stop at a candidate, so the result is the nearest candidate that survives a coin toss
std::optional<glm::vec3> Nearest(const glm::vec3& from, std::vector<std::pair<float, glm::vec3>> candidates)
{
	std::erase_if(candidates, [&from](auto& c) {
		c.first = glm::distance(glm::vec2(from.x, from.z), glm::vec2(c.second.x, c.second.z));
		return c.first >= k_SearchRadius;
	});
	std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	for (const auto& candidate : candidates)
	{
		if (std::bernoulli_distribution(0.5)(g_random))
		{
			return candidate.second;
		}
	}
	return std::nullopt;
}

/// fn_0052B1D0: houses and street lanterns, the destination raised by the object's height + 2
std::vector<std::pair<float, glm::vec3>> Lights()
{
	std::vector<std::pair<float, glm::vec3>> result;
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Abode, const Transform>([&result](entt::entity entity, const Abode& /*unused*/, const Transform& transform) {
		result.emplace_back(0.0f, transform.position + glm::vec3(0.0f, MeshHeight(entity) + 2.0f, 0.0f));
	});
	const auto lantern = resources::HashIdentifier(MeshId::ObjectTownLight);
	registry.Each<const Mesh, const Transform>([&result, lantern](entt::entity entity, const Mesh& mesh, const Transform& transform) {
		if (mesh.id == lantern)
		{
			result.emplace_back(0.0f, transform.position + glm::vec3(0.0f, MeshHeight(entity) + 2.0f, 0.0f));
		}
	});
	return result;
}

void Spawn()
{
	std::vector<glm::vec3> trees;
	std::vector<glm::vec3> rocks;
	Perches(trees, rocks);
	const auto texture = SheetTexture();
	auto& registry = Locator::entitiesRegistry::value();
	while (g_fireFlies.size() < k_MaxFireFlies)
	{
		const bool tree = std::uniform_int_distribution<int>(0, 1)(g_random) != 0;
		const auto& list = tree ? trees : rocks;
		if (list.empty())
		{
			if (trees.empty() && rocks.empty())
			{
				return;
			}
			continue;
		}
		const auto& at = list[std::uniform_int_distribution<size_t>(0, list.size() - 1)(g_random)];
		FireFly fly;
		fly.perch = glm::vec3(at.x, Ground(at), at.z);
		fly.position = fly.previous = fly.from = fly.to = fly.perch;
		fly.speedA = Random(0.0f, 0.8f) + 0.6f;
		fly.speedB = Random(0.0f, 0.8f) + 0.6f;
		fly.phaseA1 = Random(0.0f, glm::two_pi<float>());
		fly.phaseA2 = Random(0.0f, glm::two_pi<float>());
		fly.phaseB1 = Random(0.0f, glm::two_pi<float>());
		fly.phaseB2 = Random(0.0f, glm::two_pi<float>());
		fly.sprite = registry.Create();
		registry.Assign<Sprite>(fly.sprite, texture, graphics::frame_anim::SpriteCellUv(static_cast<int>(k_Frame), 8)[0],
		                        glm::vec2(1.0f / 8.0f), glm::vec4(0.0f), true);
		registry.Assign<Transform>(fly.sprite, fly.position, glm::mat3(1.0f), glm::vec3(k_HalfSize));
		g_fireFlies.push_back(fly);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireflies: {} ({} trees, {} rocks)", g_fireFlies.size(), trees.size(), rocks.size());
}

void StartFlight(FireFly& fly, const glm::vec3& to, State state)
{
	fly.from = fly.position;
	fly.to = to;
	fly.progress = 0.0f;
	fly.state = state;
	fly.duration = std::max(0.5f, glm::distance(fly.from, fly.to) / (3.0f * fly.speedA));
}

/// fn_0052B350: the first resting firefly flies out to the nearest light (none: 1.5 m aside, 4 up)
void WakeOne()
{
	const auto it = std::find_if(g_fireFlies.begin(), g_fireFlies.end(), [](const FireFly& f) { return f.resting; });
	if (it == g_fireFlies.end())
	{
		return;
	}
	auto fly = *it;
	g_fireFlies.erase(it);
	const auto destination = Nearest(fly.position, Lights());
	StartFlight(fly, destination.value_or(fly.position + glm::vec3(1.5f, 4.0f, 0.0f)), State::FlyingOut);
	fly.resting = false;
	g_fireFlies.push_back(fly); // to the end of the list
}

/// fn_0052B4C0: the first awake firefly flies back to the nearest tree or rock
void SleepOne()
{
	const auto it = std::find_if(g_fireFlies.begin(), g_fireFlies.end(), [](const FireFly& f) { return !f.resting; });
	if (it == g_fireFlies.end())
	{
		return;
	}
	std::vector<glm::vec3> trees;
	std::vector<glm::vec3> rocks;
	Perches(trees, rocks);
	std::vector<std::pair<float, glm::vec3>> candidates;
	for (const auto& p : trees)
	{
		candidates.emplace_back(0.0f, p);
	}
	for (const auto& p : rocks)
	{
		candidates.emplace_back(0.0f, p);
	}
	auto destination = Nearest(it->position, std::move(candidates));
	if (destination)
	{
		destination->y = Ground(*destination);
	}
	it->perch = destination.value_or(glm::vec3(it->position.x + 1.5f, Ground(it->position), it->position.z));
	StartFlight(*it, it->perch, State::FlyingHome);
	it->resting = true;
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
			fly.progress = std::min(1.0f, fly.progress + k_TurnSeconds / fly.duration);
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
