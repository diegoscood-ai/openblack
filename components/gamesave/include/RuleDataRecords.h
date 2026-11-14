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
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ParticleRecords.h"
#include "SaveParts.h"

// The data the particles' rules keep on their atoms and collections, for the classes whose values are a fixed row of
// bytes, words, points, matrices and references: one record for all of them, laid out by its class's row of values
// (see docs/wiki/engine/saved-games.md#the-particles-rules-data). The classes with lists of their own have their own
// records.
namespace openblack::gamesave
{

/// A value of a rule's data as the game writes it: one byte, one word, a point (three floats), a matrix (twelve floats:
/// the three rows of a turn, then a translation) or a reference to an object
enum class RuleField : uint8_t
{
	Byte,
	Word,
	Point,
	Matrix,
	Ref,
};

/// The bytes a value of the kind takes in the record's values; a reference is kept apart, as an object's number
[[nodiscard]] constexpr size_t SizeOf(RuleField field)
{
	switch (field)
	{
	case RuleField::Byte:
		return 1;
	case RuleField::Word:
		return 4;
	case RuleField::Point:
		return 12;
	case RuleField::Matrix:
		return 48;
	case RuleField::Ref:
		break;
	}
	return 0;
}

/// One class of rule data: its number, its name, and its values after the rule it belongs to, in the game's order
struct RuleDataLayout
{
	uint32_t type {0};
	std::string_view name;
	std::span<const RuleField> fields;
};

/// The layout of the class, if it is one of the rule data classes this record reads
[[nodiscard]] const RuleDataLayout* RuleDataLayoutOf(uint32_t type);

/// Every class this record reads, by their numbers
[[nodiscard]] std::span<const RuleDataLayout> RuleDataLayouts();

/// The number the record variant gives this record, which is none of the game's: the record's own class is its `type`
inline constexpr uint32_t k_RuleDataRecordType = 0xFFFFFFFFU;

/// A rule's data: its class, the part every object writes, the rule it belongs to (by its file and its place there),
/// then its class's values: those that are not references as their bytes in order, and its references in order
struct RuleDataRecord
{
	static constexpr uint32_t k_Type = k_RuleDataRecordType;
	uint32_t type {0};
	ThingPart thing;
	PersistentId persistent;
	std::vector<uint8_t> values;
	std::vector<ObjectNumber> references;

	/// A record of the class with each value 0 and each reference none; none for a class with no layout
	[[nodiscard]] static std::optional<RuleDataRecord> Of(uint32_t type);

	/// The value of the class's row at the place, of that kind; none for a place of another kind or past the row
	[[nodiscard]] std::optional<uint8_t> Byte(size_t field) const;
	[[nodiscard]] std::optional<uint32_t> Word(size_t field) const;
	[[nodiscard]] std::optional<float> Float(size_t field) const;
	[[nodiscard]] std::optional<Point> PointAt(size_t field) const;
	[[nodiscard]] std::optional<std::array<float, 12>> Matrix(size_t field) const;
	[[nodiscard]] std::optional<ObjectNumber> Reference(size_t field) const;

	/// Sets the value at the place; false, with nothing changed, for a place of another kind or past the row
	bool SetByte(size_t field, uint8_t value);
	bool SetWord(size_t field, uint32_t value);
	bool SetFloat(size_t field, float value);
	bool SetPoint(size_t field, const Point& value);
	bool SetMatrix(size_t field, const std::array<float, 12>& value);
	bool SetReference(size_t field, ObjectNumber value);

	bool operator==(const RuleDataRecord&) const = default;
};

/// Reads a rule's data after its class's number and own word, by its class's layout
bool ReadRuleData(StreamReader& reader, RuleDataRecord& record, const ReadNewObject& readNew);

/// Writes a rule's data from its class's number, by its class's layout
void WriteRuleData(StreamWriter& writer, const RuleDataRecord& record, const WriteNewObject& writeNew);

} // namespace openblack::gamesave
