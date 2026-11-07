/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WalkStateOf.h"

#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<wall_hug::WalkState> wall_hug::WalkStateOf(const ecs::Registry& registry, entt::entity walker)
{
	using wall_hug::WalkState;
	const auto wayRound = [](MoveStateClockwise clockwise, WalkState undefined, WalkState cw, WalkState ccw) {
		switch (clockwise)
		{
		case MoveStateClockwise::Clockwise:
			return cw;
		case MoveStateClockwise::CounterClockwise:
			return ccw;
		default:
			return undefined;
		}
	};
	if (const auto* linear = registry.TryGet<const MoveStateLinearTag>(walker))
	{
		return wayRound(linear->clockwise, WalkState::Linear, WalkState::LinearClockwise, WalkState::LinearAnticlockwise);
	}
	if (const auto* orbit = registry.TryGet<const MoveStateOrbitTag>(walker))
	{
		return wayRound(orbit->clockwise, WalkState::OrbitClockwise, WalkState::OrbitClockwise, WalkState::OrbitAnticlockwise);
	}
	if (const auto* exit = registry.TryGet<const MoveStateExitCircleTag>(walker))
	{
		return wayRound(exit->clockwise, WalkState::ExitClockwise, WalkState::ExitClockwise, WalkState::ExitAnticlockwise);
	}
	if (registry.AllOf<MoveStateStepThroughTag>(walker))
	{
		return WalkState::StepThrough;
	}
	if (registry.AllOf<MoveStateFinalStepTag>(walker))
	{
		return WalkState::FinalStep;
	}
	if (registry.AllOf<MoveStateArrivedTag>(walker))
	{
		return WalkState::Arrived;
	}
	return std::nullopt;
}
