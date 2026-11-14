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

// The records of a town's buildings that its lists pull in after its forests: the creche, the graveyard, the abodes
// and the fields, each an abode's parts with what its class adds. See
// docs/wiki/engine/saved-games.md#the-towns-buildings.
namespace openblack::gamesave
{

/// An abode
struct AbodeRecord
{
	static constexpr uint32_t k_Type = 0x7;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode); }
	bool operator==(const AbodeRecord&) const = default;
};

/// A town's creche, which writes an abode's record
struct CrecheRecord
{
	static constexpr uint32_t k_Type = 0x50;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode); }
	bool operator==(const CrecheRecord&) const = default;
};

/// A town's graveyard: an abode and its graves
struct GraveyardRecord
{
	static constexpr uint32_t k_Type = 0x51;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	uint32_t graves {0};

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode, record.graves); }
	bool operator==(const GraveyardRecord&) const = default;
};

/// A field: an abode with its crop's kind, its town and its farmers
struct FieldRecord
{
	static constexpr uint32_t k_Type = 0x32;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	/// The number of the field's kind in the game's tables of kinds
	uint32_t typeInfo {0};
	uint32_t firstWord {0};
	uint32_t secondWord {0};
	Ref town;
	int32_t thirdWord {0};
	uint8_t firstByte {0};
	uint32_t fourthWord {0};
	RefList farmers;
	/// The low byte of a word the field keeps: the game writes one of its four bytes
	uint8_t lastByte {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.abode, record.typeInfo, record.firstWord, record.secondWord, record.town,
		                record.thirdWord, record.firstByte, record.fourthWord, record.farmers, record.lastByte);
	}
	bool operator==(const FieldRecord&) const = default;
};

} // namespace openblack::gamesave
