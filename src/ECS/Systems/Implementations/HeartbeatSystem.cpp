/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HeartbeatSystem.h"

#include "Audio/Services/Guidance.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

float HeartbeatSystem::Pulse(PlayerNames player, float turnFraction) const
{
	// the guidance's state lives in the audio state, which the game holds from start to end
	if (!Locator::audioState::has_value() || player != creature::LocalPlayer())
	{
		return 0.0f;
	}
	return audio::guidance::HeartBeatPulse(turnFraction);
}

std::optional<ecs::heartbeat::State> HeartbeatSystem::StateOf(PlayerNames player) const
{
	if (!Locator::audioState::has_value() || player != creature::LocalPlayer())
	{
		return std::nullopt;
	}
	const auto& guidance = audio::guidance::GetState();
	return heartbeat::State {
	    .stress = guidance.heartBeatValue,
	    .rate = guidance.heartBeatPitch,
	    .phase = guidance.heartBeatPhase,
	    .pulse = guidance.heartBeatPulse,
	    .previousPulse = guidance.heartBeatPulsePrevious,
	    .believerShare = guidance.believers,
	    .influenceShare = guidance.beliefShare,
	    .destructionBeats = guidance.heartBeatOverride,
	};
}
