/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SavedRecords.h>
#include <entt/entity/entity.hpp>

namespace openblack::gamesave
{
struct WorldRead;
}

namespace openblack::ecs
{
class Registry;
}

// The world file of a saved game written from openblack's entities, in the original's layout and order: the game
// object first, then each object's record at its first reference, depth first, as the original's save walks its
// objects. Each class's record is made by a class writer that its area registers; an entity whose class has no
// writer yet is written from the record its load kept, and a loaded object no entity stands for from the world read
// the load kept, so that the file stays whole while the writers come. See
// docs/wiki/engine/saved-games.md#openblack.
namespace openblack::ecs::saved_world_write
{

class WriteContext;

/// An object of the file that is not an entity (a forest, a climate): its class's number in the file and an id its
/// source gives it, unique in the class
struct SourceKey
{
	uint32_t type {0};
	uint64_t id {0};

	auto operator<=>(const SourceKey&) const = default;
};

/// An object that is a part of an entity rather than an entity of its own (a footpath's node): the entity that holds
/// it and the number that names it there
struct Part
{
	entt::entity owner {entt::null};
	uint32_t index {0};

	bool operator==(const Part&) const = default;
};

static_assert(sizeof(entt::entity) <= sizeof(uint32_t), "a part's id holds its owner in its high 32 bits");

/// The source id of a part of an entity: its owner in the high 32 bits, its number there in the low ones, so that the
/// parts of one owner are told apart and each part keeps its id for the whole write
[[nodiscard]] constexpr uint64_t PartId(entt::entity owner, uint32_t index)
{
	return (static_cast<uint64_t>(entt::to_integral(owner)) << 32U) | index;
}

/// The owner and the number of the part a source id names (the inverse of PartId)
[[nodiscard]] constexpr Part PartOf(uint64_t id)
{
	return {.owner = static_cast<entt::entity>(static_cast<uint32_t>(id >> 32U)), .index = static_cast<uint32_t>(id)};
}

/// Makes the whole record of an entity of the class; nothing when it cannot (the write then fails and says so). The
/// references in the record are the numbers `context.Reference` gives
using ClassWriter = std::function<std::optional<gamesave::SavedRecord>(WriteContext& context, entt::entity entity)>;
/// Whether an entity is of the class: the writer of the one class that claims an entity writes it
using ClassClaim = std::function<bool(const Registry& registry, entt::entity entity)>;

/// The ids of a non-entity source's objects, in the game's order of the class (the order its list keeps them in)
using SourceKeys = std::function<std::vector<uint64_t>(const Registry& registry)>;
/// Makes the whole record of a non-entity source's object; nothing when it cannot (the write then fails and says so)
using SourceWriter = std::function<std::optional<gamesave::SavedRecord>(WriteContext& context, uint64_t id)>;

/// The game object's global lists, in the order its save writes them (`GameHead::lists`)
enum class GlobalList : uint8_t
{
	StreetLights,
	StreetLanterns,
	Rewards,
	Balls,
	Forests,
	Livings,
	Spells,
	ParticleContainers,
	Dances,
	Reactions,
	MobileObjects,
	VillagersWithoutTown,
	Fields,
	FishFarms,
	FireEffects,
	Fireflies,
	PuzzleGame,
	Fireballs,
	MapShields,
	Mist,
	FoodPiles,
	Flocks,
	InfluenceRings,
	WeatherThings,
	ScriptHighlights,
	ThingsWithPosition,
	Streams,
	Waypoints,
	Footpaths,
	Waterfalls,
	Arenas,
	Towns,
	BuildingSites,
	MultiMapFixed,
	AnimatedStatics,
	Objects,
	Trees,
	BigForests,
	Climates,
	TownCentres,
	Whales,
	TownArtifacts,
	Fragments,
	FootpathFinders,

	_COUNT
};
static_assert(static_cast<size_t>(GlobalList::_COUNT) == gamesave::k_GlobalListCount);

/// Makes a global list of the game object, in the game's order of the list: each object by the number
/// `context.Reference` gives it
using ListWriter = std::function<std::vector<gamesave::ObjectNumber>(WriteContext& context)>;

/// The class writers, by the number the world file gives each class (a record's `k_Type`), and the writers of the
/// game object's global lists
class ClassWriters
{
public:
	/// Registers the writer of a class. A second writer of the same class is refused: false. A class whose objects are
	/// named only by their class (a citadel through its first heart) is registered with no claim ({})
	bool Register(uint32_t type, ClassClaim claims, ClassWriter write);
	/// Registers the writer of a global list. A list with none is written as the load kept it. A second writer of the
	/// same list is refused: false
	bool RegisterList(GlobalList list, ListWriter write);
	[[nodiscard]] const ListWriter* ListWriterOf(GlobalList list) const;
	/// Registers a class whose objects are not entities: `keys` lists their ids in the game's order, `write` makes the
	/// record of one. A second source of the same class, or a class that has an entity writer, is refused: false
	bool RegisterSource(uint32_t type, SourceKeys keys, SourceWriter write);
	[[nodiscard]] bool HasSource(uint32_t type) const { return _sources.contains(type); }
	[[nodiscard]] const SourceWriter* SourceWriterOf(uint32_t type) const;
	[[nodiscard]] const SourceKeys* SourceKeysOf(uint32_t type) const;

