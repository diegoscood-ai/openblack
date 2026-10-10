/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandMorph.h"

#include <cassert>
#include <cmath>

#include <algorithm>

using namespace openblack;

float hand_morph::Target(float playerAlignment)
{
	return std::clamp(playerAlignment, -1.0f, 1.0f);
}

std::optional<float> hand_morph::Refresh(float drawn, float target)
{
	if (std::abs(target - drawn) >= k_RefreshThreshold)
	{
		return target;
	}
	return std::nullopt;
}

hand_morph::Change hand_morph::Advance(State& state, float playerAlignment, std::optional<bool> inInfluence)
{
	Change change;
	// A change of the interface's texture set blends the skin again, at the alignment it is drawn at
	if (inInfluence.has_value() && *inInfluence != state.inInfluence)
	{
		state.inInfluence = *inInfluence;
		change.skin = state.drawn;
	}
	state.target = Target(playerAlignment);
	if (const auto drawn = Refresh(state.drawn, state.target))
	{
		state.drawn = *drawn;
		change.skin = *drawn;
		change.shape = true;
	}
	return change;
}

hand_morph::Look hand_morph::LookOf(float drawn)
{
	return drawn < 0.0f ? Look::Evil : Look::Good;
}

float hand_morph::Weight(float drawn)
{
	return std::abs(drawn);
}

uint32_t hand_morph::BlendWeight(float drawn)
{
	return std::min<uint32_t>(static_cast<uint32_t>(std::abs(drawn) * 256.0f), 255u);
}

uint16_t hand_morph::BlendTexel(uint16_t from, uint16_t to, uint32_t weight)
{
	uint32_t out = 0;
	for (const uint32_t mask : {0xFu, 0xF0u, 0xF00u, 0xF000u})
	{
		out |= (((from & mask) * (255u - weight) + (to & mask) * weight) / 255u) & mask;
	}
	return static_cast<uint16_t>(out);
}

void hand_morph::BlendSkin(std::span<const uint16_t> base, std::span<const uint16_t> look, float drawn,
                           std::span<uint16_t> blended)
{
	assert(base.size() == look.size() && look.size() == blended.size());
	const auto weight = BlendWeight(drawn);
	std::ranges::transform(base, look, blended.begin(),
	                       [weight](uint16_t from, uint16_t to) { return BlendTexel(from, to, weight); });
}
