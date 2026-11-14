/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldWriter.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <ObjectStream.h>
#include <VanillaSaveFile.h>
#include <WorldReader.h>
#include <fmt/format.h>

#include "ECS/Components/LoadedRecord.h"
#include "ECS/Registry.h"

using namespace openblack::ecs::saved_world_write;
using openblack::gamesave::ObjectNumber;
using openblack::gamesave::SavedRecord;

namespace
{
/// The game object is the file's first object: no reference comes before its record
constexpr ObjectNumber k_GameObject = 1;

uint32_t EntityNumber(entt::entity entity)
{
	return static_cast<uint32_t>(entt::to_integral(entity));
}

/// The record of the class given that a load kept on the entity, of whichever of the world file's record types has
/// that class's number
template <size_t... Index>
std::optional<SavedRecord> KeptOn(const openblack::ecs::Registry& registry, entt::entity entity, uint32_t type,
                                  std::index_sequence<Index...> /*unused*/)
{
	std::optional<SavedRecord> found;
	const auto take = [&]<typename Record>() {
		if (!found.has_value() && Record::k_Type == type)
		{
			if (const auto* kept = registry.TryGet<openblack::ecs::components::LoadedRecord<Record>>(entity))
			{
				found.emplace(std::in_place_type<Record>, kept->record);
			}
		}
	};
	(take.template operator()<std::variant_alternative_t<Index, openblack::gamesave::SavedRecordVariant>>(), ...);
	return found;
}

/// The classes of the records a load kept on the entity
template <size_t... Index>
std::vector<uint32_t> KeptClassesOn(const openblack::ecs::Registry& registry, entt::entity entity,
                                    std::index_sequence<Index...> /*unused*/)
{
	std::vector<uint32_t> classes;
	const auto take = [&]<typename Record>() {
		if (registry.AllOf<openblack::ecs::components::LoadedRecord<Record>>(entity))
		{
			classes.push_back(Record::k_Type);
		}
	};
	(take.template operator()<std::variant_alternative_t<Index, openblack::gamesave::SavedRecordVariant>>(), ...);
	return classes;
}

constexpr auto k_RecordTypes = std::make_index_sequence<std::variant_size_v<openblack::gamesave::SavedRecordVariant>>();
} // namespace

bool ClassWriters::Register(uint32_t type, ClassClaim claims, ClassWriter write)
{
	if (_sources.contains(type))
	{
		return false;
	}
	return _writers.try_emplace(type, Entry {.claims = std::move(claims), .write = std::move(write)}).second;
}

bool ClassWriters::RegisterSource(uint32_t type, SourceKeys keys, SourceWriter write)
{
	if (_writers.contains(type))
	{
		return false;
	}
	return _sources.try_emplace(type, Source {.keys = std::move(keys), .write = std::move(write)}).second;
}

const SourceWriter* ClassWriters::SourceWriterOf(uint32_t type) const
{
	const auto found = _sources.find(type);
	return found != _sources.end() && found->second.write ? &found->second.write : nullptr;
}

const SourceKeys* ClassWriters::SourceKeysOf(uint32_t type) const
{
	const auto found = _sources.find(type);
	return found != _sources.end() && found->second.keys ? &found->second.keys : nullptr;
}

bool ClassWriters::RegisterList(GlobalList list, ListWriter write)
{
	return _lists.try_emplace(list, std::move(write)).second;
}

const ListWriter* ClassWriters::ListWriterOf(GlobalList list) const
{
	const auto found = _lists.find(list);
	return found != _lists.end() ? &found->second : nullptr;
}

const ClassWriter* ClassWriters::WriterOf(uint32_t type) const
{
	const auto found = _writers.find(type);
	return found != _writers.end() ? &found->second.write : nullptr;
}

bool ClassWriters::StillOf(uint32_t type, const Registry& registry, entt::entity entity) const
{
	const auto found = _writers.find(type);
	if (found == _writers.end() || !found->second.claims || found->second.claims(registry, entity))
	{
		return true;
	}
	// refused by its own class's claim, the entity is of another class only when another writer claims it: one no
	// writer claims is left to the class's writer, as before
	return ClaimsOf(registry, entity).empty();
}

std::vector<uint32_t> ClassWriters::ClaimsOf(const Registry& registry, entt::entity entity) const
{
	std::vector<uint32_t> classes;
	for (const auto& [type, entry] : _writers)
	{
		if (entry.claims && entry.claims(registry, entity))
		{
			classes.push_back(type);
		}
	}
	return classes;
}

