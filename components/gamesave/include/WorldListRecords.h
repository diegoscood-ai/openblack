/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <tuple>

#include "ParticleRecords.h"

// The records of the classes the game object's global lists meet: the street lights, the particles and their atoms,
// the moving objects, the land's features and statics, the climates and the weather, the spells, the shields and the
// vortexes, the arena, the pots and the puzzles (see docs/wiki/engine/saved-games.md#the-game-objects-global-lists).
namespace openblack::gamesave
{

/// What every spell writes: its position, its objects and points, its values, its four objects, its floats and its position
struct SpellFields
{
	PositionPart position;
	uint32_t word {};
	Ref object;
	uint32_t secondWord {};
	Ref other;
	std::array<Point, 3> points {};
	std::array<uint32_t, 6> words {};
	std::array<uint8_t, 60> values {};
	float value {};
	uint8_t flag {};
	uint8_t secondFlag {};
	std::array<Ref, 4> objects {};
	uint32_t thirdWord {};
	std::array<float, 3> floats {};
	Point place {};
	std::array<float, 2> lastFloats {};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.position, part.word, part.object, part.secondWord, part.other, part.points[0], part.points[1],
		                part.points[2], part.words[0], part.words[1], part.words[2], part.words[3], part.words[4],
		                part.words[5], part.values, part.value, part.flag, part.secondFlag, part.objects[0], part.objects[1],
		                part.objects[2], part.objects[3], part.thirdWord, part.floats[0], part.floats[1], part.floats[2],
		                part.place, part.lastFloats[0], part.lastFloats[1]);
	}
	bool operator==(const SpellFields&) const = default;
};

/// A point of a beam: a word and two points
struct BeamPoint
{
	uint32_t word {};
	Point first {};
	Point second {};

	static auto FieldsOf(auto& part) { return std::tie(part.word, part.first, part.second); }
	bool operator==(const BeamPoint&) const = default;
};

/// An area of a climate: a point, 80 bytes, two words and a point
struct ClimateArea
{
	Point point {};
	std::array<uint8_t, 80> values {};
	std::array<uint32_t, 2> words {};
	Point second {};

	static auto FieldsOf(auto& part) { return std::tie(part.point, part.values, part.words[0], part.words[1], part.second); }
	bool operator==(const ClimateArea&) const = default;
};

/// A cell of a puzzle: a byte and three objects
struct PuzzleCell
{
	uint8_t byte {};
	std::array<Ref, 3> objects {};

	static auto FieldsOf(auto& part) { return std::tie(part.byte, part.objects[0], part.objects[1], part.objects[2]); }
	bool operator==(const PuzzleCell&) const = default;
};

/// A row of a puzzle: nine pairs of objects, then two objects and two words
struct PuzzleRow
{
	std::array<Ref, 18> pairs {};
	std::array<Ref, 2> objects {};
	std::array<uint32_t, 2> words {};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.pairs[0], part.pairs[1], part.pairs[2], part.pairs[3], part.pairs[4], part.pairs[5], part.pairs[6],
		                part.pairs[7], part.pairs[8], part.pairs[9], part.pairs[10], part.pairs[11], part.pairs[12],
		                part.pairs[13], part.pairs[14], part.pairs[15], part.pairs[16], part.pairs[17], part.objects[0],
		                part.objects[1], part.words[0], part.words[1]);
	}
	bool operator==(const PuzzleRow&) const = default;
};

/// A pot: a moving object, its resource, its town, its amount and its flags
struct PotRecord
{
	static constexpr uint32_t k_Type = 0x58;
	ThingPart thing;
	MobileObjectFields base;
	uint32_t resourceType {};
	Ref parentTown;
	uint32_t resourceAmount {};
	uint8_t flags {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.resourceType, record.parentTown, record.resourceAmount, record.flags);
	}
	bool operator==(const PotRecord&) const = default;
};

/// A street lantern: an object, a word and a byte from the game's data
struct StreetLanternRecord
{
	static constexpr uint32_t k_Type = 0xFE;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	uint32_t word {};
	uint8_t mark {};

	static auto FieldsOf(auto& record) { return std::tie(record.position, record.object, record.word, record.mark); }
	bool operator==(const StreetLanternRecord&) const = default;
};

/// A street light: its position
struct StreetLightRecord
{
	static constexpr uint32_t k_Type = 0xFF;
	ThingPart thing;
	PositionPart position;

