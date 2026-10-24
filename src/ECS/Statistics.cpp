/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Statistics.h"

#include <ctime>

#include <algorithm>
#include <utility>
#include <vector>

#include "Common/GameRandom.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/MissionaryControl.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownStats.h"
#include "Game/GameStats.h"
#include "Help/HelpProfile.h"
#include "LHVM.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The player's primary creature, none without one
const Creature* CreatureOf(PlayerNames player)
{
	if (!Locator::playerSystem::has_value())
	{
		return nullptr;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player);
	if (!creature.has_value() || !Entities().Valid(*creature))
	{
		return nullptr;
	}
	return Entities().TryGet<const Creature>(*creature);
}

/// Counts something of a creature's, its tallies made the first time
template <typename Count>
void Tally(Registry& registry, entt::entity creature, Count&& count)
{
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	auto* tallies = registry.TryGet<CreatureTallies>(creature);
	count(tallies != nullptr ? *tallies : registry.AssignState<CreatureTallies>(creature));
}
} // namespace

void player_statistics::ProcessTurn([[maybe_unused]] uint32_t turn)
{
	auto& registry = Entities();
	registry.Each<const Player>([&registry](const Player& player) {
		// The people of the player's towns, men and women of any age
		uint32_t men = 0;
		uint32_t women = 0;
		for (const auto town : map_cells::TownsOf(player.name))
		{
			if (const auto* data = registry.TryGet<const Town>(town))
			{
				men += data->stats.males;
				women += data->stats.females;
			}
		}
		const auto slot = static_cast<size_t>(player.name);
		game_stats::TrackPopulation(slot, men, women);
		auto& stats = game_stats::Of(slot);
		stats.population.Add(static_cast<float>(men + women));
		stats.influence.Add(influence::InfluencePower(player.name));
		// The player's creature's size is noted the first time it is seen on the land
		if (stats.creatureValue == 0.0f)
		{
			if (const auto* creature = CreatureOf(player.name))
			{
				stats.creatureValue = creature->size;
			}
		}
	});
}

void player_statistics::Start()
{
	auto& registry = Entities();
	uint32_t towns = 0;
	registry.Each<const Town>([&towns](const Town&) { ++towns; });
	const auto now = static_cast<int32_t>(std::time(nullptr));
	registry.Each<const Player>([&](const Player& player) {
		const auto* creature = CreatureOf(player.name);
		game_stats::Init(static_cast<size_t>(player.name), magic::players::AlignmentOf(player.name).value,
		                 creature != nullptr ? creature->alignment : 0.0f, now, towns);
	});
}

namespace
{
/// The most and fewest that the game keeps, the fewest as 0 while it has not been taken
statistics::Extremes ExtremesOf(uint32_t highest, uint32_t lowest)
{
	return {.highest = highest, .lowest = lowest == 0xFFFFFFFFu ? 0u : lowest};
}

/// What the player's towns have now, for the box
void TownFigures(PlayerNames name, statistics::PlayerFigures& figures)
{
	auto& registry = Entities();
	std::vector<statistics::TownCounts> counts;
	for (const auto town : map_cells::TownsOf(name))
	{
		const auto* data = registry.TryGet<const Town>(town);
		if (data == nullptr)
		{
			continue;
		}
		// the town's counts as they stand, as the original keeps them running (the town turn's copy is older)
		const auto stats = town_stats::AsTheyStand(data->stats, town);
		counts.push_back({
		    .men = stats.males,
		    .women = stats.females,
		    .abodes = stats.abodesWithPlaces,
		    .civic = stats.civicBuildings,
		    .wonders = stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Wonder)),
		    .adults = stats.adults,
		    .children = stats.children,
		    .adultPlaces = stats.adultPlaces,
		    .childPlaces = stats.childPlaces,
		});
	}
	statistics::SetTownFigures(figures, counts);
}

/// What the player's primary creature has done and is now; nothing without one
void CreatureFigures(PlayerNames name, const game_stats::Stats& stats, statistics::PlayerFigures& figures)
{
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	const auto entity = Locator::playerSystem::value().GetPrimaryCreature(name);
	auto& registry = Entities();
	if (!entity.has_value() || !registry.Valid(*entity))
	{
		return;
	}
	statistics::CreatureNow now;
	if (const auto* creature = registry.TryGet<const Creature>(*entity))
	{
		now.body = statistics::CreatureBody {.alignment = creature->alignment, .size = creature->size};
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(*entity))
	{
		now.needs = statistics::CreatureNeedsNow {
		    .life = needs->needs.life,
		    .energy = needs->needs.energy,
		    .exhaustion = needs->needs.exhaustion,
		    .age = needs->needs.age,
		};
	}
	now.tallies = player_statistics::CreatureTalliesOf(name);
	statistics::SetCreatureFigures(figures, now, stats.creatureAtStart, stats.creatureValue);
}

