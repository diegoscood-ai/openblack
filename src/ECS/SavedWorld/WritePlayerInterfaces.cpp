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

#include <algorithm>
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
#include "ECS/SavedWorld/LoadLeashStatus.h"
#include "ECS/SavedWorld/LoadPlayerInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Gestures/PowerUpSystem.h"

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

/// The interface's status: the player's number, its interface and its leash's status; for the hand's player, the
/// hand's values from the live hand (saved_hand::WriteLive: the synced place and hand, the camera the sync gave, the
/// throw, the last turn's movement, the object in the hand and the last seed type); the rest, and every value of the
/// other players' statuses, as kept (a new status as its constructor leaves it: the hand holding nothing, the last seed
/// type at -1). A kept hand holding an object gone since holds nothing, as the original's hand check takes a deleted
/// object out of every player's hand each turn: the slot, the hand's count and the status's holding count at 0
std::optional<gamesave::SavedRecord> WriteStatus(WriteContext& context, uint64_t player)
{
	InterfaceStatusRecord made;
	made.hand.held.numbers = {0};
	made.fifthWord = k_NewStatusWord;
	auto record = KeptOr(context, player, std::move(made));
	if (std::ranges::any_of(record.hand.held.numbers, [&context](auto number) { return context.IsGone(number); }))
	{
		std::ranges::fill(record.hand.held.numbers, gamesave::ObjectNumber {0});
		record.hand.byte = 0;
		record.handHoldingSomething = 0;
	}
	record.thing.extra = static_cast<uint32_t>(player);
	record.playerNumber = static_cast<uint8_t>(player);
	if (player == static_cast<uint64_t>(PlayerNames::PLAYER_ONE) && Locator::handSystem::has_value())
	{
		if (const auto live = Locator::handSystem::value().GetSavedStatus(); live.has_value())
		{
			const auto held = live->held.has_value() ? context.Reference(*live->held) : 0;
			const auto lastSeedType = Locator::magicSystem::has_value()
			                              ? std::optional<int32_t>(magic::gestures::State().lastSeedType)
			                              : std::nullopt;
			ecs::saved_hand::WriteLive(record, *live, held, lastSeedType);
		}
	}
	record.playerInterface = {context.Reference(SourceKey {.type = PlayerInterfaceRecord::k_Type, .id = player})};
	record.leashStatus = {context.Reference(SourceKey {.type = LeashStatusRecord::k_Type, .id = player})};
	return record;
}

/// The leash's status: whether the leash is on, whether it works, its type, what it is tied to, the turn of its last
/// tie and, while it is on, its two lengths, from the leash of the creature the player leads when there is one
/// (saved_leash::WriteLive; else as kept: a new status as its constructor leaves it, off, working, the learning leash);
/// the player's number and its interface's status. The force is kept. A kept status tied to an object gone since is
/// tied to nothing, as the original's leash check unties a deleted object each turn; the rest stays
std::optional<gamesave::SavedRecord> WriteLeash(WriteContext& context, uint64_t player)
{
	LeashStatusRecord made;
	made.works = 1;
	made.type = static_cast<uint32_t>(LeashType::Rope);
	auto record = KeptOr(context, player, made);
	record.attachedTo.number = context.UnlessGone(record.attachedTo.number);
	record.thing.extra = static_cast<uint32_t>(player);
	record.playerNumber = static_cast<uint8_t>(player);
	const auto name = static_cast<PlayerNames>(player);
	if (const auto* leash = LeashOf(context.GetRegistry(), name); leash != nullptr)
	{
		const auto& worn = leash->worn;
		const bool tied = worn.has_value() && worn->holder == name && worn->tiedTo.has_value();
		ecs::saved_leash::WriteLive(record, *leash, name, tied ? context.Reference(*worn->tiedTo) : 0);
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
