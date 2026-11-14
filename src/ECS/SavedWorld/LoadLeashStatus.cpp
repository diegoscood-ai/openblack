/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadLeashStatus.h"

#include <cstdint>

#include <bit>
#include <utility>
#include <variant>

#include <WorldReader.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/CreatureLeash.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using components::CreatureLeash;
using gamesave::LeashStatusRecord;

namespace
{
constexpr auto k_Players = static_cast<uint64_t>(PlayerNames::_COUNT);

/// The record of the number in the loaded file
const gamesave::SavedRecord* RecordAt(const gamesave::WorldRead& world, gamesave::ObjectNumber number)
{
	if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
	{
		return nullptr;
	}
	return &*world.objects[number - 1];
}

/// The entity the load made of the loaded file's object of the number, when it made one
std::optional<entt::entity> EntityOf(const Registry& registry, const saved_world_write::LoadedObjects& loaded,
                                     gamesave::ObjectNumber number)
{
	const auto found = loaded.find(number);
	if (found == loaded.end() || found->second.entity == entt::null || !registry.Valid(found->second.entity))
	{
		return std::nullopt;
	}
	return found->second.entity;
}
} // namespace

saved_leash::Restored saved_leash::Restore(CreatureLeash& leash, const LeashStatusRecord& record, PlayerNames player,
                                           std::optional<entt::entity> tiedTo)
{
	const bool on = record.activated != 0;
	const bool tied = record.attachedTo.number != 0;
	if (on && tied && !tiedTo.has_value())
	{
		return Restored::TiedToNothingMade;
	}
	// one switch, one type and one turn for the player's leash, on or off
	const bool works = record.works != 0;
	const auto type = static_cast<LeashType>(static_cast<int32_t>(record.type));
	leash.leashWorks = works;
	leash.selected = type;
	leash.lastTiedTurn = record.lastWord;
	if (!on)
	{
		leash.worn.reset();
		return Restored::Done;
	}
	CreatureLeash::Worn worn;
	worn.type = type;
	worn.holder = player;
	worn.works = works;
	if (tied)
	{
		worn.tiedTo = tiedTo;
		worn.tiedTurn = record.lastWord;
	}
	// the lengths the leash had: at rest, then its full length
	worn.rope.slackLength = std::bit_cast<float>(record.fifthWord);
	worn.rope.maxLength = std::bit_cast<float>(record.length);
	// the rope is laid afresh from its ends on the next frame
	worn.ropeStarted = false;
	leash.worn = std::move(worn);
	leash.control = CreatureLeash::Control::Idle;
	return Restored::Done;
}

void saved_leash::WriteLive(LeashStatusRecord& record, const CreatureLeash& leash, PlayerNames player,
                            gamesave::ObjectNumber tiedTo)
{
	const auto& worn = leash.worn;
	const bool held = worn.has_value() && worn->holder == player;
	record.activated = held ? 1 : 0;
	record.works = (held ? worn->works : leash.leashWorks) ? 1 : 0;
	record.type = static_cast<uint32_t>(static_cast<int32_t>(held ? worn->type : leash.selected));
	record.attachedTo = {tiedTo};
	record.lastWord = leash.lastTiedTurn;
	if (held)
	{
		record.fifthWord = std::bit_cast<uint32_t>(worn->rope.slackLength);
		record.length = std::bit_cast<uint32_t>(worn->rope.maxLength);
	}
}

void saved_leash::Load(Registry& registry, const systems::LeashSystemInterface& leashes, const gamesave::WorldRead& world,
                       const saved_world_write::LoadedObjects& loaded)
{
	for (const auto& [number, object] : loaded)
	{
		if (object.type != LeashStatusRecord::k_Type || !object.id.has_value() || *object.id >= k_Players)
		{
			continue;
		}
		const auto* record = std::get_if<LeashStatusRecord>(RecordAt(world, number));
		if (record == nullptr)
		{
			continue;
		}
		const auto player = static_cast<PlayerNames>(*object.id);
		const auto creature = leashes.PlayersCreature(player);
		// the leash of the creature the player leads, looked up without making its storage
		if (!creature.has_value() || !registry.Valid(*creature) || !std::as_const(registry).AllOf<CreatureLeash>(*creature))
		{
			continue;
		}
		auto& leash = registry.Get<CreatureLeash>(*creature);
		const auto tiedTo =
		    record->attachedTo.number != 0 ? EntityOf(registry, loaded, record->attachedTo.number) : std::nullopt;
		if (Restore(leash, *record, player, tiedTo) == Restored::TiedToNothingMade)
		{
			if (const auto log = spdlog::get("game"); log != nullptr)
			{
				SPDLOG_LOGGER_INFO(
				    log, "Player {}'s leash is tied to object {}, which the load did not make: the leash is not put back",
				    *object.id, record->attachedTo.number);
			}
		}
	}
}

void saved_leash::SwitchAgain(systems::LeashSystemInterface& leashes, const gamesave::WorldRead& world,
                              const saved_world_write::LoadedObjects& loaded)
{
	for (const auto& [number, object] : loaded)
	{
		if (object.type != LeashStatusRecord::k_Type || !object.id.has_value() || *object.id >= k_Players)
		{
			continue;
		}
		const auto* record = std::get_if<LeashStatusRecord>(RecordAt(world, number));
		if (record == nullptr)
		{
			continue;
		}
		// the leash of the creature the player leads, switched as the status's switch says
		if (const auto creature = leashes.PlayersCreature(static_cast<PlayerNames>(*object.id)); creature.has_value())
		{
			leashes.SwitchAtLoad(*creature, record->activated != 0);
		}
	}
}
