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
#include "GameClock.h"
#include "Locator.h"
#include "LightningFlash.h"
#include "Rain.h"
#include "StormClouds.h"
#include "WeatherThing.h"

using namespace openblack;
using namespace openblack::weather;

void weather::OnLoadMap()
{
	climate::Reset();
	weather_thing::Reset();
	atmos::Reset();
	rain::Reset();
	storm_clouds::Clear();
	ResetDebugHooks();
}

void weather::ProcessTurnStart([[maybe_unused]] uint32_t turn)
{
	auto* game = Game::Instance();
	const float visualTime = game != nullptr ? game->GetDayNightClock().GetVisualTime() : 12.0f; // (port: tests only)
	atmos::UpdateGame(visualTime, game_clock::k_TurnSeconds); // 0x54E5D1: push 0x3DCCCCCD
	// then GGame::ProcessTurn sets LH3DTech::g_ambient_wind_direction from the ambient weather's wind (normalised; 0
	// in a game): no reader in openblack
}

void weather::ProcessWeatherThings()
{
	// 0x54E6CB
	weather_thing::ProcessWeatherThings();
}

void weather::ProcessClimate()
{
	// 0x54E6DA
	climate::ProcessAll(game_clock::Turn());
	RunDebugHooks(game_clock::Turn());
}

void weather::UpdateFrame(float seconds)
{
	// LH3DAtmos::Update3D 0x8357A0: first each storm's flash (fn_00837200), then the camera's (in flash::AtCamera)
	flash::UpdateFrame();
	if (Locator::camera::has_value())
	{
		rain::Update(seconds, Locator::camera::value().GetOrigin());
		// fn_0083F8B0 (from fn_005E5830, every frame): GWeather::DrawClouds of every storm
		storm_clouds::DrawFrame(seconds * 1000.0f);
	}
}
