/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WalkArrival.h"

namespace walk_arrival = openblack::ecs::walk_arrival;
namespace map_coords = openblack::map_coords;
using openblack::ecs::components::MoveState;

namespace
{

/// A whole difference squared: the integer is loaded exactly and the product rounded once to a float
float Squared(int32_t difference)
{
	const auto wide = static_cast<int64_t>(difference);
	return static_cast<float>(wide * wide);
}

/// b - a as the game subtracts two whole coordinates (wrapping)
int32_t Difference(int32_t a, int32_t b)
{
	return static_cast<int32_t>(static_cast<uint32_t>(b) - static_cast<uint32_t>(a));
}

} // namespace

bool walk_arrival::WalkIsOver(std::optional<MoveState> state, glm::vec2 position, glm::vec2 goal)
{
	if (!state.has_value())
	{
		return true;
	}
	switch (*state)
	{
	case MoveState::FinalStep:
	case MoveState::Arrived:
		return position == goal;
	case MoveState::Linear:
	case MoveState::Orbit:
	case MoveState::ExitCircle:
	case MoveState::StepThrough:
		return false;
	}
	return false;
}

bool walk_arrival::AreWeThere(const map_coords::MapCoords& position, const map_coords::MapCoords& goal, uint16_t speed,
                              float extra)
{
	const float distance2 = Squared(Difference(goal.x, position.x)) + Squared(Difference(goal.z, position.z));
	const float reach = static_cast<float>(speed) + extra;
	return distance2 < reach * reach;
}
