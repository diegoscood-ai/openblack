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
#include <tuple>
#include <vector>

#include "ParticleRecords.h"

// What the game object writes after its global lists, to the end of the world file: the help system, the script's
// objects, the camera's exclusions, the snow, the weather, the bookmarks, the land's balance, the hand, the audio, the
// rooms and the challenges; and the records of the classes it meets there (see
// docs/wiki/engine/saved-games.md#the-end-of-the-game-object).
namespace openblack::gamesave
{

/// The objects the script keeps, each with a word and a byte
inline constexpr size_t k_ScriptObjectCount = 511;
/// The bookmarks of the land
inline constexpr size_t k_BookmarkCount = 11;

/// One of the script's objects: the object, a word and a byte
struct ScriptObject
{
	Ref object;
	uint32_t word {0};
	uint8_t byte {0};

	static auto FieldsOf(auto& part) { return std::tie(part.object, part.word, part.byte); }
	bool operator==(const ScriptObject&) const = default;
};

/// A bookmark: a byte, a point, an object and two points
struct BookmarkPart
{
	uint8_t byte {0};
	Point point {};
	Ref object;
	std::array<Point, 2> points {};

	static auto FieldsOf(auto& part) { return std::tie(part.byte, part.point, part.object, part.points[0], part.points[1]); }
	bool operator==(const BookmarkPart&) const = default;
};

/// A challenge of the challenge room
struct ChallengePart
{
	uint16_t half {0};
	Counted<uint8_t> name;
	std::array<uint32_t, 3> words {};
	std::array<Point, 2> points {};
	std::array<uint32_t, 4> secondWords {};
	Counted<uint32_t> first;
	Counted<uint32_t> second;
	uint32_t lastWord {0};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.half, part.name, part.words[0], part.words[1], part.words[2], part.points[0], part.points[1],
		                part.secondWords[0], part.secondWords[1], part.secondWords[2], part.secondWords[3], part.first,
		                part.second, part.lastWord);
	}
	bool operator==(const ChallengePart&) const = default;
};

/// Everything the game object writes after its global lists
struct GameTail
{
	// The help system
	std::array<Counted<uint32_t>, 4> helpLists;
	std::array<uint32_t, 2> helpWords {};
	std::array<Ref, 3> helpObjects {};
	uint16_t helpHalf {0};
	uint32_t helpWord {0};
	Ref helpObject;
	std::array<uint32_t, 3> helpSecondWords {};
	Counted<uint32_t> helpValues;
	uint32_t helpThirdWord {0};
	Counted<uint32_t> helpArray;
	std::array<uint32_t, 3> helpInts {};
	uint16_t helpSecondHalf {0};
	std::array<uint32_t, 2> helpFourthWords {};
	uint16_t helpThirdHalf {0};
	std::array<uint32_t, 17> helpLastWords {};
	/// 1,024 entries of 16 bytes, with their count
	Counted<std::array<uint8_t, 16>> helpEntries;
	// The script's part; its state goes to its own file
	std::array<ScriptObject, k_ScriptObjectCount> scriptObjects {};
	std::array<uint32_t, 5> scriptWords {};
	std::array<Point, 2> scriptPoints {};
	std::array<uint32_t, 8> scriptSecondWords {};
	std::array<Ref, 2> scriptRefs {};
	std::array<uint32_t, 7> scriptLastWords {};
	Point point {};
	std::array<Ref, 2> objects {};
	// The camera's exclusions: a version, six values, the force field's points, and the exclusions' names
	uint32_t exclusionVersion {0};
	std::array<uint32_t, 6> exclusionValues {};
	Counted<Point> forceFieldPoints;
	uint32_t exclusionNameSize {0};
	std::vector<Counted<uint8_t>> exclusions;
	// The camera's help, the creature in the temple, the snow, the weather and other values
	std::array<uint32_t, 4> cameraWords {};
	Counted<uint32_t> snow;
	Counted<uint32_t> snowSecond;
	std::array<uint32_t, 2> snowWords {};
	std::array<uint8_t, 8> weather {};
	uint32_t word {0};
	uint8_t scriptRebootRequested {0};
	std::array<uint32_t, 5> words {};
	std::array<BookmarkPart, k_BookmarkCount> bookmarks {};
	std::array<uint32_t, 3> bookmarkWords {};
	std::array<uint8_t, 32> landBalance {};
	// The hand, the audio and the rooms
	uint32_t handWord {0};
	Ref handObject;
	std::array<uint32_t, 4> handWords {};
	Point handPoint {};
	std::array<Ref, 2> handObjects {};
	uint8_t handByte {0};
	RefList sounds;
	std::array<uint32_t, 2> audioWords {};
	Ref audioObject;
	uint32_t audioWord {0};
	uint32_t roomsAvailable {0};
	std::array<uint32_t, 5> buttonConfig {};
	/// What the "did you know" pages showed: five arrays of words with their counts, each with a word
	std::array<Counted<uint32_t>, 5> didYouKnow;
	std::array<uint32_t, 5> didYouKnowWords {};
	Listed<ChallengePart> challenges;
	/// The script's curse of the creature
	std::array<uint8_t, 12> creatureCurse {};

