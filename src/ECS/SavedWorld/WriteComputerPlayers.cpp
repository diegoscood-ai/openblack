/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' computer players, what each one's hand is doing, its steps and their arguments, what its mind has seen
// and its attitudes to the players in a saved game's world file, in the original's order, and what a load puts back of
// them (docs/wiki/engine/saved-games.md#openblack, the computer players). They are not entities: each player has one
// computer player, one state of its hand and eight attitudes, named by the player's number (and the other player's);
// a step, an argument of a step and a thing seen are named by the player's number and their places in their lists.

#include <cstdint>

#include <algorithm>
#include <bit>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <InterfaceRecords.h>
#include <SavedRecords.h>
#include <WorldReader.h>

#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ActionStateRecord;
using gamesave::ComputerAttitudeRecord;
using gamesave::ComputerPlayerRecord;
using gamesave::ComputerSeenRecord;
using gamesave::PlayerSubActionArgumentRecord;
using gamesave::PlayerSubActionRecord;

namespace
{
constexpr auto k_Players = static_cast<uint64_t>(PlayerNames::_COUNT);
/// The computer player's own speed, the range it sees things in and the most things it keeps seen, as every player's
/// computer player is made with them
constexpr float k_NewSpeed = 90.0f;
constexpr float k_NewSeenRange = 30.0f;
constexpr uint32_t k_NewMostSeen = 50;
/// The word every computer player writes from the whole game, as it starts
constexpr uint32_t k_NewGameWord = 0xFFFFFFFF;
/// The creature-like player's byte when no player is named
constexpr uint8_t k_NoCreatureLike = 0xFF;
/// The miracles' kinds the mind counts the icons of
constexpr size_t k_MagicTypes = 42;
/// The hand's place on the map as a new state of the hand has it: the middle of the cell (128, 128)
constexpr int32_t k_NewHandMapPlace = (128 << 16) | 0x8000;

/// The places in the computer player's record of its words and numbers
constexpr size_t k_SpeedWord = 0;
constexpr size_t k_SeenRangeWord = 1;
constexpr size_t k_MostSeenWord = 2;
constexpr size_t k_StartedWord = 3;
constexpr size_t k_SwitchedOnNumber = 0;
constexpr size_t k_PausedNumber = 1;
constexpr size_t k_HeldNumber = 2;
/// The places in the hand's state of the word that asks for a move first, of the word a new one has at 1 and of the
/// player's number
constexpr size_t k_CheckFirstWord = 0;
constexpr size_t k_SetWord = 6;
constexpr size_t k_PlayerWord = 7;
/// The computer player's list of what its mind has seen
constexpr size_t k_SeenList = 1;
/// The bytes every argument of a step writes one by one after its thing
constexpr size_t k_ArgumentBytes = 64;

/// The id of a thing the player's mind has seen, by its place in the mind's list
uint64_t SeenId(uint64_t player, size_t index)
{
	return (player << 32U) | static_cast<uint32_t>(index);
}

/// The id of a step of the player's hand, by its place in the steps, and of an argument of it, by its place there
uint64_t StepId(uint64_t player, size_t step)
{
	return (player << 32U) | (static_cast<uint64_t>(step & 0xFFFFU) << 16U);
}
uint64_t ArgumentId(uint64_t player, size_t step, size_t argument)
{
	return StepId(player, step) | static_cast<uint16_t>(argument);
}

/// The id of an attitude: the player's number and the other player's
uint64_t AttitudeId(uint64_t player, uint64_t toward)
{
	return player * k_Players + toward;
}

/// The record of the object of the class and id as the loaded file had it, or `made` for one made since
template <typename Record>
Record KeptOr(const WriteContext& context, uint64_t id, Record made)
{
	if (auto kept = context.KeptRecordOf(SourceKey {.type = Record::k_Type, .id = id}); kept.has_value())
	{
		if (auto* record = std::get_if<Record>(&*kept); record != nullptr)
		{
			return std::move(*record);
		}
	}
	return made;
}

/// The player's entity on this land
entt::entity PlayerEntity(const ecs::Registry& registry, PlayerNames player)
{
	entt::entity found = entt::null;
	registry.Each<const ecs::components::Player>(
	    [&found, player](entt::entity entity, const ecs::components::Player& component) {
		    if (found == entt::null && component.name == player)
		    {
			    found = entity;
		    }
	    });
	return found;
}

/// A component of the player's entity, when it has one
template <typename Component>
const Component* PlayerComponent(const ecs::Registry& registry, PlayerNames player)
{
	const auto entity = PlayerEntity(registry, player);
	return entity != entt::null ? registry.TryGet<Component>(entity) : nullptr;
}

/// The player's mind as the save writes it: the computer players' service's, else its entity's; none when it has none
const ecs::components::ComputerMind* MindOf(const ecs::Registry& registry, PlayerNames player)
{
	if (Locator::computerPlayerSystem::has_value())
	{
		return Locator::computerPlayerSystem::value().SavedMind(player);
	}
	return PlayerComponent<ecs::components::ComputerMind>(registry, player);
}

/// The players' numbers, in order: every player has a computer player
std::vector<uint64_t> PlayerKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> players;
	for (uint64_t player = 0; player < k_Players; ++player)
	{
		players.push_back(player);
	}
	return players;
}

