/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

#include "GameTurnOrder.h"

namespace openblack::simulation
{

/// The parts of a game turn that belong to the game on this computer rather than to the world: the start of a
/// skirmish's creature and the time between turns. The game does them.
class TurnViewInterface
{
public:
	virtual ~TurnViewInterface() = default;

	/// Do the part of a step of the turn that only the view has. The camera is where the turn's earlier steps left it.
	virtual void PlayViewStep(game_turn::Step step, glm::vec3 cameraPosition) = 0;

	/// The frames each second that the statistics of the game keep
	[[nodiscard]] virtual int32_t FrameRate() const = 0;
};

/// Do one step of the game turn. The parts of the view go to the view. The clock's step gives the turn number that the
/// later steps of the same turn use.
void PlayTurnStep(game_turn::Step step, TurnViewInterface& view, uint32_t& turn) noexcept;

} // namespace openblack::simulation