	[[nodiscard]] bool Has(uint32_t type) const { return _writers.contains(type); }
	[[nodiscard]] const ClassWriter* WriterOf(uint32_t type) const;
	/// The classes whose writers claim the entity, in their numbers' order: one for an entity that has a writer
	[[nodiscard]] std::vector<uint32_t> ClaimsOf(const Registry& registry, entt::entity entity) const;
	/// Whether the entity is still of the class: false only when the class's writer has a claim that refuses it and
	/// another class's writer claims it (a loaded tree that has become a dead tree since is no longer the tree its
	/// loaded number names). A class named only through another object, by its class, registers no claim
	[[nodiscard]] bool StillOf(uint32_t type, const Registry& registry, entt::entity entity) const;
	[[nodiscard]] size_t Size() const { return _writers.size(); }

private:
	struct Entry
	{
		ClassClaim claims;
		ClassWriter write;
	};
	std::map<uint32_t, Entry> _writers;
	std::map<GlobalList, ListWriter> _lists;
	struct Source
	{
		SourceKeys keys;
		SourceWriter write;
	};
	std::map<uint32_t, Source> _sources;
};

/// An object of the loaded file that became an entity, or an object of a non-entity source: the entity (or the
/// source's id, with entt::null) and the class of its record there (an entity can stand for more than one object of
/// the file, such as a citadel's first heart and the citadel)
struct LoadedObject
{
	entt::entity entity {entt::null};
	uint32_t type {0};
	/// The id a non-entity source gives the object
	std::optional<uint64_t> id;
};
/// The loaded objects, by their numbers in the loaded file
using LoadedObjects = std::unordered_map<gamesave::ObjectNumber, LoadedObject>;

/// What a world file is written from
struct WorldSource
{
	const Registry& registry;
	/// The land's file, as the game opened it
	std::string landscape;
	/// The world read the load kept: the records of the loaded objects no entity stands for; none for a game that was
	/// not loaded
	const gamesave::WorldRead* kept {nullptr};
	/// The entities and the non-entity sources' objects the loaded objects became, by their numbers in the loaded
	/// file: each area adds its own as its load makes them
	LoadedObjects loaded;
	/// The name the copy of a crossing file beside the world file takes, as a crossing file's save writes it: the world
	/// file's temporary file's with "vor" after it; empty when the save has no world file
	std::string crossingCopy;
};

/// Where a written record came from
enum class RecordSource : uint8_t
{
	/// Its class's writer, from the entity
	Writer,
	/// The record the entity's load kept on it (components::LoadedRecord)
	EntityRecord,
	/// The world read the load kept
	KeptWorld,
};

/// A record the write put in the file
struct WrittenRecord
{
	/// Its number in the file written: the order of its first reference, the game object 1
	gamesave::ObjectNumber number {0};
	uint32_t type {0};
	RecordSource source {RecordSource::Writer};
	/// The entity it was written from; entt::null for the game object, a kept world's record and a source's object
	entt::entity entity {entt::null};
	/// The non-entity source's id of the object
	std::optional<uint64_t> id;

	bool operator==(const WrittenRecord&) const = default;
};

/// A written world file
struct WorldWrite
{
	/// The whole file when complete; as far as it was written when not
	std::vector<uint8_t> bytes;
	/// The records in the order they were written
	std::vector<WrittenRecord> records;
	/// Why the write stopped: the first object it could not write; empty when the file is whole
	std::string failure;

	[[nodiscard]] bool Complete() const { return failure.empty(); }
};

/// What a class writer is given: the registry and the numbers of the objects its record names
class WriteContext
{
public:
	WriteContext(const ClassWriters& writers, const WorldSource& source);

	[[nodiscard]] const Registry& GetRegistry() const { return _source.registry; }
	[[nodiscard]] const WorldSource& Source() const { return _source; }
	/// The names of the entity's components, for a refusal's message
	[[nodiscard]] std::string ComponentsOf(entt::entity entity) const;
	[[nodiscard]] const ClassWriters& Writers() const { return _writers; }
	/// The record of the loaded file's object of that number, as the load kept it; none for a game not loaded
	[[nodiscard]] const gamesave::SavedRecord* KeptWorldRecord(gamesave::ObjectNumber number) const;
	/// Whether the loaded file's object of that number became an entity that has been destroyed since, or that is no
	/// longer of the object's class (the original deletes the object and makes another of the new class)
	[[nodiscard]] bool IsGone(gamesave::ObjectNumber number) const;
	/// A kept list of the loaded file's objects without those gone since, in its order, as an object leaves the lists
	/// that hold it as it is deleted
	void DropGone(gamesave::RefList& list) const;
	/// A kept object of the loaded file, none (0) once it is gone
	[[nodiscard]] gamesave::ObjectNumber UnlessGone(gamesave::ObjectNumber number) const { return IsGone(number) ? 0 : number; }

