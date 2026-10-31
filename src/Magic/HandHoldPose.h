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

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// How the god hand holds what is in it, a miracle's seed among the rest. The hand takes a still frame of one of its
// holding poses, chosen by how the object is held, rises by an amount that depends on that too, and sways with it as
// the cursor moves. The held model hangs below the hand point, turned with the hand. Pure functions, tested on their
// own; the hand system applies them each frame (HandSystem.cpp, HandPlacement.cpp, HandHolding.cpp).

namespace openblack::magic::hand_hold
{

/// The hand's standard height, from which the poses and the lift are measured
inline constexpr float k_StandardHandHeight = 3.2f;
/// The height of the hand's model in its bind pose, which the hand's drawn size divides its standard height by: the Y
/// extent of Hand_Boned_Base2's bones in their bind pose, from 0, as the game works it out when it loads the hand (the
/// float 0x440AD2D7)
inline constexpr float k_HandModelHeight = 555.29437f;
/// An object held above the hand lifts it by this much
inline constexpr float k_AboveLift = 0.2f;
/// An object held at the side lifts the hand by at least this much
inline constexpr float k_MinimumSideLift = 1.9f;
/// An object held above opens the hand against a reach this many times the hand's height
inline constexpr float k_AboveReachShare = 1.2f;
/// The hand sways by up to this many radians...
inline constexpr float k_MaxSway = 0.3f;
/// ...when the cursor runs this many pixels ahead of where the hand has caught up to
inline constexpr float k_SwayLag = 80.0f;

/// How a seed is held: as a miracle not yet ready until it is ready, then as its record says
[[nodiscard]] HoldType HoldTypeOf(bool ready, HoldType recorded);

/// The time in the hold's animation (lasting some milliseconds) for an object of some reach (its hold radius), with
/// the hand at a size: held above, the hand closes as the reach grows; at the side it opens; a miracle not yet ready
/// takes the middle of its pose. In whole milliseconds, the fraction cut off towards zero, while the hand holds or
/// tugs alike.
[[nodiscard]] int32_t HoldTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize);
/// The same time while the hand pulls a thing free: a thing held above opens the hand by its reach against the
/// hand's own height alone; every other hold opens it as when holding. In whole milliseconds, cut towards zero.
[[nodiscard]] int32_t TugTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize);

/// How far below the hand point the held model hangs: its hold lowering times its height
[[nodiscard]] float SeedHang(HoldType hold, float lowering, float height);
/// How far the hand rises for what it holds: a little for an object held above, its own height times its size for a
/// miracle not yet ready, and the object's hang (at least a minimum) for the rest
[[nodiscard]] float HoldLift(HoldType hold, float hang, float handSize);

/// The hand's sway from how far the cursor runs ahead of the hand, in pixels: a roll about the line from the hand to
/// the camera (x), and a pitch (y)
[[nodiscard]] glm::vec2 CursorSway(glm::vec2 cursorLag);
/// The held object's up, and the hand's: straight up turned by the roll about the line from the hand to the camera,
/// then by the pitch about the level line across it. A camera on the hand turns about no axis.
[[nodiscard]] glm::vec3 HeldUp(glm::vec3 camera, glm::vec3 hand, float roll, float pitch);
/// The hand's drawn size that the frame of a held thing is multiplied by: the hand's standard height over the height of
/// its model, times the hand's scale, worked in that order
[[nodiscard]] float HeldFrameSize(float handScale);
/// The frame of a held thing, each part as the original works it out: the level way back towards the camera from the
/// hand's heading (half a turn on from it), the side across that and the up made unit unless it is zero, and the
/// forward across the up and the side, negated unless the hand is mirrored. Each axis is then multiplied by the hand's
/// drawn size, the side's by minus that for a mirrored hand, through a full product of the frame by the size, whose
/// zero terms can change the sign of a zero part.
[[nodiscard]] glm::mat3 HeldFrame(float heading, glm::vec3 up, float handSize, bool mirrored);
/// The hold's own turn of a held thing's frame about its up: the side and the forward turned by the angle, the side
/// becoming cos x side + sin x forward and the forward cos x forward - sin x side, each product rounded to a float, with
/// the cosine taken as a float and the sine kept wider, as the original works them out
[[nodiscard]] glm::mat3 HeldTurn(const glm::mat3& frame, float yRotate);
/// The axes a held thing is drawn with, from its frame (see HeldFrame): the hold's own turn about the up (HeldTurn), which
/// is a seed's own angle and none for anything else (its cosine 1 and sine 0 still add a zero to the side and the
/// forward), then each axis made unit unless it is zero, the squares of the side added z, x, y and of the other two
/// z, y, x
[[nodiscard]] glm::mat3 HeldAxes(const glm::mat3& frame, float yRotate = 0.0f);

/// Where the held model is drawn: its hang below the hand point, along the hand's up
[[nodiscard]] glm::vec3 HangBelow(glm::vec3 hand, glm::vec3 up, float hang);

/// While the hand is about to throw what it holds it follows where it should be on a spring, so it trails the cursor
/// when it moves fast and the throw takes the spring's speed. The spring is this stiff...
inline constexpr float k_SpringStiffness = 260.0f;
/// ...and this damped...
inline constexpr float k_SpringDamping = 40.0f;
/// ...stepped every this many milliseconds (as seconds below)...
inline constexpr uint32_t k_SpringStepMs = 10;
inline constexpr float k_SpringStepSeconds = 0.01f;
/// ...and the hand never moves faster than this, the fastest a throw leaves it, in metres a second
inline constexpr float k_MaxHandSpeed = 124.0f;

/// The hand on its spring
struct HoldingSpring
{
	glm::vec3 position {0.0f};
	glm::vec3 velocity {0.0f};
	/// How far the steps have got, in milliseconds
	uint32_t stepMs {0};
};
/// The spring's steps towards where the hand should be: at least one, then until the steps have caught up with the
/// clock (milliseconds of the frames it has been held in)
void StepHoldingSpring(HoldingSpring& spring, glm::vec3 required, uint32_t clockMs);

/// A held object's place and axes
struct HeldPlace
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
};
/// Where a throw leaves from: where the hand drew what it holds, when it has drawn it, else where the game turn put it
/// (which trails the hand while it moves)
[[nodiscard]] HeldPlace ReleasePlace(const HeldPlace& turnPlace, const std::optional<HeldPlace>& drawn);

} // namespace openblack::magic::hand_hold
