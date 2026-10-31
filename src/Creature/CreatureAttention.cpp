/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureAttention.h"

#include <cmath>

#include <algorithm>

#include "Common/GUtilsX87.h"

using namespace openblack;

namespace
{
/// A minute's share of each second, as a float
constexpr float k_PerMinute = 1.0f / 60.0f;
/// Two minutes' share of each second, as a float
constexpr float k_PerTwoMinutes = 1.0f / 120.0f;

/// The length of a vector as the game sums it: z first, then y, then x, each step rounded to a float
float LengthZFirst(const glm::vec3& v)
{
	return std::sqrt(((v.z * v.z) + (v.y * v.y)) + (v.x * v.x));
}
} // namespace

bool creature_attention::IsLookingAt(const glm::vec3& origin, const glm::vec3& focus, const glm::vec3& creature)
{
	// Worked out step by step as the game's FPU does at a float's precision: the way the camera looks made a unit
	// vector by its length's reciprocal, then the way to the creature the same way, its distance kept as a float
	auto ahead = focus - origin;
	if (ahead != glm::vec3(0.0f))
	{
		ahead *= 1.0f / LengthZFirst(ahead);
	}
	auto toCreature = creature - origin;
	float distance = 0.0f;
	if (toCreature != glm::vec3(0.0f))
	{
		distance = LengthZFirst(toCreature);
		toCreature *= 1.0f / distance;
	}
	if (!(distance < k_LookingRange))
	{
		return false;
	}
	// A creature right at the camera, or a camera looking nowhere, gives nothing ahead: never more than the cosine
	const float dot = ((toCreature.z * ahead.z) + (toCreature.y * ahead.y)) + (toCreature.x * ahead.x);
	// The game compares with the cosine as the FPU leaves it, not rounded to a float
	return static_cast<double>(dot) > std::cos(static_cast<double>(k_LookingAngle));
}

double creature_attention::Share(uint32_t turnsLookedAt, uint32_t turnsLived)
{
	if (turnsLived == 0)
	{
		return 0.0;
	}
	// The lived turns are counted as a signed number when divided
	return gutils::x87::Div(static_cast<double>(turnsLookedAt), static_cast<double>(static_cast<int32_t>(turnsLived)));
}

int32_t creature_attention::HowMuch(double share)
{
	constexpr uint32_t k_Most = 4;
	const auto level = static_cast<int64_t>(gutils::x87::Mul(share, 5.0));
	// Counted without a sign, a level below nothing is the most
	const auto counted = static_cast<uint32_t>(level);
	return static_cast<int32_t>(counted > k_Most ? k_Most : counted);
}

void creature_attention::NoteTurn(bool lookedAt, uint32_t& turnsLookedAt, uint32_t& turnsNotLookedAt)
{
	if (lookedAt)
	{
		++turnsLookedAt;
		turnsNotLookedAt = 0;
	}
	else
	{
		++turnsNotLookedAt;
	}
}

uint32_t creature_attention::WholeSeconds(uint32_t turns, uint32_t msPerTurn)
{
	const uint32_t turnsPerSecond = msPerTurn > 0 ? 1000 / msPerTurn : 0;
	return turnsPerSecond > 0 ? turns / turnsPerSecond : 0;
}

float creature_attention::Loneliness(uint32_t turnsNotLookedAt, uint32_t msPerTurn)
{
	// The whole seconds times a minute's share of each, at a float's precision; past a minute it is whole
	return std::min(static_cast<float>(WholeSeconds(turnsNotLookedAt, msPerTurn)) * k_PerMinute, 1.0f);
}

float creature_attention::LackOfInteraction(uint32_t turnsSinceMadeToAct, uint32_t msPerTurn)
{
	return std::min(static_cast<float>(WholeSeconds(turnsSinceMadeToAct, msPerTurn)) * k_PerTwoMinutes, 1.0f);
}

bool creature_attention::IgnoredByPlayer(uint32_t turnsNotLookedAt, uint32_t msPerTurn)
{
	constexpr uint32_t k_IgnoredSeconds = 120;
	if (msPerTurn == 0)
	{
		return false;
	}
	// The turns of two minutes, at whole turns a second (none when a turn lasts more than a second)
	const uint32_t limit = (1000 / msPerTurn) * k_IgnoredSeconds;
	return limit < turnsNotLookedAt;
}
