/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBeliefMaths.h"

#include <cmath>

#include <algorithm>

namespace openblack::particles::town_belief
{

namespace
{
/// fmod's 2 pi: the float 2 pi widened to double, as the rule's wraps use it
constexpr double k_FmodTwoPi = 6.2831854820251465;
/// A fight's next wait and length are its usual ones times a random share between these
constexpr float k_RandomShareMin = 0.5f;
constexpr float k_RandomShareMax = 1.5f;
/// The tilt's sway: ((cos(sway) + 1) x 0.25 + 0.5), and the tilt goes on at 0.846 of the speed
constexpr float k_SwayDepth = 0.25f;
constexpr float k_SwayBase = 0.5f;
constexpr float k_TiltShare = 0.846f;
constexpr float k_MaximumDistanceScale = 10.0f;
constexpr float k_ByteMax = 255.0f;
/// The players with a symbol: the slots before the neutral one
constexpr int k_SymbolPlayers = 7;
/// The neutral player's type
constexpr int32_t k_NeutralType = 3;

float Lerp(float at0, float at1, float belief)
{
	return (at1 - at0) * belief + at0;
}

/// 0..1, 0 for not a number
float Clamped(float t)
{
	return t > 0.0f ? (t < 1.0f ? t : 1.0f) : 0.0f;
}

/// Rises from 0 to 1 halfway through and falls back to 0 at the end
float Swell(float t)
{
	const float c = Clamped(t);
	const float centred = (c + c) - 1.0f;
	return 1.0f - centred * centred;
}

/// Each sum fmod 2 pi. (approximate) the original keeps each sum in extended precision into the fmod and takes the cos
/// of the unrounded fmod result; here float sums and the cos of the stored float
float Wrapped(float angle)
{
	return static_cast<float>(std::fmod(static_cast<double>(angle), k_FmodTwoPi));
}
} // namespace

float Shown(float belief)
{
	return Clamped(belief);
}

int Rank(const std::array<float, k_PlayerCount>& beliefs, int player)
{
	const float mine = Shown(beliefs.at(static_cast<size_t>(player)));
	int rank = 0;
	for (int other = 0; other < static_cast<int>(k_PlayerCount); ++other)
	{
		const float theirs = Shown(beliefs.at(static_cast<size_t>(other)));
		if (other != player && (mine < theirs || (mine == theirs && other > player)))
		{
			++rank;
		}
	}
	return rank;
}

float DistanceScale(const Params& params, float distance)
{
	if (!params.useFixedScale)
	{
		return 1.0f;
	}
	const float scale = distance * params.fixedScale;
	if (scale < 1.0f || scale == 1.0f)
	{
		return 1.0f;
	}
	return scale < k_MaximumDistanceScale ? scale : k_MaximumDistanceScale;
}

Look Step(Symbol& symbol, const Params& params, float belief, int rank, float dt, float distanceScale,
          const std::function<float(float, float)>& random)
{
	const float shown = Shown(belief);
	Look look;
	look.scale = shown == 0.0f ? 0.0f : Lerp(params.scaleAt0, params.scaleAt1, shown);
	// the most believed stands still over the totem; the others turn at a speed across the ground
	float radius = 0.0f;
	float speed = 0.0f;
	if (rank != 0)
	{
		radius = Lerp(params.radiusAt0, params.radiusAt1, shown);
		speed = Lerp(params.speedAt0, params.speedAt1, shown) / radius;
		// the second most believed fights the first now and then. While its wait is > 0 it counts down by dt; then the
		// fight's clock goes on, and past the fight's length the next wait and length are drawn, the wait first
		if (rank == 1)
		{
			if (symbol.untilFight > 0.0f)
			{
				symbol.untilFight -= dt;
			}
			else
			{
				symbol.intoFight += dt;
				if (symbol.fightLength < symbol.intoFight)
				{
					const float wait = Lerp(params.timeBetweenFightsAt0, params.timeBetweenFightsAt1, shown);
					symbol.untilFight = random(k_RandomShareMin, k_RandomShareMax) * wait;
					const float length = Lerp(params.fightDurationAt0, params.fightDurationAt1, shown);
					symbol.intoFight = 0.0f;
					symbol.fightLength = random(k_RandomShareMin, k_RandomShareMax) * length;
				}
				look.fight = Swell(symbol.intoFight / symbol.fightLength);
			}
			speed *= (params.speedUpDuringFight - 1.0f) * look.fight + 1.0f;
		}
	}
	const float level = static_cast<float>(rank) * params.heightPerLevel;
	symbol.sway = Wrapped(dt * params.angleSpeedPhaseSpeed + symbol.sway);
	const float swayed = (std::cos(symbol.sway) + 1.0f) * k_SwayDepth + k_SwayBase;
	symbol.tilt = Wrapped(swayed * speed * dt * k_TiltShare + symbol.tilt);
	symbol.around = Wrapped(speed * dt + symbol.around);
	// x = cos(around) cos(tilt) r, y = (1 - fight) level + sin(tilt) r (fighting, it comes down to the first's level),
	// z = sin(around) cos(tilt) r, each times the distance scale
	// (approximate) the original keeps z's product in extended precision until it adds the totem's point
	const float cosTilt = std::cos(symbol.tilt);
	look.offset = glm::vec3(std::cos(symbol.around) * cosTilt * radius * distanceScale,
	                        ((1.0f - look.fight) * level + std::sin(symbol.tilt) * radius) * distanceScale,
	                        std::sin(symbol.around) * cosTilt * radius * distanceScale);
	return look;
}

uint8_t SparkAlpha(float fight)
{
	return static_cast<uint8_t>(Swell(fight) * k_ByteMax);
}

bool Sparking(const Params& params, float fight)
{
	return fight > params.fightFracWhenEmitting;
}

std::vector<int> SymbolPlayers(const std::function<int32_t(int)>& typeOf)
{
	std::vector<int> players;
	int slot = 0;
	while (true)
	{
		if (!players.empty() && slot < static_cast<int>(k_PlayerCount) && typeOf(slot) == k_NeutralType)
		{
			break;
		}
		while (slot < k_SymbolPlayers && typeOf(slot) == 0)
		{
			++slot;
		}
		if (slot >= k_SymbolPlayers)
		{
			break;
		}
		players.push_back(slot);
		++slot;
	}
	return players;
}

} // namespace openblack::particles::town_belief
