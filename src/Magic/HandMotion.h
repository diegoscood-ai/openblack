/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// How the hand moves with a miracle in it: its movement smoothed for a throw, and the pour that lifts and tips it while
// food, wood or water pours from it. Pure functions, tested on their own; the hand system keeps their state (the grain
// state, HandHolding.cpp and HandGrain.cpp). Wiki: docs/bw1-notes/miracles.md, "Food and wood".

namespace openblack::magic
{

/// The hand's movement is smoothed so that it gets this share of the way to how the hand really moves...
inline constexpr float k_HandVelocityShare = 0.8f;
/// ...in this many seconds
inline constexpr float k_HandVelocitySeconds = 0.1f;

/// The hand's smoothed movement, a step of some seconds after the last, from how fast it really moved over the step.
/// The rate is worked out in double precision and rounded once; its exponential is the game's own single precision one
[[nodiscard]] glm::vec3 FilterHandVelocity(glm::vec3 smoothed, glm::vec3 raw, float seconds);

/// The spin a hand holding a miracle gives it as it lets go: how fast the way the hand moves across the land turns, in
/// radians a second (its sideways acceleration over its smoothed speed), negative turning from x towards -z. It is
/// measured frame by frame while the miracle is held, from nothing when it comes to the hand.
struct HandSpin
{
	/// How far the hand moved over the last frame
	glm::vec3 lastStep {0.0f};
	/// The hand's sideways acceleration across the land, smoothed
	float lateral {0.0f};
	float spin {0.0f};
};
/// The sideways acceleration is smoothed so that it gets this share of the way in a tenth of a second
inline constexpr float k_HandLateralShare = 0.2f;
/// At or below this smoothed speed the hand is taken as still
inline constexpr float k_HandStill = 1e-4f;
/// A frame of some seconds on, in which the hand moved by a step, with the hand's smoothed velocity after it
void StepHandSpin(HandSpin& spin, glm::vec3 step, glm::vec3 smoothedVelocity, float seconds);

/// A curve through points, smooth everywhere (a cubic spline). At each end it either leaves at a given slope or, with
/// none given (or one above 0.99e30), doesn't bend there (a natural end). The points' x must rise. Beyond the ends the
/// end pieces carry on.
class CubicSpline
{
public:
	explicit CubicSpline(std::span<const glm::vec2> points, std::optional<float> startSlope = std::nullopt,
	                     std::optional<float> endSlope = std::nullopt);
	/// Its value at x; x itself when the two points around it share their x
	[[nodiscard]] float operator()(float x) const;

private:
	std::vector<glm::vec2> _points;
	/// The curve's second derivative at each point
	std::vector<float> _bends;
};

/// The pour's shape over its time: through these points, natural at both ends, so that it rises from rest, swells to
/// about 1.61 times the held points' height in the middle, and comes back to rest
inline constexpr std::array<glm::vec2, 4> k_PourKeyPoints {glm::vec2 {0.0f, 0.0f}, glm::vec2 {0.2f, 1.0f},
                                                           glm::vec2 {0.8f, 1.0f}, glm::vec2 {1.0f, 0.0f}};

/// How a pour moves the hand. The miracle's sprinkle gives these from its own file.
struct PourSettings
{
	/// Seconds for one rise and fall
	float totalTime {1.0f};
	/// How far up the hand rises, and how far it tips in radians, where the curve is 1
	float heightToRaise {0.0f};
	float angleToRaise {0.0f};
	/// Starts over once done, while the miracle pours, or stops
	bool loops {false};
	/// The hand stays where it was when the pour began, rising and tipping there, so that all that pours lands in one
	/// place
	bool clampHand {false};
};

/// How far a pour has lifted and tipped the hand
struct PourPose
{
	float raise {0.0f};
	float tilt {0.0f};
	/// Where the hand stays while the pour holds it, before it rises
	std::optional<glm::vec3> pinned;
};

/// A pour of the hand in progress, stepped each game turn and drawn between turns
struct PourState
{
	bool active {false};
	PourSettings settings;
	/// How far through one rise and fall, 0 to 1
	float progress {0.0f};
	PourPose previous;
	PourPose current;
	/// Where the hand was when the last pour that knew it began, which it stays at while the pour clamps it
	glm::vec3 pinned {0.0f};
};

/// Starts the pour from the pose the hand is in, the place it stays at kept from before
void StartPour(PourState& pour, const PourSettings& settings);
/// Starts the pour with the hand where it is, which it stays at when the pour clamps it
void StartPour(PourState& pour, const PourSettings& settings, const glm::vec3& hand);
/// The pour stops: the hand is let go and its pose comes back to rest over the next game turn
void StopPour(PourState& pour);
/// A game turn of some seconds
void StepPour(PourState& pour, float seconds);
/// Where the pour holds the hand, none when it doesn't clamp it
[[nodiscard]] std::optional<glm::vec3> PinnedHand(const PourState& pour);
/// The pose to draw, a fraction of the way from the last turn to the next
[[nodiscard]] PourPose PourPoseAt(const PourState& pour, float fraction);

} // namespace openblack::magic
