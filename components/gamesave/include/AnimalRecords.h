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

#include "VillagerRecords.h"

// The records of the animals and the birds, and of a town's desire flags. Every animal, bird and puzzle animal writes
// the same values: a living thing's part, then the animal's own; the birds and the wolf a spell makes add their
// spell's values. No animal state writes anything of its own. See
// docs/wiki/engine/saved-games.md#the-animals-and-the-towns-desire-flags.
namespace openblack::gamesave
{

/// What an animal adds after a living thing's part
struct AnimalFields
{
	Ref object;
	int16_t hunger {0};
	int16_t tiredness {0};
	int16_t breedingUrge {0};
	int16_t half {0};
	/// Where its food is, two words written as one value
	std::array<int32_t, 2> foodPosition {};
	Ref secondObject;
	/// Where its physical object, its sleep and its surroundings are, each one value of 4 bytes
	uint32_t physicalObjectPosition {0};
	uint32_t sleepPosition {0};
	uint32_t environmentPosition {0};
	uint8_t deathReason {0};
	/// The zoomer of its angle, one value of 48 bytes
	std::array<uint8_t, 48> angleZoomer {};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.object, part.hunger, part.tiredness, part.breedingUrge, part.half, part.foodPosition,
		                part.secondObject, part.physicalObjectPosition, part.sleepPosition, part.environmentPosition,
		                part.deathReason, part.angleZoomer);
	}
	bool operator==(const AnimalFields&) const = default;
};

/// An animal or a bird of one class: the moving thing's parts, a living thing's part and the animal's own values
template <uint32_t Type>
struct AnimalRecord
{
	static constexpr uint32_t k_Type = Type;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	MobilePart mobile;
	WallHugPart move;
	LivingFields living;
	AnimalFields animal;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.mobile, record.move, record.living, record.animal);
	}
	bool operator==(const AnimalRecord&) const = default;
};

using PuzzleSheepRecord = AnimalRecord<0x2D>;
using PuzzleLionRecord = AnimalRecord<0x2E>;
using PuzzleVillagerRecord = AnimalRecord<0x2F>;
using CowRecord = AnimalRecord<0x82>;
using SheepRecord = AnimalRecord<0x83>;
using GoatRecord = AnimalRecord<0x84>;
using HorseRecord = AnimalRecord<0x85>;
using ZebraRecord = AnimalRecord<0x86>;
using PigRecord = AnimalRecord<0x87>;
using TortoiseRecord = AnimalRecord<0x88>;
using LionRecord = AnimalRecord<0x89>;
using LeopardRecord = AnimalRecord<0x8A>;
using TigerRecord = AnimalRecord<0x8B>;
using WolfRecord = AnimalRecord<0x8C>;
using DoveRecord = AnimalRecord<0x8E>;
using CrowRecord = AnimalRecord<0x8F>;
using SwallowRecord = AnimalRecord<0x91>;
using PigeonRecord = AnimalRecord<0x92>;
using SeagullRecord = AnimalRecord<0x93>;
using BatRecord = AnimalRecord<0x94>;
using VultureRecord = AnimalRecord<0x96>;
using PieceLionRecord = AnimalRecord<0x9A>;
using PieceSheepRecord = AnimalRecord<0x9B>;
using PieceWolfRecord = AnimalRecord<0x9C>;
using PieceVillagerRecord = AnimalRecord<0x9D>;

/// A bird a spell makes: a bird and 48 bytes of its spell
template <uint32_t Type>
struct SpellBirdRecord
{
	static constexpr uint32_t k_Type = Type;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	MobilePart mobile;
	WallHugPart move;
	LivingFields living;
	AnimalFields animal;
	std::array<uint8_t, 48> spell {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.mobile, record.move, record.living, record.animal, record.spell);
	}
	bool operator==(const SpellBirdRecord&) const = default;
};

using SpellDoveRecord = SpellBirdRecord<0x90>;
using SpellBatRecord = SpellBirdRecord<0x95>;

/// A wolf a spell makes: an animal, a point, 16 bytes, a word, 48 bytes and an object
struct SpellWolfRecord
{
	static constexpr uint32_t k_Type = 0x8D;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	MobilePart mobile;
	WallHugPart move;
	LivingFields living;
	AnimalFields animal;
	Point point {};
	std::array<uint8_t, 16> sixteenBytes {};
	uint32_t word {0};
	std::array<uint8_t, 48> values {};
	Ref other;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.mobile, record.move, record.living, record.animal, record.point,
		                record.sixteenBytes, record.word, record.values, record.other);
	}
	bool operator==(const SpellWolfRecord&) const = default;
};

/// A town's desire flags: an object's parts, its town, its slot, its desire, the desire's kind and its mesh
struct TownDesireFlagsRecord
{
	static constexpr uint32_t k_Type = 0x4C;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	Ref town;
	int32_t slot {0};
	float desire {0.0f};
	uint32_t desireType {0};
	uint32_t mesh {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.town, record.slot, record.desire, record.desireType,
		                record.mesh);
	}
	bool operator==(const TownDesireFlagsRecord&) const = default;
};

} // namespace openblack::gamesave
