/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePhysiology.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesires.h"

using namespace openblack;
using namespace openblack::creature_physiology;

namespace
{
constexpr float k_SecondsPerMinute = 60.0f;
constexpr float k_MinutesPerHour = 60.0f;
/// Carrying for the species' minutes makes it this much stronger
constexpr float k_CarryStrength = 0.5f;
/// After an action, a creature counts as at most this many times bigger than size 0 for what it costs
constexpr float k_MaxActionSizeDivisor = 3.0f;

float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

Needs creature_physiology::Start(const Species& species)
{
	return {.age = 0,
	        .turns = 0,
	        .warmth = species.startWarmth,
	        .energy = species.startEnergy,
	        .itchiness = 0.0f,
	        .poo = 0.0f,
	        .exhaustion = 0.0f,
	        .dehydration = 0.0f,
	        .life = 1.0f,
	        .meals = 0};
}

bool creature_physiology::SlowsEnergy(std::optional<uint32_t> action)
{
	return action == actions::k_SleepAtHome || action == actions::k_RestToGetBetter;
}

bool creature_physiology::GrowsAsleep(std::optional<uint32_t> action)
{
	return action == actions::k_SleepAtHome || action == actions::k_SleepByObject;
}

float creature_physiology::Hunger(const Needs& needs)
{
	return 1.0f - needs.energy;
}

float creature_physiology::Growth(const Needs& needs, const Species& species, bool asleep, float turnsPerSecond)
{
	const auto minutes = species.growUpMinutes;
	if (minutes <= 0.0f || turnsPerSecond <= 0.0f)
	{
		return k_MinGrowth;
	}
	const auto grownAge = minutes / k_MinutesPerHour;
	const auto age = std::min(static_cast<float>(needs.age), grownAge);
	const auto youth = std::clamp((1.0f - (age / grownAge)) * k_YouthGrowthSlope, 0.0f, k_MaxYouthGrowth);
	const auto spare = Clamp01(needs.energy - needs.exhaustion);
	auto growth = std::max(spare * youth / (minutes * k_SecondsPerMinute * turnsPerSecond), k_MinGrowth);
	if (asleep)
	{
		growth *= k_SleepGrowthFactor;
	}
	return growth;
}

void creature_physiology::Grow(Shape& shape, float growth)
{
	shape.size = std::clamp(shape.size + growth, 0.0f, k_MaxGrownSize);
}

void creature_physiology::TickTurn(Needs& needs, Shape& shape, const Species& species, const Turn& turn)
{
	++needs.turns;
	if (turn.phase < k_BodyPhase)
	{
		return;
	}
	// A year of its life goes by each so many seconds of game time, in whole turns a second: the turns a second are cut
	// to a whole number before they are multiplied, as the original does
	const auto agePeriod = static_cast<uint32_t>(turn.turnsPerSecond) * species.secondsPerAgeTick;
	if (agePeriod > 0 && needs.turns % agePeriod == 0)
	{
		++needs.age;
	}

	// Carrying something heavy about makes it stronger
	if (turn.carriedWeight.has_value() && turn.moving && species.carryStrengthMinutes > 0.0f)
	{
		const auto turnsToStrength = species.carryStrengthMinutes * k_SecondsPerMinute * turn.turnsPerSecond;
		ModifyStrength(shape, k_CarryStrength / turnsToStrength * Clamp01(*turn.carriedWeight));
	}

	// It grows whether it stands still or moves, and is put back to its full size if something made it bigger
	if (turn.phase >= k_GrowingPhase)
	{
		Grow(shape, Growth(needs, species, GrowsAsleep(turn.action), turn.turnsPerSecond));
	}

	shape.strength = Clamp01(shape.strength * species.strengthDecay);

	// It uses up its energy, more slowly the bigger it is and much more slowly asleep or resting
	auto divisor = std::clamp(1.0f + (k_EnergySizeFactor * std::clamp(shape.size, 0.0f, 2.0f)), 1.0f, 2.0f);
	if (SlowsEnergy(turn.action) || turn.resting)
	{
		divisor += k_RestingEnergyDivisor;
	}
	needs.energy = Clamp01(needs.energy - (species.energyDrain / divisor));
	// Hungry, it burns its fat
	if (needs.energy < k_FatBurnBelowEnergy)
	{
		shape.fatness = Clamp01(shape.fatness - species.fatBurn);
	}

	// Moving tires it, young creatures fastest, and faster again with too little energy
	if (turn.moving)
	{
		const auto rate = std::max(k_YouthExhaustion - (k_ExhaustionYouthPerAge * static_cast<float>(needs.age)), 1.0f) *
		                  species.exhaustionRate;
		auto tiring = rate;
		if (needs.energy < species.lowEnergyThreshold)
		{
			tiring += k_LowEnergyExtraExhaustion * rate;
		}
		needs.exhaustion = Clamp01(needs.exhaustion + tiring);
	}

	if (turn.phase >= k_GrowingPhase)
	{
		// The turns it takes to dehydrate are cut to whole numbers as the original does: the turns a second before the
		// multiply, the product after it, a product out of the int32 range counting as INT32_MIN. Cut to none, it is
		// parched in a single turn.
		const auto wholeTurnsPerSecond = static_cast<float>(static_cast<uint32_t>(turn.turnsPerSecond));
		const auto turnsToDehydrate = static_cast<uint32_t>(map_coords::FtoL(wholeTurnsPerSecond * species.secondsToDehydrate));
		const auto thirst =
		    turnsToDehydrate > 0 ? 1.0f / static_cast<float>(turnsToDehydrate) : std::numeric_limits<float>::infinity();
		needs.dehydration = Clamp01(needs.dehydration + thirst);
	}

	// Far from the temperature it likes, it warms or cools; nothing brings it back otherwise
	const auto difference = turn.temperature - species.comfortTemperature;
	const auto change = gutils::SigmoidThreshold(k_WarmthThreshold, std::abs(difference) * k_WarmthPerDegree);
	needs.warmth = std::clamp(needs.warmth + (difference > 0.0f ? change : -change), -1.0f, 1.0f);
}

void creature_physiology::ModifyStrength(Shape& shape, float amount)
{
	shape.strength = Clamp01(shape.strength + amount);
}

void creature_physiology::ApplyActionCost(Needs& needs, Shape& shape, const ActionCost& cost, uint32_t phase)
{
	ModifyStrength(shape, cost.strengthGain);
	if (phase < k_BodyPhase)
	{
		return;
	}
	const auto divisor = std::clamp(shape.size + 1.0f, 1.0f, k_MaxActionSizeDivisor);
	needs.energy = Clamp01(needs.energy - (cost.energyCost / divisor));
	needs.exhaustion = std::min(needs.exhaustion + (cost.exhaustionCost / divisor), 1.0f);
}

float creature_physiology::Eat(Needs& needs, Shape& shape, const Species& species, float foodValue)
{
	const auto mealSize = std::min(shape.size, k_MaxMealSize) * species.foodToEnergy;
	const auto energy = mealSize > 0.0f ? std::max(foodValue, 0.0f) / mealSize : 0.0f;
	// What it eats beyond full makes it fat
	const auto over = (energy * species.overeatFatFactor) + needs.energy - 1.0f;
	if (over > 0.0f)
	{
		shape.fatness = Clamp01(shape.fatness + over);
	}
	needs.energy = std::clamp(needs.energy + energy, 0.0f, std::max(1.0f, shape.size));
	needs.poo = Clamp01(needs.poo + (Clamp01(energy) * species.pooPerEnergy));
	++needs.meals;
	return energy;
}

void creature_physiology::Drink(Needs& needs)
{
	needs.dehydration = 0.0f;
}

void creature_physiology::Poo(Needs& needs)
{
	needs.poo = 0.0f;
}

float creature_physiology::LongEnoughAsleep(const Species& species, float size, float dayCycleSeconds)
{
	// In this order, each product a float as the game thread's arithmetic rounds it
	return size * species.sleepLength * dayCycleSeconds * k_SleepTurnsPerLength;
}

bool creature_physiology::SleepTurn(Needs& needs, const Species& species, float size, uint32_t turnsAsleep, bool night,
                                    float dayCycleSeconds)
{
	const auto longEnough = LongEnoughAsleep(species, size, dayCycleSeconds);
	needs.life = Clamp01(needs.life + species.sleepHeal);
	needs.exhaustion = Clamp01(needs.exhaustion - species.sleepRecover);
	if (turnsAsleep <= k_MinSleepTurns)
	{
		return false;
	}
	return (needs.exhaustion <= 0.0f && !night) ||
	       (needs.exhaustion < k_NearlyRested && static_cast<float>(turnsAsleep) >= longEnough);
}

void creature_physiology::SitTurn(Needs& needs, const Species& species)
{
	needs.exhaustion = Clamp01(needs.exhaustion - (k_SittingRecovery * species.sleepRecover));
}

std::optional<Faint> creature_physiology::ShouldFaint(const Needs& needs, uint32_t phase, const FaintGates& gates)
{
	if (phase < k_FaintingPhase || !gates.ownedByPlayer || gates.computerPlayer || gates.scriptControlled)
	{
		return std::nullopt;
	}
	if (needs.life <= 0.0f)
	{
		return Faint::OutOfLife;
	}
	if (needs.energy <= 0.0f)
	{
		return Faint::Starving;
	}
	if (needs.exhaustion >= 1.0f)
	{
		return Faint::Exhausted;
	}
	return std::nullopt;
}

void creature_physiology::WakeFromFaint(Needs& needs)
{
	needs.exhaustion = std::min(needs.exhaustion, k_FaintWakeExhaustion);
	needs.dehydration = std::min(needs.dehydration, k_FaintWakeDehydration);
	needs.energy = std::max(needs.energy, k_FaintWakeEnergy);
}

std::optional<float> creature_physiology::SourceValue(uint32_t type, const Needs& needs, bool night)
{
	namespace sources = creature_desires::sources;
	switch (type)
	{
	case sources::k_HungerFromLowEnergy:
		return Hunger(needs);
	case sources::k_PooFromAmountOfPoo:
		return needs.poo;
	case sources::k_TirednessFromExhaustion:
		return needs.exhaustion;
	case sources::k_FearFromDark:
	case sources::k_TirednessFromNight:
		return night ? 1.0f : 0.0f;
	case sources::k_WaterFromDehydration:
		return needs.dehydration;
	case sources::k_RestoreHealthFromLife:
		return 1.0f - needs.life;
	case sources::k_GetWarmer:
		return Clamp01(-needs.warmth);
	case sources::k_GetColder:
		return Clamp01(needs.warmth);
	case sources::k_Scratch:
		return needs.itchiness;
	default:
		return std::nullopt;
	}
}
