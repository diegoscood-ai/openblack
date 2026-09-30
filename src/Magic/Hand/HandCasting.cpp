/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandCasting.h"

#include <glm/geometric.hpp>

#include "Camera/Camera.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "HandMagicFX.h"
#include "Locator.h"
#include "Magic/Gestures/GestureDebugHooks.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "PSys/Utility.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// g_game_time_inc of the last frame, for the per-turn ProcessPowerUpSystem
float g_LastFrameSeconds = 0.0f;
} // namespace

void hand_casting::OnLoadMap()
{
	gestures::Reset();
	gestures::ResetDebugHooks();
	hand_fx::Reset();
	psys::utility::Reset();
	g_LastFrameSeconds = 0.0f;
}

void hand_casting::ProcessTurn()
{
	gestures::ProcessPowerUpSystem(g_LastFrameSeconds);
}

void hand_casting::Update(float seconds)
{
	g_LastFrameSeconds = seconds;
	// the mouse messages (fn_005CEAD0) and the test strokes, then InterfaceActionProcess's ProcessPowerUpSystem
	gestures::RunDebugHooks(seconds);
	gestures::sampling::Update(seconds);
	gestures::ProcessPowerUpSystem(seconds);
	// CHand::Draw: PHandFX::DrawHandFX 0x68DD60 and DrawSpellInHand 0x46E680 (g_game_time_inc in ms)
	hand_fx::Update(seconds);
	hand_fx::UpdateInHandEffect(seconds * 1000.0f);
	// PSysUtilityPSys fn_00671DA0: the trail at the hand, magnitude handScale x f(camera distance to the hand)
	if (Locator::handSystem::has_value())
	{
		const auto& hand = Locator::handSystem::value();
		const glm::vec3 position(hand.GetHandMatrix()[3]);
		const float distance = Locator::camera::has_value() ? glm::distance(Locator::camera::value().GetOrigin(), position) : 0.0f;
		psys::utility::Update(seconds, position, hand.GetHandScale(), distance);
	}
}
