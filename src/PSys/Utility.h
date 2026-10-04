/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "Magic/Gestures/GestureShapes.h"

namespace openblack::magic::gestures
{
class GestureSystem;
struct Result;
} // namespace openblack::magic::gestures

// PSysUtilityPSys (the singleton at 0xD4E0E8): the effects the interface keeps while casting: the gesture trail
// (PARTICLE_TYPE_GESTURE_LOCAL 48, SF_GestureChain), the "recognised" sparkles (PARTICLE_TYPE_GESTURE 35, SF_Gesture)
// and the open selection (PARTICLE_TYPE_SPELL_SELECTION 28, SF_SpellSelection). Wiki: docs/bw1-notes/magic.md.

namespace openblack::psys::utility
{
/// fn_00689790 (misnamed GJMesh::GetBoundingBox), from GInterface::Success(1): the land points of the buffer and the
/// gesture's ideal shape laid on the land over the matched samples' box go into a record for UR_GesturingRecognised
/// (the SF_Gesture singleton, stepped once a turn by ProcessTurn), which turns it into the sparkles
void GestureRecognised(const magic::gestures::GestureSystem& system, const magic::gestures::Result& result);

/// The records waiting for UR_GesturingRecognised (the list 0xD4EB10 / count 0xD4EB18; it takes the newest one per step)
[[nodiscard]] std::vector<magic::gestures::RecognisedGesture>& PendingRecognised();

/// fn_00671DA0 (misnamed GInterface::InterfaceActionHandObjectApplyToPos), every frame: the trail on while the game
/// expects a gesture, following the hand with magnitude handScale x f(camera distance); the selection effect while the
/// selection is open. seconds = the frame's game time.
void Update(float seconds, const glm::vec3& handPosition, float handScale, float cameraDistance);

/// fn_006721B0 (PSysGlobal::GameLoopEnd 0x68F5C9, after the exploded meshes' slot +0x10, which
/// explode_object::GameLoopEnd keeps), once a turn: the slots +0x0C SF_OnFire, +0x18 SF_ManaPathNew, +0x1C
/// SF_BeliefSprite, +0x08 SF_Gesture (the recognised sparkles) and +0x14 SF_LightningStrike, in that order, each made
/// when missing (PSysInterface::Create(NULL, type, 0, 0, 1.0, NET)), stepped with an empty ProcessInfo (power 1,
/// enabled) and the turn's ms ([0xD01A38], vt 0xFC Process_), and dropped when it returns 5
void ProcessTurn();

/// A land is loaded
void Reset();
} // namespace openblack::psys::utility
