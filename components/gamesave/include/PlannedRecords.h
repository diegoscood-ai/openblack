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

#include <tuple>

#include "TownRecords.h"

// The records of the buildings a town plans, the spell dispenser, the citadel's entrance and a one-off spell's seed
// (see docs/wiki/engine/saved-games.md#the-planned-buildings-the-players-interface-and-the-particles).
namespace openblack::gamesave
{

/// What a planned building writes after its position: its angle, its scale, whether it was built, a word, the turn it
/// was planned, its footpath link and the row of its kind
struct PlannedFields
{
	PositionPart position;
	float yAngle {0.0f};
	float scale {0.0f};
	uint32_t wasConstructed {0};
	uint32_t word {0};
	int32_t creationTurn {0};
	Ref footpathLink;
	uint32_t info {0};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.position, part.yAngle, part.scale, part.wasConstructed, part.word, part.creationTurn,
		                part.footpathLink, part.info);
	}
	bool operator==(const PlannedFields&) const = default;
};

/// A planned building of one class, and its town
template <uint32_t Type>
struct PlannedRecord
{
	static constexpr uint32_t k_Type = Type;
	ThingPart thing;
	PlannedFields planned;
	Ref town;

	static auto FieldsOf(auto& record) { return std::tie(record.planned, record.town); }
	bool operator==(const PlannedRecord&) const = default;
};

using PlannedCitadelHeartRecord = PlannedRecord<0x39>;
using PlannedAbodeRecord = PlannedRecord<0x61>;
using PlannedTownCentreRecord = PlannedRecord<0x62>;

/// A spell dispenser: an abode with an object and four words
struct SpellDispenserRecord
{
	static constexpr uint32_t k_Type = 0x10B;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	Ref object;
	std::array<uint32_t, 4> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.abode, record.object, record.words[0], record.words[1], record.words[2],
		                record.words[3]);
	}
	bool operator==(const SpellDispenserRecord&) const = default;
};

/// The citadel's entrance: an object's parts only
struct CitadelEntranceRecord
{
	static constexpr uint32_t k_Type = 0x110;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;

	static auto FieldsOf(auto& record) { return std::tie(record.position, record.object); }
	bool operator==(const CitadelEntranceRecord&) const = default;
};

/// The seed of a one-off spell: a moving object and three words
struct OneOffSpellSeedRecord
{
	static constexpr uint32_t k_Type = 0x25;
	ThingPart thing;
	MobileObjectFields base;
	std::array<uint32_t, 3> words {};

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.words[0], record.words[1], record.words[2]); }
	bool operator==(const OneOffSpellSeedRecord&) const = default;
};

} // namespace openblack::gamesave
