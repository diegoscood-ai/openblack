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
#include "Debug/TraceLog.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/ToBeDeleted.h"
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
	/// The object the pour follows (a creature miracle's creature), entt::null for none
	entt::entity target {entt::null};
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
	return debug_env::TraceLogs(trace);
}

/// Where the hand is kept: at the followed object's head (the head bone of its turn's pose), else where the pour began
/// when it clamps the hand
std::optional<glm::vec3> HeldAt(const HandGrainState& grain)
{
	if (grain.target != entt::null && Locator::entitiesRegistry::has_value())
	{
		// (openblack) an object with no head bone pose keeps the hand where the pour began
		if (const auto head = mind_detail::HeadPosition(Locator::entitiesRegistry::value(), grain.target); head.has_value())
		{
			return head;
		}
	}
	return magic::PinnedHand(grain.pour);
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

void hand_grain::StartOnObject(entt::entity object, float totalTime, float heightToRaise, float angleToRaise, bool loop)
{
	Grain().target = object;
	Start(true, totalTime, heightToRaise, angleToRaise, loop);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand grain: pours onto {} for {:.1f} s, raise {:.2f} m, tilt {:.2f} rad",
	                   entt::to_integral(object), totalTime, heightToRaise, angleToRaise);
}

entt::entity hand_grain::Target()
{
	return Grain().target;
}

void hand_grain::Stop()
{
	Grain().target = entt::null;
	magic::StopPour(Grain().pour);
}

void hand_grain::GameTurnUpdate(float dt)
{
	// an object followed that is no longer available is let go; a pour that stops lets go of it too
	auto& grain = Grain();
	if (grain.target != entt::null && !ecs::IsAvailable(grain.target))
	{
		grain.target = entt::null;
	}
	auto& pour = grain.pour;
	const bool wasActive = pour.active;
	magic::StepPour(pour, dt);
	if (wasActive && !pour.active)
	{
		grain.target = entt::null;
	}
	if (pour.active && Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Grain trace: t {:.2f} of {:.1f} s, raise {:.2f} m, tilt {:.3f} rad, clamped {}", pour.progress,
		                   pour.settings.totalTime, pour.current.raise, pour.current.tilt, pour.settings.clampHand);
	}
}

magic::PourPose hand_grain::PoseAt(float fraction)
{
	auto pose = magic::PourPoseAt(Grain().pour, fraction);
	pose.pinned = HeldAt(Grain());
	return pose;
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
	return HeldAt(Grain());
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
		Grain().target = entt::null;
	}
}

void hand_grain::Reset()
{
	Grain() = HandGrainState {};
}
