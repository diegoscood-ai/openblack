/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "InfluenceSystem.h"

#include "ECS/Systems/HandSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void InfluenceSystem::ProcessTurn([[maybe_unused]] uint32_t turn)
{
	influence::ProcessTurn();
}

void InfluenceSystem::UpdateBorders()
{
	influence::Update3DInfluence();
}

void InfluenceSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	// The hand's point is tested against the circles only while the game runs, once the hand has a point
	if (!game_clock::IsPaused())
	{
		if (const auto& hands = Locator::handSystem::value().GetPlayerHandPositions(); hands[0].has_value())
		{
			influence::ProcessHandCrossing(*hands[0]);
		}
	}
	// Every ripple ages by the frame's game time, which is none while paused. The frame's whole milliseconds come back
	// exactly from the float
	influence::UpdateRipples(static_cast<uint32_t>(gameTime.count()));
}

float InfluenceSystem::PlayerInfluence(PlayerNames player, const glm::vec3& position) const
{
	return influence::CalculatePlayerInfluence(player, position);
}

float InfluenceSystem::PlayerInfluence(PlayerNames player, const glm::vec3& position, influence::CalcType type,
                                       bool includeAllies) const
{
	return influence::CalculatePlayerInfluence(player, position, type, includeAllies);
}

bool InfluenceSystem::IsInAntiInfluence(PlayerNames player, const glm::vec3& position) const
{
	return influence::IsInAntiInfluence(player, position);
}

std::span<const influence::Circle> InfluenceSystem::GetCircles() const
{
	return influence::Circles();
}

bool InfluenceSystem::IsBorderShown(PlayerNames player) const
{
	return influence::BoundaryShown(player);
}

std::span<const influence::Ripple> InfluenceSystem::GetRipples() const
{
	return influence::Ripples();
}