	/// The number by which a record names the entity's object, of the class its writer claims (or, with no writer, of
	/// the record its load kept). The same object has the same number for the whole write; entt::null is 0. A writer
	/// never makes a number itself: the file gives each object its own number at its first reference, in the walk's
	/// order, as the original does. 0, and the write fails, for an entity of no class or of more than one
	[[nodiscard]] gamesave::ObjectNumber Reference(entt::entity entity);
	/// The same for the entity's object of the class given, for an entity that stands for more than one object
	[[nodiscard]] gamesave::ObjectNumber Reference(entt::entity entity, uint32_t type);
	/// The same for an object of a non-entity source, numbered in the same walk
	[[nodiscard]] gamesave::ObjectNumber Reference(const SourceKey& key);
	/// The ids of a non-entity source's objects in the game's order; none for a class with no source
	[[nodiscard]] std::vector<uint64_t> Keys(uint32_t type) const;

	/// The class of the entity's object: its writer's, else that of the one record its load kept
	[[nodiscard]] std::optional<uint32_t> ClassOf(entt::entity entity) const;
	/// The record of the class given the entity's load kept on it, whole
	[[nodiscard]] std::optional<gamesave::SavedRecord> KeptRecordOf(entt::entity entity, uint32_t type) const;
	/// The record a non-entity source's object had in the loaded file, from the world read the load kept, whole: none
	/// for an object made since the load
	[[nodiscard]] std::optional<gamesave::SavedRecord> KeptRecordOf(const SourceKey& key) const;

	/// Stops the write with the reason; the first reason is kept
	void Fail(std::string reason);

	/// (openblack) the listing of every refusal (ListRefusals): each reason is kept and nothing stops, and every object
	/// referenced is noted in the order of its first reference
	void ListEveryRefusal() { _listing = true; }
	[[nodiscard]] const std::vector<std::string>& Refusals() const { return _refusals; }
	/// The objects referenced while listing, in the order of their first reference
	[[nodiscard]] const std::vector<gamesave::ObjectNumber>& Referenced() const { return _referenced; }
	[[nodiscard]] const std::string& Failure() const { return _failure; }

	/// The object a number names: its entity and class, a non-entity source's object (its id), or a record of the
	/// kept world (entt::null and no id)
	struct Target
	{
		entt::entity entity {entt::null};
		uint32_t type {0};
		std::optional<uint64_t> id;
	};
	[[nodiscard]] std::optional<Target> TargetOf(gamesave::ObjectNumber number) const;
	/// The record of an object and where it came from
	struct MadeRecord
	{
		/// On the heap: records are written inside records, and a record can be thousands of bytes
		std::unique_ptr<gamesave::SavedRecord> record;
		RecordSource source {RecordSource::Writer};
	};
	/// The record of the object the number names, made by its class's writer or taken from what its load kept; no
	/// record, and the write fails, when there is neither
	[[nodiscard]] MadeRecord RecordOf(gamesave::ObjectNumber number);

private:
	const ClassWriters& _writers;
	const WorldSource& _source;
	std::map<std::pair<entt::entity, uint32_t>, gamesave::ObjectNumber> _numbers;
	std::map<SourceKey, gamesave::ObjectNumber> _keyNumbers;
	std::unordered_map<gamesave::ObjectNumber, Target> _targets;
	gamesave::ObjectNumber _next {1};
	std::string _failure;
	bool _listing {false};
	std::vector<std::string> _refusals;
	std::vector<gamesave::ObjectNumber> _referenced;
	std::set<gamesave::ObjectNumber> _seen;
	/// Notes an object referenced while listing
	void NoteReferenced(gamesave::ObjectNumber number);
};

/// Writes the world file: the start of the file, then the game object (its class's writer, called with entt::null,
/// or the kept world's first record) and every object it names, depth first
[[nodiscard]] WorldWrite WriteWorld(const ClassWriters& writers, const WorldSource& source);

/// One reason a save would not be written: the class of the object refused, and why
struct Refusal
{
	std::string type;
	std::string reason;
	/// The class and number of the object whose record first named this one (empty for the game object itself), to
	/// find what still holds an object the game has destroyed
	std::string namedBy;

	bool operator==(const Refusal&) const = default;
};

/// (openblack, a debug readout) every reason the world file could not be written, in one pass: the same walk as
/// WriteWorld's, from the game object through every object a record references, each record made by its class's
/// writer or taken from what its load kept, but going on past each refusal. Nothing is written; WriteWorld still
/// stops at the first refusal. Empty when the file can be written whole
[[nodiscard]] std::vector<Refusal> ListRefusals(const ClassWriters& writers, const WorldSource& source);

/// (openblack, a debug readout) where the game object's record names the object: a player's place, a global list by its
/// place, a slot of the scripts' object table, a bookmark, or the tail's field by name (the help's, the script's, the
/// hand's, the audio's objects); several joined with ", ", empty when the record does not name it
[[nodiscard]] std::string GameRecordPartsNaming(const gamesave::GameHead& game, gamesave::ObjectNumber number);

} // namespace openblack::ecs::saved_world_write
