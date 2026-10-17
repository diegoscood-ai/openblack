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

#include <array>
#include <optional>

#include <entt/core/fwd.hpp>

namespace openblack::ecs::components
{
struct LivingAction;
}

/// What the villagers' states share about their clips: how long a clip lasts and whether it has played through
namespace openblack::ecs::villager_clips
{

/// A clip's length in milliseconds (its play time), by its id in the animation cache (ecs::ClipId); none for one not
/// loaded, or without the resources (the unit tests)
[[nodiscard]] std::optional<uint32_t> ClipMilliseconds(entt::id_type clip);
/// Whether a clip lasting so many milliseconds has played through so many times in the turns since the villager went
/// into its state, the turns counted in whole milliseconds as the game counts them. All unsigned, as the game compares
/// them
[[nodiscard]] constexpr bool HasPlayed(uint32_t turnsSinceStateChange, uint32_t turnMilliseconds, uint32_t playTime,
                                       uint32_t times)
{
	return turnsSinceStateChange * turnMilliseconds >= playTime * times;
}
/// What setting a clip on a villager does to its place in the clip
enum class ClipPlace : uint8_t
{
	/// It plays on from where it was, in whatever clip it now has
	Keep,
	/// It starts the clip from its beginning
	Restart,
};
/// A clip set on a villager: the same clip again plays on. A different one takes over from the same place, and starts
/// from its beginning only when that is asked for and the villager isn't dancing: a dancer keeps in time with the dance.
[[nodiscard]] constexpr ClipPlace PlaceOnSetClip(bool sameClip, bool restartAsked, bool dancing)
{
	return !sameClip && restartAsked && !dancing ? ClipPlace::Restart : ClipPlace::Keep;
}
/// What a dancer's clip does when its group starts a move: whether it starts over, and the turns its state counts since
/// then. It starts over only once it has played through since the dancer's state changed or the clip last started over,
/// so the move's clips start together without one being cut short, however many turns the move is started on.
struct DanceMoveClip
{
	bool restart;
	uint32_t turnsSinceStateChange;
};
[[nodiscard]] constexpr DanceMoveClip OnDanceMove(uint32_t turnsSinceStateChange, uint32_t turnMilliseconds, uint32_t playTime)
{
	if (HasPlayed(turnsSinceStateChange, turnMilliseconds, playTime, 1))
	{
		return {.restart = true, .turnsSinceStateChange = 0};
	}
	return {.restart = false, .turnsSinceStateChange = turnsSinceStateChange};
}
/// Whether a clip has played through so many times since the villager went into its state. A clip that is not loaded
/// has always played
[[nodiscard]] bool ClipPlayed(const components::LivingAction& action, entt::id_type clip, uint32_t times = 1);
/// What a villager's clip does when its dance group starts a move, by the clip it has and the turns since its state
/// changed. A clip that is not loaded has always played
[[nodiscard]] DanceMoveClip OnDanceMove(const components::LivingAction& action, entt::id_type clip);

/// Whether a clip plays by the ground covered while its villager is in a moving state: the clip's flag 0x200 (no play by
/// the ground) is clear and its stride is at least 0.05 m either way, the stride below which the game sets that flag
/// when it loads the clip. A stride that is not a number never plays by the ground
[[nodiscard]] bool PlaysByGround(float stride, uint32_t flags);
/// The whole milliseconds of play the ground covered in a frame gives a clip. The speed (map units a turn) times the
/// frame's game milliseconds, over the villager's scale times 655350, is the metres covered; over the clip's stride and
/// times its play time, truncated towards zero. Each step is rounded to single precision, as the game computes it
[[nodiscard]] int32_t MovingPlay(uint16_t speed, uint32_t frameMilliseconds, float scale, float stride, int32_t playTime);
/// The whole milliseconds of play a frame gives a villager's clip. In a moving state (`groundSpeed`, its whole speed) a
/// clip that plays by the ground plays by it (MovingPlay); any other clip plays by the frame's game milliseconds
[[nodiscard]] int32_t FramePlay(std::optional<uint16_t> groundSpeed, uint32_t frameMilliseconds, float scale, float stride,
                                uint32_t flags, int32_t playTime);

/// A stretch of a clip, [from, to) in milliseconds: the clip's sound events in it fire. Empty when to <= from
struct ClipSpan
{
	int32_t from {0};
	int32_t to {0};
};

/// Where a clip is after a frame, and the stretches of it the frame played, in order
struct ClipStep
{
	int32_t place {0};
	std::array<ClipSpan, 2> played {};
};

/// A clip's place (whole milliseconds) moved on by `played` milliseconds. Short of the play time it is there, and the
/// stretch from the old place to it plays. At or past the play time the stretch to the end plays; then a looping clip
/// goes on from its start by what is left over, a one-shot clip stays at its end, and in both the stretch from 0 to the
/// new place plays as well (for a one-shot clip, the whole clip once more). A place below 0 is drawn from 0.
/// (openblack) A clip with no play time stays at 0: the game would divide by zero
[[nodiscard]] ClipStep AdvanceClip(int32_t place, int32_t played, int32_t playTime, bool looping);

} // namespace openblack::ecs::villager_clips
