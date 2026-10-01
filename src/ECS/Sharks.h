/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs
{

/// The sharks (class Whale, Whale.cpp; docs/bw1-notes/rendering.md "Tiburones"). Created by the CHL CREATE of type
/// Whale (archetypes::SharkArchetype), moved by the script's WALK_PATH (not ported: the camera tracks Track%d are not
/// read yet), drawn in two parts cut by the water, with a wake of rings.

/// fn_00775140 from GGame::ProcessTurn 0x54E5C7, every turn: Whale::Process 0x775280 (turnStart = Pos). Then the
/// WALK_PATH list moves them (GlobalGameLists::Process 0x5913ED, MobileObject::MoveAlongPath 0x607790).
void ProcessSharksTurn();

/// The frame part of fn_00774E30 (called for the whole list from GLandscape::Draw 0x5E4B26), after the animations: the
/// heading of the turn's move, the drawn position between the turn's start and end by `turnFraction`
/// (g_game+0x205D64), and the wake (fn_00775170: a ring every 50 ms of the global timer 0xDCB984 at the EBone[0] point).
/// The clip time advances in UpdateAnimations; the two cut draws are Renderer::DrawCutBelowWater / DrawCutAboveWater.
void UpdateSharks(float turnFraction, float gameMilliseconds);

/// OPENBLACK_TEST_SHARK: the shark part of Land 1's FollowUs, once the landscape exists: two sharks created at
/// CONVERT_CAMERA_FOCUS(221) and (230) walking the camera tracks 21 and 20 forward from 0 to 1, as the script does.
/// "track,camera[,forward[,from[,to]]]" makes a single one on that track at that camera's focus instead.
void RunSharkDebugHook();

} // namespace openblack::ecs