	static auto FieldsOf(auto& record) { return std::tie(record.position); }
	bool operator==(const StreetLightRecord&) const = default;
};

/// A creature's poo: a moving object
struct PooRecord
{
	static constexpr uint32_t k_Type = 0x6B;
	ThingPart thing;
	MobileObjectFields base;

	static auto FieldsOf(auto& record) { return std::tie(record.base); }
	bool operator==(const PooRecord&) const = default;
};

/// A core of particle atoms
struct AtomCoreRecord
{
	static constexpr uint32_t k_Type = 0xA2;
	ThingPart thing;
	RefList atoms;
	RefList modifiers;
	RefList objects;
	Point point {};
	float value {};
	std::array<uint8_t, 48> zoomer {};
	std::array<float, 3> floats {};
	Point secondPoint {};
	uint32_t word {};
	uint32_t secondWord {};
	uint16_t half {};
	std::array<uint8_t, 56> drawPosition {};
	std::array<uint8_t, 56> secondDrawPosition {};
	float lastFloat {};
	uint32_t thirdWord {};
	std::array<uint8_t, 12> animation {};
	uint32_t fourthWord {};
	std::array<Ref, 3> others {};
	uint8_t byte {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.atoms, record.modifiers, record.objects, record.point, record.value, record.zoomer,
		                record.floats[0], record.floats[1], record.floats[2], record.secondPoint, record.word,
		                record.secondWord, record.half, record.drawPosition, record.secondDrawPosition, record.lastFloat,
		                record.thirdWord, record.animation, record.fourthWord, record.others[0], record.others[1],
		                record.others[2], record.byte);
	}
	bool operator==(const AtomCoreRecord&) const = default;
};

/// A particle's sprite
struct ParticleSpriteRecord
{
	static constexpr uint32_t k_Type = 0xB4;
	ThingPart thing;
	uint8_t renderByte {};
	uint8_t byte {};
	PersistentId persistent;
	std::array<uint8_t, 2> bytes {};
	std::array<uint32_t, 2> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.renderByte, record.byte, record.persistent, record.bytes[0], record.bytes[1], record.words[0],
		                record.words[1]);
	}
	bool operator==(const ParticleSpriteRecord&) const = default;
};

/// A bonfire: a rock's record and a word
struct BonfireRecord
{
	static constexpr uint32_t k_Type = 0x72;
	ThingPart thing;
	MultiMapFixedFields base;
	MobileStaticPart mobileStatic;
	std::array<uint32_t, 2> words {};
	Ref object;
	uint32_t word {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.mobileStatic, record.words[0], record.words[1], record.object, record.word);
	}
	bool operator==(const BonfireRecord&) const = default;
};

/// A firefly
struct FireFlyRecord
{
	static constexpr uint32_t k_Type = 0x2A;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	uint32_t word {};
	std::array<Point, 4> points {};
	std::array<uint32_t, 6> words {};
	std::array<float, 6> floats {};
	uint8_t byte {};
	Counted<uint32_t> first;
	Counted<uint32_t> second;
	uint8_t flag {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.word, record.points[0], record.points[1], record.points[2],
		                record.points[3], record.words[0], record.words[1], record.words[2], record.words[3], record.words[4],
		                record.words[5], record.floats[0], record.floats[1], record.floats[2], record.floats[3],
		                record.floats[4], record.floats[5], record.byte, record.first, record.second, record.flag);
	}
	bool operator==(const FireFlyRecord&) const = default;
};

/// A particle drawn as a point
struct ParticlePointRecord
{
	static constexpr uint32_t k_Type = 0xA8;
	ThingPart thing;
	uint8_t renderByte {};

	static auto FieldsOf(auto& record) { return std::tie(record.renderByte); }
	bool operator==(const ParticlePointRecord&) const = default;
};

/// A script's marker: its position
struct ScriptMarkerRecord
{
	static constexpr uint32_t k_Type = 0x7C;
	ThingPart thing;
	PositionPart position;

	static auto FieldsOf(auto& record) { return std::tie(record.position); }
	bool operator==(const ScriptMarkerRecord&) const = default;
};

/// An arena: its position, three words, its two objects with their count and an object
struct ArenaRecord
{
	static constexpr uint32_t k_Type = 0x41;
	ThingPart thing;
	PositionPart position;
	std::array<uint32_t, 3> words {};
	RefList objects;
	Ref other;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.words[0], record.words[1], record.words[2], record.objects, record.other);
	}
	bool operator==(const ArenaRecord&) const = default;
};

