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
#include <tuple>

#include "TownRecords.h"

// The records a save meets next through a town and its villagers: the town centre, a script's reminder of a living
// thing, a static object that can move, a fish farm, a living thing's path, a standard building site and a flock (see
// docs/wiki/engine/saved-games.md#the-town-centre-the-flocks-and-the-building-sites); then the totem statue, the magic
// wood, the workshop, the scaffold and the reaction (see docs/wiki/engine/saved-games.md#the-workshops-and-the-reactions).
namespace openblack::gamesave
{

/// A town's centre: an abode with its totem statue and its six spell icons
struct TownCentreRecord
{
	static constexpr uint32_t k_Type = 0x60;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	Ref totemStatue;
	RefList icons;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode, record.totemStatue, record.icons); }
	bool operator==(const TownCentreRecord&) const = default;
};

/// What a script asked a living thing to remember: an object, values the reader keeps as they are, another object
struct ScriptReminderRecord
{
	static constexpr uint32_t k_Type = 0x105;
	ThingPart thing;
	Ref object;
	std::array<uint8_t, 12> firstValue {};
	uint32_t secondValue {0};
	std::array<uint8_t, 8> thirdValue {};
	std::array<uint32_t, 5> words {};
	Ref other;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.object, record.firstValue, record.secondValue, record.thirdValue, record.words[0],
		                record.words[1], record.words[2], record.words[3], record.words[4], record.other);
	}
	bool operator==(const ScriptReminderRecord&) const = default;
};

/// A static object that can move: a building of several cells with its player and two words
struct MobileStaticRecord
{
	static constexpr uint32_t k_Type = 0x54;
	ThingPart thing;
	MultiMapFixedFields base;
	MobileStaticPart mobileStatic;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.mobileStatic); }
	bool operator==(const MobileStaticRecord&) const = default;
};

/// A fish farm: its villagers, with a raw count, its town and two floats
struct FishFarmRecord
{
	static constexpr uint32_t k_Type = 0x5B;
	ThingPart thing;
	MultiMapFixedFields base;
	RefList villagers;
	Ref town;
	float first {0.0f};
	float second {0.0f};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.villagers, record.town, record.first, record.second);
	}
	bool operator==(const FishFarmRecord&) const = default;
};

/// The path a living thing follows on a track
struct DataPathRecord
{
	static constexpr uint32_t k_Type = 0x99;
	ThingPart thing;
	int32_t trackNumber {0};
	float endPercentage {0.0f};
	uint32_t reverse {0};
	float frame {0.0f};
	float framesPerTurn {0.0f};
	float last {0.0f};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.trackNumber, record.endPercentage, record.reverse, record.frame, record.framesPerTurn,
		                record.last);
	}
	bool operator==(const DataPathRecord&) const = default;
};

/// A standard building site: its building, its workers and scaffolds, its building positions and its wood pile
struct StandardBuildingSiteRecord
{
	static constexpr uint32_t k_Type = 0x5C;
	ThingPart thing;
	Ref rootBuilding;
	/// Three lists, the first with its count as a raw word
	RefList workers;
	RefList scaffolds;
	RefList third;
	uint32_t word {0};
	/// The positions to build at, one raw value of 1,536 bytes
	std::array<uint8_t, 0x600> positions {};
	uint32_t secondWord {0};
	uint32_t thirdWord {0};
	float forcedDesire {0.0f};
	float life {0.0f};
	Ref woodPile;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.rootBuilding, record.workers, record.scaffolds, record.third, record.word, record.positions,
		                record.secondWord, record.thirdWord, record.forcedDesire, record.life, record.woodPile);
	}
	bool operator==(const StandardBuildingSiteRecord&) const = default;
};

/// A flock of animals or birds
struct FlockRecord
{
	static constexpr uint32_t k_Type = 0x7E;
	ThingPart thing;
	ContainerFields container;
	/// Three words the game works out as it writes them
	std::array<uint32_t, 3> computed {};
	Ref shepherd;
	Ref town;
	Ref citadelHeart;
	/// Its members, with their count as a raw word
	RefList members;
	uint32_t maxMembers {0};
	uint32_t word {0};
	uint16_t domainRadius {0};
	uint16_t flockDistance {0};
	float flyingHeight {0.0f};
	uint32_t followShepherd {0};
	uint32_t secondWord {0};
	MapCoords savedDomainCentre {};
	MapCoords otherCentre {};
	std::array<uint32_t, 3> words {};
	Ref savedThing;
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.container, record.computed[0], record.computed[1], record.computed[2], record.shepherd,
		                record.town, record.citadelHeart, record.members, record.maxMembers, record.word, record.domainRadius,
		                record.flockDistance, record.flyingHeight, record.followShepherd, record.secondWord,
		                record.savedDomainCentre, record.otherCentre, record.words[0], record.words[1], record.words[2],
		                record.savedThing, record.lastWord);
	}
	bool operator==(const FlockRecord&) const = default;
};

/// A town centre's totem statue: a building of several cells with an object and three words
struct TotemStatueRecord
{
	static constexpr uint32_t k_Type = 0x7F;
	ThingPart thing;
	MultiMapFixedFields base;
	Ref object;
	std::array<uint32_t, 3> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.object, record.words[0], record.words[1], record.words[2]);
	}
	bool operator==(const TotemStatueRecord&) const = default;
};

/// A pile of magic wood: a pile of wood and its player
struct MagicWoodRecord
{
	static constexpr uint32_t k_Type = 0x1F;
	ThingPart thing;
	MobileObjectFields base;
	PotFields pile;
	Ref player;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.pile, record.player); }
	bool operator==(const MagicWoodRecord&) const = default;
};

/// A workshop: an abode with two objects, two words, a counted list of bytes and a list with a raw count
struct WorkshopRecord
{
	static constexpr uint32_t k_Type = 0x52;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	Ref first;
	Ref second;
	uint32_t firstWord {0};
	uint32_t secondWord {0};
	Counted<uint8_t> bytes;
	RefList objects;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.abode, record.first, record.second, record.firstWord, record.secondWord,
		                record.bytes, record.objects);
	}
	bool operator==(const WorkshopRecord&) const = default;
};

/// A building site's scaffold: a moving object with its building's objects and values
struct ScaffoldRecord
{
	static constexpr uint32_t k_Type = 0x5F;
	ThingPart thing;
	MobileObjectFields base;
	std::array<Ref, 3> objects {};
	uint32_t firstWord {0};
	Ref other;
	uint32_t secondWord {0};
	uint32_t thirdWord {0};
	uint16_t half {0};
	Ref last;
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.objects[0], record.objects[1], record.objects[2], record.firstWord, record.other,
		                record.secondWord, record.thirdWord, record.half, record.last, record.lastWord);
	}
	bool operator==(const ScaffoldRecord&) const = default;
};

/// A reaction of living things to an object: the object, the living things that react (a stack, its count then each
/// from the top), and its values
struct ReactionRecord
{
	static constexpr uint32_t k_Type = 0x26;
	ThingPart thing;
	Ref object;
	RefList livings;
	uint32_t firstWord {0};
	uint32_t secondWord {0};
	Ref second;
	uint32_t thirdWord {0};
	uint32_t fourthWord {0};
	Ref third;
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.object, record.livings, record.firstWord, record.secondWord, record.second, record.thirdWord,
		                record.fourthWord, record.third, record.lastWord);
	}
	bool operator==(const ReactionRecord&) const = default;
};

} // namespace openblack::gamesave