WriteContext::WriteContext(const ClassWriters& writers, const WorldSource& source)
    : _writers(writers)
    , _source(source)
{
	// The loaded objects keep their numbers of the loaded file, so that a kept record names the same object as an
	// entity's writer does; an object made since takes a number past them all
	ObjectNumber last = k_GameObject;
	if (_source.kept != nullptr)
	{
		last = std::max(last, static_cast<ObjectNumber>(_source.kept->objects.size()));
	}
	for (const auto& [number, object] : _source.loaded)
	{
		last = std::max(last, number);
		if (object.id.has_value())
		{
			_keyNumbers.try_emplace(SourceKey {.type = object.type, .id = *object.id}, number);
			_targets.try_emplace(number, Target {.entity = entt::null, .type = object.type, .id = object.id});
			continue;
		}
		_numbers.try_emplace({object.entity, object.type}, number);
		_targets.try_emplace(number, Target {.entity = object.entity, .type = object.type});
	}
	_next = last + 1;
}

const SavedRecord* WriteContext::KeptWorldRecord(ObjectNumber number) const
{
	if (_source.kept == nullptr || number == 0 || number > _source.kept->objects.size() ||
	    !_source.kept->objects[number - 1].has_value())
	{
		return nullptr;
	}
	return &*_source.kept->objects[number - 1];
}

std::optional<uint32_t> WriteContext::ClassOf(entt::entity entity) const
{
	const auto claims = _writers.ClaimsOf(_source.registry, entity);
	if (claims.size() == 1)
	{
		return claims.front();
	}
	if (!claims.empty())
	{
		return std::nullopt;
	}
	const auto kept = KeptClassesOn(_source.registry, entity, k_RecordTypes);
	return kept.size() == 1 ? std::optional(kept.front()) : std::nullopt;
}

std::optional<SavedRecord> WriteContext::KeptRecordOf(entt::entity entity, uint32_t type) const
{
	return KeptOn(_source.registry, entity, type, k_RecordTypes);
}

std::optional<SavedRecord> WriteContext::KeptRecordOf(const SourceKey& key) const
{
	// A loaded object keeps its number of the loaded file, which names its record in the kept world; an object made
	// since has a number past the kept world's
	const auto found = _keyNumbers.find(key);
	if (found == _keyNumbers.end())
	{
		return std::nullopt;
	}
	const auto* kept = KeptWorldRecord(found->second);
	if (kept == nullptr || gamesave::TypeOf(*kept) != key.type)
	{
		return std::nullopt;
	}
	return *kept;
}

ObjectNumber WriteContext::Reference(entt::entity entity)
{
	if (entity == entt::null)
	{
		return 0;
	}
	const auto type = ClassOf(entity);
	if (!type.has_value())
	{
		Fail(fmt::format("entity {} is of no class the writers or its kept records name, or of more than one: its "
		                 "referrer must name the class (its components: {})",
		                 EntityNumber(entity), ComponentsOf(entity)));
		return 0;
	}
	return Reference(entity, *type);
}

std::string WriteContext::ComponentsOf(entt::entity entity) const
{
	// the components' type names, without their namespaces, for a refusal's reader to tell what the object is
	std::string names;
	_source.registry.EachStorage([&names, entity](entt::id_type /*id*/, const auto& storage) {
		if (storage.type() == entt::type_id<entt::entity>() || !storage.contains(entity))
		{
			return;
		}
		auto name = std::string(storage.type().name());
		if (const auto colon = name.rfind("::"); colon != std::string::npos)
		{
			name = name.substr(colon + 2);
		}
		names += names.empty() ? name : " " + name;
	});
	return names;
}

ObjectNumber WriteContext::Reference(entt::entity entity, uint32_t type)
{
	if (entity == entt::null)
	{
		return 0;
	}
	const auto [found, made] = _numbers.try_emplace({entity, type}, _next);
	if (made)
	{
		_targets.emplace(_next, Target {.entity = entity, .type = type});
		++_next;
	}
	NoteReferenced(found->second);
	return found->second;
}

ObjectNumber WriteContext::Reference(const SourceKey& key)
{
	const auto [found, made] = _keyNumbers.try_emplace(key, _next);
	if (made)
	{
		_targets.emplace(_next, Target {.entity = entt::null, .type = key.type, .id = key.id});
		++_next;
	}
	NoteReferenced(found->second);
	return found->second;
}

