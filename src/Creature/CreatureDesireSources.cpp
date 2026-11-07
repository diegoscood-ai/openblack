/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDesireSources.h"

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_desire_sources;

namespace
{
/// The game multiplies by these, not divides
constexpr float k_PerMostHeldRadius = 1.0f / k_MostHeldRadius;
constexpr float k_PerMostHandMovement = 1.0f / k_MostHandMovement;

[[nodiscard]] float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

float creature_desire_sources::InterestNow(PlayerInterest& interest, const std::optional<glm::vec3>& hand,
                                           std::optional<float> heldRadius)
{
	if (!hand.has_value())
	{
		return 0.0f;
	}
	if (!interest.lastHand.has_value())
	{
		interest.lastHand = hand;
	}
	interest.handMovement =
	    std::clamp(interest.handMovement + glm::distance(*hand, *interest.lastHand), 0.0f, k_MostHandMovement);
	interest.handMovement *= k_HandMovementFade;
	interest.lastHand = hand;
	const auto held = heldRadius.has_value() ? std::clamp(*heldRadius, 0.0f, k_MostHeldRadius) * k_PerMostHeldRadius : 0.0f;
	return Clamp01(interest.handMovement * k_PerMostHandMovement + held);
}

void creature_desire_sources::UpdateInterest(PlayerInterest& interest, float now)
{
	interest.value = now > interest.value ? now : interest.value + (now - interest.value) * 0.5f;
}
