/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <bit>
#include <functional>
#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How a creature lets go of what it holds: thrown at something, tossed away, lobbed or put down. What is thrown then
/// flies as any thrown object does, through the physics objects.
namespace openblack::creature_throw
{
/// The pull of gravity on what is thrown, in units a second each second
constexpr float k_Gravity = 9.81f;

/// The throwing animations, flat and high, blended by how high the target is; and those that let go of what is held:
/// tossing it away, eating it, putting it down and lobbing it gently
constexpr size_t k_HurlFlat = 91;
constexpr size_t k_HurlHigh = 92;
constexpr size_t k_Discard = 95;
constexpr size_t k_Eat = 96;
constexpr size_t k_PutDown = 97;
constexpr size_t k_GentleLob = 98;

/// Tossing something away, it leaves with this share of the hand's own speed at that moment
constexpr float k_DiscardSpeedShare = 0.6f;
/// The hand's speed is measured over this many milliseconds of the animation before it lets go
constexpr float k_HandSpeedSpanMs = 100.0f;
/// A creature throws nothing at anything nearer than this share of its height, 15 units at size 1
constexpr float k_MinThrowHeightShare = 0.66f;
constexpr float k_HeightAtSizeOne = 15.0f;

/// What a throw's flight time squared is for each unit of distance along the ground: one over gravity worked out in
/// single precision, which leaves it one step under the exact value
constexpr float k_FlightSecondsSquaredPerDistance = std::bit_cast<float>(uint32_t {0x3DD0C43D});

/// How far apart two points are along the ground, leaving their heights out
[[nodiscard]] float GroundDistance(const glm::vec3& from, const glm::vec3& to);

/// How long something a creature throws takes to reach a target a distance away along the ground: the square root of
/// the distance times k_FlightSecondsSquaredPerDistance, each step in single precision
[[nodiscard]] float ThrowFlightTime(float groundDistance);

/// The velocity that takes something from where the hand lets go of it to the target in a time, falling as it goes:
/// the way there less the fall of half the time squared times gravity, times one over the time, each step in single
/// precision
[[nodiscard]] glm::vec3 ThrowVelocity(const glm::vec3& target, const glm::vec3& release, float seconds);

/// Where a creature throws at a thing, and how long the throw is to take
struct ThrowAim
{
	glm::vec3 point {0.0f};
	float seconds {0.0f};
};

/// Aiming at a thing that may be moving, from where the creature stands: the time to where the thing is now, where it
/// will be after that time, the time to there, and then where it will be after that second time. `whereAfter` gives
/// where the thing will be some seconds from now.
[[nodiscard]] ThrowAim AimAhead(const glm::vec3& creature, const glm::vec3& thing,
                                const std::function<glm::vec3(float)>& whereAfter);

/// Where a thing that stays where it is is aimed at: its place, half its height above the ground there
[[nodiscard]] glm::vec3 MiddleOf(const glm::vec3& place, float ground, float height);

/// Aiming at a thing that stays where it is, from where the creature stands: at its middle, in the time to there
[[nodiscard]] ThrowAim AimAtStillThing(const glm::vec3& creature, const glm::vec3& place, float ground, float height);

/// Where a villager will be some seconds from now, as a creature's throw leads it: its middle moved on along the ground
/// by its step (whole map units a turn) times the whole turns in the time, each step in single precision. One whose
/// top state is a final one is aimed at where it is.
[[nodiscard]] glm::vec3 VillagerAfter(const glm::vec3& middle, glm::ivec2 step, bool topStateFinal, uint32_t turnsPerSecond,
                                      float seconds);

/// Where a creature will be some seconds from now, as another creature's throw aims at it: its body's place moved on
/// along its heading by its speed (units a second) times the time, at its height above the ground there. The ground
/// is asked for at that point truncated to whole map units, the way the game keeps a place; every step is the game's
/// float step, the sine and cosine at the FPU's own precision.
[[nodiscard]] glm::vec3 CreatureAfter(const glm::vec3& body, float heading, float speed, float height, float seconds,
                                      const std::function<float(glm::vec2)>& groundAt);

/// Whether a target is far enough along the ground to throw at, for a creature of a size
[[nodiscard]] bool FarEnoughToThrow(float groundDistance, float size);

/// How much of the high throw to blend into the flat one, 0 to 1. Each slope is height over distance ahead: the
/// target's, and the hand's where it lets go in the flat and in the high throw.
[[nodiscard]] float HighThrowWeight(float targetSlope, float flatSlope, float highSlope);

/// The hand's velocity in units a second, from where it is when it lets go and where it was a span of milliseconds
/// before
[[nodiscard]] glm::vec3 HandVelocity(const glm::vec3& atRelease, const glm::vec3& before, float spanMs);

/// The velocity something tossed away leaves with: a share of the hand's own, measured in the creature's space, flipped
/// to the other side when tossed by the other hand and turned with the creature
[[nodiscard]] glm::vec3 TossVelocity(const glm::vec3& handVelocity, bool mirrored, const glm::mat3& rotation, float share);

/// Going to throw into something, a creature stands no nearer to it than the distance it was given, twice its height
/// and the thing's radius together, and no further than three and a half times its height
constexpr float k_ThrowStandHeights = 2.0f;
constexpr float k_ThrowReachHeights = 3.5f;
/// Backing away to throw, it goes to within twice its height of the point it backs away to
constexpr float k_ThrowBackOffHeights = 2.0f;
/// Walking up to throw where it may go no nearer than it may stand off, it may stand off this much more
constexpr float k_ThrowWalkUpSpare = 0.01f;

/// What a creature does to get where it can throw into something from: nothing when it is there already, backing away
/// to a point, or walking up to the thing until within a ring about it
struct ThrowStand
{
	enum class Kind : uint8_t
	{
		There,
		BackOff,
		WalkUp,
	};
	Kind kind {Kind::There};
	glm::vec2 point {0.0f};
	float minDistance {0.0f};
	float maxDistance {0.0f};
};
/// Where a creature of a height goes to throw into a thing of a radius, keeping the distance it was given; none when it
/// stands right on the thing's centre, with no way to back away
[[nodiscard]] std::optional<ThrowStand> WhereToThrowFrom(glm::vec2 creature, glm::vec2 target, float height, float targetRadius,
                                                         float keep);

} // namespace openblack::creature_throw
