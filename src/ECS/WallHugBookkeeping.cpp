/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WallHugBookkeeping.h"

#include "3D/MapCoords.h"

namespace openblack::ecs::wall_hug
{

namespace
{
/// The steps ahead a walker jumps when it turns round
constexpr int32_t k_TurnRoundSteps = 3;
/// Half a turn, and a quarter
constexpr uint16_t k_HalfTurn = 0x400;
constexpr uint16_t k_QuarterTurn = 0x200;
constexpr uint16_t k_AngleMask = 0x7FF;
/// A map cell is 65536 whole units; its middle is half of one past its corner
constexpr uint32_t k_CellMiddle = 0x8000;
} // namespace

CircleGone OnCircleGone(WalkState state)
{
	switch (state)
	{
	case WalkState::Linear:
	case WalkState::LinearClockwise:
	case WalkState::LinearAnticlockwise:
		return {.state = state, .reAim = false, .lookAgain = true, .letGo = false};
	case WalkState::OrbitClockwise:
		return {.state = WalkState::LinearClockwise, .reAim = true, .lookAgain = true, .letGo = false};
	case WalkState::OrbitAnticlockwise:
		return {.state = WalkState::LinearAnticlockwise, .reAim = true, .lookAgain = true, .letGo = false};
	case WalkState::ExitClockwise:
		return {.state = WalkState::LinearClockwise, .reAim = true, .lookAgain = true, .letGo = true};
	case WalkState::ExitAnticlockwise:
		return {.state = WalkState::LinearAnticlockwise, .reAim = true, .lookAgain = true, .letGo = true};
	default:
		return {.state = state, .reAim = false, .lookAgain = false, .letGo = true};
	}
}

glm::vec2 LandscapeCircleCentre(glm::ivec2 cell)
{
	const auto middle = [](int32_t index) {
		const uint32_t whole = (static_cast<uint32_t>(static_cast<uint16_t>(index)) << 16) | k_CellMiddle;
		return map_coords::ToMetres(static_cast<int32_t>(whole));
	};
	return {middle(cell.x), middle(cell.y)};
}

int32_t RunTrial(const std::function<bool()>& moveTo)
{
	int32_t turnsLeft = k_TrialTurns;
	while (!moveTo() && turnsLeft-- > 0)
	{
	}
	return turnsLeft;
}

bool DidBetter(const TrialResult& a, const TrialResult& b)
{
	if (a.turnsLeft != b.turnsLeft)
	{
		return a.turnsLeft > b.turnsLeft;
	}
	// Only "not less" rejects, so an unordered pair counts as nearer
	return !(a.distanceSq >= b.distanceSq);
}

TurnedRound TurnRound(const WalkPose& pose)
{
	WalkState next;
	bool lookAlongCircle = false;
	switch (pose.state)
	{
	case WalkState::LinearClockwise:
		next = WalkState::LinearAnticlockwise;
		break;
	case WalkState::LinearAnticlockwise:
		next = WalkState::LinearClockwise;
		break;
	case WalkState::OrbitClockwise:
		next = WalkState::OrbitAnticlockwise;
		lookAlongCircle = true;
		break;
	case WalkState::OrbitAnticlockwise:
		next = WalkState::OrbitClockwise;
		lookAlongCircle = true;
		break;
	case WalkState::ExitClockwise:
		next = WalkState::ExitAnticlockwise;
		break;
	case WalkState::ExitAnticlockwise:
		next = WalkState::ExitClockwise;
		break;
	default:
		return {.pose = pose, .turned = false, .lookAlongCircle = false};
	}
	WalkPose turned = pose;
	turned.state = next;
	turned.position = pose.position + pose.step * k_TurnRoundSteps;
	turned.step.x = -pose.step.x;
	turned.step.y = static_cast<int32_t>(-pose.stepAltitude);
	turned.stepAltitude = static_cast<float>(-pose.step.y);
	turned.angle = static_cast<uint16_t>((pose.angle + k_HalfTurn) & k_AngleMask);
	return {.pose = turned, .turned = true, .lookAlongCircle = lookAlongCircle};
}

std::optional<uint16_t> HeadingWhenKept(WalkState state, uint16_t angleToCentre)
{
	if (state == WalkState::OrbitClockwise)
	{
		return static_cast<uint16_t>((angleToCentre + k_QuarterTurn) & k_AngleMask);
	}
	if (state == WalkState::OrbitAnticlockwise)
	{
		return static_cast<uint16_t>((angleToCentre - k_QuarterTurn) & k_AngleMask);
	}
	return std::nullopt;
}

} // namespace openblack::ecs::wall_hug