/// Every player's eight attitudes, by the player's number and the other player's
std::vector<uint64_t> AttitudeKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> attitudes;
	for (uint64_t player = 0; player < k_Players; ++player)
	{
		for (uint64_t toward = 0; toward < k_Players; ++toward)
		{
			attitudes.push_back(AttitudeId(player, toward));
		}
	}
	return attitudes;
}

/// What every player's mind has seen, by the player and the place in the mind's list
std::vector<uint64_t> SeenKeys(const ecs::Registry& registry)
{
	std::vector<uint64_t> seen;
	for (uint64_t player = 0; player < k_Players; ++player)
	{
		if (const auto* mind = MindOf(registry, static_cast<PlayerNames>(player)); mind != nullptr)
		{
			for (size_t index = 0; index < mind->seen.size(); ++index)
			{
				seen.push_back(SeenId(player, index));
			}
		}
	}
	return seen;
}

/// Every step of the players' hands, or every argument of them, by the player and their places
std::vector<uint64_t> StepKeys(const ecs::Registry& registry, bool arguments)
{
	std::vector<uint64_t> keys;
	for (uint64_t player = 0; player < k_Players; ++player)
	{
		const auto* hand = PlayerComponent<ecs::components::ComputerHand>(registry, static_cast<PlayerNames>(player));
		for (size_t step = 0; hand != nullptr && step < hand->steps.size(); ++step)
		{
			if (!arguments)
			{
				keys.push_back(StepId(player, step));
				continue;
			}
			for (size_t argument = 0; argument < hand->steps[step].arguments.size(); ++argument)
			{
				keys.push_back(ArgumentId(player, step, argument));
			}
		}
	}
	return keys;
}

/// A new computer player, as every player's is made: its tree's nodes held back not at all, each at its row's weight
ComputerPlayerRecord NewComputerPlayer()
{
	ComputerPlayerRecord made;
	made.words[k_SpeedWord] = std::bit_cast<uint32_t>(k_NewSpeed);
	made.words[k_SeenRangeWord] = std::bit_cast<uint32_t>(k_NewSeenRange);
	made.words[k_MostSeenWord] = k_NewMostSeen;
	made.gameWord = k_NewGameWord;
	made.table.assign(k_MagicTypes, 0);
	made.byte = k_NoCreatureLike;
	const auto mind = computer_player::MakeMind();
	for (size_t node = 0; node < made.desires.size() && node < mind.nodes.size(); ++node)
	{
		made.desires.at(node) = {.suppression = mind.nodes[node].suppression, .personality = mind.nodes[node].personality};
	}
	return made;
}

