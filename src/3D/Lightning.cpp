/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Lightning.h"

#include <algorithm>

namespace openblack::lightning
{

namespace
{
/// The dip, from just after 0.2 seconds to just before 0.5
[[nodiscard]] bool InDip(float time)
{
	return time < 0.5f && time > 0.2f;
}
} // namespace

Flash Strike(const glm::vec3& position, float radius, float strength)
{
	return {.position = position, .radius = radius, .strength = strength, .time = 0.0f, .active = true};
}

Flash Advance(Flash flash, float seconds)
{
	flash.time += seconds;
	// Only an active flash goes out
	if (flash.active && flash.time > k_FlashSeconds)
	{
		flash.active = false;
	}
	return flash;
}

float Brightness(const Flash& flash)
{
	if (!flash.active)
	{
		return 0.0f;
	}
	float brightness = 1.0f - flash.time;
	if (InDip(flash.time))
	{
		brightness = 0.1f;
	}
	return brightness * flash.strength;
}

float GlowStrength(const Flash& flash)
{
	if (!flash.active)
	{
		return 0.0f;
	}
	const float fade = 1.0f - flash.time;
	float glow = fade * fade * fade;
	if (InDip(flash.time))
	{
		glow = 0.1f;
	}
	return glow * flash.strength;
}

uint8_t LandLightFlash(float brightness)
{
	const float f = std::clamp(brightness, 0.0f, 1.0f);
	return static_cast<uint8_t>(static_cast<int>(f * 255.0f));
}

} // namespace openblack::lightning
