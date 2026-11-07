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
/// The rain's strength of 100 for each percent
constexpr int32_t k_Opacity = 88;
/// A land block's side: a splash falls anywhere on its block
constexpr float k_BlockSide = 160.0f;
/// A splash is this far above the ground, and its half width grows to its size plus a random part of the variation
constexpr float k_SplashHeight = 1.0f;
constexpr float k_SplashGrowth = 0.5f;
constexpr float k_SplashGrowthVariation = 0.3f;
constexpr uint8_t k_SplashCell = 0x30;

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

int32_t Strength(int32_t wettest)
{
	return std::min(wettest * k_Opacity / 100, 0xFF);
}

std::optional<uint8_t> SplashAlpha(int32_t strength)
{
	if (strength <= k_LeastSplashStrength)
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>(std::clamp(strength * 255 / k_Opacity, 0, 255));
}

water_rings::Ring Splash(glm::vec2 centre, uint32_t argb, const Random& random, const HeightAt& heightAt)
{
	const float z = random(0.0f, k_BlockSide) + centre.y - (k_BlockSide * 0.5f);
	const float x = random(0.0f, k_BlockSide) + centre.x - (k_BlockSide * 0.5f);
	const float y = heightAt(glm::vec2(x, z)) + k_SplashHeight;
	const float growth = random(0.0f, k_SplashGrowthVariation) + k_SplashGrowth;
	return {.position = {x, y, z},
	        .growth = growth,
	        .angle = 0.0f,
	        .aspect = 1.0f,
	        .rate = 1.0f,
	        .cell = k_SplashCell,
	        .argb = argb};
}

std::array<glm::vec3, 2> Ends(const Streak& streak, float height)
{
	return {glm::vec3(streak.x, k_Bottom, streak.z), glm::vec3(streak.x + streak.dx, height, streak.z + streak.dz)};
}

} // namespace openblack::rain