/// The data of a beam's sub-collection
struct BeamSubCollectionRecord
{
	static constexpr uint32_t k_Type = 0xB9;
	ThingPart thing;
	PersistentId persistent;
	uint32_t word {};
	Listed<BeamPoint> points;
	std::array<uint32_t, 2> lastWords {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.persistent, record.word, record.points, record.lastWords[0], record.lastWords[1]);
	}
	bool operator==(const BeamSubCollectionRecord&) const = default;
};

/// The data of an emitter's collection
struct EmitterCollectionRecord
{
	static constexpr uint32_t k_Type = 0xED;
	ThingPart thing;
	PersistentId persistent;
	std::array<uint32_t, 2> words {};
	uint8_t byte {};

	static auto FieldsOf(auto& record) { return std::tie(record.persistent, record.words[0], record.words[1], record.byte); }
	bool operator==(const EmitterCollectionRecord&) const = default;
};

/// A moving object of no other class
struct MobileObjectRecord
{
	static constexpr uint32_t k_Type = 0x53;
	ThingPart thing;
	MobileObjectFields base;

	static auto FieldsOf(auto& record) { return std::tie(record.base); }
	bool operator==(const MobileObjectRecord&) const = default;
};

/// A block of the Hanoi puzzle
struct HanoiBlockRecord
{
	static constexpr uint32_t k_Type = 0x30;
	ThingPart thing;
	MobileObjectFields base;
	Point point {};
	std::array<Ref, 3> objects {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.point, record.objects[0], record.objects[1], record.objects[2]);
	}
	bool operator==(const HanoiBlockRecord&) const = default;
};

/// A reward
struct RewardRecord
{
	static constexpr uint32_t k_Type = 0x46;
	ThingPart thing;
	MobileObjectFields base;
	std::array<Ref, 3> objects {};
	std::array<uint32_t, 10> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.objects[0], record.objects[1], record.objects[2], record.words[0], record.words[1],
		                record.words[2], record.words[3], record.words[4], record.words[5], record.words[6], record.words[7],
		                record.words[8], record.words[9]);
	}
	bool operator==(const RewardRecord&) const = default;
};

/// A joint of a particle chain
struct ParticleChainJointRecord
{
	static constexpr uint32_t k_Type = 0xB3;
	ThingPart thing;
	uint8_t renderByte {};
	Ref chain;
	std::array<uint32_t, 3> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.renderByte, record.chain, record.words[0], record.words[1], record.words[2]);
	}
	bool operator==(const ParticleChainJointRecord&) const = default;
};

/// Mist: its position and three words
struct MistRecord
{
	static constexpr uint32_t k_Type = 0x48;
	ThingPart thing;
	PositionPart position;
	std::array<uint32_t, 3> words {};

	static auto FieldsOf(auto& record) { return std::tie(record.position, record.words[0], record.words[1], record.words[2]); }
	bool operator==(const MistRecord&) const = default;
};

/// A stream: its nodes, 16 bytes each with their count, and two words
struct StreamRecord
{
	static constexpr uint32_t k_Type = 0x47;
	ThingPart thing;
	Counted<std::array<uint8_t, 16>> nodes;
	std::array<uint32_t, 2> words {};

	static auto FieldsOf(auto& record) { return std::tie(record.nodes, record.words[0], record.words[1]); }
	bool operator==(const StreamRecord&) const = default;
};

/// An influence ring: its position, the object it follows (0 for none), its player, its radius and its anti word (any
/// but 0 for an anti-influence ring)
struct InfluenceRingRecord
{
	static constexpr uint32_t k_Type = 0x49;
	ThingPart thing;
	PositionPart position;
	Ref attached;
	Ref player;
	float radius {};
	uint32_t anti {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.attached, record.player, record.radius, record.anti);
	}
	bool operator==(const InfluenceRingRecord&) const = default;
};

/// A waypoint: its position
struct WaypointRecord
{
	static constexpr uint32_t k_Type = 0xA1;
	ThingPart thing;
	PositionPart position;

	static auto FieldsOf(auto& record) { return std::tie(record.position); }
	bool operator==(const WaypointRecord&) const = default;
};

/// A feature of the land: a building of several cells
struct FeatureRecord
{
	static constexpr uint32_t k_Type = 0x74;
	ThingPart thing;
	MultiMapFixedFields base;

