/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSoundQueue.h"

#include <cmath>

#include <algorithm>
#include <ranges>

using namespace openblack;
using namespace openblack::creature_audio;

namespace
{
/// The drawn fraction is never taken past two turns
constexpr float k_MaxDrawnFraction = 2.0f;

/// The kinds of moment an animation file lists; any other kind is ignored
constexpr int32_t k_VoiceKind = 0;
constexpr int32_t k_HairKind = 1;
constexpr int32_t k_GenericKind = 2;
} // namespace

void TurnQueue::Add(const QueuedSound& sound)
{
	if (_sounds.size() >= k_MaxQueued)
	{
		return;
	}
	// before the first one it does not come after
	const auto at =
	    std::ranges::find_if(_sounds, [&sound](const QueuedSound& queued) { return !(sound.fraction > queued.fraction); });
	_sounds.insert(at, sound);
}

std::vector<QueuedSound> TurnQueue::TakeDrawn(float turnFraction)
{
	const float drawn = turnFraction >= k_MaxDrawnFraction ? k_MaxDrawnFraction : turnFraction;
	// up to the first one the drawing has not passed
	const auto end = std::ranges::find_if(_sounds, [drawn](const QueuedSound& queued) { return !(queued.fraction < drawn); });
	std::vector<QueuedSound> taken(_sounds.begin(), end);
	_sounds.erase(_sounds.begin(), end);
	return taken;
}

std::vector<QueuedSound> TurnQueue::TakeAll()
{
	std::vector<QueuedSound> taken;
	taken.swap(_sounds);
	return taken;
}

void creature_audio::CheckMoments(std::span<const SoundEvent> events, int32_t oldMs, int32_t newMs, TurnQueue& queue,
                                  const HairChange& hair)
{
	if (oldMs == newMs)
	{
		return;
	}
	for (const auto& event : events | std::views::reverse)
	{
		if (event.timeMs < oldMs || event.timeMs >= newMs)
		{
			continue;
		}
		const auto kind = static_cast<int32_t>(event.kind);
		if (kind == k_HairKind)
		{
			if (hair)
			{
				hair(static_cast<int32_t>(event.action), event.mode == 0);
			}
		}
		else if (kind == k_VoiceKind || kind == k_GenericKind)
		{
			const float fraction = static_cast<float>(event.timeMs - oldMs) / static_cast<float>(newMs - oldMs);
			queue.Add({.fraction = fraction, .action = event.action, .mode = event.mode, .generic = kind == k_GenericKind});
		}
	}
}

int32_t creature_audio::AdvanceLooping(const AnimationMoments& animation, int32_t timeMs, int32_t deltaMs, TurnQueue& queue,
                                       const HairChange& hair)
{
	if (deltaMs == 0)
	{
		return timeMs;
	}
	auto next = timeMs + deltaMs;
	CheckMoments(animation.events, timeMs, next, queue, hair);
	if (next >= animation.durationMs)
	{
		next -= animation.durationMs;
		if (next > 0)
		{
			// on from its start, measured as one stretch with the part before the end
			CheckMoments(animation.events, timeMs - animation.durationMs, next, queue, hair);
		}
	}
	return next < animation.durationMs ? next : animation.durationMs - 1;
}

int32_t creature_audio::AdvanceOnce(const AnimationMoments& animation, int32_t timeMs, int32_t deltaMs, TurnQueue& queue,
                                    const HairChange& hair)
{
	const auto next = std::min(animation.durationMs, timeMs + deltaMs);
	CheckMoments(animation.events, timeMs, next, queue, hair);
	return next;
}

void creature_audio::QueueTurn(const creature_layers::TurnPassed& passed, std::span<const Played> lastSlots,
                               std::span<const Played> slots, std::optional<size_t> soundingSlot, const InfoOf& infoOf,
                               TurnQueue& queue, const HairChange& hair)
{
	const auto check = [&](const creature_layers::PassedAnimation& layer) {
		const auto info = infoOf(layer.animation);
		if (!info.has_value())
		{
			return;
		}
		for (const auto& stretch : layer.advance.Passed())
		{
			CheckMoments(info->events, stretch.fromMs, stretch.toMs, queue, hair);
		}
	};
	for (const auto& layer : passed.overlays)
	{
		check(layer);
	}
	if (slots.empty())
	{
		if (passed.action.has_value())
		{
			check(*passed.action);
		}
		return;
	}
	if (!soundingSlot.has_value() || *soundingSlot >= slots.size())
	{
		return;
	}
	const auto& slot = slots[*soundingSlot];
	const auto previous = std::ranges::find(lastSlots, slot.animation, &Played::animation);
	const auto fired = LayerEvents(previous != lastSlots.end() ? std::optional(*previous) : std::nullopt, slot, false, infoOf);
	// last first, so that those at one fraction keep the order they are listed in
	for (const auto& [fraction, event] : fired | std::views::reverse)
	{
		if (event.kind == EventKind::HairGroup)
		{
			if (hair)
			{
				hair(static_cast<int32_t>(event.action), event.mode == 0);
			}
			continue;
		}
		queue.Add(
		    {.fraction = fraction, .action = event.action, .mode = event.mode, .generic = event.kind == EventKind::Generic});
	}
}

float creature_audio::CameraDistance(glm::vec3 camera, glm::vec3 creature)
{
	const float dx = camera.x - creature.x;
	const float dy = camera.y - creature.y;
	const float dz = camera.z - creature.z;
	const float depthAndHeight = (dz * dz) + (dy * dy);
	return std::sqrt(depthAndHeight + (dx * dx));
}
