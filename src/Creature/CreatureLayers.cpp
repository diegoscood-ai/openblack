/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLayers.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

using namespace openblack;
using namespace openblack::creature_layers;

namespace
{
/// Every animation but the stand plays this fast at size 0, and this much more slowly for each size bigger
constexpr float k_PlaybackRateAtSizeZero = 1.6f;
constexpr float k_PlaybackSlowingPerSize = 0.425f;
/// Faces run back to their start this much more slowly when no other face is wanted
constexpr float k_FaceRelaxSlowing = 4.0f;
/// The same in a turn's whole milliseconds: they are shifted down this many bits, a quarter rounded down
constexpr int32_t k_FaceRelaxShift = 2;
/// A face pulled with no time given is held for an hour
constexpr float k_FaceHeldUnlessTold = 3'600'000.0f;
/// The head settles on its target when this near it
constexpr float k_Settled = 1e-4f;
/// The head turns sideways only when the direction to its target is further than this off each map axis
constexpr float k_OffAxis = 1e-4f;

constexpr std::array<std::string_view, animations::k_FaceCount> k_FaceNames {
    "smile", "grimace", "growl", "scared", "sad", "amazed", "puzzled", "laugh", "ooh", "aah", "spare1", "spare2",
};
constexpr std::array<std::string_view, animations::k_ActionCount> k_ActionNames {
    "summon",      "angry",      "hungry", "happy",    "sad",           "tired",       "hot",        "cold",
    "scratch",     "frightened", "sneeze", "confused", "feeling_nice",  "impress",     "need_a_poo", "feel_playful",
    "play_action", "look_at_me", "taunt",  "drink",    "friendly_wave", "embarrassed", "pick_me",
};
constexpr std::array<std::string_view, animations::k_GestureCount> k_GestureNames {
    "nod", "shake", "yawn", "thirsty", "squirt_water", "talk", "spare1",
};

/// Whole milliseconds added and taken away as 32-bit numbers, wrapping round rather than overflowing
[[nodiscard]] constexpr int32_t WrappingAdd(int32_t a, int32_t b)
{
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

[[nodiscard]] constexpr int32_t WrappingSub(int32_t a, int32_t b)
{
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/// a less b, or 0 when that is below 0
[[nodiscard]] constexpr int32_t DownToZero(int32_t a, int32_t b)
{
	const auto less = WrappingSub(a, b);
	return less < 0 ? 0 : less;
}
} // namespace

std::string_view animations::Name(size_t animation)
{
	if (animation == k_Stand)
	{
		return "stand";
	}
	if (animation >= k_FirstFace && animation < k_FirstFace + k_FaceCount)
	{
		return k_FaceNames.at(animation - k_FirstFace);
	}
	if (animation >= k_FirstAction && animation < k_FirstAction + k_ActionCount)
	{
		return k_ActionNames.at(animation - k_FirstAction);
	}
	if (animation >= k_FirstGesture && animation < k_FirstGesture + k_GestureCount)
	{
		return k_GestureNames.at(animation - k_FirstGesture);
	}
	switch (animation)
	{
	case k_StartSleep:
		return "start_sleep";
	case k_Sleep:
		return "sleep";
	case k_EndSleep:
		return "end_sleep";
	case k_StartPoo:
		return "start_poo";
	case k_Poo:
		return "poo";
	case k_EndPoo:
		return "end_poo";
	case k_StartPuke:
		return "start_puke";
	case k_Puke:
		return "puke";
	case k_EndPuke:
		return "end_puke";
	case k_Eat:
		return "eat";
	case k_Faint:
		return "faint";
	case k_GetUp:
		return "get_up";
	case k_StartSit:
		return "start_sit";
	case k_Sit:
		return "sit";
	case k_EndSit:
		return "end_sit";
	case k_LookRightLeft:
		return "right_left";
	case k_LookDownUp:
		return "down_up";
	case k_SitLookRightLeft:
		return "sit_r_l";
	case k_SitLookDownUp:
		return "sit_d_u";
	default:
		return {};
	}
}

float creature_layers::PlaybackRate(float size)
{
	return k_PlaybackRateAtSizeZero - (k_PlaybackSlowingPerSize * size);
}

bool creature_layers::IsPlaying(const BodyAction& body)
{
	return body.kind != BodyAction::Kind::Stand;
}

bool creature_layers::IsLooping(const BodyAction& body)
{
	return body.kind == BodyAction::Kind::Sequence && body.phase == BodyAction::Phase::Loop;
}

size_t creature_layers::CurrentAnimation(const BodyAction& body)
{
	switch (body.kind)
	{
	case BodyAction::Kind::Once:
		return body.animations[0];
	case BodyAction::Kind::Sequence:
		return body.animations.at(static_cast<size_t>(body.phase));
	case BodyAction::Kind::Stand:
	default:
		return animations::k_Stand;
	}
}

std::optional<BodyAction> creature_layers::PlayOnce(const BodyAction& body, size_t animation, bool mirrored)
{
	if (IsPlaying(body))
	{
		return std::nullopt;
	}
	return BodyAction {.kind = BodyAction::Kind::Once,
	                   .phase = BodyAction::Phase::Start,
	                   .animations = {animation, animation, animation},
	                   .timeMs = 0.0f,
	                   .mirrored = mirrored,
	                   .endWanted = false};
}

std::optional<BodyAction> creature_layers::PlaySequence(const BodyAction& body, size_t start, size_t loop, size_t end,
                                                        bool holdLoop)
{
	if (IsPlaying(body))
	{
		return std::nullopt;
	}
	return BodyAction {.kind = BodyAction::Kind::Sequence,
	                   .phase = BodyAction::Phase::Start,
	                   .animations = {start, loop, end},
	                   .timeMs = 0.0f,
	                   .mirrored = false,
	                   .endWanted = false,
	                   .holdLoop = holdLoop};
}

BodyAction creature_layers::EndLoop(BodyAction body)
{
	if (body.kind == BodyAction::Kind::Sequence)
	{
		body.endWanted = true;
	}
	return body;
}

BodyAction creature_layers::AdvanceBody(BodyAction body, float milliseconds, std::optional<uint32_t> duration)
{
	if (!IsPlaying(body))
	{
		body.timeMs = 0.0f;
		return body;
	}
	if (body.timedByPlayer)
	{
		return body;
	}
	const auto length = static_cast<float>(duration.value_or(0));
	const auto stand = [] { return BodyAction {}; };
	if (body.kind == BodyAction::Kind::Once)
	{
		body.timeMs += milliseconds;
		return body.timeMs >= length ? stand() : body;
	}

	switch (body.phase)
	{
	case BodyAction::Phase::Start:
		body.timeMs += milliseconds;
		if (body.timeMs >= length)
		{
			body.phase = BodyAction::Phase::Loop;
			body.timeMs = 0.0f;
		}
		break;
	case BodyAction::Phase::Loop:
		if (body.endWanted || (length <= 0.0f && !body.holdLoop))
		{
			body.phase = BodyAction::Phase::End;
			body.timeMs = 0.0f;
			break;
		}
		body.timeMs = body.holdLoop ? std::max(length - 1.0f, 0.0f) : std::fmod(body.timeMs + milliseconds, length);
		break;
	case BodyAction::Phase::End:
		body.timeMs += milliseconds;
		if (body.timeMs >= length)
		{
			return stand();
		}
		break;
	}
	return body;
}

FaceLayer creature_layers::PullFace(FaceLayer face, size_t animation, float milliseconds, creature_face::Cue cue)
{
	face.wanted = animation;
	face.remainingMs = milliseconds > 0.0f ? milliseconds : k_FaceHeldUnlessTold;
	face.cue = cue;
	return face;
}

FaceLayer creature_layers::RelaxFace(FaceLayer face)
{
	face.wanted.reset();
	face.remainingMs = 0.0f;
	face.cue = creature_face::Cue::None;
	return face;
}

FaceLayer creature_layers::AdvanceFace(FaceLayer face, float milliseconds, std::optional<uint32_t> duration)
{
	// The wanted face is let go of once its time is up
	face.remainingMs = std::max(face.remainingMs - milliseconds, 0.0f);
	if (face.remainingMs <= 0.0f)
	{
		face = RelaxFace(face);
	}

	if (face.current == face.wanted)
	{
		// The expression plays through once and holds its last frame
		if (face.current.has_value() && duration.has_value())
		{
			face.timeMs = std::min(face.timeMs + milliseconds, static_cast<float>(*duration));
		}
		return face;
	}
	if (face.current.has_value())
	{
		face.timeMs -= face.wanted.has_value() ? milliseconds : milliseconds / k_FaceRelaxSlowing;
		if (face.timeMs <= 0.0f)
		{
			face.timeMs = 0.0f;
			face.current.reset();
		}
	}
	if (!face.current.has_value())
	{
		face.current = face.wanted;
		face.timeMs = 0.0f;
	}
	return face;
}

std::optional<GestureLayer> creature_layers::PlayGesture(const GestureLayer& gesture, size_t animation)
{
	if (gesture.animation.has_value())
	{
		return std::nullopt;
	}
	return GestureLayer {.animation = animation, .timeMs = 0.0f};
}

GestureLayer creature_layers::AdvanceGesture(GestureLayer gesture, float milliseconds, std::optional<uint32_t> duration)
{
	if (!gesture.animation.has_value())
	{
		return gesture;
	}
	gesture.timeMs += milliseconds;
	if (!duration.has_value() || gesture.timeMs >= static_cast<float>(*duration))
	{
		return {};
	}
	return gesture;
}

TimeAdvance creature_layers::AdvanceOnce(int32_t timeMs, int32_t deltaMs, int32_t durationMs)
{
	const auto next = std::min(durationMs, WrappingAdd(timeMs, deltaMs));
	return {.timeMs = next, .stretches = {{{.fromMs = timeMs, .toMs = next}, {}}}, .passedCount = 1};
}

TimeAdvance creature_layers::AdvanceLooping(int32_t timeMs, int32_t deltaMs, int32_t durationMs)
{
	TimeAdvance advance {.timeMs = timeMs, .stretches = {}, .passedCount = 0};
	if (deltaMs == 0)
	{
		return advance;
	}
	auto next = WrappingAdd(timeMs, deltaMs);
	advance.stretches.at(advance.passedCount++) = {.fromMs = timeMs, .toMs = next};
	if (next >= durationMs)
	{
		next = WrappingSub(next, durationMs);
		if (next > 0)
		{
			// on from its start, measured as one stretch with the part before the end
			advance.stretches.at(advance.passedCount++) = {.fromMs = WrappingSub(timeMs, durationMs), .toMs = next};
		}
	}
	advance.timeMs = next < durationMs ? next : WrappingSub(durationMs, 1);
	return advance;
}

TurnFaceAdvance creature_layers::AdvanceFaceByTurn(TurnFace face, int32_t ms, std::optional<int32_t> currentDurationMs)
{
	if (face.heldMs > 0)
	{
		face.heldMs = DownToZero(face.heldMs, ms);
	}
	if (face.heldMs == 0)
	{
		face.wanted.reset();
	}

	if (face.wanted == face.current)
	{
		if (!face.current.has_value() || !currentDurationMs.has_value())
		{
			return {.face = face, .passed = std::nullopt};
		}
		const auto played = AdvanceOnce(face.timeMs, ms, *currentDurationMs);
		face.timeMs = played.timeMs;
		return {.face = face, .passed = played.Passed().front()};
	}
	if (face.current.has_value())
	{
		const auto back = face.wanted.has_value() ? ms : ms >> k_FaceRelaxShift;
		face.timeMs = DownToZero(face.timeMs, back);
		if (face.timeMs == 0)
		{
			face.current.reset();
		}
	}
	if (!face.current.has_value())
	{
		face.current = face.wanted;
	}
	return {.face = face, .passed = std::nullopt};
}

LayerByTurn<FaceLayer> creature_layers::AdvanceFaceLayerByTurn(FaceLayer face, int32_t ms,
                                                               std::optional<int32_t> currentDurationMs)
{
	const TurnFace turn {
	    .current = face.current,
	    .wanted = face.wanted,
	    .heldMs = static_cast<int32_t>(face.remainingMs),
	    .timeMs = static_cast<int32_t>(face.timeMs),
	};
	const auto played = AdvanceFaceByTurn(turn, ms, currentDurationMs);
	face.current = played.face.current;
	face.timeMs = static_cast<float>(played.face.timeMs);
	face.remainingMs = static_cast<float>(played.face.heldMs);
	if (!played.face.wanted.has_value())
	{
		face = RelaxFace(face);
	}
	std::optional<PassedAnimation> passed;
	if (played.passed.has_value() && face.current.has_value())
	{
		passed = PassedAnimation {
		    .animation = *face.current,
		    .advance = {.timeMs = played.face.timeMs, .stretches = {{*played.passed, {}}}, .passedCount = 1},
		};
	}
	return {.layer = face, .passed = passed};
}

LayerByTurn<GestureLayer> creature_layers::AdvanceGestureByTurn(GestureLayer gesture, int32_t step,
                                                                std::optional<int32_t> durationMs)
{
	if (!gesture.animation.has_value())
	{
		return {.layer = gesture, .passed = std::nullopt};
	}
	if (!durationMs.has_value())
	{
		return {.layer = {}, .passed = std::nullopt};
	}
	const auto time = static_cast<int32_t>(gesture.timeMs);
	// a step that reaches the end is cut to the end, and the gesture is over once played to it
	const bool finished = WrappingAdd(time, step) >= *durationMs;
	const auto delta = finished ? WrappingSub(*durationMs, time) : step;
	const auto played = AdvanceLooping(time, delta, *durationMs);
	const PassedAnimation passed {.animation = *gesture.animation, .advance = played};
	if (finished)
	{
		return {.layer = {}, .passed = passed};
	}
	gesture.timeMs = static_cast<float>(played.timeMs);
	return {.layer = gesture, .passed = passed};
}

LayerByTurn<BodyAction> creature_layers::AdvanceBodyByTurn(BodyAction body, int32_t step, std::optional<int32_t> durationMs)
{
	if (!IsPlaying(body))
	{
		body.timeMs = 0.0f;
		return {.layer = body, .passed = std::nullopt};
	}
	if (body.timedByPlayer)
	{
		return {.layer = body, .passed = std::nullopt};
	}
	const auto animation = CurrentAnimation(body);
	const auto length = durationMs.value_or(0);
	const auto time = static_cast<int32_t>(body.timeMs);

	if (body.kind == BodyAction::Kind::Sequence && body.phase == BodyAction::Phase::Loop)
	{
		if (body.endWanted || (length <= 0 && !body.holdLoop))
		{
			body.phase = BodyAction::Phase::End;
			body.timeMs = 0.0f;
			return {.layer = body, .passed = std::nullopt};
		}
		if (body.holdLoop)
		{
			body.timeMs = static_cast<float>(std::max(length - 1, 0));
			return {.layer = body, .passed = std::nullopt};
		}
		const auto looped = AdvanceLooping(time, step, length);
		body.timeMs = static_cast<float>(looped.timeMs);
		return {.layer = body, .passed = PassedAnimation {.animation = animation, .advance = looped}};
	}

	// Played once, or a sequence's start or end: the step that reaches the end plays it to the end, and it is over
	if (WrappingAdd(time, step) >= length)
	{
		const PassedAnimation passed {.animation = animation, .advance = AdvanceOnce(time, step, length)};
		if (body.kind == BodyAction::Kind::Sequence && body.phase == BodyAction::Phase::Start)
		{
			body.phase = BodyAction::Phase::Loop;
			body.timeMs = 0.0f;
			return {.layer = body, .passed = passed};
		}
		return {.layer = {}, .passed = passed};
	}
	const auto played = AdvanceLooping(time, step, length);
	body.timeMs = static_cast<float>(played.timeMs);
	return {.layer = body, .passed = PassedAnimation {.animation = animation, .advance = played}};
}

LookAxis creature_layers::TurnHead(LookAxis axis, float target, float acceleration, float seconds, float limit)
{
	const auto error = std::abs(target - axis.angle);
	if (error < k_Settled)
	{
		return {.angle = target, .velocity = 0.0f};
	}
	if (seconds <= 0.0f)
	{
		return axis;
	}
	// The fastest it may go: able to stop in time at this acceleration, and not past the target in this step
	const auto maxSpeed = std::min(std::sqrt(2.0f * error * acceleration), error / seconds);
	if (axis.angle < target)
	{
		axis.velocity += acceleration * seconds;
	}
	else
	{
		axis.velocity -= acceleration * seconds;
	}
	axis.velocity = std::clamp(axis.velocity, -maxSpeed, maxSpeed);
	axis.angle = std::clamp(axis.angle + (axis.velocity * seconds), -limit, limit);
	return axis;
}

LookAngles creature_layers::AnglesTowards(const glm::vec3& head, const glm::vec3& ahead, const glm::vec3& target)
{
	const auto offset = target - head;
	const auto length = glm::length(offset);
	if (length <= 0.0f)
	{
		return {.yaw = 0.0f, .pitch = 0.0f};
	}
	const auto direction = offset / length;
	const auto pitch = std::asin(std::clamp(direction.y, -1.0f, 1.0f));
	const glm::vec2 flat {direction.x, direction.z};
	const glm::vec2 forward {ahead.x, ahead.z};
	// A target straight along either map axis from the eyes, however far, gives no sideways turn, as in the original
	if (!(std::abs(direction.x) > k_OffAxis && std::abs(direction.z) > k_OffAxis) || glm::length(forward) <= 0.0f)
	{
		return {.yaw = 0.0f, .pitch = pitch};
	}
	// The world is left-handed: seen from above with y up, x to the right, z points away, so a target clockwise from
	// ahead is on the creature's right and one anticlockwise on its left
	const auto cross = (forward.x * flat.y) - (forward.y * flat.x);
	const auto dot = glm::dot(forward, flat);
	return {.yaw = std::atan2(cross, dot), .pitch = pitch};
}

uint32_t creature_layers::LookTime(float angle, float limit, uint32_t duration)
{
	if (limit <= 0.0f)
	{
		return duration / 2;
	}
	const auto fraction = std::clamp((angle + limit) / (2.0f * limit), 0.0f, 1.0f);
	return static_cast<uint32_t>(fraction * static_cast<float>(duration));
}