std::vector<uint64_t> WriteContext::Keys(uint32_t type) const
{
	const auto* keys = _writers.SourceKeysOf(type);
	return keys != nullptr ? (*keys)(_source.registry) : std::vector<uint64_t> {};
}

void WriteContext::NoteReferenced(ObjectNumber number)
{
	if (_listing && number != 0 && _seen.insert(number).second)
	{
		_referenced.push_back(number);
	}
}

void WriteContext::Fail(std::string reason)
{
	if (_listing)
	{
		_refusals.push_back(std::move(reason));
		return;
	}
	if (_failure.empty())
	{
		_failure = std::move(reason);
	}
}

bool WriteContext::IsGone(ObjectNumber number) const
{
	const auto found = _targets.find(number);
	if (found == _targets.end() || found->second.entity == entt::null)
	{
		return false;
	}
	const auto entity = found->second.entity;
	return !_source.registry.Valid(entity) || !_writers.StillOf(found->second.type, _source.registry, entity);
}

void WriteContext::DropGone(gamesave::RefList& list) const
{
	std::erase_if(list.numbers, [this](ObjectNumber number) { return IsGone(number); });
}

std::optional<WriteContext::Target> WriteContext::TargetOf(ObjectNumber number) const
{
	if (const auto found = _targets.find(number); found != _targets.end())
	{
		return found->second;
	}
	if (number == k_GameObject && _writers.Has(gamesave::k_GameType))
	{
		return Target {.entity = entt::null, .type = gamesave::k_GameType};
	}
	if (_source.kept != nullptr && number >= 1 && number <= _source.kept->objects.size() &&
	    _source.kept->objects[number - 1].has_value())
	{
		return Target {.entity = entt::null, .type = gamesave::TypeOf(*_source.kept->objects[number - 1])};
	}
	return std::nullopt;
}

WriteContext::MadeRecord WriteContext::RecordOf(ObjectNumber number)
{
	const auto target = TargetOf(number);
	if (!target.has_value())
	{
		Fail(fmt::format("object {} is neither an entity's nor a record of the kept world", number));
		return {};
	}
	if (target->id.has_value())
	{
		// A non-entity source's object: its source's writer, else the record of the loaded object it was
		if (const auto* write = _writers.SourceWriterOf(target->type); write != nullptr)
		{
			auto record = (*write)(*this, *target->id);
			if (!record.has_value() || gamesave::TypeOf(*record) != target->type)
			{
				Fail(fmt::format("the source of {} made no record of its class for its object {}",
				                 gamesave::TypeName(target->type), *target->id));
				return {};
			}
			return {.record = std::make_unique<SavedRecord>(std::move(*record)), .source = RecordSource::Writer};
		}
		if (const auto* kept = KeptWorldRecord(number); kept != nullptr)
		{
			return {.record = std::make_unique<SavedRecord>(*kept), .source = RecordSource::KeptWorld};
		}
		Fail(fmt::format("the object {} of {} has no writer and no record kept from a load", *target->id,
		                 gamesave::TypeName(target->type)));
		return {};
	}
	if (target->entity == entt::null)
	{
		// The game object, written by its class's writer when there is one, and a loaded object no entity stands for
		if (const auto* write = _writers.WriterOf(target->type); write != nullptr && number == k_GameObject)
		{
			auto record = (*write)(*this, entt::null);
			if (!record.has_value())
			{
				Fail(fmt::format("the writer of {} made no record", gamesave::TypeName(target->type)));
				return {};
			}
			return {.record = std::make_unique<SavedRecord>(std::move(*record)), .source = RecordSource::Writer};
		}
		return {.record = std::make_unique<SavedRecord>(*_source.kept->objects[number - 1]), .source = RecordSource::KeptWorld};
	}
	if (!_source.registry.Valid(target->entity))
	{
		Fail(fmt::format("entity {} of {} (object {}) is gone", EntityNumber(target->entity), gamesave::TypeName(target->type),
		                 number));
		return {};
	}
	if (!_writers.StillOf(target->type, _source.registry, target->entity))
	{
		// the entity has changed class since its object was numbered (a tree become a dead tree): the original deleted
		// that object and made another, so a record still naming the old one names an object that is gone
		Fail(fmt::format("entity {} of {} (object {}) is gone: it is no longer of that class (its components: {})",
		                 EntityNumber(target->entity), gamesave::TypeName(target->type), number, ComponentsOf(target->entity)));
		return {};
	}
	if (const auto* write = _writers.WriterOf(target->type); write != nullptr)
	{
		auto record = (*write)(*this, target->entity);
		if (!record.has_value())
		{
			Fail(fmt::format("the writer of {} made no record of entity {}", gamesave::TypeName(target->type),
			                 EntityNumber(target->entity)));
			return {};
		}
		if (gamesave::TypeOf(*record) != target->type)
		{
			Fail(fmt::format("the writer of {} made a record of {} for entity {}", gamesave::TypeName(target->type),
			                 gamesave::TypeName(gamesave::TypeOf(*record)), EntityNumber(target->entity)));
			return {};
		}
		return {.record = std::make_unique<SavedRecord>(std::move(*record)), .source = RecordSource::Writer};
	}
	if (auto kept = KeptRecordOf(target->entity, target->type); kept.has_value())
	{
		return {.record = std::make_unique<SavedRecord>(std::move(*kept)), .source = RecordSource::EntityRecord};
	}
	Fail(fmt::format("entity {} of {} has no writer and no record kept from a load", EntityNumber(target->entity),
	                 gamesave::TypeName(target->type)));
	return {};
}

