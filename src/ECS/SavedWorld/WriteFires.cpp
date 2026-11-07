/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fires' records of a saved game's world file, in the game's list order, the newest first
// (docs/wiki/engine/saved-games.md#openblack, the fires).

#include <cstdint>

#include <algorithm>
#include <bit>
#include <iterator>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

#include <EffectRecords.h>
#include <SavedRecords.h>

#include "ECS/Fire/FireEffect.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::FireEffectRecord;

namespace
{
/// The game object is the file's first object
constexpr gamesave::ObjectNumber k_GameObject = 1;

[[nodiscard]] gamesave::ObjectNumber FireReference(WriteContext& context, const ecs::fire::FireEffect* fire)
{
	return fire != nullptr ? context.Reference(SourceKey {.type = FireEffectRecord::k_Type, .id = fire->id}) : 0;
}

/// The fires' handles in the game's list order: the newest first
std::vector<uint64_t> FireKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> keys;
	if (!Locator::fireSystem::has_value())
	{
		return keys;
	}
	for (const auto* fire : Locator::fireSystem::value().List())
	{
		keys.push_back(fire->id);
	}
	return keys;
}

/// A fire: its charring, the two tags around its own, its temperatures, its object, its heater, its player and its
/// reaction, its flags, the next fire of its group and its root, its firemen, and the two tags again
std::optional<gamesave::SavedRecord> WriteFire(WriteContext& context, uint64_t id)
{
	if (!Locator::fireSystem::has_value())
	{
		return std::nullopt;
	}
	auto& fires = Locator::fireSystem::value();
	const auto* fire = fires.FindById(static_cast<uint32_t>(id));
	if (fire == nullptr)
	{
		return std::nullopt;
	}
	std::optional<FireEffectRecord> kept;
	if (auto record = context.KeptRecordOf(SourceKey {.type = FireEffectRecord::k_Type, .id = id}); record.has_value())
	{
		kept = std::get<FireEffectRecord>(std::move(*record));
	}
	FireEffectRecord record = kept.value_or(FireEffectRecord {});
	record.word = std::bit_cast<uint32_t>(fire->charring);
	record.firstMark = fires.ProcessTag();
	record.byte = fire->tag;
	record.secondMark = fires.CreateTag();
	record.words = {std::bit_cast<uint32_t>(fire->temperature), std::bit_cast<uint32_t>(fire->previous)};
	record.objects[0].number = context.Reference(fire->object);
	record.objects[1].number = fire->source != entt::null ? context.Reference(fire->source) : 0;
	record.objects[2].number = fire->player.has_value() ? PlayerReference(context, *fire->player) : 0;
	// its reaction: openblack's reactions have no records yet, so as the load kept it, else none
	record.objects[3].number = kept.has_value() ? kept->objects[3].number : 0;
	record.secondByte = fire->flags;
	// the next fire of its group and its root; one the load did not make (of an object it does not make yet) as kept
	const auto linkOf = [&context, &kept](size_t index, const ecs::fire::FireEffect* link) -> gamesave::ObjectNumber {
		if (link != nullptr)
		{
			return FireReference(context, link);
		}
		const auto number = kept.has_value() ? kept->others.at(index).number : 0;
		const auto target = number != 0 ? context.TargetOf(number) : std::nullopt;
		const bool notMade = target.has_value() && !target->id.has_value() && target->entity == entt::null;
		return notMade ? number : 0;
	};
	record.others[0].number = linkOf(0, fire->next);
	record.others[1].number = linkOf(1, fire->root);
	record.burning.numbers.clear();
	for (const auto fireman : fire->firemen)
	{
		record.burning.numbers.push_back(context.Reference(fireman));
	}
	record.lastFirstMark = record.firstMark;
	record.lastSecondMark = record.secondMark;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterFireWriters(ClassWriters& writers)
{
	writers.RegisterSource(FireEffectRecord::k_Type, FireKeys, WriteFire);
	writers.RegisterList(GlobalList::FireEffects, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> made;
		for (const auto id : context.Keys(FireEffectRecord::k_Type))
		{
			made.push_back(context.Reference(SourceKey {.type = FireEffectRecord::k_Type, .id = id}));
		}
		const auto* kept = context.KeptWorldRecord(k_GameObject);
		const auto* head = kept != nullptr ? std::get_if<gamesave::GameHead>(kept) : nullptr;
		if (head == nullptr)
		{
			return made;
		}
		// a loaded game's: the fires made since at the head, newest first, then the list as loaded, each fire the load
		// made while it burns and each one it did not make (of an object it does not make yet) as kept
		const auto& loaded = head->lists.at(static_cast<size_t>(GlobalList::FireEffects)).numbers;
		const std::unordered_set<gamesave::ObjectNumber> wasLoaded(loaded.begin(), loaded.end());
		const std::unordered_set<gamesave::ObjectNumber> burning(made.begin(), made.end());
		std::vector<gamesave::ObjectNumber> numbers;
		std::ranges::copy_if(made, std::back_inserter(numbers),
		                     [&wasLoaded](gamesave::ObjectNumber number) { return !wasLoaded.contains(number); });
		for (const auto number : loaded)
		{
			const auto target = context.TargetOf(number);
			const bool notMade = target.has_value() && !target->id.has_value() && target->entity == entt::null;
			if (burning.contains(number) || notMade)
			{
				numbers.push_back(number);
			}
		}
		return numbers;
	});
}
