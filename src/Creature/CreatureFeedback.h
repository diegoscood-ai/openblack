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

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFace.h"

/// How the player's hand rewards and punishes a creature. The hand can't pick a creature up: clicking one holds the
/// hand to it instead. Held still over its body, the hand strokes it, and the creature plays a pleased animation for
/// the part of the body stroked. Swept fast across it, the hand slaps it, high or low and hard or gently, and it reels.
/// Each stroke and slap adds to a running sum, which the creature's mind is told of as the hand lets go.
namespace openblack::creature_feedback
{
/// The parts of the body a stroke can land on, each nearest one of the body's bones
enum class BodyPart : uint8_t
{
	Head,
	RightArmpit,
	LeftArmpit,
	Belly,
	Groin,
	RightFoot,
	LeftFoot,
	RightHand,
	LeftHand,
};
constexpr size_t k_BodyPartCount = 9;

/// For each part stroked: the pleased animation, whether it plays mirrored (the left side's are the right's mirrored),
/// and the face pulled for two seconds: an aah for the head and belly, an ooh for the groin and a smile elsewhere
constexpr std::array<size_t, k_BodyPartCount> k_RewardAnimations {154, 155, 155, 157, 158, 159, 159, 161, 161};
constexpr std::array<bool, k_BodyPartCount> k_RewardMirrored {false, false, true, false, false, false, true, false, true};
constexpr std::array<creature_face::Face, k_BodyPartCount> k_RewardFaces {
    creature_face::Face::Aah,   creature_face::Face::Smile, creature_face::Face::Smile,
    creature_face::Face::Aah,   creature_face::Face::Ooh,   creature_face::Face::Smile,
    creature_face::Face::Smile, creature_face::Face::Smile, creature_face::Face::Smile,
};
constexpr float k_RewardFaceMs = 2000.0f;
/// The face pulled being stroked on a part
[[nodiscard]] constexpr creature_face::Request RewardFace(BodyPart part)
{
	return {.face = k_RewardFaces.at(static_cast<size_t>(part)),
	        .milliseconds = k_RewardFaceMs,
	        .cue = creature_face::Cue::Stroked};
}

/// Where each part a stroke can land on is, in part order; none for a part whose bone the pose does not have
using PartPlaces = std::array<std::optional<glm::vec3>, k_BodyPartCount>;
/// A part is the one stroked only nearer the touch than this, else the head is
constexpr float k_NearestPartWithin = 1000.0f;
/// The part nearest a point the hand touches, given where each part's bone is, in part order: the first of the nearest
/// strictly nearer than k_NearestPartWithin, each distance sqrt((x² + z²) + y²) in floats; the head when none is (no
/// part placed, or all as far or farther)
[[nodiscard]] BodyPart NearestPart(const glm::vec3& touch, const PartPlaces& parts);
/// Where each part is: the place of the bone that stands for it (`bones`, in part order) in a pose of the body in the
/// world, none where the pose lacks the bone
[[nodiscard]] PartPlaces PlacesOfParts(std::span<const uint32_t, k_BodyPartCount> bones, std::span<const glm::mat4> pose);

/// The game weighs the hand's strokes and slaps once a frame, each step taking that frame's milliseconds. A fix kept
/// beside it, unused: steps at most this many times a second, each taking all the time since the last
constexpr float k_StepsPerSecond = 60.0f;
/// The milliseconds the next paced step runs on after a frame of some milliseconds, the time left over kept in
/// `sinceStep`; nothing until a step's worth has gone by
[[nodiscard]] std::optional<float> Pace(float& sinceStep, float frameMs);

/// The clocks a stroke waits on, in milliseconds
struct StrokeClocks
{
	/// Since the last stroke or slap played, or since taking hold
	float sinceTouch {0.0f};
	/// Starts at 0 on taking hold and is set to k_ReadyAfterTouchMs after each stroke or slap that plays: a stroke needs
	/// it at k_ReadyForStrokeMs, so the first comes two seconds after taking hold and the next a second after the last
	float ready {0.0f};
	/// How long the hand has been on the body, in all, since taking hold or the last slap
	float contact {0.0f};
};
constexpr float k_ReadyForStrokeMs = 2000.0f;
constexpr float k_ReadyAfterTouchMs = 1000.0f;
/// The hand has to have been on the body this long, in all, before it strokes, and the cursor must be moving across
constexpr float k_StrokeHoldMs = 1000.0f;
/// After a stroke or slap the next stroke waits this long at no reward, twice that at the most punishment, nothing at the
/// most reward
constexpr float k_StrokeWaitMs = 1250.0f;
[[nodiscard]] float StrokeWaitMs(float sum);
/// Whether a stroke is due on a part: another part than the last stroked, the clocks run long enough for the reward so far
[[nodiscard]] bool StrokeDue(const StrokeClocks& clocks, std::optional<BodyPart> last, BodyPart part, float sum);
/// A stroke interrupts nothing that is no further through than this; a slap nothing no further than this. The stroke's
/// is a double and the slap's a float, as the original keeps them
constexpr double k_StrokeInterruptsAfter = 0.8;
constexpr double k_SlapInterruptsAfter = static_cast<double>(0.35f);
/// A slap needs this long since taking hold or the last stroke (slaps themselves don't hold the next one back)
constexpr float k_SlapReadyMs = 1000.0f;

/// The held creature looks at the eye while the hand is within this share of its height of its body's bottom, else at
/// the hand moved back this share of its height along the line of sight
constexpr float k_LookAtHandBeyond = 0.6f;
constexpr float k_LookBackFromHand = 0.3f;
/// Where the creature the hand holds looks, each frame of the hand on it: at the eye while the hand is no further than
/// k_LookAtHandBeyond of its height from the body's bottom (the distance's squares summed x, z, then y), else at the
/// hand moved back k_LookBackFromHand of its height along the line of sight (each part the share's product with the
/// sight's, then the hand's added)
[[nodiscard]] glm::vec3 HeldLookPoint(glm::vec3 eye, glm::vec3 sight, glm::vec3 hand, glm::vec3 bottom, float height);

/// Missing the body, the hand looks at it once more through the pixel halfway between the cursor's and the last frame's:
/// each coordinate's sum halved in whole pixels, rounded down
[[nodiscard]] constexpr glm::ivec2 SecondLookPixel(glm::ivec2 cursor, glm::ivec2 lastCursor)
{
	return {(cursor.x + lastCursor.x) >> 1, (cursor.y + lastCursor.y) >> 1};
}
/// Whether a slap may count, from how long it has been since taking hold or the last stroke: more than a second
[[nodiscard]] bool SlapReady(float sinceHoldOrStrokeMs);

/// The hand's sweep is weighed as a slap only as it first comes onto the body, or as the cursor crosses the middle of the
/// screen, on the body or not
[[nodiscard]] bool SlapConsidered(bool onBody, bool wasOnBody, float cursorX, float lastCursorX, float screenMiddle);

/// Whether a stroke or a slap may break into the body's action, `timeMs` into an animation `durationMs` long: only once
/// it is further through than `interruptsAfter`. The share is the whole milliseconds over the length as a float, and it
/// is compared with the share as it is given
[[nodiscard]] bool MayInterrupt(float timeMs, float durationMs, double interruptsAfter);

/// The slapping animations, from the head to the feet, and their gentle versions, all to the right side
constexpr size_t k_SlapHead = 165;
constexpr size_t k_SlapWaist = 167;
constexpr size_t k_SlapFeet = 169;
constexpr size_t k_GentleOffset = 6;
/// Heights up the creature, as shares of its height, below which a slap is at the feet, then at the waist; above that
/// at the head, and nothing at or above the last
constexpr float k_FeetBelow = 0.4f;
constexpr float k_WaistBelow = 0.7f;
constexpr float k_SlapAbove = 1.1f;
/// The hand slaps when it moves faster than this many of the creature's heights a second, gently below the second
constexpr float k_SlapSpeed = 5.0f;
constexpr float k_HardSlapSpeed = 9.0f;

enum class SlapBand : uint8_t
{
	Feet,
	Waist,
	Head,
};

struct Slap
{
	size_t animation;
	SlapBand band;
	bool gentle;
	/// Played to the left side, when the hand swept to the left across the screen
	bool mirrored;
};

/// The slap a hand moving across the body makes, if any: by its height above the ground and its speed across the land,
/// against the creature's height, and which way it moves across the screen
[[nodiscard]] std::optional<Slap> ClassifySlap(float handHeight, float speed, float creatureHeight, bool sweepsLeft);
/// How fast the hand sweeps across the land: the cursor's point this frame against its point last frame, both at the
/// hand's depth, up and down not counting, over the frame's seconds. Each way across is times the inverse of the
/// seconds before they are squared and summed, as the game sums them
[[nodiscard]] float SweepSpeed(glm::vec3 now, glm::vec3 last, float seconds);

/// Hard slaps are counted in a row at one height. A slap at another height starts the count afresh.
struct HardSlaps
{
	SlapBand band {SlapBand::Feet};
	uint32_t count {0};
};
constexpr uint32_t k_HardSlapsToBruise = 3;
/// Counts a slap; whether it bruises: a hard slap that lands on the body once there have been three or more in a row at
/// one height, the count starting again after each bruise. Those that miss the body still count
[[nodiscard]] bool CountSlap(HardSlaps& slaps, const Slap& slap, bool onBody);
/// The bruise's column in the skin's marks: 7 for a hard slap just past the hard speed, less the faster, down to 0
[[nodiscard]] uint8_t BruiseColumn(float speed, float creatureHeight);
/// A bruise takes this much of the creature's life, never leaving less
constexpr float k_BruiseLife = 0.01f;
[[nodiscard]] float LifeAfterBruise(float life);

/// The hand's cycles while it is held to a creature
enum class TouchCycle : uint8_t
{
	/// Beside the body
	Wiggle,
	/// The caress, which taking hold starts with and stroking plays
	Tickle,
	Slap,
};
/// A step's cycle, and how many times the step changed it to the slap or the caress: each of those changes starts the
/// hand's blend afresh, while turning back to the wiggle does not
struct CycleStep
{
	TouchCycle cycle;
	uint8_t blendStarts;
};
/// The cycle after a step: a fast hand slaps, stroking caresses, and off the body a caress turns back to the wiggle; a
/// slapping hand keeps slapping until it strokes again
[[nodiscard]] CycleStep NextCycle(TouchCycle cycle, bool fast, bool stroking, bool onBody);

/// What each stroke and slap adds to the running sum of the player's feedback, which stays from -1 to 1
constexpr float k_StrokeAmount = 0.1f;
constexpr float k_GentleSlapAmount = 0.1f;
constexpr float k_HardSlapAmount = 0.2f;
/// Slapping a creature that was enjoying being stroked, the sum past this, takes twice as much off
constexpr float k_EnjoyingSum = 0.25f;

[[nodiscard]] float AfterStroke(float sum);
[[nodiscard]] float AfterSlap(float sum, bool gentle);
/// What the mind is told when the hand lets go
[[nodiscard]] float Delivered(float sum);
/// Feedback this slight only makes the creature look at the player
constexpr float k_SlightFeedback = 0.01f;
/// Whether feedback is that slight, either way
[[nodiscard]] bool IsSlightFeedback(float feedback);

/// How the creature's attitude to the player follows feedback: mostly what it was, nudged by what it got
[[nodiscard]] float AttitudeAfter(float attitude, float feedback);
/// The average of the feedback it has had, the newest counting most
[[nodiscard]] float AverageAfter(float average, float feedback);

/// A piece of the body the hand can touch: a capsule round the line from one bone to the next
struct Capsule
{
	glm::vec3 from;
	glm::vec3 to;
	float radius;
};
/// A creature of size 1 is about this many units tall
constexpr float k_HeightAtSizeOne = 15.0f;
/// The body is taken as capsules round its bones this thick, as shares of its height
constexpr float k_BodyRadiusShare = 0.12f;
/// The body as posed: a capsule from each bone to its parent, the bones' matrices in the mesh's space placed in the world
[[nodiscard]] std::vector<Capsule> BodyCapsules(std::span<const uint32_t> parents, std::span<const glm::mat4> boneMatrices,
                                                const glm::mat4& placement, float radius);
/// How far a point is outside the body, or less than 0 inside it
[[nodiscard]] float DistanceOutside(const glm::vec3& point, std::span<const Capsule> capsules);
/// How far along a ray (direction a unit vector) it first meets the body, 0 when it starts inside; none when it meets
/// none within the length
[[nodiscard]] std::optional<float> EntryAlong(const glm::vec3& origin, const glm::vec3& direction,
                                              std::span<const Capsule> capsules, float length);
/// The way out of the body at a point of it: from the axis of the capsule the point is nearest the surface of to the
/// point, not made a unit vector; none without capsules
[[nodiscard]] std::optional<glm::vec3> OutwardAt(const glm::vec3& point, std::span<const Capsule> capsules);
/// Where a line of sight first touches the body, as how many of the direction's lengths along it, if it does
[[nodiscard]] std::optional<float> RayHit(const glm::vec3& origin, const glm::vec3& direction,
                                          std::span<const Capsule> capsules);
} // namespace openblack::creature_feedback
