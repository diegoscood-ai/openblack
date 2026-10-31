/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScaffoldGhostRules.h"

#include <algorithm>

namespace openblack::ecs
{

std::optional<scaffold_ghost::HeldPose> scaffold_ghost::HeldPoseOf(const HeldPhantom& phantom, uint32_t visualMs,
                                                                   float spinPerTurn) noexcept
{
	// what is left of the fade: the timer less the part of the turn, the smaller of the two taken unsigned, so that a
	// timer below zero keeps the part and leaves nothing
	const uint32_t part = visualMs % k_TurnMs;
	const auto timer = static_cast<uint32_t>(phantom.fade);
	const auto left = static_cast<int32_t>(timer - std::min(part, timer));
	float scale = phantom.scale;
	if (left < 1)
	{
		// a shrunk ghost is not drawn; a grown one at its full size
		if (!phantom.appearing)
		{
			return std::nullopt;
		}
	}
	else if (phantom.appearing)
	{
		const float grown = static_cast<float>(left) * k_SizePerMs;
		scale = (1.0f - grown) * scale;
	}
	else
	{
		const float shrinking = static_cast<float>(left) * scale;
		scale = shrinking * k_SizePerMs;
	}
	// the share of the turn drawn so far times a turn's spin, on from the turn's angle
	const float share = static_cast<float>(part) * k_TurnPerMs;
	const float spin = share * spinPerTurn;
	return HeldPose {.scale = scale, .yAngle = spin + phantom.yAngle};
}

uint8_t scaffold_ghost::HeldAlpha(uint32_t landLight) noexcept
{
	const float alpha = static_cast<float>(landLight >> 24u) * k_HeldAlphaPart;
	return static_cast<uint8_t>(static_cast<int32_t>(alpha));
}

} // namespace openblack::ecs
