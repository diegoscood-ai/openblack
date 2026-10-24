/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StatisticsRules.h"

#include <algorithm>

namespace openblack::statistics
{

std::optional<MiracleKind> MiracleKindOf(MagicType type)
{
	switch (type)
	{
	case MagicType::Fireball:
	case MagicType::FireballPowerUpOne:
	case MagicType::FireballPowerUpTwo:
	case MagicType::LightningBolt:
	case MagicType::LightningBoltPowerUpOne:
	case MagicType::LightningBoltPowerUpTwo:
	case MagicType::ExplosionOne:
	case MagicType::ExplosionOnePuOne:
	case MagicType::ExplosionOnePuTwo:
	case MagicType::StormWindRain:
	case MagicType::StormWindRainLightning:
	case MagicType::Tornado:
	case MagicType::FlockGround:
		return MiracleKind::Aggressive;
	case MagicType::Heal:
	case MagicType::HealPowerUpOne:
	case MagicType::Teleport:
	case MagicType::Forest:
	case MagicType::Food:
	case MagicType::FoodPowerUpOne:
	case MagicType::Shield:
	case MagicType::PhysicalShield:
	case MagicType::Wood:
	case MagicType::Water:
	case MagicType::WaterPowerUpOne:
	case MagicType::FlockFlying:
		return MiracleKind::Nice;
	case MagicType::CreatureSpellFreeze:
	case MagicType::CreatureSpellSmall:
	case MagicType::CreatureSpellBig:
	case MagicType::CreatureSpellWeak:
	case MagicType::CreatureSpellStrong:
	case MagicType::CreatureSpellFat:
	case MagicType::CreatureSpellThin:
	case MagicType::CreatureSpellInvisible:
	case MagicType::CreatureSpellCompassion:
	case MagicType::CreatureSpellAngry:
	case MagicType::CreatureSpellItchy:
		return MiracleKind::Creature;
	default:
		return std::nullopt;
	}
}

uint32_t MiraclesCast(std::span<const uint32_t> castsOfType, MiracleKind kind)
{
	uint32_t total = 0;
	for (size_t type = 0; type < castsOfType.size(); ++type)
	{
		if (MiracleKindOf(static_cast<MagicType>(type)) == kind)
		{
			total += castsOfType[type];
		}
	}
	return total;
}

uint32_t TotalBuildings(std::span<const TownCounts> towns)
{
	uint32_t abodes = 0;
	uint32_t civic = 0;
	uint32_t total = 0;
	for (const auto& town : towns)
	{
		civic += town.civic;
		abodes += town.abodes;
		total += abodes + civic;
	}
	return total;
}

float TotalBelief(std::span<const float> beliefInPlayer)
{
	float belief = 0.0f;
	for (const auto inPlayer : beliefInPlayer)
	{
		belief = belief + inPlayer;
	}
	return belief * k_BeliefScale;
}

void SetTownFigures(PlayerFigures& figures, std::span<const TownCounts> towns)
{
	for (const auto& town : towns)
	{
		figures.men += town.men;
		figures.women += town.women;
		figures.abodes += town.abodes;
		figures.civic += town.civic;
		figures.wonders += town.wonders;
		figures.population += town.adults + town.children;
		// The places for adults and for children in its homes
		figures.capacity += town.adultPlaces + town.childPlaces;
	}
	figures.towns = static_cast<uint32_t>(towns.size());
	figures.totalBuildings = TotalBuildings(towns);
}

void SetCreatureFigures(PlayerFigures& figures, const std::optional<CreatureNow>& creature, float alignmentAtStart,
                        float sizeAtStart)
{
	if (!creature.has_value())
	{
		return;
	}
	if (creature->body.has_value())
	{
		const auto& body = *creature->body;
		figures.creatureAlignment = body.alignment;
		figures.creatureAlignmentChange = body.alignment - alignmentAtStart;
		// The creature's size the first time it was seen is the start of its growth
		if (sizeAtStart > 0.0f)
		{
			figures.creatureGrowth = (body.size / sizeAtStart) - 1.0f;
		}
	}
	constexpr float k_Hundred = 100.0f;
	if (creature->needs.has_value())
	{
		const auto& needs = *creature->needs;
		figures.creatureDamage = (1.0f - needs.life) * k_Hundred;
		figures.creatureHunger = (1.0f - needs.energy) * k_Hundred;
		figures.creatureTiredness = needs.exhaustion * k_Hundred;
		figures.creatureAge = needs.age;
	}
	figures.creature = creature->tallies;
}

int32_t Percent(float share)
{
	return static_cast<int32_t>(share * 100.0f);
}

WorldFigures World(const WorldInputs& inputs)
{
	WorldFigures figures;
	uint32_t men = 0;
	uint32_t women = 0;
	uint32_t housed = 0;
	uint32_t housedMen = 0;
	uint32_t wonders = 0;
	std::array<uint32_t, 13> disciples {};
	for (const auto& town : inputs.towns)
	{
		men += town.men;
		women += town.women;
		housed += town.housed;
		housedMen += town.housedMen;
		wonders += town.wonders;
		for (size_t i = 0; i < disciples.size(); ++i)
		{
			disciples.at(i) += town.disciples.at(i);
		}
	}
	const uint32_t believers = men + women;
	figures.population = static_cast<int32_t>(inputs.worldPopulation);
	figures.believersPercent = inputs.worldPopulation != 0 && believers != 0
	                               ? Percent(static_cast<float>(believers) / static_cast<float>(inputs.worldPopulation))
	                               : 0;
	// The share of men is of the people in the towns' homes and their homeless, as they stand
	figures.malePercent = housed != 0 ? Percent(static_cast<float>(housedMen) / static_cast<float>(housed)) : 0;
	figures.deaths = static_cast<int32_t>(inputs.deaths);
	figures.births = static_cast<int32_t>(inputs.births);
	figures.sacrifices = static_cast<int32_t>(inputs.sacrifices);
	figures.buildings = static_cast<int32_t>(TotalBuildings(inputs.towns));
	figures.wonders = static_cast<int32_t>(wonders);
	// Every kind of disciple in the player's towns but their missionaries, who count where they preach instead
	constexpr auto k_Missionary = static_cast<size_t>(VillagerDisciple::Missionary);
	uint32_t total = inputs.missionariesInOtherTowns;
	for (size_t i = 0; i < disciples.size(); ++i)
	{
		if (i != k_Missionary)
		{
			total += disciples.at(i);
		}
	}
	figures.disciples = static_cast<int32_t>(total);
	const auto kind = [&disciples](VillagerDisciple disciple) {
		return static_cast<int32_t>(disciples.at(static_cast<size_t>(disciple)));
	};
	figures.discipleKinds = {
	    kind(VillagerDisciple::Builder),   kind(VillagerDisciple::Breeder),  kind(VillagerDisciple::Fisherman),
	    kind(VillagerDisciple::Farmer),    kind(VillagerDisciple::Forester), static_cast<int32_t>(inputs.missionaries),
	    kind(VillagerDisciple::Craftsman), kind(VillagerDisciple::Trader),
	};
	return figures;
}

void FrameRateMeter::Frame(uint32_t milliseconds, uint32_t frames)
{
	if (!_started)
	{
		_started = true;
		_since = milliseconds;
	}
	const auto elapsed = milliseconds - _since;
	if (elapsed < k_LeastMilliseconds)
	{
		return;
	}
	// The frames are first noted once the first three seconds have gone, so the first rate comes later
	if (!_framesNoted)
	{
		_framesNoted = true;
		_framesSince = frames;
	}
	const auto drawn = frames - _framesSince;
	if (drawn < k_LeastFrames)
	{
		return;
	}
	const auto rate = static_cast<int32_t>((static_cast<int64_t>(drawn) * 1000) / static_cast<int64_t>(elapsed));
	if (rate < k_LeastRate)
	{
		return;
	}
	_rate = rate;
	_since = milliseconds;
	_framesSince = frames;
}

HelpFigures HelpFiguresFrom(const std::function<uint32_t(HelpEvent)>& count)
{
	constexpr float k_DraggedPerCount = 3.0f;
	constexpr float k_ZoomsPerCount = 9.0f;
	return {
	    .distanceDragged = static_cast<uint32_t>(static_cast<float>(count(HelpEvent::Dragging)) * k_DraggedPerCount),
	    .timeLookingAtSky = count(HelpEvent::LookingAtSky),
	    .timeLookingAtGround = count(HelpEvent::LookingAtGround),
	    .gestures = count(HelpEvent::Gestures),
	    .zooms = static_cast<uint32_t>(static_cast<float>(count(HelpEvent::Zooms)) * k_ZoomsPerCount),
	    .rotations = count(HelpEvent::Rotations),
	    .pitches = count(HelpEvent::Pitches),
	    .interfaceActions = count(HelpEvent::InterfaceActions),
	    .thrown = count(HelpEvent::Thrown),
	    .pickedUp = count(HelpEvent::PickedUp),
	    .fightAttacks = count(HelpEvent::FightAttacks),
	    .fightBlocks = count(HelpEvent::FightBlocks),
	    .fightSteps = count(HelpEvent::FightSteps),
	};
}

} // namespace openblack::statistics
