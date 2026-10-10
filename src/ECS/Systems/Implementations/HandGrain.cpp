/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandGrain.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include <spdlog/spdlog.h>

#include "Debug/DebugEnv.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/HandMotion.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
struct HandGrainState
{
	magic::PourState pour;
	bool holdingSeed {false};
};

/// The grain sprinkle's hand raise (the miracles' service's hand magic state)
HandGrainState& Grain()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("hand_grain: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value().HandMagic().Get<HandGrainState>();
}

/// The fraction of the turn of the game clock
float TurnFraction()
{
	return game_clock::TurnFraction();
}

bool Trace()
{
	static const bool trace = debug_env::SpellTrace() || debug_env::HandTrace();
	return trace;
}
} // namespace

void hand_grain::Start(bool clampHand, float totalTime, float heightToRaise, float angleToRaise, bool loop)
{
	const magic::PourSettings settings {.totalTime = totalTime,
	                                    .heightToRaise = heightToRaise,
	                                    .angleToRaise = angleToRaise,
	                                    .loops = loop,
	                                    .clampHand = clampHand};
	// the hand's point. (approximate) openblack's left hand transform stands in for it (the grip point while it holds
	// something); without a hand the point kept from before
	if (Locator::handSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(hand))
		{
			magic::StartPour(Grain().pour, settings, registry.Get<const ecs::components::Transform>(hand).position);
			return;
		}
	}
	magic::StartPour(Grain().pour, settings);
}

void hand_grain::Stop()
{
	magic::StopPour(Grain().pour);
}

void hand_grain::GameTurnUpdate(float dt)
{
	// (pending) a creature it follows: creatures are not ported yet
	auto& pour = Grain().pour;
	magic::StepPour(pour, dt);
	if (pour.active && Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Grain trace: t {:.2f} of {:.1f} s, raise {:.2f} m, tilt {:.3f} rad, clamped {}", pour.progress,
		                   pour.settings.totalTime, pour.current.raise, pour.current.tilt, pour.settings.clampHand);
	}
}

magic::PourPose hand_grain::PoseAt(float fraction)
{
	return magic::PourPoseAt(Grain().pour, fraction);
}

float hand_grain::Height()
{
	return magic::PourPoseAt(Grain().pour, TurnFraction()).raise;
}

float hand_grain::Tilt()
{
	return magic::PourPoseAt(Grain().pour, TurnFraction()).tilt;
}

std::optional<glm::vec3> hand_grain::ClampedPosition()
{
	return magic::PinnedHand(Grain().pour);
}

bool hand_grain::Active()
{
	return Grain().pour.active;
}

glm::vec3 hand_grain::Debug()
{
	const auto& pour = Grain().pour;
	return {pour.progress, pour.current.raise, pour.current.tilt};
}

void hand_grain::SetHoldingSeed(bool holding)
{
	if (holding == Grain().holdingSeed)
	{
		return;
	}
	Grain().holdingSeed = holding;
	if (holding)
	{
		// entering (after the holding state's enter): the grain state is cleared
		Grain() = HandGrainState {};
		Grain().holdingSeed = true;
	}
	else
	{
		// leaving: off, no creature followed
		Grain().pour.active = false;
	}
}

void hand_grain::Reset()
{
	Grain() = HandGrainState {};
}
