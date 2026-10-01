/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StormDebugHooks.h"

#include <cstdio>
#include <cstdlib>

#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/Weather/LightningFlash.h"
#include "ECS/Weather/Weather.h"
#include "Game.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "PSys/PSysManager.h"
#include "PSys/Rules/Storm.h"

using namespace openblack;

namespace
{
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};

std::vector<Shot> ParseShots(const char* name)
{
	std::vector<Shot> list;
	const char* value = std::getenv(name);
	if (value == nullptr)
	{
		return list;
	}
	std::stringstream stream(value);
	std::string item;
	while (std::getline(stream, item, ';'))
	{
		const auto comma = item.find(',');
		if (comma != std::string::npos)
		{
			list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
		}
	}
	return list;
}

/// OPENBLACK_TEST_STORM_STRIKE_SHOT="<n>,<path>[;...]": a screenshot at the n-th strike from the clouds
std::vector<Shot>& StrikeShots()
{
	static std::vector<Shot> shots = ParseShots("OPENBLACK_TEST_STORM_STRIKE_SHOT");
	return shots;
}

std::vector<Shot>& Shots()
{
	static std::vector<Shot> shots = [] {
		std::vector<Shot> list;
		const char* value = std::getenv("OPENBLACK_TEST_STORM_SHOT");
		if (value == nullptr)
		{
			return list;
		}
		std::stringstream stream(value);
		std::string item;
		while (std::getline(stream, item, ';'))
		{
			const auto comma = item.find(',');
			if (comma != std::string::npos)
			{
				list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
			}
		}
		return list;
	}();
	return shots;
}

bool g_HaveFirst = false;
unsigned int g_FirstTurn = 0;
} // namespace

void magic::storm_debug::OnTurn(entt::entity spell)
{
	const unsigned int turn = CurrentTurn();
	if (!g_HaveFirst)
	{
		g_HaveFirst = true;
		g_FirstTurn = turn;
		// OPENBLACK_TEST_STORM_PILE="x,z,amount[,wood]": a food (or wood) pile there when the first storm is cast, for
		// the tornado to take (a test helper: PotArchetype::Create, as a dropped pile)
		if (const char* pile = std::getenv("OPENBLACK_TEST_STORM_PILE"); pile != nullptr && Locator::terrainSystem::has_value())
		{
			float x = 0.0f;
			float z = 0.0f;
			int amount = 0;
			char kind[16] = {};
			if (std::sscanf(pile, "%f,%f,%d,%15s", &x, &z, &amount, kind) >= 3)
			{
				const glm::vec3 position(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z);
				const auto type = std::string(kind) == "wood" ? PotInfo::WoodPile_5 : PotInfo::FoodPile;
				const auto created = ecs::archetypes::PotArchetype::Create(position, 0.0f, type, amount);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm test: pile {} of {} at ({:.1f}, {:.1f}) -> entity {}",
				                   static_cast<int>(type), amount, x, z, static_cast<uint32_t>(created));
			}
		}
	}
	if (Game::Instance() != nullptr)
	{
		for (auto& shot : Shots())
		{
			if (!shot.done && turn >= g_FirstTurn + shot.turns)
			{
				shot.done = true;
				Game::Instance()->RequestScreenshot(shot.path);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm test: screenshot {} turns after the first storm (turn {}) -> {}",
				                   shot.turns, turn, shot.path);
				break; // one request per turn
			}
		}
	}
	if (Game::Instance() != nullptr)
	{
		for (auto& shot : StrikeShots())
		{
			if (!shot.done && psys::storm::StrikeCount() >= shot.turns)
			{
				shot.done = true;
				Game::Instance()->RequestScreenshot(shot.path);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm test: screenshot at strike {} (turn {}) -> {}", shot.turns, turn,
				                   shot.path);
				break;
			}
		}
	}
	if (!psys::storm::TraceEnabled() || (turn - g_FirstTurn) % 10 != 0)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& component = registry.Get<const ecs::components::Spell>(spell);
	const glm::vec3 centre = ToWorld(glm::vec3(component.position.x, 0.0f, component.position.z));
	const auto weather = weather::ComputeWeather(centre);
	const auto* effect = psys::manager::Find(component.psys);
	uint8_t flash = 0;
	if (Locator::camera::has_value())
	{
		flash = weather::LightningFlashAtCamera(centre);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Storm trace: turn {} spell {} age {:.1f} chants {:.0f} closed {} at ({:.1f}, {:.1f}): rain {} overcast {} "
	                   "wind ({}, {}) temp {} | atoms {} carried {} flash {}",
	                   turn, static_cast<uint32_t>(spell), component.age, component.chants, component.closedDown,
	                   component.position.x, component.position.z, weather.rain, weather.overcast, weather.windX,
	                   weather.windZ, weather.temperature, effect != nullptr ? effect->AtomCount() : 0,
	                   psys::storm::CarriedObjectCount(), flash);
}

void magic::storm_debug::Reset()
{
	g_HaveFirst = false;
	for (auto& shot : Shots())
	{
		shot.done = false;
	}
}