/// The computer player: its player, its hand's state and its eight attitudes by the context's references; whether it
/// is switched on, has made its start on the land, is paused and the player its creature's size is like, from the
/// player's computer player; whether a script holds its hand and the player's own speed, from its hand when it has one;
/// the icons its mind counted, its tree's nodes (how much each is held back and its weight) and what it has seen, from
/// its mind. Its place, its other lists, the range it sees things in, the most it keeps seen, the word of the whole game and a
/// word stay as loaded (a new one as every player's is made)
std::optional<gamesave::SavedRecord> WriteComputerPlayer(WriteContext& context, uint64_t player)
{
	auto record = KeptOr(context, player, NewComputerPlayer());
	const auto& registry = context.GetRegistry();
	const auto name = static_cast<PlayerNames>(player);
	record.thing.extra = static_cast<uint32_t>(player);
	// a loaded one names its player as the file did; one made since, the player's source object
	if (!context.KeptRecordOf(SourceKey {.type = ComputerPlayerRecord::k_Type, .id = player}).has_value())
	{
		record.player = PlayerObject(context, static_cast<int>(player));
	}
	record.actionState = context.Reference(SourceKey {.type = ActionStateRecord::k_Type, .id = player});
	if (const auto* state = PlayerComponent<ecs::components::ComputerPlayer>(registry, name); state != nullptr)
	{
		record.numbers[k_SwitchedOnNumber] = state->switchedOn ? 1 : 0;
		record.numbers[k_PausedNumber] = state->paused ? 1 : 0;
		record.words[k_StartedWord] = state->started ? 1 : 0;
		record.byte = state->creatureLike.has_value() ? static_cast<uint8_t>(*state->creatureLike) : k_NoCreatureLike;
	}
	if (const auto* hand = PlayerComponent<ecs::components::ComputerHand>(registry, name); hand != nullptr)
	{
		record.numbers[k_HeldNumber] = hand->held ? 1 : 0;
		record.words[k_SpeedWord] = std::bit_cast<uint32_t>(hand->playerSpeed);
	}
	if (const auto* mind = MindOf(registry, name); mind != nullptr && mind->nodes.size() == record.desires.size())
	{
		record.table.assign(mind->iconsFound.begin(), mind->iconsFound.end());
		for (size_t node = 0; node < record.desires.size(); ++node)
		{
			record.desires.at(node) = {.suppression = mind->nodes[node].suppression,
			                           .personality = mind->nodes[node].personality};
		}
		// what it has seen, the newest first, as the mind's list holds it
		auto& seen = record.lists.at(k_SeenList);
		seen.clear();
		for (size_t index = 0; index < mind->seen.size(); ++index)
		{
			seen.push_back(context.Reference(SourceKey {.type = ComputerSeenRecord::k_Type, .id = SeenId(player, index)}));
		}
	}
	record.attitudes.resize(k_Players);
	for (uint64_t toward = 0; toward < k_Players; ++toward)
	{
		record.attitudes.at(toward) =
		    context.Reference(SourceKey {.type = ComputerAttitudeRecord::k_Type, .id = AttitudeId(player, toward)});
	}
	return record;
}

/// What the computer player's hand is doing: its computer player by the context's reference, and its steps and their
/// speed from the hand when it has one; its place and the rest stay as loaded (a new one as it is made: in the middle
/// of the map, asking for a move first, at the computer player's speed)
std::optional<gamesave::SavedRecord> WriteActionState(WriteContext& context, uint64_t player)
{
	ActionStateRecord made;
	made.position.position = {.x = k_NewHandMapPlace, .z = k_NewHandMapPlace, .altitude = 0.0f};
	made.words[k_CheckFirstWord] = 1;
	made.words[k_SetWord] = 1;
	made.words[k_PlayerWord] = static_cast<uint32_t>(player);
	made.first = k_NewSpeed;
	auto record = KeptOr(context, player, made);
	record.thing.extra = static_cast<uint32_t>(player);
	record.computerPlayer = context.Reference(SourceKey {.type = ComputerPlayerRecord::k_Type, .id = player});
	if (const auto* hand =
	        PlayerComponent<ecs::components::ComputerHand>(context.GetRegistry(), static_cast<PlayerNames>(player));
	    hand != nullptr)
	{
		record.first = hand->speed;
		record.objects.clear();
		for (size_t step = 0; step < hand->steps.size(); ++step)
		{
			record.objects.push_back(
			    context.Reference(SourceKey {.type = PlayerSubActionRecord::k_Type, .id = StepId(player, step)}));
		}
	}
	return record;
}

/// The step of the player's hand the id names, when the hand has it
const ecs::components::HandStep* StepOf(const ecs::Registry& registry, uint64_t id)
{
	const auto player = static_cast<PlayerNames>(id >> 32U);
	const auto step = static_cast<size_t>((id >> 16U) & 0xFFFFU);
	const auto* hand = PlayerComponent<ecs::components::ComputerHand>(registry, player);
	return hand != nullptr && step < hand->steps.size() ? &hand->steps[step] : nullptr;
}

/// A step of the hand: what it does and its arguments by the context's references, from the hand; one the hand no
/// longer has stays as loaded
std::optional<gamesave::SavedRecord> WriteStep(WriteContext& context, uint64_t id)
{
	auto record = KeptOr(context, id, PlayerSubActionRecord {});
	if (const auto* step = StepOf(context.GetRegistry(), id); step != nullptr)
	{
		record.word = static_cast<uint32_t>(step->type);
		record.arguments.numbers.clear();
		for (size_t argument = 0; argument < step->arguments.size(); ++argument)
		{
			record.arguments.numbers.push_back(context.Reference(
			    SourceKey {.type = PlayerSubActionArgumentRecord::k_Type, .id = id | static_cast<uint16_t>(argument)}));
		}
	}
	return record;
}