WorldWrite openblack::ecs::saved_world_write::WriteWorld(const ClassWriters& writers, const WorldSource& source)
{
	WorldWrite world;
	WriteContext context(writers, source);
	world.bytes = gamesave::WriteVanillaFraming(source.landscape);
	const auto framing = gamesave::ReadVanillaFraming(world.bytes);
	gamesave::StreamWriter stream({.objects = k_GameObject, .checksum = framing.has_value() ? framing->checksum : 0});

	// Each object's record at its first reference: its number in the file is the stream's count at that moment
	gamesave::WriteNewObject writeNew;
	const auto write = [&](gamesave::StreamWriter& writer, ObjectNumber handle, ObjectNumber number) {
		if (!context.Failure().empty())
		{
			writer.Fail();
			return;
		}
		const auto made = context.RecordOf(handle);
		if (made.record == nullptr)
		{
			writer.Fail();
			return;
		}
		const auto target = context.TargetOf(handle);
		world.records.push_back({.number = number,
		                         .type = gamesave::TypeOf(*made.record),
		                         .source = made.source,
		                         .entity = target.has_value() ? target->entity : entt::null,
		                         .id = target.has_value() ? target->id : std::nullopt});
		gamesave::Write(writer, *made.record, writeNew);
	};
	writeNew = [&](gamesave::StreamWriter& writer, ObjectNumber handle) { write(writer, handle, writer.Objects()); };

	if (const auto game = context.TargetOf(k_GameObject); !game.has_value() || game->type != gamesave::k_GameType)
	{
		world.failure = "there is no game object: no writer of its class and no kept world";
		return world;
	}
	write(stream, k_GameObject, k_GameObject);
	world.failure = context.Failure();
	if (world.failure.empty() && stream.Failed())
	{
		world.failure = "the stream stopped";
	}
	const auto body = stream.Take();
	world.bytes.insert(world.bytes.end(), body.begin(), body.end());
	return world;
}

std::string openblack::ecs::saved_world_write::GameRecordPartsNaming(const gamesave::GameHead& game, ObjectNumber number)
{
	std::vector<std::string> parts;
	if (number == 0)
	{
		return {};
	}
	const auto check = [&parts, number](ObjectNumber named, const std::string& part) {
		if (named == number)
		{
			parts.push_back(part);
		}
	};
	for (size_t i = 0; i < game.players.size(); ++i)
	{
		check(game.players.at(i), fmt::format("player {}", i));
	}
	for (size_t i = 0; i < game.lists.size(); ++i)
	{
		if (std::ranges::find(game.lists.at(i).numbers, number) != game.lists.at(i).numbers.end())
		{
			parts.push_back(fmt::format("global list {}", i));
		}
	}
	const auto& tail = game.tail;
	for (size_t i = 0; i < tail.helpObjects.size(); ++i)
	{
		check(tail.helpObjects.at(i).number, fmt::format("help object {}", i));
	}
	check(tail.helpObject.number, "help's object");
	for (size_t i = 0; i < tail.scriptObjects.size(); ++i)
	{
		check(tail.scriptObjects.at(i).object.number, fmt::format("script table slot {}", i));
	}
	for (size_t i = 0; i < tail.scriptRefs.size(); ++i)
	{
		check(tail.scriptRefs.at(i).number, fmt::format("script's object {}", i));
	}
	for (size_t i = 0; i < tail.objects.size(); ++i)
	{
		check(tail.objects.at(i).number, fmt::format("tail object {}", i));
	}
	for (size_t i = 0; i < tail.bookmarks.size(); ++i)
	{
		check(tail.bookmarks.at(i).object.number, fmt::format("bookmark {}", i));
	}
	check(tail.handObject.number, "hand's object");
	for (size_t i = 0; i < tail.handObjects.size(); ++i)
	{
		check(tail.handObjects.at(i).number, fmt::format("hand object {}", i));
	}
	if (std::ranges::find(tail.sounds.numbers, number) != tail.sounds.numbers.end())
	{
		parts.push_back("audio's sounds");
	}
	check(tail.audioObject.number, "audio's object");
	std::string joined;
	for (const auto& part : parts)
	{
		joined += (joined.empty() ? "" : ", ") + part;
	}
	return joined;
}