	static auto FieldsOf(auto& record) { return std::tie(record.base); }
	bool operator==(const FeatureRecord&) const = default;
};

/// A weather thing's storm as the thing writes it: its place (its descriptor's), its descriptor, its age, its speed
/// and where it moves to
struct WeatherStorm
{
	Point point {};
	std::array<uint8_t, 80> descriptor {};
	uint32_t age {};
	uint32_t speed {};
	Point target {};

	bool operator==(const WeatherStorm&) const = default;
};

/// Whether the thing has a storm (a word, 1 or 0), then its storm when it has one (a thing whose storm is over writes
/// the word 0 and nothing more)
struct WeatherStormPart
{
	uint32_t present {0};
	std::optional<WeatherStorm> storm;

	bool operator==(const WeatherStormPart&) const = default;
};
bool ReadPart(StreamReader& reader, WeatherStormPart& part, const ReadNewObject& readNew);
void WritePart(StreamWriter& writer, const WeatherStormPart& part, const WriteNewObject& writeNew);

/// A thing of the weather: its place, its own descriptor, its storm and whether the wind moves it
struct WeatherThingRecord
{
	static constexpr uint32_t k_Type = 0x43;
	ThingPart thing;
	PositionPart position;
	std::array<uint8_t, 80> descriptor {};
	WeatherStormPart storm;
	uint32_t affectedByWind {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.descriptor, record.storm, record.affectedByWind);
	}
	bool operator==(const WeatherThingRecord&) const = default;
};

/// A static object that animates: its frames with their count and a word
struct AnimatedStaticRecord
{
	static constexpr uint32_t k_Type = 0x75;
	ThingPart thing;
	MultiMapFixedFields base;
	Counted<uint32_t> frames;
	uint32_t word {};

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.frames, record.word); }
	bool operator==(const AnimatedStaticRecord&) const = default;
};

/// A climate
struct ClimateRecord
{
	static constexpr uint32_t k_Type = 0xFC;
	ThingPart thing;
	Point point {};
	std::array<uint32_t, 8> words {};
	std::array<uint8_t, 16> sixteen {};
	std::array<uint8_t, 8> eight {};
	Point secondPoint {};
	uint32_t word {};
	uint8_t byte {};
	Listed<ClimateArea> areas;
	std::array<uint32_t, 7> lastWords {};
	uint8_t lastByte {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.point, record.words[0], record.words[1], record.words[2], record.words[3], record.words[4],
		                record.words[5], record.words[6], record.words[7], record.sixteen, record.eight, record.secondPoint,
		                record.word, record.byte, record.areas, record.lastWords[0], record.lastWords[1], record.lastWords[2],
		                record.lastWords[3], record.lastWords[4], record.lastWords[5], record.lastWords[6], record.lastByte);
	}
	bool operator==(const ClimateRecord&) const = default;
};

/// A spell that writes only what every spell writes: the plain spell (fireball, lightning, the beam's explosion),
/// the storm and the tornado, and the healing spell
template <uint32_t Type>
struct SpellOnlyRecordOf
{
	static constexpr uint32_t k_Type = Type;
	ThingPart thing;
	SpellFields spell;

	static auto FieldsOf(auto& record) { return std::tie(record.spell); }
	bool operator==(const SpellOnlyRecordOf&) const = default;
};

using SpellRecord = SpellOnlyRecordOf<0x9>;
using SpellStormAndTornadoRecord = SpellOnlyRecordOf<0x10>;
using SpellHealRecord = SpellOnlyRecordOf<0x12>;

/// The food and wood spells: every spell's values and a byte
struct SpellResourceRecord
{
	static constexpr uint32_t k_Type = 0xA;
	ThingPart thing;
	SpellFields spell;
	uint8_t byte {};

	static auto FieldsOf(auto& record) { return std::tie(record.spell, record.byte); }
	bool operator==(const SpellResourceRecord&) const = default;
};

/// A spell with objects: every spell's values and its objects, with their count; the teleport writes the same
template <uint32_t Type>
struct SpellWithObjectsRecordOf
{
	static constexpr uint32_t k_Type = Type;
	ThingPart thing;
	SpellFields spell;
	RefList objects;

	static auto FieldsOf(auto& record) { return std::tie(record.spell, record.objects); }
	bool operator==(const SpellWithObjectsRecordOf&) const = default;
};

