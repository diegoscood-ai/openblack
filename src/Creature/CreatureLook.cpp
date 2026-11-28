/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLook.h"

#include <cmath>

#include <algorithm>
#include <array>


using namespace openblack;
using namespace openblack::creature_look;

namespace
{
constexpr std::array<float, 11> k_Interest {0.9f, 0.9f, 0.85f, 0.7f, 0.5f, 0.4f, 0.3f, 0.25f, 0.9f, 0.4f, 0.0f};
/// The land the creature looks over is a square of map cells, this many metres across each
constexpr float k_CellMetres = 10.0f;
/// The square is the odd number of cells nearest below the square root of this much per size, and this much more
constexpr float k_CellsSquaredPerSize = 60.0f;
constexpr float k_CellsSquaredAtSizeZero = 160.0f;
} // namespace

float creature_look::InterestOf(Interest kind)
{
	return k_Interest.at(static_cast<size_t>(kind));
}

float creature_look::LookRange(float size)
{
	const auto root = std::sqrt(std::max((k_CellsSquaredPerSize * size) + k_CellsSquaredAtSizeZero, 0.0f));
	const auto halfCells = static_cast<uint32_t>(root) / 2;
	return k_CellMetres * static_cast<float>((halfCells * 2) + 1);
}

uint32_t creature_look::SweepCells(float shownSize)
{
	// Worked out as the game does, in the FPU's extended precision, then truncated
	const auto across = static_cast<uint32_t>(
	    static_cast<int32_t>(std::sqrt((static_cast<double>(shownSize) * static_cast<double>(k_CellsSquaredPerSize)) +
	                                   static_cast<double>(k_CellsSquaredAtSizeZero))));
	const auto side = ((across >> 1) * 2) + 1;
	return side * side;
}

float creature_look::DistanceFactor(float distance, float range)
{
	// A distance that is not a number gives 1 - distance / range, as the game's compare does
	return distance < range * 0.5f ? 1.0f : 1.0f - (distance / range);
}

float creature_look::WeightOf(const Weights& weights, Sight sight)
{
	switch (sight)
	{
	case Sight::Mountains:
		return weights.mountains;
	case Sight::Water:
		return weights.water;
	case Sight::Thing:
	default:
		return weights.things;
	}
}

bool creature_look::Watches(const Target& target)
{
	return target.id.has_value() || target.place.has_value();
}

float creature_look::Hold(const Target& target, float distance, float range, float weight)
{
	if (!Watches(target))
	{
		return 0.0f;
	}
	// The game keeps the distance from 0 to the range. A distance below 0 or that is not a number gives 0.
	float held = distance;
	if (!(held >= 0.0f))
	{
		held = 0.0f;
	}
	else if (held > range)
	{
		held = range;
	}
	return weight * target.interest * DistanceFactor(held, range);
}

float creature_look::Catches(const Target& target, float weight, float interest, float distance, float range,
                             uint32_t turnsPerSecond)
{
	const auto watchedSeconds = turnsPerSecond > 0 ? target.watchedTurns / turnsPerSecond : 0u;
	const auto boredom =
	    1.0f - (static_cast<float>(std::min(watchedSeconds, k_BoredSeconds)) / static_cast<float>(k_BoredSeconds));
	return boredom * weight * DistanceFactor(distance, range) * interest;
}

namespace
{
/// Whether a sight that catches the eye this much takes over what the creature watches
bool TakesOver(const Target& target, float catches, float watchedDistance, float range, const Weights& weights)
{
	return !Watches(target) || Hold(target, watchedDistance, range, WeightOf(weights, target.sight)) < catches;
}
} // namespace

bool creature_look::Consider(Target& target, const Candidate& candidate, float watchedDistance, float range,
                             const Weights& weights, uint32_t turnsPerSecond)
{
	const auto interest = InterestOf(candidate.kind);
	const auto catches = Catches(target, weights.things, interest, candidate.distance, range, turnsPerSecond);
	if (!TakesOver(target, catches, watchedDistance, range, weights))
	{
		return false;
	}
	target = {.id = candidate.id, .sight = Sight::Thing, .interest = interest, .point = target.point, .watchedTurns = 0};
	return true;
}

bool creature_look::ConsiderLand(Target& target, const LandCandidate& candidate, float watchedDistance, float range,
                                 const Weights& weights, uint32_t turnsPerSecond)
{
	const auto catches =
	    Catches(target, WeightOf(weights, candidate.sight), k_LandInterest, candidate.distance, range, turnsPerSecond);
	if (!TakesOver(target, catches, watchedDistance, range, weights))
	{
		return false;
	}
	target = {.sight = candidate.sight,
	          .place = candidate.place,
	          .interest = k_LandInterest,
	          .point = target.point,
	          .watchedTurns = 0};
	return true;
}

void creature_look::Watch(Target& target, bool inView)
{
	if (!Watches(target))
	{
		return;
	}
	if (inView)
	{
		++target.watchedTurns;
	}
	else
	{
		target = {};
	}
}

glm::vec3 creature_look::LookAboutPoint(const Target& target, glm::vec3 destination)
{
	return Watches(target) ? target.point : destination;
}
