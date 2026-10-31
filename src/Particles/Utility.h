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

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Magic/Gestures/GestureShapes.h"

namespace openblack::magic::gestures
{
class GestureSystem;
struct Result;
} // namespace openblack::magic::gestures

// The utility effects the interface keeps while casting: the gesture trail
// (PARTICLE_TYPE_GESTURE_LOCAL 48, SF_GestureChain), the "recognised" sparkles (PARTICLE_TYPE_GESTURE 35, SF_Gesture)
// and the open selection (PARTICLE_TYPE_SPELL_SELECTION 28, SF_SpellSelection). Wiki:
// docs/wiki/magic/hand-casting.md#utility-effects.

namespace openblack::particles::utility
{
/// The utility effects, made once: the trail (48) and its "active", the recognised sparkles (35), SF_OnFire (37),
/// SF_LightningStrike (61), SF_ManaPathNew (22), SF_BeliefSprite (24), the selection (28) and its "active". (The
/// exploded meshes' SF_ExplodeObject is Particles/Rules/ExplodeObject.cpp's own.) Kept in the particle system's state
struct UtilityEffects
{
	uint32_t trail {0};
	bool trailActive {false};
	uint32_t recognised {0};
	uint32_t onFire {0};
	uint32_t lightningStrike {0};
	uint32_t manaPath {0};
	uint32_t belief {0};
	uint32_t selection {0};
	bool selectionActive {false};
};

/// When a gesture is recognised: the land points of the buffer and the gesture's ideal shape laid on the land over
/// the matched samples' box go into a record for UR_GesturingRecognised (the SF_Gesture effect, stepped once a turn
/// by ProcessTurn), which turns it into the sparkles and the light sheet, in the colour of the player whose interface it is
void GestureRecognised(const magic::gestures::GestureSystem& system, const magic::gestures::Result& result, PlayerNames player);

/// How far along a pixel's line a point of the shape goes where the screen shows no land under it
inline constexpr float k_ShapeOffTheLand = 400.0f;
/// The land along a pixel's line: the land or the near sea under it, else the point `length` along the line
using LandAlongPixel = std::function<glm::vec3(glm::ivec2 pixel, float length)>;
/// A gesture's shape on the screen
using ShapeLookup = std::function<const magic::gestures::Shape&(magic::gestures::Gesture gesture)>;

/// GestureRecognised's record before it is handed on: the buffer's land points, and the gesture's shape fitted to the
/// matched samples' pixel box and laid on the land along each point's pixel (k_ShapeOffTheLand off the land), squashed
/// up the screen by 0.75 at a time, at most 15 times, until its depth across `cameraForward` (on the ground) is no
/// more than twice its width, then resampled to as many points as the stroke. None with fewer than two samples, no
/// gesture, or fewer than two land points
[[nodiscard]] std::optional<magic::gestures::RecognisedGesture>
RecognisedTrail(const magic::gestures::GestureSystem& system, const magic::gestures::Result& result, const ShapeLookup& shapeOf,
                glm::vec3 cameraForward, const LandAlongPixel& landAlongPixel);

/// A gesture's shape shown on the land as if it had just been drawn and recognised, without anyone drawing it (a
/// reward chest showing the gesture of a power-up): its square, so many metres across, laid on the land from a corner
/// towards the east and south (magic::gestures::LandGestureTrail), for the same effect, in the colour of the player it
/// is shown for, as that player's interface's: this computer's (`ofThisInterface`, heard in 2D) or another's (heard at
/// the corner). Nothing for no gesture or a shape that isn't loaded
void ShowGestureOnLand(GestureType gesture, const glm::vec3& corner, float size, PlayerNames player, bool ofThisInterface);

/// The records waiting for UR_GesturingRecognised (it takes the newest one per step)
[[nodiscard]] std::vector<magic::gestures::RecognisedGesture>& PendingRecognised();

/// The trail's magnitude for the camera's distance from the hand, which the hand's scale multiplies: from 0.2 next to
/// the camera to 1 at 50 and on to 500, then up to 1.5 at 1500 and beyond, in straight lines between
[[nodiscard]] float TrailScale(float distance);

/// Every frame: the trail on while the game expects a gesture, following the hand with magnitude handScale x
/// f(camera distance); the selection effect while the selection is open. seconds = the frame's game time.
void Update(float seconds, const glm::vec3& handPosition, float handScale, float cameraDistance);

/// Once a turn, at the end of the game loop (after the exploded meshes' effect, which explode_object::GameLoopEnd
/// keeps): SF_OnFire, SF_ManaPathNew, SF_BeliefSprite, SF_Gesture (the recognised sparkles) and SF_LightningStrike, in
/// that order, each made when missing, stepped with an empty ProcessInfo (power 1, enabled) and the turn's ms, and
/// dropped when it ends
void ProcessTurn();

/// A land is loaded
void Reset();
} // namespace openblack::particles::utility
