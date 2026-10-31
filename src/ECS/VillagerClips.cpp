/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerClips.h"

#include <cmath>

#include <algorithm>

#include "3D/L3DAnim.h"
#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "GameClock.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// A speed in map units a turn times milliseconds, over this, is metres: the game's constant, a little under 6553.6 map
/// units a metre times 100 ms a turn
constexpr float k_SpeedMillisecondsPerMetre = 655350.0f;
/// The clip flag that keeps a clip off the ground covered, and the stride below which the game sets it
constexpr uint32_t k_NoGroundPlayFlag = 0x200;
constexpr float k_LeastGroundStride = 0.05f;
} // namespace

std::optional<uint32_t> villager_clips::ClipMilliseconds(entt::id_type clip)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(clip))
	{
		return std::nullopt;
	}
	return animations.Handle(clip)->GetPlayTime();
}

bool villager_clips::ClipPlayed(const components::LivingAction& action, entt::id_type clip, uint32_t times)
{
	return HasPlayed(action.turnsSinceStateChange, game_clock::k_MsPerTurn, ClipMilliseconds(clip).value_or(0), times);
}

villager_clips::DanceMoveClip villager_clips::OnDanceMove(const components::LivingAction& action, entt::id_type clip)
{
	return OnDanceMove(action.turnsSinceStateChange, game_clock::k_MsPerTurn, ClipMilliseconds(clip).value_or(0));
}

bool villager_clips::PlaysByGround(float stride, uint32_t flags)
{
	return (flags & k_NoGroundPlayFlag) == 0 && std::abs(stride) >= k_LeastGroundStride;
}

int32_t villager_clips::MovingPlay(uint16_t speed, uint32_t frameMilliseconds, float scale, float stride, int32_t playTime)
{
	// The speed times the milliseconds is a whole number (kept to 32 bits) before it becomes a float
	const auto covered = static_cast<float>(static_cast<uint32_t>(speed) * frameMilliseconds);
	const float perMetre = scale * k_SpeedMillisecondsPerMetre;
	const float metres = covered / perMetre;
	const float cycles = metres / stride;
	const float milliseconds = cycles * static_cast<float>(playTime);
	return map_coords::FtoL(milliseconds);
}

int32_t villager_clips::FramePlay(std::optional<uint16_t> groundSpeed, uint32_t frameMilliseconds, float scale, float stride,
                                  uint32_t flags, int32_t playTime)
{
	if (groundSpeed.has_value() && PlaysByGround(stride, flags))
	{
		return MovingPlay(*groundSpeed, frameMilliseconds, scale, stride, playTime);
	}
	// The frame's milliseconds, taken as a signed whole number
	return static_cast<int32_t>(frameMilliseconds);
}

villager_clips::ClipStep villager_clips::AdvanceClip(int32_t place, int32_t played, int32_t playTime, bool looping)
{
	if (playTime <= 0)
	{
		return {};
	}
	// A whole-number sum that wraps as the game's does
	const auto next = static_cast<int32_t>(static_cast<uint32_t>(place) + static_cast<uint32_t>(played));
	ClipStep step;
	if (next < playTime)
	{
		step.place = next;
		step.played[0] = {.from = place, .to = next};
	}
	else
	{
		step.place = looping ? next % playTime : playTime;
		step.played[0] = {.from = place, .to = playTime};
		if (step.place != 0)
		{
			step.played[1] = {.from = 0, .to = step.place};
		}
	}
	step.place = std::max(step.place, 0);
	return step;
}