using SpellWithObjectsRecord = SpellWithObjectsRecordOf<0x14>;
using SpellTeleportRecord = SpellWithObjectsRecordOf<0x15>;

/// What a flock spell adds to a spell with objects: an object, two words, a point and a word
struct SpellFlockFields
{
	Ref object;
	std::array<uint32_t, 2> words {};
	Point point {};
	uint32_t lastWord {};

	static auto FieldsOf(auto& part) { return std::tie(part.object, part.words[0], part.words[1], part.point, part.lastWord); }
	bool operator==(const SpellFlockFields&) const = default;
};

/// The flock spell on the ground
struct SpellFlockGroundRecord
{
	static constexpr uint32_t k_Type = 0xE;
	ThingPart thing;
	SpellFields spell;
	RefList objects;
	SpellFlockFields flock;

	static auto FieldsOf(auto& record) { return std::tie(record.spell, record.objects, record.flock); }
	bool operator==(const SpellFlockGroundRecord&) const = default;
};

/// The flying flock spell: the flock's values, two words and a byte
struct SpellFlockFlyingRecord
{
	static constexpr uint32_t k_Type = 0xD;
	ThingPart thing;
	SpellFields spell;
	RefList objects;
	SpellFlockFields flock;
	std::array<uint32_t, 2> words {};
	uint8_t byte {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.spell, record.objects, record.flock, record.words[0], record.words[1], record.byte);
	}
	bool operator==(const SpellFlockFlyingRecord&) const = default;
};

/// The forest spell: every spell's values, a byte, an object and a word
struct SpellForestRecord
{
	static constexpr uint32_t k_Type = 0xF;
	ThingPart thing;
	SpellFields spell;
	uint8_t byte {};
	Ref object;
	uint32_t word {};

	static auto FieldsOf(auto& record) { return std::tie(record.spell, record.byte, record.object, record.word); }
	bool operator==(const SpellForestRecord&) const = default;
};

/// A creature spell: every spell's values, an object and a byte
struct SpellCreatureRecord
{
	static constexpr uint32_t k_Type = 0x13;
	ThingPart thing;
	SpellFields spell;
	Ref object;
	uint8_t byte {};

	static auto FieldsOf(auto& record) { return std::tie(record.spell, record.object, record.byte); }
	bool operator==(const SpellCreatureRecord&) const = default;
};

/// The water spell
struct SpellWaterRecord
{
	static constexpr uint32_t k_Type = 0xC;
	ThingPart thing;
	SpellFields spell;
	uint32_t word {};

	static auto FieldsOf(auto& record) { return std::tie(record.spell, record.word); }
	bool operator==(const SpellWaterRecord&) const = default;
};

/// The shield spell
struct SpellShieldRecord
{
	static constexpr uint32_t k_Type = 0x11;
	ThingPart thing;
	SpellFields spell;
	RefList objects;
	RefList shields;
	std::array<Ref, 2> others {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.spell, record.objects, record.shields, record.others[0], record.others[1]);
	}
	bool operator==(const SpellShieldRecord&) const = default;
};

/// The data of the hand's sprinkle
struct HandSprinkleCollectionRecord
{
	static constexpr uint32_t k_Type = 0xF5;
	ThingPart thing;
	PersistentId persistent;
	uint8_t byte {};
	uint32_t word {};

	static auto FieldsOf(auto& record) { return std::tie(record.persistent, record.byte, record.word); }
	bool operator==(const HandSprinkleCollectionRecord&) const = default;
};

/// A physical shield
struct PhysicalShieldRecord
{
	static constexpr uint32_t k_Type = 0x17;
	ThingPart thing;
	FixedFields base;
	Ref owner;
	uint32_t info {};
	std::array<uint32_t, 2> words {};
	std::array<uint8_t, 52> first {};
	std::array<uint8_t, 52> second {};
	std::array<uint32_t, 6> secondWords {};
	std::array<uint8_t, 2> bytes {};
	uint32_t lastWord {};
	Ref object;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.owner, record.info, record.words[0], record.words[1], record.first, record.second,
		                record.secondWords[0], record.secondWords[1], record.secondWords[2], record.secondWords[3],
		                record.secondWords[4], record.secondWords[5], record.bytes[0], record.bytes[1], record.lastWord,
		                record.object);
	}
	bool operator==(const PhysicalShieldRecord&) const = default;
};

