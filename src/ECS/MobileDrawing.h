/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// How the original draws villagers and animals between game turns (docs/bw1-notes/animation.md; research
/// dev\tmp_dis\anim\draw_interp.md). The simulation moves them once per turn (10 per second); every rendered frame
/// fn_0051AF00 draws a moving one between its position at the start and the end of the last turn by the turn's fraction
/// (one turn behind, the two land heights lerped), Villager::Draw turns the drawn yaw towards the real one (3 rad/s, faster
/// past 90 degrees), and fn_0051B220 shears it along the slope while it stands on the land.

/// Living::ProcessLiving (0x5EC810), each turn before the living move: the start of this turn's move
void BeginMobileTurn();

/// Every frame: the drawn positions, with `turnFraction` (0..0.99, g_game+0x205D64) and the frame's game milliseconds
void UpdateMobileDrawing(float turnFraction, float milliseconds);

/// The object jumped (EndPhysics, the hand, a script teleport): no slide from where it was
void SnapDrawPosition(entt::entity entity);

} // namespace openblack::ecs
