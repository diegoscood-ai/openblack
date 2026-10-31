/*******************************************************************************
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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandMorph.h"
#include "Enums.h"

/// Where the player's hand is placed each frame and how it is posed there: the cursor's spring-damped copy and the lean
/// it gives, when the hand's up takes a new target, the normal state, the distance the hand aims at, the holding
/// spring, how high it holds what it holds and the still pose of each hold, and where the held thing is kept each turn.
///
/// Pure functions, tested on their own; the hand's system gathers the facts and applies them.
namespace openblack::hand_place
{

/// The cursor's spring-damped copy trails the cursor by at most this many pixels on each axis
inline constexpr float k_SmoothMouseLag = 80.0f;
/// The copy's spring stiffness, and what is left of its velocity after a second
inline constexpr float k_SmoothMouseStiffness = 20.0f;
inline constexpr float k_SmoothMouseDamping = 0.03f;

/// The cursor's spring-damped copy, in the game's pixels
struct SmoothMouse
{
	glm::vec2 position {0.0f};
	glm::vec2 velocity {0.0f};
};
/// A frame of the cursor's copy some seconds long: it moves by its velocity, kept within the lag of the cursor on each
/// axis, then its velocity is pulled towards the cursor by the stiffness and damped
[[nodiscard]] SmoothMouse StepSmoothMouse(SmoothMouse mouse, glm::vec2 cursor, float seconds);

/// The gaps the hand leans by, in pixels, each kept within -80..80, or nothing where it does not lean that way
struct Lean
{
	std::optional<float> sideways;
	std::optional<float> forwards;
};
/// How far the hand leans from the gap between the cursor and its copy: sideways by the copy's lag behind the cursor
/// (the other way round while gripping the land), back and forth by the cursor's lag behind the copy up the screen,
/// each gap kept within the lag. A sideways gap of a ten-thousandth or less leans nothing, and so does any gap up the
/// screen while the copy's own height is that small
[[nodiscard]] Lean LeanMotion(glm::vec2 cursor, glm::vec2 smooth, bool gripping);

/// The hand's up takes a new target only when the cursor's x has changed since it last took one
[[nodiscard]] bool TakesNewUpTarget(float cursorX, float lastCursorX);

/// The hand is in its normal state when it holds nothing and pulls nothing (or holds it no way) and is not working a
/// totem
[[nodiscard]] bool IsNormalState(bool holdingOrTugging, HoldType hold, bool workingTotem);

/// The action collide's point: the position of the object the hand collided with, which over a spell icon's seed graphic
/// is the icon's own, else the land under the cursor
[[nodiscard]] std::optional<glm::vec3> CollidePointOf(std::optional<glm::vec3> collided, std::optional<glm::vec3> land);

/// Over a worship icon the hand aims this far short of the icon, whatever the hand's size
inline constexpr float k_IconDepthOffset = 3.2f;
/// The distance from the eye the hand eases towards: a screen object's depth when one is under the hand, else the
/// distance of the point it aims at less how far short of it it stops; never less than 1
[[nodiscard]] float HandDistanceTarget(bool screenObject, float screenObjectDepth, float pointDistance, float depthOffset);

/// Whether the hand is in its holding state, where its spring works: it holds a thing it is not pulling out of the
/// ground, holds it some way, and it is not a miracle's seed (which follows the hand directly)
[[nodiscard]] bool SpringHoldingState(bool holding, bool tugging, HoldType hold, bool seed);
/// What the holding spring does in a frame
enum class SpringFrame : uint8_t
{
	/// It is on and steps the hand towards where it should be
	Step,
	/// The hand is put where it should be; the spring stays as it is (off, or on with nowhere to step from)
	Follow,
	/// The hand is put where it should be and the spring takes hold there, with no speed
	Start,
};
/// A frame of the spring: it steps while it is on in the holding state and the hand has been placed before; else the
/// hand is placed where it should be, and the spring starts once the hand holds a thing and is ready to throw it (the
/// second press). Out of the holding state the spring is off
[[nodiscard]] SpringFrame SpringFrameOf(bool holdingState, bool active, bool placedBefore, bool readyToThrow);

/// How high the hand holds what it holds above the grip: the hold's lift, and a tenth of the thing's height more for a
/// rooted thing (a standing tree)
inline constexpr float k_RootedLiftShare = 0.1f;
[[nodiscard]] float HoldHeight(float lift, bool rooted, float height);

/// The still pose the hand takes as it holds a thing
enum class HoldClip : uint8_t
{
	/// No still pose: the hand's own animation plays
	None,
	/// The magic grip's wiggle, for a miracle's seed not yet ready
	Wiggle,
	/// On the palm
	Above,
	/// From the side, for the side, tree and villager grips (and the magic grip without its wiggle)
	Side,
};
/// Which of the hold poses the hand has
struct HoldClips
{
	bool wiggle {false};
	bool above {false};
	bool side {false};
};
/// The still pose for a hold, among those the hand has: none unless it holds something
[[nodiscard]] HoldClip HoldClipOf(bool holding, HoldType hold, HoldClips has);

/// Where the held thing is kept each turn, at the hand's turn position: x and z through the map's fixed-point
/// coordinates, and its height above the land there kept as a float (the land looked up at the hand's own position),
/// added to the land's height at the kept x and z. Without a land the height is the hand's
[[nodiscard]] glm::vec3 HeldTurnPlace(glm::vec3 hand, const land_morph::Ground* ground);

} // namespace openblack::hand_place
