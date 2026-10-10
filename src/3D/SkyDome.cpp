/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyDome.h"

#include <algorithm>

namespace openblack::sky_dome
{

namespace
{
/// A fraction of 255, cut to a whole number
uint8_t Weight(float fraction)
{
	return static_cast<uint8_t>(static_cast<int>(fraction * 255.0f));
}
} // namespace

Pair AlignmentPair(float alignment)
{
	if (alignment > 1.0f)
	{
		return {.lower = 1, .upper = 2, .weight = Weight(std::min(alignment, 2.0f) - 1.0f)};
	}
	return {.lower = 0, .upper = 1, .weight = Weight(std::max(alignment, 0.0f))};
}

uint8_t Darkness(float alignment)
{
	// 1 when good, a half when neutral, 0 when evil
	const float goodness = 1.0f - (alignment * 0.5f);
	if (!(goodness > 0.6f))
	{
		return 0;
	}
	// Each step rounded to a float, as the game's arithmetic is
	const auto darkness = static_cast<int>((goodness - 0.6f) * 225.0f);
	return static_cast<uint8_t>(std::clamp(darkness, 0, 90));
}

float ThroughOvercast(float alpha, float overcast, bool fog)
{
	const auto whole = static_cast<int>(alpha);
	if (!fog || !(overcast > 0.0f))
	{
		return static_cast<float>(whole);
	}
	return static_cast<float>(static_cast<int>(static_cast<float>(whole) / ((std::min(overcast, 1.0f) * 8.0f) + 1.0f)));
}

Tint TintOf(const TintInputs& inputs)
{
	// The overcast is held to 1 at most; one below 0 is used as it is
	const int overcast = static_cast<int>(std::min(inputs.overcast, 1.0f) * 255.0f);
	// Both colours lose this much of 256 to the darkness, rounded up
	const int dark = inputs.weather ? (inputs.darkness * inputs.darkness) / 150 : 0;
	const int halfFlash = inputs.flash / 2;
	Tint tint;
	for (glm::length_t c = 0; c < 3; ++c)
	{
		int modulate = 255;
		int add = 0;
		if (inputs.fog)
		{
			// From white towards the haze's colour, and the haze's colour added, by the overcast. Each is kept to its
			// 8-bit channel, which only cuts anything when the overcast is below 0
			const int haze = static_cast<int>(inputs.hazeColour[c]);
			modulate = (255 + (((haze - 255) * overcast) >> 8)) & 0xFF;
			add = ((haze * overcast) >> 8) & 0xFF;
		}
		modulate += (-modulate * dark) >> 8;
		add += (-add * dark) >> 8;
		// A flash takes the first towards white, and the second half as far
		modulate += ((255 - modulate) * inputs.flash) >> 8;
		add += ((255 - add) * halfFlash) >> 8;
		tint.modulate[c] = static_cast<uint8_t>(modulate);
		tint.add[c] = static_cast<uint8_t>(add);
	}
	return tint;
}

} // namespace openblack::sky_dome
