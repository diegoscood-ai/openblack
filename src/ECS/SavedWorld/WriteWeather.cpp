/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The climates' records of a saved game's world file, with their natural storms, in the original's order
// (docs/wiki/engine/saved-games.md#openblack, the climates).

#include <cstdint>
#include <cstring>

#include <bit>
#include <optional>
#include <string>
#include <vector>

#include <SavedRecords.h>
#include <WorldListRecords.h>

#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/WeatherThing.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ClimateRecord;
using gamesave::WeatherThingRecord;
namespace climate = openblack::weather::climate;
namespace storms = openblack::weather::storms;

namespace
{
/// The climates' ids in the game's list order: the newest first
std::vector<uint64_t> ClimateKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> ids;
	climate::ForEach([&ids](const climate::Climate& made) { ids.push_back(static_cast<uint32_t>(made.id)); });
	return ids;
}

/// A natural storm as its climate writes it: its place, its descriptor, its age, its speed and where it moves to
std::optional<gamesave::ClimateArea> StormArea(storms::StormId id)
{
	std::optional<gamesave::ClimateArea> area;
	// a storm marked for deletion is still in its climate's list
	storms::ForEach([&area, id](const storms::Storm& storm) {
		if (storm.id != id)
		{
			return;
		}
		const auto& position = storm.descriptor.position;
		area = gamesave::ClimateArea {
		    .point = {position.x, position.y, position.z},
		    .values = ecs::saved_world_load::StormDescriptorBytes(storm.descriptor),
		    .words = {std::bit_cast<uint32_t>(storm.age), std::bit_cast<uint32_t>(storm.speed)},
		    .second = {storm.target.x, storm.target.y, storm.target.z},
		};
	});
	return area;
}

/// A climate: its place, its two radii, its id, the climate system's next id, its row, the system's last day, season
/// and last season, its rain, its temperature and the one it moves to, its wind, the most storms it has, whether it is
/// the world's, its natural storms (its list, newest first), and its storms' height, fall speed, the word the game keeps
/// and its lightning
std::optional<gamesave::SavedRecord> WriteClimate(WriteContext& context, uint64_t id)
{
	const climate::Climate* found = nullptr;
	climate::ForEach([&found, id](const climate::Climate& made) {
		if (found == nullptr && static_cast<uint32_t>(made.id) == id)
		{
			found = &made;
		}
	});
	if (found == nullptr)
	{
		context.Fail("climate " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	const auto& made = *found;
	ClimateRecord record = made.saved.value_or(ClimateRecord {});
	record.point = {std::bit_cast<float>(made.x), std::bit_cast<float>(made.z), made.y};
	record.words[0] = std::bit_cast<uint32_t>(made.innerRadius);
	record.words[1] = std::bit_cast<uint32_t>(made.outerRadius);
	record.words[2] = static_cast<uint32_t>(made.id);
	record.words[4] = ecs::saved_world_load::ClimateInfoRowOf(static_cast<uint32_t>(made.info));
	const auto counters = climate::CurrentCounters();
	record.words[3] = static_cast<uint32_t>(counters.nextId);
	record.words[5] = static_cast<uint32_t>(counters.lastDay);
	record.words[6] = counters.season;
	record.words[7] = counters.lastSeason;
	std::memcpy(record.sixteen.data(), &made.rain.desire, sizeof(float));
	std::memcpy(record.sixteen.data() + 4, &made.rain.dryDays, sizeof(int32_t));
	std::memcpy(record.sixteen.data() + 8, &made.rain.rainingDays, sizeof(int32_t));
	record.sixteen[12] = made.rain.flags;
	std::memcpy(record.eight.data(), &made.temperature, sizeof(float));
	std::memcpy(record.eight.data() + 4, &made.targetTemperature, sizeof(float));
	record.secondPoint = {made.windX, made.windZ, made.windAngle};
	record.word = static_cast<uint32_t>(made.maxStorms);
	record.byte = made.world ? 1 : 0;
	record.areas.elements.clear();
	for (const auto storm : made.storms)
	{
		if (const auto area = StormArea(storm); area.has_value())
		{
			record.areas.elements.push_back(*area);
		}
	}
	record.lastWords = {static_cast<uint32_t>(made.stormElevation), std::bit_cast<uint32_t>(made.fallSpeed),
	                    std::bit_cast<uint32_t>(made.unknown0x70),  std::bit_cast<uint32_t>(made.sheetMin),
	                    std::bit_cast<uint32_t>(made.sheetMax),     std::bit_cast<uint32_t>(made.forkMin),
	                    std::bit_cast<uint32_t>(made.forkMax)};
	record.lastByte = made.lightning;
	return record;
}
/// A thing of the weather: its place, its own descriptor, its storm while the storm is in
/// the storms' list (one marked for deletion still is), and whether the wind moves it
std::optional<gamesave::SavedRecord> WriteWeatherThing(WriteContext& context, entt::entity entity)
{
	const auto& thing = context.GetRegistry().Get<const ecs::components::WeatherThing>(entity);
	std::optional<WeatherThingRecord> kept;
	if (auto record = context.KeptRecordOf(entity, WeatherThingRecord::k_Type); record.has_value())
	{
		kept = std::get<WeatherThingRecord>(std::move(*record));
	}
	WeatherThingRecord record = kept.value_or(WeatherThingRecord {});
	record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
	record.descriptor = ecs::saved_world_load::StormDescriptorBytes(thing.descriptor);
	record.storm = {};
	if (const auto area = thing.storm != storms::k_NoStorm ? StormArea(thing.storm) : std::nullopt; area.has_value())
	{
		record.storm = {.present = 1,
		                .storm = gamesave::WeatherStorm {.point = area->point,
		                                                 .descriptor = area->values,
		                                                 .age = area->words[0],
		                                                 .speed = area->words[1],
		                                                 .target = area->second}};
	}
	record.affectedByWind = thing.affectedByWind ? 1 : 0;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterWeatherWriters(ClassWriters& writers)
{
	writers.Register(
	    WeatherThingRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::WeatherThing>(entity); },
	    WriteWeatherThing);
	writers.RegisterList(GlobalList::WeatherThings, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto thing : weather::weather_thing::Things())
		{
			numbers.push_back(context.Reference(thing));
		}
		return numbers;
	});
	writers.RegisterSource(ClimateRecord::k_Type, ClimateKeys, WriteClimate);
	writers.RegisterList(GlobalList::Climates, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto id : context.Keys(ClimateRecord::k_Type))
		{
			numbers.push_back(context.Reference(SourceKey {.type = ClimateRecord::k_Type, .id = id}));
		}
		return numbers;
	});
}
