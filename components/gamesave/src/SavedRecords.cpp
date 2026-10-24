/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedRecords.h"

#include <initializer_list>
#include <span>
#include <type_traits>
#include <utility>

using namespace openblack::gamesave;

namespace
{
/// The most elements that a read of a counted list accepts, so that bad bytes cannot ask for a huge list
constexpr uint32_t k_MostElements = 0x100000;
/// The longest text that a read accepts
constexpr uint32_t k_MostText = 260;

/// A counted list of values: the count, then each value
template <typename T>
void PutCounted(StreamWriter& writer, const std::vector<T>& values)
{
	writer.Put(static_cast<uint32_t>(values.size()));
	for (const auto& value : values)
	{
		writer.Put(value);
	}
}

template <typename T>
bool GetCounted(StreamReader& reader, std::vector<T>& values)
{
	uint32_t count = 0;
	if (!reader.Get(count) || count > k_MostElements)
	{
		reader.Fail();
		return false;
	}
	values.resize(count);
	for (auto& value : values)
	{
		reader.Get(value);
	}
	return !reader.Failed();
}

/// A counted list of references
void PutReferences(StreamWriter& writer, const std::vector<ObjectNumber>& numbers, const WriteNewObject& writeNew)
{
	writer.Put(static_cast<uint32_t>(numbers.size()));
	for (const auto number : numbers)
	{
		WriteReference(writer, number, writeNew);
	}
}

bool GetReferences(StreamReader& reader, std::vector<ObjectNumber>& numbers, const ReadNewObject& readNew)
{
	uint32_t count = 0;
	if (!reader.Get(count) || count > k_MostElements)
	{
		reader.Fail();
		return false;
	}
	numbers.assign(count, 0);
	for (auto& number : numbers)
	{
		if (!ReadReference(reader, number, readNew))
		{
			return false;
		}
	}
	return true;
}

/// A text: its length with the end mark, then each letter as one value
void PutText(StreamWriter& writer, const std::string& text)
{
	writer.Put(static_cast<uint32_t>(text.size() + 1));
	for (const char letter : text)
	{
		writer.Put(letter);
	}
	writer.Put('\0');
}

bool GetText(StreamReader& reader, std::string& text)
{
	uint32_t length = 0;
	if (!reader.Get(length) || length == 0 || length > k_MostText)
	{
		reader.Fail();
		return false;
	}
	text.clear();
	for (uint32_t i = 0; i < length; ++i)
	{
		char letter = 0;
		reader.Get(letter);
		if (i + 1 < length)
		{
			text.push_back(letter);
		}
	}
	return !reader.Failed();
}

bool HasPhysics(const ThingPart& thing, const PositionPart& position)
{
	return (position.flags & k_PositionFlagInPhysics) != 0 && (thing.flags & k_ThingFlagCarriedByParticles) == 0;
}

void WriteGame(StreamWriter& writer, const GameHead& game, const WriteNewObject& writeNew)
{
	WriteThing(writer, k_GameType, game.thing);
	PutText(writer, game.revision);
	writer.Put(game.gameFlags);
	writer.Put(game.landNumber);
	for (const auto player : game.players)
	{
		WriteReference(writer, player, writeNew);
	}
	for (const auto value : game.data)
	{
		writer.Put(value);
	}
	for (const auto value : game.bytes)
	{
		writer.Put(value);
	}
	writer.Put(game.word);
	for (const auto& list : game.lists)
	{
		writer.Put(static_cast<uint32_t>(list.numbers.size()));
		for (const auto number : list.numbers)
		{
			WriteReference(writer, number, writeNew);
		}
	}
	for (const auto value : game.words)
	{
		writer.Put(value);
	}
	WritePart(writer, game.tail, writeNew);
}

bool ReadGame(StreamReader& reader, GameHead& game, const ReadNewObject& readNew)
{
	if (!ReadThing(reader, game.thing) || !GetText(reader, game.revision))
	{
		return false;
	}
	reader.Get(game.gameFlags);
	reader.Get(game.landNumber);
	for (auto& player : game.players)
	{
		if (!ReadReference(reader, player, readNew))
		{
			return false;
		}
	}
	for (auto& value : game.data)
	{
		reader.Get(value);
	}
	for (auto& value : game.bytes)
	{
		reader.Get(value);
	}
	reader.Get(game.word);
	for (auto& list : game.lists)
	{
		uint32_t count = 0;
		if (!reader.Get(count) || count > k_MostElements)
		{
			reader.Fail();
			return false;
		}
		list.numbers.assign(count, 0);
		for (auto& number : list.numbers)
		{
			if (!ReadReference(reader, number, readNew))
			{
				return false;
			}
		}
	}
	for (auto& value : game.words)
	{
		reader.Get(value);
	}
	return !reader.Failed() && ReadPart(reader, game.tail, readNew);
}

void WritePlayer(StreamWriter& writer, const PlayerRecord& player, const WriteNewObject& writeNew)
{
	WriteThing(writer, k_PlayerType, player.thing);
	writer.Put(player.playerNumber);
	writer.Put(player.alignment);
	writer.Put(player.info);
	PutCounted(writer, player.tribalPower);
	PutCounted(writer, player.maxTribalPower);
	writer.Put(player.type);
	PutCounted(writer, player.name);
	writer.Put(player.userId);
	writer.Put(player.cheatOn);
	writer.Put(player.windResistance);
	writer.Put(player.allMagicEnabled);
	writer.Put(player.cheatWord);
	writer.Put(player.infiniteInfluence);
	WriteReference(writer, player.computerPlayer, writeNew);
	writer.Put(player.totalPopulation);
	writer.Put(player.start);
	PutCounted(writer, player.magicTypeEnabled);
	PutCounted(writer, player.magicTypeEverBeenEnabled);
	WriteReference(writer, player.gameStats, writeNew);
	WriteReference(writer, player.citadel, writeNew);
	WriteReference(writer, player.creature, writeNew);
	PutReferences(writer, player.towns, writeNew);
	PutReferences(writer, player.teleports, writeNew);
	writer.Put(player.influencePower);
	writer.Put(player.averageInfluencePower);
	PutCounted(writer, player.damageFromPlayer);
	PutCounted(writer, player.allies);
	PutReferences(writer, player.interfaces, writeNew);
	writer.Put(player.hasLost);
	writer.Put(player.lastWord);
}

bool ReadPlayer(StreamReader& reader, PlayerRecord& player, const ReadNewObject& readNew)
{
	if (!ReadThing(reader, player.thing))
	{
		return false;
	}
	reader.Get(player.playerNumber);
	reader.Get(player.alignment);
	reader.Get(player.info);
	GetCounted(reader, player.tribalPower);
	GetCounted(reader, player.maxTribalPower);
	reader.Get(player.type);
	GetCounted(reader, player.name);
	reader.Get(player.userId);
	reader.Get(player.cheatOn);
	reader.Get(player.windResistance);
	reader.Get(player.allMagicEnabled);
	reader.Get(player.cheatWord);
	reader.Get(player.infiniteInfluence);
	if (reader.Failed() || !ReadReference(reader, player.computerPlayer, readNew))
	{
		return false;
	}
	reader.Get(player.totalPopulation);
	reader.Get(player.start);
	GetCounted(reader, player.magicTypeEnabled);
	GetCounted(reader, player.magicTypeEverBeenEnabled);
	if (reader.Failed() || !ReadReference(reader, player.gameStats, readNew) ||
	    !ReadReference(reader, player.citadel, readNew) || !ReadReference(reader, player.creature, readNew) ||
	    !GetReferences(reader, player.towns, readNew) || !GetReferences(reader, player.teleports, readNew))
	{
		return false;
	}
	reader.Get(player.influencePower);
	reader.Get(player.averageInfluencePower);
	GetCounted(reader, player.damageFromPlayer);
	GetCounted(reader, player.allies);
	if (reader.Failed() || !GetReferences(reader, player.interfaces, readNew))
	{
		return false;
	}
	reader.Get(player.hasLost);
	reader.Get(player.lastWord);
	return !reader.Failed();
}

void WriteComputerPlayer(StreamWriter& writer, const ComputerPlayerRecord& computer, const WriteNewObject& writeNew)
{
	WriteThing(writer, k_ComputerPlayerType, computer.thing);
	Write(writer, computer.thing, computer.position);
	WriteReference(writer, computer.player, writeNew);
	WriteReference(writer, computer.actionState, writeNew);
	for (const auto& list : computer.lists)
	{
		PutReferences(writer, list, writeNew);
	}
	for (const auto word : computer.words)
	{
		writer.Put(word);
	}
	writer.Put(computer.gameWord);
	PutCounted(writer, computer.table);
	writer.Put(computer.tableWord);
	for (const auto number : computer.numbers)
	{
		writer.Put(number);
	}
	writer.Put(computer.byte);
	PutReferences(writer, computer.attitudes, writeNew);
	for (const auto& node : computer.desires)
	{
		writer.Put(node.suppression);
		writer.Put(node.personality);
	}
}

bool ReadComputerPlayer(StreamReader& reader, ComputerPlayerRecord& computer, const ReadNewObject& readNew)
{
	if (!ReadThing(reader, computer.thing) || !Read(reader, computer.position) ||
	    !ReadReference(reader, computer.player, readNew) || !ReadReference(reader, computer.actionState, readNew))
	{
		return false;
	}
	for (auto& list : computer.lists)
	{
		if (!GetReferences(reader, list, readNew))
		{
			return false;
		}
	}
	for (auto& word : computer.words)
	{
		reader.Get(word);
	}
	reader.Get(computer.gameWord);
	GetCounted(reader, computer.table);
	reader.Get(computer.tableWord);
	for (auto& number : computer.numbers)
	{
		reader.Get(number);
	}
	reader.Get(computer.byte);
	if (reader.Failed() || !GetReferences(reader, computer.attitudes, readNew))
	{
		return false;
	}
	for (auto& node : computer.desires)
	{
		reader.Get(node.suppression);
		reader.Get(node.personality);
	}
	return !reader.Failed();
}

void WriteActionState(StreamWriter& writer, const ActionStateRecord& state, const WriteNewObject& writeNew)
{
	WriteThing(writer, k_ActionStateType, state.thing);
	Write(writer, state.thing, state.position);
	WriteReference(writer, state.computerPlayer, writeNew);
	PutReferences(writer, state.objects, writeNew);
	for (const auto word : state.words)
	{
		writer.Put(word);
	}
	writer.Put(state.block);
	writer.Put(state.point);
	writer.Put(state.first);
	WriteReference(writer, state.object, writeNew);
	writer.Put(state.second);
	writer.Put(state.otherPoint);
}

bool ReadActionState(StreamReader& reader, ActionStateRecord& state, const ReadNewObject& readNew)
{
	if (!ReadThing(reader, state.thing) || !Read(reader, state.position) ||
	    !ReadReference(reader, state.computerPlayer, readNew) || !GetReferences(reader, state.objects, readNew))
	{
		return false;
	}
	for (auto& word : state.words)
	{
		reader.Get(word);
	}
	reader.Get(state.block);
	reader.Get(state.point);
	reader.Get(state.first);
	if (reader.Failed() || !ReadReference(reader, state.object, readNew))
	{
		return false;
	}
	reader.Get(state.second);
	reader.Get(state.otherPoint);
	return !reader.Failed();
}

void WriteGameStats(StreamWriter& writer, const GameStatsRecord& stats, const WriteNewObject& writeNew)
{
	WriteThing(writer, k_GameStatsType, stats.thing);
	WriteReference(writer, stats.player, writeNew);
	writer.Put(stats.playersThatLeft);
	writer.Put(stats.position);
	writer.Put(stats.lossReason);
	writer.Put(stats.townStats);
	for (const auto value : stats.counts)
	{
		writer.Put(value);
	}
	writer.Put(stats.influence);
	writer.Put(stats.population);
	for (const auto& values : {std::span<const uint32_t>(stats.results), std::span<const uint32_t>(stats.run),
	                           std::span<const uint32_t>(stats.casts), std::span<const uint32_t>(stats.last)})
	{
		for (const auto value : values)
		{
			writer.Put(value);
		}
	}
}

bool ReadGameStats(StreamReader& reader, GameStatsRecord& stats, const ReadNewObject& readNew)
{
	if (!ReadThing(reader, stats.thing) || !ReadReference(reader, stats.player, readNew))
	{
		return false;
	}
	reader.Get(stats.playersThatLeft);
	reader.Get(stats.position);
	reader.Get(stats.lossReason);
	reader.Get(stats.townStats);
	for (auto& value : stats.counts)
	{
		reader.Get(value);
	}
	reader.Get(stats.influence);
	reader.Get(stats.population);
	for (auto values : {std::span<uint32_t>(stats.results), std::span<uint32_t>(stats.run), std::span<uint32_t>(stats.casts),
	                    std::span<uint32_t>(stats.last)})
	{
		for (auto& value : values)
		{
			reader.Get(value);
		}
	}
	return !reader.Failed();
}
} // namespace