/// An argument of a step: its kind, its thing by the context's reference, its number, its amount and its point, from
/// the hand; the bytes after its thing and the word after its number stay as loaded (zero for a new one), and one the
/// hand no longer has stays as loaded
std::optional<gamesave::SavedRecord> WriteArgument(WriteContext& context, uint64_t id)
{
	PlayerSubActionArgumentRecord made;
	made.bytes.values.assign(k_ArgumentBytes, 0);
	auto record = KeptOr(context, id, std::move(made));
	const auto argument = static_cast<size_t>(id & 0xFFFFU);
	if (const auto* step = StepOf(context.GetRegistry(), id); step != nullptr && argument < step->arguments.size())
	{
		const auto& value = step->arguments[argument];
		record.word = static_cast<uint32_t>(value.kind);
		record.object = {value.thing != entt::null ? context.Reference(value.thing) : 0};
		record.secondWord = static_cast<uint32_t>(value.intValue);
		record.thirdWord = std::bit_cast<uint32_t>(value.floatValue);
		record.point = {value.point.x, value.point.y, value.point.z};
	}
	return record;
}

/// A thing the player's mind has seen: what it is (a forest by its key, a tree or a seed by the context's reference)
/// and the turn it was first seen, from the mind; one whose thing openblack does not hold stays as loaded
std::optional<gamesave::SavedRecord> WriteSeen(WriteContext& context, uint64_t id)
{
	auto record = KeptOr(context, id, ComputerSeenRecord {});
	const auto index = static_cast<size_t>(id & 0xFFFFFFFFU);
	if (const auto* mind = MindOf(context.GetRegistry(), static_cast<PlayerNames>(id >> 32U));
	    mind != nullptr && index < mind->seen.size())
	{
		const auto& seen = mind->seen[index];
		record.word = seen.turn;
		if (seen.forest.has_value())
		{
			record.object = {context.Reference(SourceKey {.type = gamesave::ForestRecord::k_Type, .id = *seen.forest})};
		}
		else if (seen.thing != entt::null)
		{
			record.object = {context.Reference(seen.thing)};
		}
	}
	return record;
}

