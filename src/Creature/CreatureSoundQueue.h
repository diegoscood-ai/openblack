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
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/AnimEffectKeys.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureLayers.h"

/// A creature's sounds as the game times them. Its animations are advanced once a creature turn, in whole
/// milliseconds; each sound moment an animation passes in that stretch is queued with how far through the stretch it
/// lies. Each frame drawn between that turn and the next plays the queued sounds whose moment the drawing has reached,
/// and the next turn first plays whatever is left.
namespace openblack::creature_audio
{
/// A sound waiting in a creature's queue
struct QueuedSound
{
	/// How far through the turn's stretch of its animation the moment lies, 0 to 1
	float fraction;
	audio::SoundAction action;
	/// 0 plays the sound, 1 stops it and any other value lets its loop run out (see ActionOf)
	int32_t mode;
	/// From the bank all creatures share, otherwise from the species' own
	bool generic;

	constexpr bool operator==(const QueuedSound&) const noexcept = default;
};

/// The sounds a creature's animations passed, kept in order of fraction. A sound joins the queue before any already
/// there at the same fraction, and is dropped while the queue holds k_MaxQueued.
class TurnQueue
{
public:
	void Add(const QueuedSound& sound);
	/// Takes out, in order, the sounds whose fraction is below the turn fraction being drawn (taken as at most 2)
	[[nodiscard]] std::vector<QueuedSound> TakeDrawn(float turnFraction);
	/// Takes out every sound, in order
	[[nodiscard]] std::vector<QueuedSound> TakeAll();
	[[nodiscard]] std::span<const QueuedSound> Pending() const noexcept { return _sounds; }

private:
	std::vector<QueuedSound> _sounds;
};

/// An animation's sound moments, in the order its file lists them, and its length in milliseconds
struct AnimationMoments
{
	std::span<const SoundEvent> events;
	int32_t durationMs;
};

/// Shows (true) or hides a tuft of hair, by its group's index
using HairChange = std::function<void(int32_t group, bool shown)>;

/// The moments an animation passes playing from oldMs up to newMs: each one at or after oldMs and before newMs is
/// queued, a sound with its fraction (time - oldMs) / (newMs - oldMs), a hair group shown when its mode is 0 and hidden
/// otherwise. The moments are visited from the last listed to the first, so those at one time are queued in the order
/// listed. Nothing is passed when the time stands still or goes back.
void CheckMoments(std::span<const SoundEvent> events, int32_t oldMs, int32_t newMs, TurnQueue& queue, const HairChange& hair);

/// A looping animation played on by deltaMs from timeMs: the moments up to timeMs + deltaMs, then, when that reaches
/// its end, those from its start to where it wraps round to. Returns the new time, kept below the animation's end.
/// Nothing is passed and the time stays when deltaMs is 0.
[[nodiscard]] int32_t AdvanceLooping(const AnimationMoments& animation, int32_t timeMs, int32_t deltaMs, TurnQueue& queue,
                                     const HairChange& hair);

/// An animation played once, on by deltaMs from timeMs: the moments up to the new time, which stops at its end.
/// Returns the new time.
[[nodiscard]] int32_t AdvanceOnce(const AnimationMoments& animation, int32_t timeMs, int32_t deltaMs, TurnQueue& queue,
                                  const HairChange& hair);

/// The moments a body passed in a turn, queued in the order its layers moved on: the face, the gesture and a blow's
/// wobble, each over the stretches of its animation's time it passed (CheckMoments), then its own action. While it plays
/// animations together by weight (`slots`, as the turn leaves them), those stand in for its action, as they do in its
/// pose: the heaviest (`soundingSlot`) passes the moments from where the same animation was at the end of the turn
/// before (`lastSlots`), and one just brought in passes none until the next turn. A slot's moments at one time keep the
/// order they are listed in. An animation `infoOf` does not know passes nothing
void QueueTurn(const creature_layers::TurnPassed& passed, std::span<const Played> lastSlots, std::span<const Played> slots,
               std::optional<size_t> soundingSlot, const InfoOf& infoOf, TurnQueue& queue, const HairChange& hair);

/// How far a creature's sound is from the camera, summed depth first, then height, then width
[[nodiscard]] float CameraDistance(glm::vec3 camera, glm::vec3 creature);

/// Whether a sound moves the creed's glow on the creature's hands rather than making a noise of its own: the creed glow
/// actions, from the left hand's to the middle's
[[nodiscard]] constexpr bool MovesCreedGlow(audio::SoundAction action)
{
	return action >= audio::SoundAction::CreedGlowLeft0 && action <= audio::SoundAction::CreedGlowCentre100;
}
} // namespace openblack::creature_audio