uint32_t openblack::gamesave::TypeOf(const SavedRecord& record)
{
	return std::visit([](const auto& value) { return std::decay_t<decltype(value)>::k_Type; }, record);
}

bool openblack::gamesave::IsKnownType(uint32_t type)
{
	return [type]<size_t... Index>(std::index_sequence<Index...>) {
		return ((std::variant_alternative_t<Index, SavedRecord>::k_Type == type) || ...);
	}(std::make_index_sequence<std::variant_size_v<SavedRecord>> {});
}

void openblack::gamesave::WriteThing(StreamWriter& writer, uint32_t type, const ThingPart& thing)
{
	writer.Put(type);
	writer.Put(thing.extra);
	writer.PutChecksum();
	writer.Put(thing.destroyed);
	writer.Put(thing.flags);
}

bool openblack::gamesave::ReadThing(StreamReader& reader, ThingPart& thing)
{
	reader.GetChecksum(thing.checksum);
	reader.Get(thing.destroyed);
	reader.Get(thing.flags);
	return !reader.Failed();
}

void openblack::gamesave::WriteReference(StreamWriter& writer, ObjectNumber number, const WriteNewObject& writeNew)
{
	if (writer.PutReference(NumberKey(number)) && writeNew)
	{
		writeNew(writer, number);
	}
}

