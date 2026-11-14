/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AltarNeeds.h"

using namespace openblack::worship;

namespace
{
/// Each person at the site who does not dance fills a hundredth of the hiding sign
constexpr float k_HidingShare = 0.01f;
} // namespace

float altar_needs::AverageLifeDesired(std::span<const std::vector<float>> groups)
{
	// one float operation per statement, as the original rounds each step: the groups in order, each one's dancers in
	// order
	float sum = 0.0f;
	uint32_t count = 0;
	for (const auto& group : groups)
	{
		for (const float life : group)
		{
			const float want = 1.0f - life;
			sum = want + sum;
		}
		count += static_cast<uint32_t>(group.size());
	}
	if (count == 0)
	{
		return 0.0f;
	}
	return sum / static_cast<float>(count);
}

float altar_needs::DesireForRest(float average)
{
	// more than 1 is held at 1; anything else (a NaN too) is kept
	if (average > 1.0f)
	{
		return 1.0f;
	}
	return average;
}

float altar_needs::PeopleHidingIndicator(int32_t villagersAtSite, int32_t dancers)
{
	// the difference as an unsigned count: fewer at the site than dance wraps around to a great many
	const uint32_t hiding = static_cast<uint32_t>(villagersAtSite) - static_cast<uint32_t>(dancers);
	const float shown = static_cast<float>(hiding) * k_HidingShare;
	// more than 1 (or unordered) is held at 1
	if (!(shown <= 1.0f))
	{
		return 1.0f;
	}
	return shown;
}
