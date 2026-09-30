/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WeatherLoop.h"

#include "3D/DayNightClock.h"
#include "Atmos.h"
#include "Camera/Camera.h"
#include "Climate.h"
#include "Game.h"
#include "Locator.h"
#include "Rain.h"
#include "WeatherThing.h"

using namespace openblack;
using namespace openblack::weather;

namespace
{
uint32_t g_turn = 0;
}

void weather::OnLoadMap()
{
	climate::Reset();
	weather_thing::Reset();
	atmos::Reset();
	rain::Reset();
	ResetDebugHooks();
}

void weather::ProcessTurnStart(uint32_t turn)
{
	g_turn = turn;
	auto* game = Game::Instance();
	const float visualTime = game != nullptr ? game->GetDayNightClock().GetVisualTime() : 12.0f; // (port: tests only)
	atmos::UpdateGame(visualTime, 0.1f);
	// then GGame::ProcessTurn sets LH3DTech::g_ambient_wind_direction from the ambient weather's wind (normalised; 0
	// in a game): no reader in openblack
}

void weather::ProcessTurnEnd()
{
	weather_thing::ProcessWeatherThings();
	climate::ProcessAll(g_turn);
	RunDebugHooks(g_turn);
}

void weather::UpdateFrame(float seconds)
{
	if (Locator::camera::has_value())
	{
		rain::Update(seconds, Locator::camera::value().GetOrigin());
	}
}