bool openblack::gamesave::ReadReference(StreamReader& reader, ObjectNumber& number, const ReadNewObject& readNew)
{
	ObjectKey key = k_NoObject;
	const auto kind = reader.GetReference(key);
	number = NumberOf(key).value_or(0);
	switch (kind)
	{
	case ReferenceKind::None:
	case ReferenceKind::Earlier:
		return true;
	case ReferenceKind::New:
		if (!readNew)
		{
			reader.Fail();
			return false;
		}
		return readNew(reader, number);
	case ReferenceKind::Bad:
		break;
	}
	return false;
}

void openblack::gamesave::Write(StreamWriter& writer, const ThingPart& /*thing*/, const PositionPart& position)
{
	writer.Put(position.position);
	writer.Put(position.flags);
}

bool openblack::gamesave::Read(StreamReader& reader, PositionPart& position)
{
	reader.Get(position.position);
	reader.Get(position.flags);
	return !reader.Failed();
}

void openblack::gamesave::Write(StreamWriter& writer, const ThingPart& thing, const PositionPart& position,
                                const ObjectPart& object, const WriteNewObject& writeNew)
{
	Write(writer, thing, position);
	WriteObject(writer, thing, position, object, writeNew);
}

void openblack::gamesave::WriteObject(StreamWriter& writer, const ThingPart& thing, const PositionPart& position,
                                      const ObjectPart& object, const WriteNewObject& writeNew)
{
	writer.Put(object.info);
	writer.Put(object.coords);
	writer.Put(object.creationIndex);
	writer.Put(object.life);
	writer.Put(object.scale);
	writer.Put(object.heading);
	WriteReference(writer, object.fire, writeNew);
	if (HasPhysics(thing, position))
	{
		// An object with no body in the physics writes a body that stands still
		const auto physics = object.physics.value_or(PhysicsPart {});
		writer.Put(physics.matrix);
		writer.Put(physics.velocity);
		writer.Put(physics.spin);
	}
}

