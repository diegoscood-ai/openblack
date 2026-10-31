/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSwap.h"

#include <cmath>

#include <algorithm>

#include "Common/ModelInverseSquareRoot.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_swap;

std::optional<Bonus> creature_swap::BonusOf(int32_t species)
{
	switch (species)
	{
	case 3:
		return Bonus::Leopard;
	case 6:
		return Bonus::Horse;
	case 14:
		return Bonus::Mandrill;
	case 15:
		return Bonus::Rhino;
	case 16:
		return Bonus::Gorilla;
	default:
		return std::nullopt;
	}
}

bool creature_swap::IsAvailable(int32_t species, const Unlocked& unlocked)
{
	const auto bonus = BonusOf(species);
	return !bonus.has_value() || unlocked.test(static_cast<size_t>(*bonus));
}

bool creature_swap::IsSettled(const Shape& now, const Shape& target)
{
	// Each gap is a float, as the game's single precision works it out
	const float sizeGap = target.size - now.size;
	const float alignmentGap = target.alignment - now.alignment;
	return std::abs(static_cast<double>(sizeGap)) < k_Settled && std::abs(static_cast<double>(alignmentGap)) < k_Settled;
}

Shape creature_swap::Ease(const Shape& now, const Shape& target)
{
	// Every step rounds to a float, as the game's single precision does: the gap, its share, then the sum
	const float sizeGap = target.size - now.size;
	const float alignmentGap = target.alignment - now.alignment;
	const float sizeStep = sizeGap * k_EaseShare;
	const float alignmentStep = alignmentGap * k_EaseShare;
	const float size = sizeStep + now.size;
	const float alignment = alignmentStep + now.alignment;
	return {.size = size, .alignment = std::clamp(alignment, -1.0f, 1.0f)};
}

uint32_t creature_swap::RescaledSightings(uint32_t seen, float neededBefore, float neededAfter)
{
	const auto factor = neededBefore > 0.0f ? static_cast<double>(neededAfter) / static_cast<double>(neededBefore) : 1.0;
	const auto scaled = static_cast<double>(seen) * factor;
	return scaled <= 0.0 ? 0u : static_cast<uint32_t>(scaled);
}

bool creature_swap::HasPlayer(PlayerNames owner)
{
	return owner < PlayerNames::_COUNT;
}

PlayerNames creature_swap::BeamPlayer(PlayerNames owner, PlayerNames localPlayer)
{
	return HasPlayer(owner) ? owner : localPlayer;
}

bool creature_swap::IsPlasmaTurn(uint32_t gameTurn)
{
	return gameTurn % k_PlasmaEveryTurns == 0;
}

particles::PlasmaCommand creature_swap::SwapPlasma(glm::vec3 ownBone, glm::vec3 otherBone, uint32_t alphaDraw)
{
	// Each step a float, as the game's single precision works it out: the line, its length squared, then each part
	// times the inverse square root
	glm::vec3 line = otherBone - ownBone;
	const float zz = line.z * line.z;
	const float yy = line.y * line.y;
	const float xx = line.x * line.x;
	const float nearness = (zz + yy) + xx;
	if (nearness < k_PlasmaNearSquared)
	{
		line = glm::vec3(0.0f, 1.0f, 0.0f);
	}
	const float lengthSquared = ((line.y * line.y) + (line.z * line.z)) + (line.x * line.x);
	const float inverse = gutils::ModelInverseSquareRoot(lengthSquared);
	const glm::vec3 direction(line.x * inverse, line.y * inverse, line.z * inverse);
	return {
	    .start = ownBone,
	    .end = otherBone,
	    .startTangent = direction,
	    .endTangent = direction,
	    .life = k_PlasmaLife,
	    .speed = k_PlasmaSpeed,
	    .alpha = static_cast<uint8_t>(k_PlasmaLeastAlpha + alphaDraw),
	};
}
