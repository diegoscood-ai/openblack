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

#include "SaveFields.h"

// The records of a fire, a rock and a spell's seed (see docs/wiki/engine/saved-games.md#the-creature).
namespace openblack::gamesave
{

/// A fire: its values, the objects it burns and is near, and the objects of its list, with their count. Two bytes
/// that the game writes from its own data, the same in every fire, come after its first values and at its end.
struct FireEffectRecord
{
	static constexpr uint32_t k_Type = 0x29;
	ThingPart thing;
	uint32_t word {0};
	uint8_t firstMark {0};
	uint8_t byte {0};
	uint8_t secondMark {0};
	std::array<uint32_t, 2> words {};
	std::array<Ref, 4> objects {};
	uint8_t secondByte {0};
	std::array<Ref, 2> others {};
	RefList burning;
	uint8_t lastFirstMark {0};
	uint8_t lastSecondMark {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.word, record.firstMark, record.byte, record.secondMark, record.words[0], record.words[1],
		                record.objects[0], record.objects[1], record.objects[2], record.objects[3], record.secondByte,
		                record.others[0], record.others[1], record.burning, record.lastFirstMark, record.lastSecondMark);
	}
	bool operator==(const FireEffectRecord&) const = default;
};

/// A rock: a static object that can move, four words and an object
struct RockRecord
{
	static constexpr uint32_t k_Type = 0x6F;
	ThingPart thing;
	MultiMapFixedFields base;
	MobileStaticPart mobileStatic;
	std::array<uint32_t, 2> words {};
	Ref object;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.mobileStatic, record.words[0], record.words[1], record.object);
	}
	bool operator==(const RockRecord&) const = default;
};

/// A spell's seed: an object's parts, a word, a byte, the row of its kind, four objects, two bytes, seven words, a
/// flag, two words and a flag
struct SpellSeedRecord
{
	static constexpr uint32_t k_Type = 0x24;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	uint32_t word {0};
	uint8_t byte {0};
	uint32_t info {0};
	std::array<Ref, 4> objects {};
	std::array<uint8_t, 2> bytes {};
	std::array<uint32_t, 7> words {};
	uint8_t flag {0};
	std::array<uint32_t, 2> lastWords {};
	uint8_t lastFlag {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.word, record.byte, record.info, record.objects[0],
		                record.objects[1], record.objects[2], record.objects[3], record.bytes[0], record.bytes[1],
		                record.words[0], record.words[1], record.words[2], record.words[3], record.words[4], record.words[5],
		                record.words[6], record.flag, record.lastWords[0], record.lastWords[1], record.lastFlag);
	}
	bool operator==(const SpellSeedRecord&) const = default;
};

} // namespace openblack::gamesave
