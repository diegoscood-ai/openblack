/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandCasting.h"

#include <cstdio>
#include <cstdlib>

#include "Camera/Camera.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Particles/Utility.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
struct HandCastingState
{
	/// The last frame's game time, for the per-turn ProcessPowerUpSystem
	float lastFrameSeconds {0.0f};
};

/// This module's state (the miracles' service's hand magic state)
HandCastingState& Casting()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("magic::hand_casting: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value().HandMagic().Get<HandCastingState>();
}

/// The gestures (Locator::gestureSystem)
ecs::systems::GestureSystemInterface& Gestures()
{
	if (!Locator::gestureSystem::has_value())
	{
		std::fputs("magic::hand_casting: no gesture system in the locator (Locator::gestureSystem)\n", stderr);
		std::abort();
	}
	return Locator::gestureSystem::value();
}
} // namespace

void hand_casting::OnLoadMap()
{
	Gestures().Reset();
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().Reset();
	}
	psys::utility::Reset();
	Casting().lastFrameSeconds = 0.0f;
}

void hand_casting::ProcessTurn()
{
	gestures::ProcessPowerUpSystem(Casting().lastFrameSeconds);
}

void hand_casting::Update(float seconds)
{
	Casting().lastFrameSeconds = seconds;
	// the mouse messages and the test strokes, then the interface action's ProcessPowerUpSystem.
	// (not ported) the original runs the interface action once per queued button message and once with none, so a
	// frame with two button edges runs it twice; a hand demo replays its records the same way.
	// openblack reads the buttons once a frame
	Gestures().Update({.seconds = seconds});
	// the hand's draw: the hand effects and the spell in the hand
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().UpdateHand(seconds);
	}
	// the utility effect's trail at the hand, magnitude handScale x f(camera distance to the hand)
	if (Locator::handSystem::has_value())
	{
		const auto& hand = Locator::handSystem::value();
		const glm::vec3 position(hand.GetHandMatrix()[3]);
		// without a camera the hand is at no distance from it
		const glm::vec3 camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : position;
		Locator::particleSystem::value().UpdateFrame(
		    seconds, {.position = position, .size = hand.GetHandScale(), .cameraPosition = camera});
	}
}
