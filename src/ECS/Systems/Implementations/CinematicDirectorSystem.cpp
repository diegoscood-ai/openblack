/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CinematicDirectorSystem.h"

using namespace openblack::ecs::systems;

void CinematicDirectorSystem::FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds)
{
	_fade.FadeTo(red, green, blue, seconds);
}

void CinematicDirectorSystem::FadeBackToNormal(float seconds)
{
	_fade.FadeBackToNormal(seconds);
}

bool CinematicDirectorSystem::IsFadeFinished() const
{
	return _fade.IsFinished();
}

uint32_t CinematicDirectorSystem::GetFadeColour() const
{
	return _fade.GetColour();
}

void CinematicDirectorSystem::SetFadeColour(uint32_t argb)
{
	_fade.SetColour(argb);
}

void CinematicDirectorSystem::SetWideScreen(bool on, float transitionSeconds)
{
	_bars.Set(on, transitionSeconds);
}

void CinematicDirectorSystem::SnapWideScreen()
{
	_bars.Snap();
}

bool CinematicDirectorSystem::IsWideScreenOn() const
{
	return _bars.IsOn();
}

bool CinematicDirectorSystem::IsWideScreenTransitionFinished() const
{
	return _bars.IsTransitionFinished();
}

float CinematicDirectorSystem::GetWideScreenFraction() const
{
	return _bars.GetFraction();
}

void CinematicDirectorSystem::ProcessTurn()
{
	_fade.ProcessTurn();
}

void CinematicDirectorSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	_bars.Update(gameTime.count());
}
