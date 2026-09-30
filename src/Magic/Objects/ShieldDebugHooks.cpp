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
} // namespace

void magic::shield_debug::OnTurn(unsigned int created, unsigned int turn)
{
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