bool openblack::gamesave::Read(StreamReader& reader, const ThingPart& thing, const PositionPart& position, ObjectPart& object,
                               const ReadNewObject& readNew)
{
	reader.Get(object.info);
	reader.Get(object.coords);
	reader.Get(object.creationIndex);
	reader.Get(object.life);
	reader.Get(object.scale);
	reader.Get(object.heading);
	if (reader.Failed() || !ReadReference(reader, object.fire, readNew))
	{
		return false;
	}
	object.physics.reset();
	if (HasPhysics(thing, position))
	{
		PhysicsPart physics;
		reader.Get(physics.matrix);
		reader.Get(physics.velocity);
		reader.Get(physics.spin);
		object.physics = physics;
	}
	return !reader.Failed();
}

void openblack::gamesave::Write(StreamWriter& writer, const FixedPart& fixed, const WriteNewObject& writeNew)
{
	WriteReference(writer, fixed.townArtifact, writeNew);
}

bool openblack::gamesave::Read(StreamReader& reader, FixedPart& fixed, const ReadNewObject& readNew)
{
	return ReadReference(reader, fixed.townArtifact, readNew);
}

void openblack::gamesave::Write(StreamWriter& writer, const MultiMapFixedPart& part, const WriteNewObject& writeNew)
{
	writer.Put(part.fixedFlags);
	writer.Put(part.percentBuilt);
	WriteReference(writer, part.footpathLink, writeNew);
	WriteReference(writer, part.buildingSite, writeNew);
}

