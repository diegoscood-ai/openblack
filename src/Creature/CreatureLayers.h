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
#include <numbers>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "Creature/CreatureFace.h"

/// What a creature's body plays, layer by layer. The body plays one animation at a time: standing and breathing, an
/// action that plays once (a yawn of tiredness, a wave), or a start, loop and end (sitting down, sitting, getting up).
/// On top of it, the head turns towards what the creature looks at, the face pulls an expression, and a gesture such as
/// a nod can play once. Animations are known by their place in the creature spec file.
namespace openblack::creature_layers
{
/// The places of the animations the creatures' minds play in the creature spec file (Data/ctrspec27.txt, its lines
/// counted from the first animation after the version, the section lines starting with '=' skipped)
namespace animations
{
constexpr size_t k_Stand = 0;
/// The faces: smile, grimace, growl, scared, sad, amazed, puzzled, laugh, ooh, aah, and two spare
constexpr size_t k_FirstFace = 16;
constexpr size_t k_FaceCount = 12;
/// The ten faces the idle creature pulls at random
constexpr size_t k_IdleFaceCount = 10;
/// The start, loop and end of sleeping, having a poo and being sick
constexpr size_t k_StartSleep = 28;
constexpr size_t k_Sleep = 29;
constexpr size_t k_EndSleep = 30;
constexpr size_t k_StartPoo = 31;
constexpr size_t k_Poo = 32;
constexpr size_t k_EndPoo = 33;
constexpr size_t k_StartPuke = 34;
constexpr size_t k_Puke = 35;
constexpr size_t k_EndPuke = 36;
constexpr size_t k_StartSit = 37;
constexpr size_t k_Sit = 38;
constexpr size_t k_EndSit = 39;
/// The actions that play once, from summoning to "pick me"
constexpr size_t k_FirstAction = 52;
constexpr size_t k_ActionCount = 23;
constexpr size_t k_Summon = 52;
constexpr size_t k_Angry = 53;
constexpr size_t k_Hungry = 54;
constexpr size_t k_Happy = 55;
constexpr size_t k_Sad = 56;
constexpr size_t k_Tired = 57;
constexpr size_t k_Hot = 58;
constexpr size_t k_Cold = 59;
constexpr size_t k_Scratch = 60;
constexpr size_t k_Frightened = 61;
constexpr size_t k_Confused = 63;
constexpr size_t k_FeelingNice = 64;
constexpr size_t k_Impress = 65;
constexpr size_t k_NeedAPoo = 66;
constexpr size_t k_FeelPlayful = 67;
constexpr size_t k_Taunt = 70;
constexpr size_t k_Drink = 71;
constexpr size_t k_FriendlyWave = 72;
/// Turning the head right to left and looking down to up, standing and sitting. The middle keyframe looks ahead.
constexpr size_t k_LookRightLeft = 75;
constexpr size_t k_LookDownUp = 76;
constexpr size_t k_SitLookRightLeft = 77;
constexpr size_t k_SitLookDownUp = 78;
/// Eating what it holds
constexpr size_t k_Eat = 96;
/// Fainting, and getting up again
constexpr size_t k_Faint = 106;
constexpr size_t k_GetUp = 107;
/// The gestures played on top of the body: nod, shake, yawn, thirsty, squirt water, talk
constexpr size_t k_FirstGesture = 200;
constexpr size_t k_GestureCount = 7;
constexpr size_t k_Yawn = 202;
constexpr size_t k_Talk = 205;

/// The spec file's name of an animation the minds play, without its type letter, or empty for others
[[nodiscard]] std::string_view Name(size_t animation);
} // namespace animations

/// How fast every animation but the stand plays: bigger creatures move more slowly
[[nodiscard]] float PlaybackRate(float size);

/// What the body plays
struct BodyAction
{
	enum class Kind : uint8_t
	{
		/// Standing, breathing
		Stand,
		/// An action played once, then back to standing
		Once,
		/// A start, a loop for as long as wanted, and an end, then back to standing
		Sequence,
	};
	enum class Phase : uint8_t
	{
		Start,
		Loop,
		End,
	};
	Kind kind {Kind::Stand};
	Phase phase {Phase::Start};
	/// The action played once, or the start, loop and end
	std::array<size_t, 3> animations {};
	float timeMs {0.0f};
	/// Played left to right
	bool mirrored {false};
	/// A sequence ends once its loop is told to
	bool endWanted {false};
	/// The loop holds its last frame rather than playing, as lying where it fell
	bool holdLoop {false};
	/// Its time is set each frame by what plays it, as a fight plays its fighters' moves at their own pace, so the frame
	/// doesn't move it on as well
	bool timedByPlayer {false};
};

[[nodiscard]] bool IsPlaying(const BodyAction& body);
/// Whether the body is in the loop of a sequence, such as sitting
[[nodiscard]] bool IsLooping(const BodyAction& body);
/// The animation the body plays now
[[nodiscard]] size_t CurrentAnimation(const BodyAction& body);
/// An action that plays once, unless the body already plays one: nothing starts while one plays
[[nodiscard]] std::optional<BodyAction> PlayOnce(const BodyAction& body, size_t animation, bool mirrored);
/// A start, loop and end, unless the body already plays an action; the loop can hold its last frame
[[nodiscard]] std::optional<BodyAction> PlaySequence(const BodyAction& body, size_t start, size_t loop, size_t end,
                                                     bool holdLoop = false);
/// A sequence's loop ends and its end plays
[[nodiscard]] BodyAction EndLoop(BodyAction body);
/// The body some milliseconds on, given how long the animation it plays now lasts, or nothing when the species has no
/// such animation. Each animation starts from its beginning, without blending into the last. A body timed by what plays it
/// stays where it was put.
[[nodiscard]] BodyAction AdvanceBody(BodyAction body, float milliseconds, std::optional<uint32_t> duration);

/// The face's expression, played on top of the body. A face is pulled for some time: it plays through once and holds
/// its last frame, the full expression, until that time is up. Then it relaxes, running back to its start a quarter as
/// fast as it played. Pulling another face first runs the current one back to its start at full speed, then plays the
/// new one. Pulling the face already held keeps holding it for the new time.
struct FaceLayer
{
	std::optional<size_t> current;
	float timeMs {0.0f};
	std::optional<size_t> wanted;
	/// How much longer the wanted face is held for
	float remainingMs {0.0f};
	/// Why the wanted face was pulled
	creature_face::Cue cue {creature_face::Cue::None};
};
/// A face pulled for some milliseconds, or for an hour when none are given
[[nodiscard]] FaceLayer PullFace(FaceLayer face, size_t animation, float milliseconds,
                                 creature_face::Cue cue = creature_face::Cue::None);
/// No face is wanted any more: the current one relaxes
[[nodiscard]] FaceLayer RelaxFace(FaceLayer face);
[[nodiscard]] FaceLayer AdvanceFace(FaceLayer face, float milliseconds, std::optional<uint32_t> duration);

/// A gesture played once on top of the body, such as a nod or a yawn
struct GestureLayer
{
	std::optional<size_t> animation;
	float timeMs {0.0f};
};
/// A gesture, unless one already plays
[[nodiscard]] std::optional<GestureLayer> PlayGesture(const GestureLayer& gesture, size_t animation);
[[nodiscard]] GestureLayer AdvanceGesture(GestureLayer gesture, float milliseconds, std::optional<uint32_t> duration);

/// A stretch of an animation's own time, from `fromMs` up to `toMs`, over which the moments it lists (its sounds, its
/// hair) are looked for
struct PassedStretch
{
	int32_t fromMs;
	int32_t toMs;

