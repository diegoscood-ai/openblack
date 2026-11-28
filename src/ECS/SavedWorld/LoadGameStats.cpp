/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadGameStats.h"

#include <cstdint>

#include <bit>
#include <optional>
#include <variant>

#include <WorldReader.h>
#include <spdlog/spdlog.h>

#include "ECS/Registry.h"
#include "ECS/Systems/GameStatsSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The spells cast the record keeps, in its order: the magic types 1 to 35, then 41
constexpr size_t k_CastsInOrder = 35;
constexpr uint32_t k_LastCastType = 41;

void PutBackHistory(const gamesave::StatsGraph& graph, game_stats::History& history)
{
	history.buckets = graph.data;
	history.capacity = graph.size;
	history.count = graph.index;
	history.period = graph.samplesPerSlot;
	history.samples = graph.samplesInSlot;
}

/// The player the record's player names, none for any other object
std::optional<size_t> PlayerOf(const gamesave::WorldRead& read, gamesave::ObjectNumber object)
{
	if (object == 0 || object > read.objects.size() || !read.objects.at(object - 1).has_value())
	{
		return std::nullopt;
	}
	const auto* player = std::get_if<gamesave::PlayerRecord>(&*read.objects.at(object - 1));
	if (player == nullptr || player->playerNumber >= game_stats::k_Players)
	{
		return std::nullopt;
	}
	return player->playerNumber;
}
} // namespace

void load_game_stats::PutBack(const gamesave::GameStatsRecord& record, game_stats::Stats& stats, game_stats::Statics& statics,
                              components::PlayerMagic& magic, uint32_t entitiesMade)
{
	statics.playersOut = record.playersThatLeft;
	// the town totals as the save kept them, standing in the turn they were summed in (none while never summed)
	if (record.townStats[0] > 0)
	{
		stats.loadedTownTotals = record.townStats;
	}
	stats.finishPlace = record.position;
	stats.finishValue = record.lossReason;
	// the counts, from the births to the wood used
	const auto& counts = record.counts;
	stats.births = counts[0];
	stats.deaths = counts[1];
	stats.maxPopulation = counts[2];
	stats.maxMales = counts[3];
	stats.maxFemales = counts[4];
	stats.minPopulation = counts[5];
	stats.minMales = counts[6];
	stats.minFemales = counts[7];
	stats.townTakenOverPopulation = counts[8];
	stats.desireSum = std::bit_cast<float>(counts[9]);
	stats.desireCount = counts[10];
	stats.buildingsBuilt = counts[11];
	for (size_t kind = 0; kind < stats.buildingsBuiltByKind.size(); ++kind)
	{
		stats.buildingsBuiltByKind.at(kind) = counts.at(12 + kind);
	}
	for (size_t disciple = 0; disciple < stats.disciples.size(); ++disciple)
	{
		stats.disciples.at(disciple) = counts.at(15 + disciple);
	}
	stats.foodEaten = counts[23];
	stats.woodUsed = counts[24];
	PutBackHistory(record.influence, stats.influence);
	PutBackHistory(record.population, stats.population);
	// the results, from the gods defeated to the buildings stopped, and the three words after them
	const auto& results = record.results;
	stats.godsDefeated = results[0];
	stats.alignmentAtStart = std::bit_cast<float>(results[1]);
	stats.killedVillagers = results[2];
	stats.maxWorldBelief = std::bit_cast<float>(results[3]);
	stats.minWorldBelief = std::bit_cast<float>(results[4]);
	stats.creatureValue = std::bit_cast<float>(results[5]);
	stats.creatureAtStart = std::bit_cast<float>(results[6]);
	stats.buildingStopped = results[7];
	for (size_t word = 0; word < stats.u1084.size(); ++word)
	{
		stats.u1084.at(word) = results.at(8 + word);
	}
	// the whole run's figures
	const auto& run = record.run;
	statics.linesOfCode = run[0];
	statics.maxFrameRate = std::bit_cast<float>(run[1]);
	statics.minFrameRate = std::bit_cast<float>(run[2]);
	// the objects created: the record's count where the registry's count stands now, the count going on from there
	statics.objectsCreatedOffset = run[3] - entitiesMade;
	statics.startTime = std::bit_cast<int32_t>(run[4]);
	statics.version = run[5];
	// the spells cast, the chants used, the sacrifices and the towns at the start
	for (size_t type = 1; type <= k_CastsInOrder; ++type)
	{
		magic.castCount.at(type) = record.casts.at(type - 1);
	}
	magic.castCount.at(k_LastCastType) = record.casts.at(k_CastsInOrder);
	magic.chantsUsed = std::bit_cast<float>(record.last[0]);
	stats.sacrifices = record.last[1];
	statics.townCount = record.last[2];
}

size_t load_game_stats::PutBackAll(const gamesave::WorldRead& read)
{
	if (!Locator::gameStatsSystem::has_value())
	{
		return 0;
	}
	auto& players = Locator::gameStatsSystem::value().PlayerStats();
	size_t putBack = 0;
	for (const auto& saved : read.objects)
	{
		const auto* record = saved.has_value() ? std::get_if<gamesave::GameStatsRecord>(&*saved) : nullptr;
		if (record == nullptr)
		{
			continue;
		}
		const auto player = PlayerOf(read, record->player);
		if (!player.has_value())
		{
			continue;
		}
		PutBack(*record, players.at(*player), game_stats::GetStatics(),
		        magic::players::MagicOf(static_cast<PlayerNames>(*player)),
		        Locator::entitiesRegistry::has_value() ? Locator::entitiesRegistry::value().GetEntitiesMade() : 0);
		++putBack;
	}
	// (debug readout) what the load put back
	if (auto log = spdlog::get("game"); log != nullptr)
	{
		const auto& statics = game_stats::GetStatics();
		log->info("Load: the statistics of {} players put back: the start time {}, {} lines of code run, {} players out",
		          putBack, statics.startTime, statics.linesOfCode, statics.playersOut);
	}
	return putBack;
}
