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

#include "Enums.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Particles/ManaPathMaths.h"

namespace openblack::magic::gestures
{
class GestureSystem;
struct Result;
} // namespace openblack::magic::gestures

// The utility effects the interface keeps while casting: the gesture trail
// (PARTICLE_TYPE_GESTURE_LOCAL 48), the "recognised" sparkles (PARTICLE_TYPE_GESTURE 35)
// and the open selection (PARTICLE_TYPE_SPELL_SELECTION 28). Wiki:
// docs/wiki/magic/hand-casting.md#utility-effects.

namespace openblack::particles::utility
{
/// The utility effects, made once: the trail (48) and its "active", the recognised sparkles (35), the burning (37),
/// the lightning strike (61), the mana path (22), the belief sprite (24), the selection (28) and its "active". (The
/// exploded meshes' effect is Particles/Rules/ExplodeObject.cpp's own.) Kept in the particle system's state
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
	/// The multi pick-up's pieces (SF_MultiPickUpFood, ...FoodPoisoned, ...FoodFish, ...Wood), 0 while none
	uint32_t pickup {0};
};

/// When a gesture is recognised: the land points of the buffer and the gesture's ideal shape laid on the land over
/// the matched samples' box go into a record for GesturingRecognised (the recognised sparkles, stepped once a turn
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

/// The records waiting for GesturingRecognised (it takes the newest one per step)
[[nodiscard]] std::vector<magic::gestures::RecognisedGesture>& PendingRecognised();

/// The trail's magnitude for the camera's distance from the hand, which the hand's scale multiplies: from 0.2 next to
/// the camera to 1 at 50 and on to 500, then up to 1.5 at 1500 and beyond, in straight lines between
[[nodiscard]] float TrailScale(float distance);

/// Every frame: the trail on while the game expects a gesture, following the hand with magnitude handScale x
/// f(camera distance); the selection effect while the selection is open. seconds = the frame's game time.
void Update(float seconds, const glm::vec3& handPosition, float handScale, float cameraDistance);

/// Once a turn, at the end of the game loop (after the exploded meshes' effect, which explode_object::GameLoopEnd
/// keeps): the burning, the mana path, the belief sprite, the recognised sparkles and the lightning strike effects, in
/// that order, each made when missing, stepped with an empty ProcessInfo (power 1, enabled) and the turn's ms, and
/// dropped when it ends
void ProcessTurn();

/// A spark of the hand's mana path past the border, given to the mana path (made when missing, as the turn's step makes
/// it), whose ManaPathNew lets it out at its next step
void AddHandManaPathSpark(const mana_path::Spark& spark);

/// The hand starts taking a pile, a field or a fish farm in batches: the pieces' effect of that kind, the old one
/// killed first, made with no player on the local stream (the original's CreatePickupPSys)
void StartPickup(ParticleType type);
/// The batch ends: the pieces' effect goes with its atoms
void StopPickup();
/// The frame's step of the pieces' effect, while it lives: the hand as the gesture position, power 1, enabled, the
/// frame's game ms (at least 1 ms); it is killed past 60 seconds of age or once it ends
void StepPickup(const glm::vec3& handPosition);

/// A land is loaded
void Reset();
} // namespace openblack::particles::utility