bool openblack::gamesave::Read(StreamReader& reader, MultiMapFixedPart& part, const ReadNewObject& readNew)
{
	reader.Get(part.fixedFlags);
	reader.Get(part.percentBuilt);
	return !reader.Failed() && ReadReference(reader, part.footpathLink, readNew) &&
	       ReadReference(reader, part.buildingSite, readNew);
}

void openblack::gamesave::Write(StreamWriter& writer, const MobilePart& mobile)
{
	writer.Put(mobile.mobileWord);
}

bool openblack::gamesave::Read(StreamReader& reader, MobilePart& mobile)
{
	return reader.Get(mobile.mobileWord);
}

void openblack::gamesave::Write(StreamWriter& writer, const MobileObjectPart& part, const WriteNewObject& writeNew)
{
	WriteReference(writer, part.object, writeNew);
	writer.Put(part.xAngle);
	writer.Put(part.zAngle);
	WriteReference(writer, part.path, writeNew);
}

bool openblack::gamesave::Read(StreamReader& reader, MobileObjectPart& part, const ReadNewObject& readNew)
{
	if (!ReadReference(reader, part.object, readNew))
	{
		return false;
	}
	reader.Get(part.xAngle);
	reader.Get(part.zAngle);
	return !reader.Failed() && ReadReference(reader, part.path, readNew);
}

void openblack::gamesave::Write(StreamWriter& writer, const MobileStaticPart& part, const WriteNewObject& writeNew)
{
	WriteReference(writer, part.player, writeNew);
	writer.Put(part.words[0]);
	writer.Put(part.words[1]);
}

