/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Rain.h"

#include <algorithm>

namespace openblack::rain
{

namespace
{
/// The half width of the square a streak stands in, about the block's centre
constexpr float k_Spread = 160.0f;
constexpr float k_Slant = 15.0f;
/// How far through its life a streak goes in a second
constexpr float k_LifeRate = 2.4f;
/// How far the rain's fall moves towards a storm's in a frame, and its bounds
constexpr float k_FollowRate = 0.3f;
constexpr float k_LowestHeight = 40.0f;
constexpr float k_HighestHeight = 640.0f;
constexpr float k_SlowestFall = 0.3f;
constexpr float k_FastestFall = 5.0f;
/// The streaks fade in over this much of their life, and out from the second
constexpr float k_FadeIn = 0.05f;
constexpr float k_FadeOut = 0.95f;

/// Drops what is left of a number past its whole part
float Fraction(float value)
{
	return value - static_cast<float>(static_cast<int32_t>(value));
}
} // namespace

Streak Place(const Random& random)
{
	Streak streak;
	streak.x = random(-k_Spread, k_Spread) * 0.5f;
	streak.z = random(-k_Spread, k_Spread) * 0.5f;
	streak.dx = random(-k_Slant, k_Slant);
	streak.dz = random(-k_Slant, k_Slant);
	streak.scroll = random(0.0f, 1.0f);
	streak.speed = random(0.1f, 0.2f);
	streak.phase = random(0.0f, 1.0f);
	return streak;
}

void Step(std::span<Streak> streaks, float seconds, float fallSpeed, const Random& random)
{
	const float life = seconds * k_LifeRate;
	for (auto& streak : streaks)
	{
		streak.scroll += fallSpeed * streak.speed * seconds;
		if (streak.scroll > 1.0f)
		{
			streak.scroll = Fraction(streak.scroll);
		}
		streak.phase += life;
		if (streak.phase > 1.0f)
		{
			// A new life somewhere else
			streak.phase = Fraction(streak.phase);
			streak.dx = random(-k_Slant, k_Slant);
			streak.dz = random(-k_Slant, k_Slant);
			streak.x = random(-k_Spread, k_Spread) * 0.5f;
			streak.z = random(-k_Spread, k_Spread) * 0.5f;
		}
	}
}

Fall Follow(Fall current, std::optional<Fall> storm)
{
	const auto target = storm.value_or(Fall {});
	current.height += (target.height - current.height) * k_FollowRate;
	current.speed += (target.speed - current.speed) * k_FollowRate;
	current.speed = !(current.speed > k_SlowestFall) ? k_SlowestFall : std::min(current.speed, k_FastestFall);
	current.height = !(current.height > k_LowestHeight) ? k_LowestHeight : std::min(current.height, k_HighestHeight);
	return current;
}

float PhaseFade(float phase)
{
	if (phase < k_FadeIn)
	{
		return phase * 20.0f;
	}
	if (phase > k_FadeOut)
	{
		return (1.0f - phase) * 20.0f;
	}
	return 1.0f;
}

std::array<glm::vec3, 2> Ends(const Streak& streak, float height)
{
	return {glm::vec3(streak.x, k_Bottom, streak.z), glm::vec3(streak.x + streak.dx, height, streak.z + streak.dz)};
}

} // namespace openblack::rain