	bool operator==(const GameTail&) const = default;
};

bool ReadPart(StreamReader& reader, GameTail& part, const ReadNewObject& readNew);
void WritePart(StreamWriter& writer, const GameTail& part, const WriteNewObject& writeNew);

/// What a camera's mode adds, by its save number: none (0), the follow mode (0x46: six words, two points, an object and
/// a word), the heading's (0x48: four words and an object) or the free mode (0x65: four words)
struct CameraModePart
{
	uint32_t mode {0};
	std::array<uint32_t, 6> words {};
	std::array<Point, 2> points {};
	Ref object;
	uint32_t lastWord {0};

	bool operator==(const CameraModePart&) const = default;
};

bool ReadPart(StreamReader& reader, CameraModePart& part, const ReadNewObject& readNew);
void WritePart(StreamWriter& writer, const CameraModePart& part, const WriteNewObject& writeNew);

/// The camera
struct CameraRecord
{
	static constexpr uint32_t k_Type = 0x77;
	ThingPart thing;
	PositionPart position;
	Point point {};
	std::array<uint32_t, 4> words {};
	std::array<uint8_t, 8> eight {};
	std::array<uint8_t, 144> first {};
	std::array<uint8_t, 144> second {};
	std::array<uint8_t, 48> zoomer {};
	uint8_t byte {0};
	CameraModePart mode;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.point, record.words[0], record.words[1], record.words[2], record.words[3],
		                record.eight, record.first, record.second, record.zoomer, record.byte, record.mode);
	}
	bool operator==(const CameraRecord&) const = default;
};

/// A script's timer: its position and two words
struct ScriptTimerRecord
{
	static constexpr uint32_t k_Type = 0x7D;
	ThingPart thing;
	PositionPart position;
	std::array<uint32_t, 2> words {};

	static auto FieldsOf(auto& record) { return std::tie(record.position, record.words[0], record.words[1]); }
	bool operator==(const ScriptTimerRecord&) const = default;
};

/// An object of no other class: an object's parts only
struct BaseOnlyRecord
{
	static constexpr uint32_t k_Type = 0xF6;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;

	static auto FieldsOf(auto& record) { return std::tie(record.position, record.object); }
	bool operator==(const BaseOnlyRecord&) const = default;
};

/// The music of a thing: a word, the thing, four words and a point
struct ThingMusicInfoRecord
{
	static constexpr uint32_t k_Type = 0x109;
	ThingPart thing;
	uint32_t word {0};
	Ref object;
	std::array<uint32_t, 4> words {};
	Point point {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.word, record.object, record.words[0], record.words[1], record.words[2], record.words[3],
		                record.point);
	}
	bool operator==(const ThingMusicInfoRecord&) const = default;
};

/// The data of a willow wisp's collection
struct WillowWispCollectionRecord
{
	static constexpr uint32_t k_Type = 0xEE;
	ThingPart thing;
	PersistentId persistent;
	Point point {};
	std::array<uint32_t, 2> words {};
	uint8_t byte {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.persistent, record.point, record.words[0], record.words[1], record.byte);
	}
	bool operator==(const WillowWispCollectionRecord&) const = default;
};

/// The data of an atom that fades out once its condition holds
struct FadeOutAtomDataRecord
{
	static constexpr uint32_t k_Type = 0xDA;
	ThingPart thing;
	PersistentId persistent;
	uint8_t byte {0};
	std::array<uint32_t, 3> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.persistent, record.byte, record.words[0], record.words[1], record.words[2]);
	}
	bool operator==(const FadeOutAtomDataRecord&) const = default;
};

} // namespace openblack::gamesave