bool openblack::gamesave::Read(StreamReader& reader, MobileStaticPart& part, const ReadNewObject& readNew)
{
	if (!ReadReference(reader, part.player, readNew))
	{
		return false;
	}
	reader.Get(part.words[0]);
	reader.Get(part.words[1]);
	return !reader.Failed();
}

void openblack::gamesave::Write(StreamWriter& writer, const SavedRecord& record, const WriteNewObject& writeNew)
{
	std::visit(
	    [&writer, &writeNew](const auto& value) {
		    using T = std::decay_t<decltype(value)>;
		    if constexpr (HasFields<T>)
		    {
			    WriteFields(writer, value, writeNew);
		    }
		    else if constexpr (std::is_same_v<T, GameHead>)
		    {
			    WriteGame(writer, value, writeNew);
		    }
		    else if constexpr (std::is_same_v<T, PlayerRecord>)
		    {
			    WritePlayer(writer, value, writeNew);
		    }
		    else if constexpr (std::is_same_v<T, ComputerPlayerRecord>)
		    {
			    WriteComputerPlayer(writer, value, writeNew);
		    }
		    else if constexpr (std::is_same_v<T, ActionStateRecord>)
		    {
			    WriteActionState(writer, value, writeNew);
		    }
		    else if constexpr (std::is_same_v<T, ComputerAttitudeRecord>)
		    {
			    WriteThing(writer, k_ComputerAttitudeType, value.thing);
			    writer.Put(value.attitude);
		    }
		    else if constexpr (std::is_same_v<T, GameStatsRecord>)
		    {
			    WriteGameStats(writer, value, writeNew);
		    }
	    },
	    record);
}

bool openblack::gamesave::ReadRecord(StreamReader& reader, uint32_t type, uint32_t extra, SavedRecord& record,
                                     const ReadNewObject& readNew)
{
	// Each record is made in place: the walk reads records inside records, so the stack keeps none of them
	const auto make = [&record, extra]<typename T>() -> T& {
		auto& value = record.template emplace<T>();
		value.thing.extra = extra;
		return value;
	};
	switch (type)
	{
	case k_GameType:
	{
		make.template operator()<GameHead>();
		return ReadGame(reader, std::get<GameHead>(record), readNew);
	}
	case k_PlayerType:
	{
		make.template operator()<PlayerRecord>();
		return ReadPlayer(reader, std::get<PlayerRecord>(record), readNew);
	}
	case k_ComputerPlayerType:
	{
		make.template operator()<ComputerPlayerRecord>();
		return ReadComputerPlayer(reader, std::get<ComputerPlayerRecord>(record), readNew);
	}
	case k_ActionStateType:
	{
		make.template operator()<ActionStateRecord>();
		return ReadActionState(reader, std::get<ActionStateRecord>(record), readNew);
	}
	case k_ComputerAttitudeType:
	{
		make.template operator()<ComputerAttitudeRecord>();
		auto& attitude = std::get<ComputerAttitudeRecord>(record);
		return ReadThing(reader, attitude.thing) && reader.Get(attitude.attitude);
	}
	case k_GameStatsType:
	{
		make.template operator()<GameStatsRecord>();
		return ReadGameStats(reader, std::get<GameStatsRecord>(record), readNew);
	}
	default:
		break;
	}
	// The records whose values are listed in order
	const auto readListed = [&]<size_t... Index>(std::index_sequence<Index...>) {
		bool read = false;
		const bool found = ([&] {
			using T = std::variant_alternative_t<Index, SavedRecord>;
			if constexpr (HasFields<T>)
			{
				if (T::k_Type == type)
				{
					read = ReadFields(reader, make.template operator()<T>(), readNew);
					return true;
				}
			}
			return false;
		}() || ...);
		if (!found)
		{
			reader.SetUnknownType(type);
		}
		return read;
	};
	return readListed(std::make_index_sequence<std::variant_size_v<SavedRecord>> {});
}
