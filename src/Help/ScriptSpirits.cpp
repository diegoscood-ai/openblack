/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptSpirits.h"

#include "Common/HelpText.h"

namespace openblack::help::script_spirits
{

namespace
{
constexpr float k_AlignmentSteps = 7.0f;
constexpr float k_MostGood = 6.0f;
constexpr int32_t k_Neutral = 3;
constexpr uint32_t k_RandomHalf = 50;

enum ScriptSpirit : int32_t
{
	Evil = 2,
	Alignment = 3,
	AntiAlignment = 4,
	Random = 5,
};
} // namespace

int32_t DiscreteAlignment(float alignment)
{
	// The game runs the FPU at single precision, so each step rounds to a float
	float steps = (alignment - -1.0f) / (1.0f - -1.0f) * k_AlignmentSteps;
	// Not a number is held at the most good too
	if (!(steps < k_MostGood))
	{
		steps = k_MostGood;
	}
	return static_cast<int32_t>(steps);
}

int32_t HelpSpiritOf(int32_t scriptSpirit, int32_t discreteAlignment, const std::function<uint32_t()>& rand100)
{
	switch (scriptSpirit)
	{
	case Evil:
		return k_EvilSpirit;
	case Alignment:
		return discreteAlignment < k_Neutral ? k_EvilSpirit : k_GoodSpirit;
	case AntiAlignment:
		return discreteAlignment >= k_Neutral ? k_EvilSpirit : k_GoodSpirit;
	case Random:
		return (rand100 ? rand100() : 0) > k_RandomHalf ? k_EvilSpirit : k_GoodSpirit;
	default:
		return k_GoodSpirit;
	}
}

int32_t SpiritWhoTalks(int32_t narrator)
{
	if (narrator == helptext::k_NarratorGoodSpirit)
	{
		return k_GoodSpirit;
	}
	return narrator == helptext::k_NarratorEvilSpirit ? k_EvilSpirit : 0;
}

bool IsScreenFraction(float value)
{
	return value >= 0.0f && !(value > 1.0f);
}

} // namespace openblack::help::script_spirits
