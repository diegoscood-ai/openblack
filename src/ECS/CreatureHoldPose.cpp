/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHoldPose.h"

#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::optional<ecs::creature_hold_pose::HoldingHand> ecs::creature_hold_pose::HoldingHandOf(const Registry& registry,
                                                                                           entt::entity creature)
{
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	if (held == nullptr)
	{
		return std::nullopt;
	}
	std::optional<creature_object_actions::Kind> playing;
	if (const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	    action != nullptr && action->phase == CreatureObjectAction::Phase::Playing &&
	    (action->status == creature_object_actions::Status::Running ||
	     action->status == creature_object_actions::Status::Contact))
	{
		playing = action->kind;
	}
	// waiting for a thing and stepping across to it keep the pose; the catch itself does not
	if (const auto* catching = registry.TryGet<const CreatureCatching>(creature);
	    catching != nullptr && catching->stage == CreatureCatching::Stage::Reaching)
	{
		return std::nullopt;
	}
	if (!creature_object_actions::ShowsHoldPose(playing))
	{
		return std::nullopt;
	}
	return HoldingHand {.bone = held->bone, .leftHand = held->mirrored};
}
