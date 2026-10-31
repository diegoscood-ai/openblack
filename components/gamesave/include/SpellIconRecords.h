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

// The records of the spell icons of the towns and the worship sites, the wonder, a script's highlight, the magic food,
// a workshop's building site and the dead and felled trees (see
// docs/wiki/engine/saved-games.md#the-particles-the-spell-icons-and-the-destruction-meshes).
namespace openblack::gamesave
{

/// What every spell icon writes: a building of several cells, the row of its kind, a point and a word
struct SpellIconFields
{
	MultiMapFixedFields base;
	uint32_t info {0};
	Point point {};
	uint32_t word {0};

	static auto FieldsOf(auto& part) { return std::tie(part.base, part.info, part.point, part.word); }
	bool operator==(const SpellIconFields&) const = default;
};

/// A town's spell icon, and its town
struct TownSpellIconRecord
{
	static constexpr uint32_t k_Type = 0x55;
	ThingPart thing;
	SpellIconFields icon;
	Ref town;

	static auto FieldsOf(auto& record) { return std::tie(record.icon, record.town); }
	bool operator==(const TownSpellIconRecord&) const = default;
};

/// A town centre's spell icon: a town's icon, an object and three words with their count
struct TownCentreSpellIconRecord
{
	static constexpr uint32_t k_Type = 0x56;
	ThingPart thing;
	SpellIconFields icon;
	Ref town;
	Ref object;
	Counted<uint32_t> words;

	static auto FieldsOf(auto& record) { return std::tie(record.icon, record.town, record.object, record.words); }
	bool operator==(const TownCentreSpellIconRecord&) const = default;
};

/// A worship site's spell icon
struct WorshipSpellIconRecord
{
	static constexpr uint32_t k_Type = 0x78;
	ThingPart thing;
	SpellIconFields icon;
	uint16_t half {0};
	Ref object;
	std::array<uint32_t, 2> words {};
	uint16_t secondHalf {0};
	uint8_t byte {0};
	uint32_t thirdWord {0};
	Ref other;
	uint32_t fourthWord {0};
	/// Its objects, with their count
	RefList objects;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.icon, record.half, record.object, record.words[0], record.words[1], record.secondHalf,
		                record.byte, record.thirdWord, record.other, record.fourthWord, record.objects);
	}
	bool operator==(const WorshipSpellIconRecord&) const = default;
};

/// A wonder: an abode and a word
struct WonderRecord
{
	static constexpr uint32_t k_Type = 0x80;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	uint32_t word {0};

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode, record.word); }
	bool operator==(const WonderRecord&) const = default;
};

/// A script's highlight: a fixed object and six words
struct ScriptHighlightRecord
{
	static constexpr uint32_t k_Type = 0x3F;
	ThingPart thing;
	FixedFields base;
	std::array<uint32_t, 6> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.words[0], record.words[1], record.words[2], record.words[3], record.words[4],
		                record.words[5]);
	}
	bool operator==(const ScriptHighlightRecord&) const = default;
};

/// Magic food: a food pile with its store, and its player
struct MagicFoodRecord
{
	static constexpr uint32_t k_Type = 0x1E;
	ThingPart thing;
	MobileObjectFields base;
	PotFields pile;
	Ref store;
	Ref player;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.pile, record.store, record.player); }
	bool operator==(const MagicFoodRecord&) const = default;
};

/// A workshop's building site: a building site without the standard one's wood pile
struct WorkshopBuildingSiteRecord
{
	static constexpr uint32_t k_Type = 0x5E;
	ThingPart thing;
	Ref rootBuilding;
	RefList workers;
	RefList scaffolds;
	RefList third;
	uint32_t word {0};
	std::array<uint8_t, 0x600> positions {};
	uint32_t secondWord {0};
	uint32_t thirdWord {0};
	float forcedDesire {0.0f};
	float life {0.0f};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.rootBuilding, record.workers, record.scaffolds, record.third, record.word, record.positions,
		                record.secondWord, record.thirdWord, record.forcedDesire, record.life);
	}
	bool operator==(const WorkshopBuildingSiteRecord&) const = default;
};

/// A dead or felled tree of one class: a rock's record, the row of its kind and a word
template <uint32_t Type>
struct DeadTreeRecordOf
{
	static constexpr uint32_t k_Type = Type;
	ThingPart thing;
	MultiMapFixedFields base;
	MobileStaticPart mobileStatic;
	std::array<uint32_t, 2> words {};
	Ref object;
	uint32_t info {0};
	uint32_t word {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.mobileStatic, record.words[0], record.words[1], record.object, record.info,
		                record.word);
	}
	bool operator==(const DeadTreeRecordOf&) const = default;
};

using DeadTreeRecord = DeadTreeRecordOf<0x70>;
using FelledTreeRecord = DeadTreeRecordOf<0x71>;

} // namespace openblack::gamesave