	constexpr bool operator==(const PassedStretch&) const noexcept = default;
};

/// An animation's time moved on in whole milliseconds, as the game turn moves it, and the stretches it passed on the
/// way, in the order they are looked at
struct TimeAdvance
{
	int32_t timeMs;
	std::array<PassedStretch, 2> stretches;
	size_t passedCount;

	[[nodiscard]] std::span<const PassedStretch> Passed() const { return std::span(stretches).first(passedCount); }
};

/// An animation played once, moved on by deltaMs from timeMs: the new time stops at its end, durationMs. The one
/// stretch passed runs from timeMs to the new time, even when they are the same or the time was already past the end
/// (it then comes back to the end). Sums wrap round as 32-bit numbers do
[[nodiscard]] TimeAdvance AdvanceOnce(int32_t timeMs, int32_t deltaMs, int32_t durationMs);

/// A looping animation moved on by deltaMs from timeMs. Nothing is passed and the time stays, even past the end, when
/// deltaMs is 0. Otherwise the stretch from timeMs to timeMs + deltaMs is passed; when that reaches the end it comes
/// round once, by one durationMs, and when it is then past the start, the stretch from timeMs - durationMs to it is
/// passed too. A time still at or past the end after coming round once is held one millisecond before it: so is a
/// step of two lengths or more, and an animation of no length gives -1. Sums wrap round as 32-bit numbers do
[[nodiscard]] TimeAdvance AdvanceLooping(int32_t timeMs, int32_t deltaMs, int32_t durationMs);

/// The face as the game turn moves it, in whole milliseconds (see FaceLayer)
struct TurnFace
{
	std::optional<size_t> current;
	std::optional<size_t> wanted;
	/// How much longer the wanted face is held for: it is let go of when this comes down to 0. A time below 0 is never
	/// counted down, so the face is held until another is pulled
	int32_t heldMs {0};
	int32_t timeMs {0};

