/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GestureSystem.h"

#include <utility>

#include "Magic/Gestures/GestureDebugHooks.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/PowerUpSystem.h"

using namespace openblack::ecs::systems;

namespace
{
GestureSystem::Passes MagicGesturePasses()
{
	namespace gestures = openblack::magic::gestures;
	return {
	    .frame =
	        [](float seconds) {
		        gestures::RunDebugHooks(seconds);
		        gestures::sampling::Update(seconds);
		        gestures::ProcessPowerUpSystem(seconds);
	        },
	    .newLand =
	        [] {
		        gestures::Reset();
		        gestures::ResetDebugHooks();
	        },
	};
}
} // namespace

GestureSystem::GestureSystem()
    : _passes(MagicGesturePasses())
{
}

GestureSystem::GestureSystem(Passes passes)
    : _passes(std::move(passes))
{
}

std::vector<GestureEvent> GestureSystem::TakeEvents()
{
	return std::exchange(_events, {});
}

void GestureSystem::Inject(const GestureEvent& event)
{
	_events.push_back(event);
}

void GestureSystem::Reset()
{
	_events.clear();
	if (_passes.newLand)
	{
		_passes.newLand();
	}
}

void GestureSystem::Update(const Frame& frame)
{
	if (_passes.frame)
	{
		_passes.frame(frame.seconds);
	}
}

void GestureSystem::ForgetPath()
{
	openblack::magic::gestures::ClearBuffer();
}

float GestureSystem::GetScreenAspect() const
{
	return openblack::magic::gestures::sampling::ScreenRatio();
}

float GestureSystem::GetCircleSecondsLeft() const
{
	return openblack::magic::gestures::CircleSecondsLeft(openblack::magic::gestures::State());
}

bool GestureSystem::IsGesturing() const
{
	return openblack::magic::gestures::IsGesturing();
}
