/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShieldDebugHooks.h"

#include <cstdlib>

#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "Game.h"

using namespace openblack;

namespace
{
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};

std::vector<Shot>& Shots()
{
	static std::vector<Shot> shots = [] {
		std::vector<Shot> list;
		const char* value = std::getenv("OPENBLACK_TEST_SHIELD_SHOT");
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

unsigned int g_Created = 0;
bool g_AnyShield = false;
} // namespace

void magic::shield_debug::OnTurn(unsigned int created, unsigned int turn)
{
	g_Created = created;
	g_AnyShield = true;
	if (Game::Instance() == nullptr)
	{
		return;
	}
	for (auto& shot : Shots())
	{
		if (!shot.done && turn >= created + shot.turns)
		{
			shot.done = true;
			Game::Instance()->RequestScreenshot(shot.path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Shield test: screenshot {} turns after the shield (turn {}) -> {}",
			                   shot.turns, turn, shot.path);
			return; // one request per frame
		}
	}
}

void magic::shield_debug::OnFrame()
{
	static const auto burst = [] {
		struct Burst
		{
			unsigned int turns {0};
			int frames {0};
			float slow {0.0f};
			std::string prefix;
		} parsed;
		const char* value = std::getenv("OPENBLACK_TEST_SHIELD_FRAMES");
		if (value == nullptr)
		{
			return parsed;
		}
		std::stringstream stream(value);
		std::string item;
		std::getline(stream, item, ',');
		parsed.turns = static_cast<unsigned int>(std::atoi(item.c_str()));
		std::getline(stream, item, ',');
		parsed.frames = std::atoi(item.c_str());
		std::getline(stream, parsed.prefix);
		// "<prefix>@<slow>": the turn lasts <slow> times longer from the first shot on, so that several shots (each
		// stalls the frame) fall inside one turn
		if (const auto at = parsed.prefix.rfind('@'); at != std::string::npos)
		{
			parsed.slow = static_cast<float>(std::atof(parsed.prefix.substr(at + 1).c_str()));
			parsed.prefix.resize(at);
		}
		return parsed;
	}();
	static int taken = 0;
	auto* game = Game::Instance();
	if (burst.frames <= 0 || taken >= burst.frames || !g_AnyShield || game == nullptr ||
	    game->GetTurn() < g_Created + burst.turns)
	{
		return;
	}
	if (taken == 0 && burst.slow > 0.0f)
	{
		game->SetGameSpeed(burst.slow);
	}
	const auto path = burst.prefix + "_" + std::to_string(taken) + ".png";
	game->RequestScreenshot(path);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Shield test: frame shot {} at turn {} fraction {:.3f} -> {}", taken,
	                   game->GetTurn(), game->GetTurnFraction(), path);
	++taken;
}