	constexpr bool operator==(const TurnFace&) const noexcept = default;
};

/// The face after a turn, and the stretch of the face's own time it passed when it played
struct TurnFaceAdvance
{
	TurnFace face;
	std::optional<PassedStretch> passed;
};

/// The face after a turn of `ms`. The time it is held for counts down, kept at 0 or above, and at 0 the wanted face is
/// let go of. While the current face is the one wanted, it plays on once (AdvanceOnce) towards its end,
/// `currentDurationMs`, the length of the current face's animation (none when the species lacks it: the face then
/// stays where it is). Otherwise the current face runs back towards its start, kept at 0 or above, by the turn's
/// milliseconds while another face is wanted, or by a quarter of them (shifted down two bits, so rounded down) when
/// none is; at its start it is dropped. Then, when no face is current, the wanted one becomes current, from the time
/// the last one reached
[[nodiscard]] TurnFaceAdvance AdvanceFaceByTurn(TurnFace face, int32_t ms, std::optional<int32_t> currentDurationMs);

/// The stretches of one animation's time a layer passed in a turn, over which the moments it lists (its sounds, its hair)
/// are looked for
struct PassedAnimation
{
	size_t animation;
	TimeAdvance advance;
};

/// What a body's layers passed in a turn: the face, the gesture and a blow's wobble, in the order they moved on, then
/// its own action
struct TurnPassed
{
	std::vector<PassedAnimation> overlays;
	std::optional<PassedAnimation> action;
};

/// A layer after a turn, and what of its animation it passed, if it played
template <typename Layer>
struct LayerByTurn
{
	Layer layer;
	std::optional<PassedAnimation> passed;
};

/// The face after a turn of `ms` whole milliseconds (AdvanceFaceByTurn on the face's times, taken to whole
/// milliseconds), given how long its current face's animation lasts. Once the wanted face is let go of, the face relaxes
/// (RelaxFace)
[[nodiscard]] LayerByTurn<FaceLayer> AdvanceFaceLayerByTurn(FaceLayer face, int32_t ms,
                                                            std::optional<int32_t> currentDurationMs);

/// A gesture or a blow's wobble after a turn's step, given how long its animation lasts (none when the species lacks it:
/// it ends at once). A step that reaches the end plays it to its end and ends it; otherwise it moves on as it loops
/// (AdvanceLooping)
[[nodiscard]] LayerByTurn<GestureLayer> AdvanceGestureByTurn(GestureLayer gesture, int32_t step,
                                                             std::optional<int32_t> durationMs);

/// The body after a turn's step, given how long the animation it plays now lasts (none counts as no length). An action
/// played once, a sequence's start and its end are done when the step reaches their end: they play to it (AdvanceOnce),
/// then the start goes on to the loop from its beginning and the others back to standing; until then they move on as
/// they loop (AdvanceLooping). The loop loops, unless it is told to end (or has no length and does not hold), when the
/// end starts, or holds its last frame. Standing, the time is 0: the breath plays the stand. A body timed by what plays
/// it stays where it was put. Times are taken to whole milliseconds
[[nodiscard]] LayerByTurn<BodyAction> AdvanceBodyByTurn(BodyAction body, int32_t step, std::optional<int32_t> durationMs);

/// How far the head is turned on one axis, and how fast it is turning
struct LookAxis
{
	float angle {0.0f};
	float velocity {0.0f};
};
/// The head turns right and left as far as half a turn each way, and down and up a quarter turn each way
constexpr float k_YawLimit = std::numbers::pi_v<float>;
constexpr float k_PitchLimit = std::numbers::pi_v<float> / 2.0f;
/// The head speeds up towards where it should turn at a steady rate, and slows so as to stop there: never faster than
/// would let it stop in time, nor than would overshoot in this step. It settles once within a hair of the target, and
/// never turns past the limit either way.
[[nodiscard]] LookAxis TurnHead(LookAxis axis, float target, float acceleration, float seconds, float limit);

/// Which way to turn the head to look at a point, in radians: yaw to the left of ahead, pitch up. A point straight
/// along a map axis from the head gives no yaw
struct LookAngles
{
	float yaw;
	float pitch;
};
[[nodiscard]] LookAngles AnglesTowards(const glm::vec3& head, const glm::vec3& ahead, const glm::vec3& target);
/// The time in a head turning animation that shows an angle: the middle looks ahead, the ends as far as the limit
[[nodiscard]] uint32_t LookTime(float angle, float limit, uint32_t duration);
} // namespace openblack::creature_layers