statistics::PlayerFigures FiguresOf(PlayerNames name)
{
	auto& registry = Entities();
	const auto& stats = game_stats::Of(static_cast<size_t>(name));
	statistics::PlayerFigures figures {.player = name};
	figures.influence = influence::InfluencePower(name);
	// TODO(gods-defeated): openblack does not count the gods a player defeated yet, so the row reads 0
	figures.godsDefeated = 0;
	figures.alignmentChange = magic::players::AlignmentOf(name).value - stats.alignmentAtStart;
	figures.peopleKilled = stats.killedVillagers;
	// The belief in the player summed over every town
	std::vector<float> beliefs;
	registry.Each<const Town>(
	    [name, &beliefs](const Town& town) { beliefs.push_back(town.belief.belief.at(static_cast<size_t>(name))); });
	figures.belief = statistics::TotalBelief(beliefs);
	figures.maxBelief = stats.maxWorldBelief * statistics::k_BeliefScale;
	figures.minBelief = stats.minWorldBelief * statistics::k_BeliefScale;
	figures.artefacts = Locator::artefactSystem::has_value() ? Locator::artefactSystem::value().CountFor(name) : 0;
	figures.buildingsDestroyed = stats.buildingStopped;
	CreatureFigures(name, stats, figures);

	figures.births = stats.births;
	figures.deaths = stats.deaths;
	TownFigures(name, figures);
	figures.populationExtremes = ExtremesOf(stats.maxPopulation, stats.minPopulation);
	figures.menExtremes = ExtremesOf(stats.maxMales, stats.minMales);
	figures.womenExtremes = ExtremesOf(stats.maxFemales, stats.minFemales);
	figures.converted = stats.townTakenOverPopulation;
	figures.satisfaction = stats.desireCount != 0 ? stats.desireSum / static_cast<float>(stats.desireCount) : 0.0f;
	// Homes, civic buildings and wonders, by the first of their abode type's bits
	figures.built = {
	    .total = stats.buildingsBuilt,
	    .abodes = stats.buildingsBuiltByKind.at(0),
	    .civic = stats.buildingsBuiltByKind.at(1),
	    .wonders = stats.buildingsBuiltByKind.at(2),
	};
	figures.disciplesMade = stats.disciples;
	figures.foodUsed = stats.foodEaten;
	figures.woodUsed = stats.woodUsed;

	const auto& magic = magic::players::ReadMagicOf(name);
	for (uint32_t type = 0; type < figures.castsOfType.size(); ++type)
	{
		figures.castsOfType.at(type) = game_stats::SpellsCast(magic, type);
	}
	figures.chantsUsed = magic.chantsUsed;
	figures.sacrifices = stats.sacrifices;

	figures.influenceHistory = stats.influence.Finished();
	figures.populationHistory = stats.population.Finished();
	return figures;
}
} // namespace

statistics::BoxFigures player_statistics::Box()
{
	auto& registry = Entities();
	statistics::BoxFigures box;
	// When the box takes the figures, the lines of code take one more step
	game_stats::AddToTotalLinesOfCodeExecuted(&game_random::LocalRand);
	// The box counts the human and computer gods among the first four places, and shows that many from player one up,
	// a place without a god showing its empty figures
	std::vector<statistics::BoxPlayer> players;
	registry.Each<const Player, const PlayerMagic>([&players](const Player& player, const PlayerMagic& magic) {
		players.push_back({.name = player.name, .type = magic.playerType});
	});
	box.playersInGame = statistics::PlayersShown(players);
	for (uint32_t i = 0; i < box.playersInGame; ++i)
	{
		box.players.push_back(FiguresOf(static_cast<PlayerNames>(i)));
	}
	box.landNumber = influence::LandNumber();

	const auto& statics = game_stats::GetStatics();
	auto& tech = box.tech;
	tech.linesOfCode = statics.linesOfCode;
	tech.maxFrameRate = statics.maxFrameRate;
	tech.minFrameRate = statics.minFrameRate;
	// Every entity made from the land's start, those destroyed since too
	tech.objectsCreated = registry.GetEntitiesMade();
	// The time played is the game turns over 10, as the game reads it
	tech.timePlayed = Locator::time::has_value() ? statistics::TimePlayed(Locator::time::value().GetTurn()) : 0;
	tech.scriptInstructions = Locator::vm::has_value() ? Locator::vm::value().GetExecutedInstructions() : 0;
	tech.statisticsCount = statics.version;
	if (Locator::helpProfileSystem::has_value())
	{
		const auto& profile = std::as_const(Locator::helpProfileSystem::value()).Get();
		tech.help = statistics::HelpFiguresFrom(
		    [&profile](statistics::HelpEvent event) { return profile.Count(static_cast<uint32_t>(event)).Total(); });
	}
	return box;
}