std::vector<Refusal> openblack::ecs::saved_world_write::ListRefusals(const ClassWriters& writers, const WorldSource& source)
{
	std::vector<Refusal> refusals;
	WriteContext context(writers, source);
	context.ListEveryRefusal();
	const auto game = context.TargetOf(k_GameObject);
	if (!game.has_value() || game->type != gamesave::k_GameType)
	{
		refusals.push_back({.type = "game", .reason = "there is no game object: no writer of its class and no kept world"});
		return refusals;
	}
	// the game object, then every object referenced, each once, in the order of its first reference; a kept record's
	// references are numbers of the loaded file, which the walk follows as WriteWorld's stream does
	std::vector<ObjectNumber> order {k_GameObject};
	std::set<ObjectNumber> done;
	// the object whose record first named each one, for the refusals' messages
	std::unordered_map<ObjectNumber, ObjectNumber> namedBy;
	const auto join = [&order, &done, &namedBy](ObjectNumber referenced, ObjectNumber by) {
		if (!done.contains(referenced) && std::ranges::find(order, referenced) == order.end())
		{
			order.push_back(referenced);
			namedBy.try_emplace(referenced, by);
		}
	};
	const auto nameOf = [&context](ObjectNumber number) {
		const auto target = context.TargetOf(number);
		return fmt::format("{} {}", target.has_value() ? gamesave::TypeName(target->type) : std::string("object"), number);
	};
	// the game object's record as made, to name the part of it that names an object refused
	std::optional<gamesave::GameHead> gameRecord;
	ObjectNumber previous = 0;
	for (size_t next = 0;; ++next)
	{
		// the objects the writers referenced so far join the walk, named by the record made last
		for (const auto number : context.Referenced())
		{
			join(number, previous);
		}
		if (next >= order.size())
		{
			break;
		}
		const auto number = order.at(next);
		done.insert(number);
		previous = number;
		const auto before = context.Refusals().size();
		const auto target = context.TargetOf(number);
		const auto type = target.has_value() ? gamesave::TypeName(target->type) : std::string("object");
		const auto made = context.RecordOf(number);
		if (number == k_GameObject && made.record != nullptr)
		{
			if (const auto* head = std::get_if<gamesave::GameHead>(made.record.get()); head != nullptr)
			{
				gameRecord = *head;
			}
		}
		const auto by = namedBy.find(number);
		auto namer = by != namedBy.end() && by->second != 0 ? nameOf(by->second) : std::string {};
		if (by != namedBy.end() && by->second == k_GameObject && gameRecord.has_value())
		{
			if (const auto part = GameRecordPartsNaming(*gameRecord, number); !part.empty())
			{
				namer += ": " + part;
			}
		}
		for (size_t i = before; i < context.Refusals().size(); ++i)
		{
			refusals.push_back({.type = type, .reason = context.Refusals().at(i), .namedBy = namer});
		}
		// the objects the record names (a kept record's by their loaded numbers) are walked too: written on a stream of
		// their own, each reference new to it is one
		if (made.record != nullptr)
		{
			gamesave::StreamWriter names({});
			gamesave::Write(names, *made.record, [&join, number](gamesave::StreamWriter& /*writer*/, ObjectNumber referenced) {
				join(referenced, number);
			});
		}
	}
	return refusals;
}
