/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>

#include "ObjectStream.h"

// The parts that the records of the world file's objects share, the base classes' values, and
// the references between the objects. Every record starts with the part every object writes (its
// class's number, a word of its own, the running sum, whether it is destroyed and its flags); then
// each class adds its values, the base class first. A reference to another object is its number in
// the stream: an object new to the stream has its whole record at once after its number, so each
// reader and writer takes the function that reads or writes the records of the new objects it
// meets.
namespace openblack::gamesave
{

/// An object of the stream by its number; 0 is no object
using ObjectNumber = uint32_t;

/// The game's position on the map: x and z in 16.16 parts of a cell, then the height
struct MapCoords
{
	int32_t x {0};
	int32_t z {0};
	float altitude {0.0f};

	bool operator==(const MapCoords&) const = default;
};
static_assert(sizeof(MapCoords) == 12);

/// A point in the world, in the game's units
using Point = std::array<float, 3>;

/// The flag of a thing that a particle system carries
inline constexpr uint8_t k_ThingFlagCarriedByParticles = 0x10;
/// The flag of a positioned thing that the physics moves
inline constexpr uint16_t k_PositionFlagInPhysics = 0x40;

/// The start of every record: what the class writes for every object
struct ThingPart
{
	/// A word that the class writes, for example the number of a player
	uint32_t extra {0};
	/// The running sum of the stream at the record, as the file has it. A write puts the writer's own sum.
	uint32_t checksum {0};
	uint32_t destroyed {0};
	uint8_t flags {0};

	bool operator==(const ThingPart&) const = default;
};

/// What a thing with a position adds
struct PositionPart
{
	MapCoords position {};
	uint16_t flags {0};

	bool operator==(const PositionPart&) const = default;
};

/// The body of an object that the physics moves at the save
struct PhysicsPart
{
	/// Three rows of the turn, then the position
	std::array<float, 12> matrix {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
	Point velocity {};
	Point spin {};

	bool operator==(const PhysicsPart&) const = default;
};

/// What an object adds
struct ObjectPart
{
	/// The number of the object's kind in the game's tables of kinds
	uint32_t info {0};
	MapCoords coords {};
	uint32_t creationIndex {0};
	float life {1.0f};
	float scale {1.0f};
	float heading {0.0f};
	ObjectNumber fire {0};
	/// Present only when the position flags say that the physics moves the object
	std::optional<PhysicsPart> physics;

	bool operator==(const ObjectPart&) const = default;
};

/// What a fixed object adds: the town artifact it is
struct FixedPart
{
	ObjectNumber townArtifact {0};

	bool operator==(const FixedPart&) const = default;
};

/// What a fixed object of several map cells adds
struct MultiMapFixedPart
{
	uint8_t fixedFlags {0};
	float percentBuilt {0.0f};
	ObjectNumber footpathLink {0};
	ObjectNumber buildingSite {0};

	bool operator==(const MultiMapFixedPart&) const = default;
};

/// What every moving thing adds
struct MobilePart
{
	uint16_t mobileWord {0};

	bool operator==(const MobilePart&) const = default;
};

/// What a moving object adds, after the moving thing's part
struct MobileObjectPart
{
	/// The object it carries or follows
	ObjectNumber object {0};
	float xAngle {0.0f};
	float zAngle {0.0f};
	ObjectNumber path {0};

	bool operator==(const MobileObjectPart&) const = default;
};

/// What a static object that can move adds, after the parts of a fixed object of several map cells
struct MobileStaticPart
{
	ObjectNumber player {0};
	/// Two words the reader keeps as they are
	std::array<uint32_t, 2> words {};

	bool operator==(const MobileStaticPart&) const = default;
};

/// Reads the record of a new object the reader meets, after its number: gives false when it cannot
using ReadNewObject = std::function<bool(StreamReader&, ObjectNumber)>;
/// Writes the record of a new object the writer meets, after its number
using WriteNewObject = std::function<void(StreamWriter&, ObjectNumber)>;

/// The values of the parts, without the part every object writes first
void Write(StreamWriter& writer, const ThingPart& thing, const PositionPart& position);
void Write(StreamWriter& writer, const ThingPart& thing, const PositionPart& position, const ObjectPart& object,
           const WriteNewObject& writeNew);
/// The object's values alone, after its position's
void WriteObject(StreamWriter& writer, const ThingPart& thing, const PositionPart& position, const ObjectPart& object,
                 const WriteNewObject& writeNew);
void Write(StreamWriter& writer, const FixedPart& fixed, const WriteNewObject& writeNew);
void Write(StreamWriter& writer, const MultiMapFixedPart& part, const WriteNewObject& writeNew);
void Write(StreamWriter& writer, const MobilePart& mobile);
void Write(StreamWriter& writer, const MobileObjectPart& part, const WriteNewObject& writeNew);
void Write(StreamWriter& writer, const MobileStaticPart& part, const WriteNewObject& writeNew);
bool Read(StreamReader& reader, PositionPart& position);
bool Read(StreamReader& reader, const ThingPart& thing, const PositionPart& position, ObjectPart& object,
          const ReadNewObject& readNew);
bool Read(StreamReader& reader, FixedPart& fixed, const ReadNewObject& readNew);
bool Read(StreamReader& reader, MultiMapFixedPart& part, const ReadNewObject& readNew);
bool Read(StreamReader& reader, MobilePart& mobile);
bool Read(StreamReader& reader, MobileObjectPart& part, const ReadNewObject& readNew);
bool Read(StreamReader& reader, MobileStaticPart& part, const ReadNewObject& readNew);

/// Writes the part every object writes first: its class's number, its own word, the running sum, destroyed and flags
void WriteThing(StreamWriter& writer, uint32_t type, const ThingPart& thing);
/// Reads the part every object writes first, after its class's number and its own word, which the reader of the
/// record reads to know its class. The checksum read is the file's.
bool ReadThing(StreamReader& reader, ThingPart& thing);

/// Writes a reference, and the record of the object when it is new to the stream
void WriteReference(StreamWriter& writer, ObjectNumber number, const WriteNewObject& writeNew);
/// Reads a reference, and the record of the object when it is new to the stream
bool ReadReference(StreamReader& reader, ObjectNumber& number, const ReadNewObject& readNew);

} // namespace openblack::gamesave