void player_statistics::CountCreatureMeal(Registry& registry, entt::entity creature, uint32_t type)
{
	// Only a kill makes the tallies: a meal of anything else counts nothing
	if (type != creature_eating::k_CreatureSort && type != creature_eating::k_VillagerSort &&
	    type != creature_eating::k_AnimalSort)
	{
		return;
	}
	Tally(registry, creature, [type](CreatureTallies& tallies) { creature_eating::CountMeal(tallies.meals, type); });
}

void player_statistics::CountCreatureMushroom(Registry& registry, entt::entity creature)
{
	Tally(registry, creature, [](CreatureTallies& tallies) { ++tallies.mushroomsEaten; });
}

void player_statistics::CountCreaturePoo(Registry& registry, entt::entity creature)
{
	Tally(registry, creature, [](CreatureTallies& tallies) { ++tallies.poos; });
}

statistics::CreatureTallies player_statistics::CreatureTalliesOf(PlayerNames player)
{
	statistics::CreatureTallies tallies;
	if (!Locator::playerSystem::has_value())
	{
		return tallies;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player);
	auto& registry = Entities();
	if (!creature.has_value() || !registry.Valid(*creature))
	{
		return tallies;
	}
	if (const auto* kept = registry.TryGet<const CreatureTallies>(*creature))
	{
		tallies.peopleKilled = kept->meals.peopleKilled;
		tallies.animalsKilled = kept->meals.animalsKilled;
		tallies.creaturesKilled = kept->meals.creaturesKilled;
		tallies.poos = kept->poos;
		tallies.mushroomsEaten = kept->mushroomsEaten;
	}
	if (const auto* record = registry.TryGet<const CreatureFightRecord>(*creature))
	{
		tallies.battlesFought = record->fights;
		tallies.battlesWon = record->wins;
	}
	return tallies;
}

statistics::WorldFigures player_statistics::World(PlayerNames player)
{
	auto& registry = Entities();
	std::vector<statistics::TownCounts> towns;
	for (const auto town : map_cells::TownsOf(player))
	{
		const auto* data = registry.TryGet<const Town>(town);
		if (data == nullptr)
		{
			continue;
		}
		// the buildings as they stand (the original's counts move as abodes join or leave the town, not at its turn)
		const auto buildings = town_stats::BuildingsOf(town);
		statistics::TownCounts counts {
		    .men = data->stats.males,
		    .women = data->stats.females,
		    .abodes = buildings.abodesWithPlaces,
		    .civic = buildings.civicBuildings,
		    .wonders = buildings.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Wonder)),
		};
		for (size_t i = 0; i < counts.disciples.size(); ++i)
		{
			counts.disciples.at(i) = data->stats.disciples.at(i);
		}
		// The people in the town's homes and its homeless, the men among them anyone who is not a woman
		const auto housedOne = [&counts, &registry](entt::entity villager) {
			const auto* person = registry.TryGet<const Villager>(villager);
			if (person == nullptr)
			{
				return;
			}
			++counts.housed;
			if (person->sex != Villager::Sex::FEMALE)
			{
				++counts.housedMen;
			}
		};
		for (const auto abode : town_stats::AbodesOf(town))
		{
			for (const auto villager : abode_villagers::VillagersOf(abode))
			{
				housedOne(villager);
			}
		}
		for (const auto villager : data->homelessVillagers)
		{
			housedOne(villager);
		}
		towns.push_back(counts);
	}
	statistics::WorldInputs inputs {.towns = towns};
	// Every villager in the world, of any town or none, until it starts to die
	registry.Each<const Villager>([&inputs](const Villager& villager) {
		if ((villager.status & Villager::k_StatusDead) == 0)
		{
			++inputs.worldPopulation;
		}
	});
	// The player's missionaries: every missionary preaching to a town for the player (by its reaction's player), and of
	// those, the ones preaching to towns the player does not own
	registry.Each<const Town>([&inputs, &registry, player](const Town& town) {
		for (const auto control : town.missionaries)
		{
			const auto* preaching = registry.TryGet<const MissionaryControl>(control);
			const auto* reaction = preaching != nullptr ? effects::reactions::Find(preaching->reaction) : nullptr;
			if (reaction == nullptr || reaction->player != player)
			{
				continue;
			}
			++inputs.missionaries;
			if (town.owner != player)
			{
				++inputs.missionariesInOtherTowns;
			}
		}
	});
	const auto& stats = game_stats::Of(static_cast<size_t>(player));
	inputs.deaths = stats.deaths;
	inputs.births = stats.births;
	inputs.sacrifices = stats.sacrifices;
	return statistics::World(inputs);
}