/// A vortex into another land
struct LandscapeVortexInRecord
{
	static constexpr uint32_t k_Type = 0x1B;
	ThingPart thing;
	MultiMapFixedFields base;
	MobileStaticPart mobileStatic;
	Point point {};
	std::array<uint32_t, 4> words {};
	Counted<uint8_t> bytes;
	std::array<Ref, 2> objects {};
	Counted<uint32_t> values;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.mobileStatic, record.point, record.words[0], record.words[1], record.words[2],
		                record.words[3], record.bytes, record.objects[0], record.objects[1], record.values);
	}
	bool operator==(const LandscapeVortexInRecord&) const = default;
};

/// The data of an atom's sound
struct SoundAtomDataRecord
{
	static constexpr uint32_t k_Type = 0xDC;
	ThingPart thing;
	PersistentId persistent;
	uint8_t byte {};

	static auto FieldsOf(auto& record) { return std::tie(record.persistent, record.byte); }
	bool operator==(const SoundAtomDataRecord&) const = default;
};

/// A particle drawn as an animated textured object
struct ParticleTexturedObjectRecord
{
	static constexpr uint32_t k_Type = 0xAE;
	ThingPart thing;
	uint8_t renderByte {};
	PersistentId persistent;
	uint8_t byte {};
	std::array<uint32_t, 2> words {};
	std::array<uint8_t, 3> bytes {};
	uint32_t lastWord {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.renderByte, record.persistent, record.byte, record.words[0], record.words[1], record.bytes[0],
		                record.bytes[1], record.bytes[2], record.lastWord);
	}
	bool operator==(const ParticleTexturedObjectRecord&) const = default;
};

/// A chain of particles: its joints, 28 bytes each with their count
struct ChainRecord
{
	static constexpr uint32_t k_Type = 0xB6;
	ThingPart thing;
	PersistentId persistent;
	std::array<uint32_t, 8> words {};
	uint8_t byte {};
	uint32_t word {};
	Counted<std::array<uint8_t, 28>> joints;
	uint32_t lastWord {};
	uint8_t lastByte {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.persistent, record.words[0], record.words[1], record.words[2], record.words[3], record.words[4],
		                record.words[5], record.words[6], record.words[7], record.byte, record.word, record.joints,
		                record.lastWord, record.lastByte);
	}
	bool operator==(const ChainRecord&) const = default;
};

/// A particle drawn as mist
struct ParticleMistRecord
{
	static constexpr uint32_t k_Type = 0xAB;
	ThingPart thing;
	uint8_t renderByte {};
	PersistentId persistent;
	uint32_t word {};
	uint8_t byte {};
	uint16_t half {};
	Point point {};
	uint32_t lastWord {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.renderByte, record.persistent, record.word, record.byte, record.half, record.point,
		                record.lastWord);
	}
	bool operator==(const ParticleMistRecord&) const = default;
};

/// The data of a beam's atom
struct BeamAtomDataRecord
{
	static constexpr uint32_t k_Type = 0xBF;
	ThingPart thing;
	PersistentId persistent;
	uint32_t word {};
	Ref object;
	std::array<Point, 2> points {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.persistent, record.word, record.object, record.points[0], record.points[1]);
	}
	bool operator==(const BeamAtomDataRecord&) const = default;
};

/// A particle's sound: a word, its action, a word, an object and a byte
struct ParticleSoundRecord
{
	static constexpr uint32_t k_Type = 0x16;
	ThingPart thing;
	uint32_t word {};
	std::array<uint8_t, 24> action {};
	uint32_t secondWord {};
	Ref object;
	uint8_t byte {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.word, record.action, record.secondWord, record.object, record.byte);
	}
	bool operator==(const ParticleSoundRecord&) const = default;
};

/// The data of a collection of atoms at a target
struct TargetAtomsCollectionRecord
{
	static constexpr uint32_t k_Type = 0xC4;
	ThingPart thing;
	PersistentId persistent;
	uint32_t word {};
	Ref target;

	static auto FieldsOf(auto& record) { return std::tie(record.persistent, record.word, record.target); }
	bool operator==(const TargetAtomsCollectionRecord&) const = default;
};

/// The data of a trail: its places, 64 bytes each with their count
struct TrailCollectionRecord
{
	static constexpr uint32_t k_Type = 0xF3;
	ThingPart thing;
	PersistentId persistent;
	uint8_t byte {};
	uint32_t word {};
	Counted<std::array<uint8_t, 64>> places;

