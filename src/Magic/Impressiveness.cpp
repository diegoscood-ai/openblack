/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Impressiveness.h"

#include "Common/GUtilsDistance.h"

using namespace openblack;

float magic::ImpressiveValue(const ImpressionInputs& inputs)
{
	// The miracle's side first: the reaction's multiplier, its own value, the land's balance
	const float miracle = inputs.reactionMultiplier * inputs.impressiveValue * inputs.landBalance;
	// Then the watcher's side: the weariness, the power, the distance's share; and the two together
	const float watcher = inputs.boredom * inputs.power * gutils::DistanceChangeToBelief(inputs.distance, inputs.maxDistance);
	return watcher * miracle;
}

float magic::ReactionMultiplier(Reaction type, float tableMultiplier, std::optional<float> townDesire)
{
	if (type != Reaction::ReactToFood && type != Reaction::ReactToWood)
	{
		return tableMultiplier;
	}
	return townDesire.value_or(1.0f);
}

float magic::ShieldImpressiveValue(Reaction type, bool townAttackedByShieldPlayer, float spellValue)
{
	if (townAttackedByShieldPlayer && (type == Reaction::ReactToMagicShield || type == Reaction::ReactToMagicShieldStruck))
	{
		return 0.0f;
	}
	return type == Reaction::ReactToMagicShieldDestroyed ? spellValue * k_ShieldDestroyedImpressiveness : spellValue;
}

float magic::TownShare(float populationForUnmodifiedBelief, uint32_t people)
{
	// The people counted unsigned. (approximate) the original keeps the sums at the FPU's precision and rounds only the
	// share; here each step is a float
	return (populationForUnmodifiedBelief + k_TownShareSmall) / (static_cast<float>(people) + k_TownShareSmall);
}

float magic::ImpressionAlignment(float alignmentModifier, std::optional<float> townRawDesire)
{
	return townRawDesire.has_value() ? *townRawDesire * alignmentModifier : alignmentModifier;
}
