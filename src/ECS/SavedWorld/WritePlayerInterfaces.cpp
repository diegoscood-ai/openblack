/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' interfaces, their statuses and their leashes' statuses in a saved game's world file, in the original's
// order (docs/wiki/engine/saved-games.md#openblack, the players' interfaces). They are not entities: each player has
// one of each, named by the player's number.

#include <cstdint>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <InterfaceRecords.h>
#include <SavedRecords.h>
#include <WorldReader.h>

#include "ECS/Components/CreatureLeash.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::InterfaceStatusRecord;
using gamesave::LeashStatusRecord;
using gamesave::PlayerInterfaceRecord;

namespace
{
constexpr auto k_Players = static_cast<uint64_t>(PlayerNames::_COUNT);
/// A new status's word that starts at -1
constexpr uint32_t k_NewStatusWord = 0xFFFFFFFF;

/// The record of the player's object of the class as the loaded file had it, or `made` for one made since
template <typename Record>
Record KeptOr(const WriteContext& context, uint64_t player, Record made)
{
	if (auto kept = context.KeptRecordOf(SourceKey {.type = Record::k_Type, .id = player}); kept.has_value())
	{
		if (auto* record = std::get_if<Record>(&*kept); record != nullptr)
		{
			return std::move(*record);
		}
	}
	return made;
}

/// The leash of the creature the player leads, when there is one
const ecs::components::CreatureLeash* LeashOf(const ecs::Registry& registry, PlayerNames player)
{
	if (!Locator::leashSystem::has_value())
	{
		return nullptr;
	}
	const auto creature = Locator::leashSystem::value().PlayersCreature(player);
	if (!creature.has_value() || !registry.Valid(*creature))
	{
		return nullptr;
	}
	return registry.TryGet<ecs::components::CreatureLeash>(*creature);
}

/// The players' numbers, in order: every player has an interface
std::vector<uint64_t> PlayerKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> players;
	for (uint64_t player = 0; player < k_Players; ++player)
	{
		players.push_back(player);
	}
	return players;
}

/// The player's interface: its place as kept and its status. Its own word is the player's number
std::optional<gamesave::SavedRecord> WriteInterface(WriteContext& context, uint64_t player)
{
	auto record = KeptOr(context, player, PlayerInterfaceRecord {});
	record.thing.extra = static_cast<uint32_t>(player);
	record.status = {context.Reference(SourceKey {.type = InterfaceStatusRecord::k_Type, .id = player})};
	return record;
}

/// The interface's status: the player's number, its interface and its leash's status; the hand, its throw and the
/// camera, whose values at a save are not read, as kept (a new status as its constructor leaves it: the hand holding
/// nothing, the word after the leash at -1)
std::optional<gamesave::SavedRecord> WriteStatus(WriteContext& context, uint64_t player)
{
	InterfaceStatusRecord made;
	made.hand.held.numbers = {0};
	made.fifthWord = k_NewStatusWord;
	auto record = KeptOr(context, player, std::move(made));
	record.thing.extra = static_cast<uint32_t>(player);
	record.playerNumber = static_cast<uint8_t>(player);
	record.playerInterface = {context.Reference(SourceKey {.type = PlayerInterfaceRecord::k_Type, .id = player})};
	record.leashStatus = {context.Reference(SourceKey {.type = LeashStatusRecord::k_Type, .id = player})};
	return record;
}

/// The leash's status: whether the leash is on, whether it works, its type and what it is tied to, from the leash of
/// the creature the player leads when there is one (else as kept: a new status as its constructor leaves it, off,
/// working, the learning leash); the player's number and its interface's status. The force and the two words of its
/// length are kept
std::optional<gamesave::SavedRecord> WriteLeash(WriteContext& context, uint64_t player)
{
	LeashStatusRecord made;
	made.works = 1;
	made.type = static_cast<uint32_t>(LeashType::Rope);
	auto record = KeptOr(context, player, made);
	record.thing.extra = static_cast<uint32_t>(player);
	record.playerNumber = static_cast<uint8_t>(player);
	const auto name = static_cast<PlayerNames>(player);
	if (const auto* leash = LeashOf(context.GetRegistry(), name); leash != nullptr)
	{
		const auto& worn = leash->worn;
		const bool held = worn.has_value() && worn->holder == name;
		record.activated = held ? 1 : 0;
		record.works = (held ? worn->works : leash->leashWorks) ? 1 : 0;
		record.type = static_cast<uint32_t>(static_cast<int32_t>(held ? worn->type : leash->selected));
		record.attachedTo = {held && worn->tiedTo.has_value() ? context.Reference(*worn->tiedTo) : 0};
	}
	record.status = {context.Reference(SourceKey {.type = InterfaceStatusRecord::k_Type, .id = player})};
	return record;
}

/// The record of the number in the loaded file
const gamesave::SavedRecord* RecordAt(const gamesave::WorldRead& world, gamesave::ObjectNumber number)
{
	if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
	{
		return nullptr;
	}
	return &*world.objects[number - 1];
}
} // namespace

void openblack::ecs::saved_world_write::RegisterPlayerInterfaceWriters(ClassWriters& writers)
{
	writers.RegisterSource(PlayerInterfaceRecord::k_Type, PlayerKeys, WriteInterface);
	writers.RegisterSource(InterfaceStatusRecord::k_Type, PlayerKeys, WriteStatus);
	writers.RegisterSource(LeashStatusRecord::k_Type, PlayerKeys, WriteLeash);
}

void openblack::ecs::saved_world_write::KeepPlayerInterfaces(const gamesave::WorldRead& world, LoadedObjects& loaded)
{
	const auto* game = std::get_if<gamesave::GameHead>(RecordAt(world, 1));
	if (game == nullptr)
	{
		return;
	}
	const auto keep = [&loaded](gamesave::ObjectNumber number, uint32_t type, uint64_t player) {
		loaded.try_emplace(number, LoadedObject {.type = type, .id = player});
	};
	for (const auto number : game->players)
	{
		const auto* player = std::get_if<gamesave::PlayerRecord>(RecordAt(world, number));
		if (player == nullptr || player->playerNumber >= k_Players)
		{
			continue;
		}
		const uint64_t id = player->playerNumber;
		// the player's own interface is the first of its list; the others it may list are kept as they are
		for (const auto listed : player->interfaces)
		{
			const auto* face = std::get_if<PlayerInterfaceRecord>(RecordAt(world, listed));
			if (face == nullptr)
			{
				continue;
			}
			keep(listed, PlayerInterfaceRecord::k_Type, id);
			if (const auto* status = std::get_if<InterfaceStatusRecord>(RecordAt(world, face->status.number));
			    status != nullptr)
			{
				keep(face->status.number, InterfaceStatusRecord::k_Type, id);
				if (std::get_if<LeashStatusRecord>(RecordAt(world, status->leashStatus.number)) != nullptr)
				{
					keep(status->leashStatus.number, LeashStatusRecord::k_Type, id);
				}
			}
			break;
		}
	}
}