/// An attitude of the player to another, from the player's mind (as a new mind's when it has none)
std::optional<gamesave::SavedRecord> WriteAttitude(WriteContext& context, uint64_t id)
{
	const auto player = id / k_Players;
	const auto toward = id % k_Players;
	auto record = KeptOr(context, id, ComputerAttitudeRecord {});
	if (const auto* mind = MindOf(context.GetRegistry(), static_cast<PlayerNames>(player)); mind != nullptr)
	{
		record.attitude = mind->attitude.at(toward);
	}
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

/// The mind a computer player's record holds: a new mind with the record's icons counted, its nodes' values, its
/// attitudes and what it has seen (each thing as the load made it: a forest by its key, a tree or a seed as its
/// entity; one the load made nothing of is kept with its turn, and written as loaded)
ecs::components::ComputerMind MindOfRecord(const gamesave::WorldRead& world, const LoadedObjects& loaded,
                                           const ComputerPlayerRecord& record)
{
	auto mind = computer_player::MakeMind();
	std::copy_n(record.table.begin(), std::min(record.table.size(), mind.iconsFound.size()), mind.iconsFound.begin());
	for (size_t node = 0; node < mind.nodes.size() && node < record.desires.size(); ++node)
	{
		mind.nodes[node].suppression = record.desires.at(node).suppression;
		mind.nodes[node].personality = record.desires.at(node).personality;
	}
	for (size_t toward = 0; toward < mind.attitude.size() && toward < record.attitudes.size(); ++toward)
	{
		if (const auto* attitude = std::get_if<ComputerAttitudeRecord>(RecordAt(world, record.attitudes[toward]));
		    attitude != nullptr)
		{
			mind.attitude.at(toward) = attitude->attitude;
		}
	}
	for (const auto number : record.lists.at(k_SeenList))
	{
		const auto* seen = std::get_if<ComputerSeenRecord>(RecordAt(world, number));
		if (seen == nullptr)
		{
			continue;
		}
		ecs::components::ComputerMindSeen entry {.turn = seen->word};
		if (const auto made = loaded.find(seen->object.number); made != loaded.end())
		{
			if (made->second.id.has_value() && made->second.type == gamesave::ForestRecord::k_Type)
			{
				entry.forest = static_cast<uint32_t>(*made->second.id);
			}
			else if (!made->second.id.has_value())
			{
				entry.thing = made->second.entity;
			}
		}
		mind.seen.push_back(entry);
	}
	return mind;
}

/// The switches a computer player's record holds
ecs::components::ComputerPlayer StateOfRecord(const ComputerPlayerRecord& record)
{
	ecs::components::ComputerPlayer state;
	state.switchedOn = record.numbers[k_SwitchedOnNumber] != 0;
	state.started = record.words[k_StartedWord] != 0;
	state.paused = record.numbers[k_PausedNumber] != 0;
	if (record.byte < k_Players)
	{
		state.creatureLike = static_cast<PlayerNames>(record.byte);
	}
	return state;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterComputerPlayerWriters(ClassWriters& writers)
{
	writers.RegisterSource(ComputerPlayerRecord::k_Type, PlayerKeys, WriteComputerPlayer);
	writers.RegisterSource(ActionStateRecord::k_Type, PlayerKeys, WriteActionState);
	writers.RegisterSource(ComputerAttitudeRecord::k_Type, AttitudeKeys, WriteAttitude);
	writers.RegisterSource(ComputerSeenRecord::k_Type, SeenKeys, WriteSeen);
	writers.RegisterSource(
	    PlayerSubActionRecord::k_Type, [](const Registry& registry) { return StepKeys(registry, false); }, WriteStep);
	writers.RegisterSource(
	    PlayerSubActionArgumentRecord::k_Type, [](const Registry& registry) { return StepKeys(registry, true); },
	    WriteArgument);
}

void openblack::ecs::saved_world_write::LoadComputerPlayers(const gamesave::WorldRead& world, LoadedObjects& loaded)
{
	const auto* game = std::get_if<gamesave::GameHead>(RecordAt(world, 1));
	if (game == nullptr)
	{
		return;
	}
	const auto keep = [&loaded](gamesave::ObjectNumber number, uint32_t type, uint64_t id) {
		loaded.try_emplace(number, LoadedObject {.type = type, .id = id});
	};
	for (const auto number : game->players)
	{
		const auto* player = std::get_if<gamesave::PlayerRecord>(RecordAt(world, number));
		if (player == nullptr || player->playerNumber >= k_Players)
		{
			continue;
		}
		const uint64_t id = player->playerNumber;
		const auto* computer = std::get_if<ComputerPlayerRecord>(RecordAt(world, player->computerPlayer));
		if (computer == nullptr)
		{
			continue;
		}
		keep(player->computerPlayer, ComputerPlayerRecord::k_Type, id);
		if (const auto* state = std::get_if<ActionStateRecord>(RecordAt(world, computer->actionState)); state != nullptr)
		{
			keep(computer->actionState, ActionStateRecord::k_Type, id);
			// its steps and their arguments, by their places
			for (size_t step = 0; step < state->objects.size(); ++step)
			{
				const auto* action = std::get_if<PlayerSubActionRecord>(RecordAt(world, state->objects[step]));
				if (action == nullptr)
				{
					continue;
				}
				keep(state->objects[step], PlayerSubActionRecord::k_Type, StepId(id, step));
				for (size_t argument = 0; argument < action->arguments.numbers.size(); ++argument)
				{
					const auto number = action->arguments.numbers[argument];
					if (std::get_if<PlayerSubActionArgumentRecord>(RecordAt(world, number)) != nullptr)
					{
						keep(number, PlayerSubActionArgumentRecord::k_Type, ArgumentId(id, step, argument));
					}
				}
			}
		}
		// what its mind has seen, by their places in the mind's list (the records of things seen, as the load puts them
		// back)
		size_t seen = 0;
		for (const auto listed : computer->lists.at(k_SeenList))
		{
			if (std::get_if<ComputerSeenRecord>(RecordAt(world, listed)) != nullptr)
			{
				keep(listed, ComputerSeenRecord::k_Type, SeenId(id, seen++));
			}
		}
		for (size_t toward = 0; toward < computer->attitudes.size() && toward < k_Players; ++toward)
		{
			if (std::get_if<ComputerAttitudeRecord>(RecordAt(world, computer->attitudes[toward])) != nullptr)
			{
				keep(computer->attitudes[toward], ComputerAttitudeRecord::k_Type, AttitudeId(id, toward));
			}
		}
		if (Locator::computerPlayerSystem::has_value())
		{
			Locator::computerPlayerSystem::value().Restore(static_cast<PlayerNames>(id), StateOfRecord(*computer),
			                                               MindOfRecord(world, loaded, *computer));
		}
	}
}

gamesave::ObjectNumber openblack::ecs::saved_world_write::PlayerObject(WriteContext& context, int player)
{
	if (player < 0 || player >= static_cast<int>(gamesave::k_PlayerCount))
	{
		return 0;
	}
	// the players are a source's objects, by their numbers
	return context.Reference(SourceKey {.type = gamesave::k_PlayerType, .id = static_cast<uint64_t>(player)});
}
