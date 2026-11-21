/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenFade.h"

#include "GameClock.h"

using namespace openblack;
using namespace openblack::gui;

namespace
{
/// The sequence mode that runs neither fade
constexpr int32_t k_SequenceModeNoFade = 3;
/// The frame delta is in milliseconds, the fade speed in seconds
constexpr float k_MsToSeconds = 0.001f;
/// The fade value (0..1) to an alpha byte
constexpr float k_AlphaScale = 255.0f;
} // namespace

void ScreenFade::FadeFrom(float amount)
{
	current = amount;
	target = 0.0f;
}

void ScreenFade::FadeFrom(float amount, uint32_t colourRgb)
{
	FadeFrom(amount);
	rgb = colourRgb;
	done = 0;
}

void ScreenFade::FadeThrough(uint32_t colourRgb)
{
	current = 0.0f;
	target = 1.0f;
	rgb = colourRgb;
	done = 0;
}

bool ScreenFade::Runs(int32_t mode) const noexcept
{
	if (mode == k_SequenceModeNoFade)
	{
		return false;
	}
	// Runs while the fade has not reached its target, and always in the citadel's mode; otherwise the script fade runs
	return target != current || mode == game_clock::k_SequenceModeCitadel;
}

uint32_t ScreenFade::Update(uint32_t deltaMs) noexcept
{
	// The original runs its floating point at single precision, so every step here rounds to float, as float
	// arithmetic does. (inferred) that nothing raises the precision again between frames
	const float step = static_cast<float>(deltaMs) * k_MsToSeconds;
	uint32_t colourRgb = rgb;
	if (current < target)
	{
		const float value = current + step;
		current = value;
		// Only past the target: landing on it exactly leaves target == current, the fade then stops running and is
		// held, and `done` never goes up
		if (value > target)
		{
			++done;
			current = target;
			target = 0.0f;
		}
	}
	else
	{
		const float value = current - step;
		current = value;
		if (value < target)
		{
			current = target;
			++done;
			if (target <= 0.0f)
			{
				rgb = 0;
				colourRgb = 0;
			}
		}
	}
	// 0xFF above 1.0, else current * 255 rounded to float and truncated
	const uint32_t alpha = current > 1.0f ? 0xFFu : static_cast<uint32_t>(static_cast<int32_t>(current * k_AlphaScale));
	// alpha in the top byte over the colour's RGB, for the script fade
	return (alpha << 24) + (colourRgb & 0x00FFFFFFu);
}