	static auto FieldsOf(auto& record) { return std::tie(record.persistent, record.byte, record.word, record.places); }
	bool operator==(const TrailCollectionRecord&) const = default;
};

/// A puzzle game
struct PuzzleGameRecord
{
	static constexpr uint32_t k_Type = 0x2C;
	ThingPart thing;
	PositionPart position;
	std::array<uint32_t, 4> words {};
	Counted<uint32_t> values;
	RefList first;
	RefList second;
	RefList third;
	std::array<Ref, 7> objects {};
	std::array<PuzzleCell, 9> cells {};
	std::array<PuzzleRow, 8> rows {};
	std::array<Ref, 18> lastPairs {};
	std::array<Ref, 18> pieces {};
	RefList fourth;
	std::array<uint32_t, 2> secondWords {};
	uint16_t half {};
	std::array<uint32_t, 6> thirdWords {};
	std::array<Ref, 4> others {};
	uint32_t lastWord {};
	std::array<uint16_t, 2> halves {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(
		    record.position, record.words[0], record.words[1], record.words[2], record.words[3], record.values, record.first,
		    record.second, record.third, record.objects[0], record.objects[1], record.objects[2], record.objects[3],
		    record.objects[4], record.objects[5], record.objects[6], record.cells[0], record.cells[1], record.cells[2],
		    record.cells[3], record.cells[4], record.cells[5], record.cells[6], record.cells[7], record.cells[8],
		    record.rows[0], record.rows[1], record.rows[2], record.rows[3], record.rows[4], record.rows[5], record.rows[6],
		    record.rows[7], record.lastPairs[0], record.lastPairs[1], record.lastPairs[2], record.lastPairs[3],
		    record.lastPairs[4], record.lastPairs[5], record.lastPairs[6], record.lastPairs[7], record.lastPairs[8],
		    record.lastPairs[9], record.lastPairs[10], record.lastPairs[11], record.lastPairs[12], record.lastPairs[13],
		    record.lastPairs[14], record.lastPairs[15], record.lastPairs[16], record.lastPairs[17], record.pieces[0],
		    record.pieces[1], record.pieces[2], record.pieces[3], record.pieces[4], record.pieces[5], record.pieces[6],
		    record.pieces[7], record.pieces[8], record.pieces[9], record.pieces[10], record.pieces[11], record.pieces[12],
		    record.pieces[13], record.pieces[14], record.pieces[15], record.pieces[16], record.pieces[17], record.fourth,
		    record.secondWords[0], record.secondWords[1], record.half, record.thirdWords[0], record.thirdWords[1],
		    record.thirdWords[2], record.thirdWords[3], record.thirdWords[4], record.thirdWords[5], record.others[0],
		    record.others[1], record.others[2], record.others[3], record.lastWord, record.halves[0], record.halves[1]);
	}
	bool operator==(const PuzzleGameRecord&) const = default;
};

/// What a vortex keeps of the land it leads to: a byte, a word and two texts with their counts
struct VortexSaveRecord
{
	static constexpr uint32_t k_Type = 0x106;
	ThingPart thing;
	uint8_t byte {};
	uint32_t word {};
	Counted<uint8_t> name;
	Counted<uint8_t> file;

	static auto FieldsOf(auto& record) { return std::tie(record.byte, record.word, record.name, record.file); }
	bool operator==(const VortexSaveRecord&) const = default;
};

/// The data of an atom a vortex attracts
struct VortexAttractAtomDataRecord
{
	static constexpr uint32_t k_Type = 0xC9;
	ThingPart thing;
	PersistentId persistent;
	uint32_t word {};
	Point point {};
	std::array<uint8_t, 3> bytes {};
	uint32_t secondWord {};
	Ref object;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.persistent, record.word, record.point, record.bytes[0], record.bytes[1], record.bytes[2],
		                record.secondWord, record.object);
	}
	bool operator==(const VortexAttractAtomDataRecord&) const = default;
};

/// A particle drawn as a game object
struct ParticleGameObjectRecord
{
	static constexpr uint32_t k_Type = 0xB2;
	ThingPart thing;
	uint8_t renderByte {};
	uint32_t word {};
	std::array<Ref, 2> objects {};
	std::array<uint8_t, 48> zoomer {};
	uint8_t byte {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.renderByte, record.word, record.objects[0], record.objects[1], record.zoomer, record.byte);
	}
	bool operator==(const ParticleGameObjectRecord&) const = default;
};

} // namespace openblack::gamesave
