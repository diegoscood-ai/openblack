/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RuleDataRecords.h"

#include <cstring>

#include <algorithm>

using namespace openblack::gamesave;

namespace
{
constexpr RuleField k_Byte = RuleField::Byte;
constexpr RuleField k_Word = RuleField::Word;
constexpr RuleField k_Point = RuleField::Point;
constexpr RuleField k_Matrix = RuleField::Matrix;
constexpr RuleField k_Ref = RuleField::Ref;

// Each class's values after the rule it belongs to, in the order the game writes them (raffclar's layouts, each checked
// against the exe's save function)
constexpr std::array k_VolumeEffectOnObject {k_Word, k_Ref};
constexpr std::array k_Plasma {k_Point, k_Point, k_Point, k_Point, k_Word, k_Word, k_Byte};
constexpr std::array k_BeliefSprite {k_Point, k_Byte};
// the end, the start and the way across the ground; the curve's terms from the cube's to the constant's; the path's
// length across the ground, the weave's size, its frequency and where along the noise it starts; how far along it is
constexpr std::array k_ManaPath {k_Point, k_Point, k_Point, k_Point, k_Point, k_Point,
                                 k_Point, k_Word,  k_Word,  k_Word,  k_Word,  k_Word};
constexpr std::array k_CreatureSpellGenericCollection {k_Byte, k_Point, k_Point, k_Byte, k_Word, k_Word};
constexpr std::array k_CreatureSpellGenericAtom {k_Word, k_Word, k_Word, k_Word, k_Word};
constexpr std::array k_OrientSpriteWithVelocity {k_Byte, k_Point, k_Word, k_Byte};
constexpr std::array k_CreatureSpellCompassion {k_Byte, k_Word, k_Word};
constexpr std::array k_CreatureSpell {k_Byte, k_Word, k_Ref};
constexpr std::array k_OneByte {k_Byte};
constexpr std::array k_CreatureSpellFreeze {k_Byte, k_Byte, k_Word, k_Word};
constexpr std::array k_ByteThreeWords {k_Byte, k_Word, k_Word, k_Word};
constexpr std::array k_BytePoint {k_Byte, k_Point};
constexpr std::array k_CloudGatherCollection {k_Point, k_Word, k_Word, k_Byte, k_Word, k_Word, k_Word,
                                              k_Word,  k_Ref,  k_Ref,  k_Byte, k_Byte, k_Byte, k_Byte};
constexpr std::array k_CloudGatherAtom {k_Word, k_Word, k_Word, k_Byte, k_Word, k_Word};
constexpr std::array k_ThreeWords {k_Word, k_Word, k_Word};
constexpr std::array k_TwoWords {k_Word, k_Word};
constexpr std::array k_Tornado {k_Byte, k_Point, k_Point, k_Point, k_Point, k_Point, k_Word, k_Word, k_Byte, k_Word, k_Byte};
constexpr std::array k_LightningSprite {k_Word, k_Byte, k_Word, k_Word, k_Byte};
constexpr std::array k_ByteWordRef {k_Byte, k_Word, k_Ref};
constexpr std::array k_OnePoint {k_Point};
constexpr std::array k_ShieldSpark {k_Word, k_Word, k_Word, k_Word, k_Matrix};
constexpr std::array k_OneWord {k_Word};
constexpr std::array k_GlintsParent {k_Byte, k_Word, k_Word, k_Ref};
constexpr std::array k_ByteWord {k_Byte, k_Word};
constexpr std::array k_WordByte {k_Word, k_Byte};
constexpr std::array k_OrientWithVelocity {k_Point, k_Word, k_Byte};
constexpr std::array k_WordRef {k_Word, k_Ref};
constexpr std::array k_ByteRef {k_Byte, k_Ref};
constexpr std::array k_ByteTwoWords {k_Byte, k_Word, k_Word};
constexpr std::array k_HealChakra {k_Word, k_Ref, k_Byte, k_Byte, k_Byte, k_Word, k_Word};

/// The classes by their numbers. The tornado's flying collections write themselves as its debris collections (0xD5),
/// so no save holds 0xD6
constexpr std::array k_Layouts {
    RuleDataLayout {.type = 0xBB, .name = "volume effect on an object's atom data", .fields = k_VolumeEffectOnObject},
    RuleDataLayout {.type = 0xBD, .name = "plasma atom data", .fields = k_Plasma},
    RuleDataLayout {.type = 0xBE, .name = "belief sprite atom data", .fields = k_BeliefSprite},
    RuleDataLayout {.type = 0xC0, .name = "mana path atom data", .fields = k_ManaPath},
    RuleDataLayout {.type = 0xC1, .name = "creature spell's collection data", .fields = k_CreatureSpellGenericCollection},
    RuleDataLayout {.type = 0xC2, .name = "creature spell's atom data", .fields = k_CreatureSpellGenericAtom},
    RuleDataLayout {.type = 0xC3, .name = "sprite turned with its velocity's atom data", .fields = k_OrientSpriteWithVelocity},
    RuleDataLayout {.type = 0xC5, .name = "creature compassion spell's collection data", .fields = k_CreatureSpellCompassion},
    RuleDataLayout {.type = 0xC6, .name = "creature spell collection data", .fields = k_CreatureSpell},
    RuleDataLayout {.type = 0xC7, .name = "creature itch spell's collection data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xC8, .name = "creature freeze spell's collection data", .fields = k_CreatureSpellFreeze},
    RuleDataLayout {.type = 0xCB, .name = "sphere surface tracer atom data", .fields = k_ByteThreeWords},
    RuleDataLayout {.type = 0xCC, .name = "shield deflections atom data", .fields = k_BytePoint},
    RuleDataLayout {.type = 0xCD, .name = "forest path atom data", .fields = k_ByteThreeWords},
    RuleDataLayout {.type = 0xCE, .name = "explosion atom data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xD0, .name = "flocking collection data", .fields = k_BytePoint},
    RuleDataLayout {.type = 0xD1, .name = "cloud mover collection data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xD2, .name = "cloud gather collection data", .fields = k_CloudGatherCollection},
    RuleDataLayout {.type = 0xD3, .name = "cloud gather atom data", .fields = k_CloudGatherAtom},
    RuleDataLayout {.type = 0xD4, .name = "tornado's flying atom data", .fields = k_ThreeWords},
    RuleDataLayout {.type = 0xD5, .name = "tornado's debris collection data", .fields = k_TwoWords},
    RuleDataLayout {.type = 0xD7, .name = "tornado collection data", .fields = k_Tornado},
    RuleDataLayout {.type = 0xD9, .name = "lightning sprite emitter's collection data", .fields = k_LightningSprite},
    RuleDataLayout {.type = 0xDB, .name = "fireball on an atom's atom data", .fields = k_ByteWordRef},
    RuleDataLayout {.type = 0xDD, .name = "sound removal's atom data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xDE, .name = "ripple gravity with a floor's atom data", .fields = k_OnePoint},
    RuleDataLayout {.type = 0xDF, .name = "banked turning atom data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xE0, .name = "shield spark atom data", .fields = k_ShieldSpark},
    RuleDataLayout {.type = 0xE1, .name = "vapour end effect atom data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xE2, .name = "sub-collections added to an atom's atom data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xE3, .name = "new base atom's atom data", .fields = k_OneWord},
    RuleDataLayout {.type = 0xE4, .name = "glints on a target's parent atom data", .fields = k_GlintsParent},
    RuleDataLayout {.type = 0xE5, .name = "glints on a target's atom data", .fields = k_OneWord},
    RuleDataLayout {.type = 0xE6, .name = "sprite turned at a random angle's atom data", .fields = k_ByteWord},
    RuleDataLayout {.type = 0xE7, .name = "side spin atom data", .fields = k_WordByte},
    RuleDataLayout {.type = 0xE8, .name = "initial spin atom data", .fields = k_WordByte},
    RuleDataLayout {.type = 0xE9, .name = "rule removal after a condition's atom data", .fields = k_ByteWord},
    RuleDataLayout {.type = 0xEA, .name = "turned with its velocity's atom data", .fields = k_OrientWithVelocity},
    RuleDataLayout {.type = 0xEB, .name = "follow targets atom data", .fields = k_WordRef},
    RuleDataLayout {.type = 0xEC, .name = "defensive sphere collection data", .fields = k_ByteRef},
    RuleDataLayout {.type = 0xEF, .name = "burst from a parent atom's collection data", .fields = k_ByteTwoWords},
    RuleDataLayout {.type = 0xF0, .name = "emitter from a parent atom's collection data", .fields = k_ByteTwoWords},
    RuleDataLayout {.type = 0xF1, .name = "follow targets collection data", .fields = k_OneByte},
    RuleDataLayout {.type = 0xF2, .name = "healing chakra atom data", .fields = k_HealChakra},
    RuleDataLayout {.type = 0xF4, .name = "lightning fork flicker collection data", .fields = k_OneWord},
    RuleDataLayout {.type = 0x10E, .name = "light sheet on an object's atom data", .fields = k_OneWord},
};

static_assert(std::ranges::is_sorted(k_Layouts, {}, &RuleDataLayout::type));

/// Where the value at the place is: its offset in the values, or its index among the references
std::optional<size_t> PlaceOf(uint32_t type, size_t field, RuleField kind)
{
	const auto* layout = RuleDataLayoutOf(type);
	if (layout == nullptr || field >= layout->fields.size() || layout->fields[field] != kind)
	{
		return std::nullopt;
	}
	const auto before = layout->fields.first(field);
	if (kind == RuleField::Ref)
	{
		return static_cast<size_t>(std::ranges::count(before, RuleField::Ref));
	}
	size_t offset = 0;
	for (const auto value : before)
	{
		offset += SizeOf(value);
	}
	return offset;
}

template <typename T>
std::optional<T> ValueAt(const RuleDataRecord& record, size_t field, RuleField kind)
{
	const auto offset = PlaceOf(record.type, field, kind);
	if (!offset.has_value() || *offset + sizeof(T) > record.values.size())
	{
		return std::nullopt;
	}
	T value {};
	std::memcpy(&value, record.values.data() + *offset, sizeof(T));
	return value;
}

template <typename T>
bool SetValueAt(RuleDataRecord& record, size_t field, RuleField kind, const T& value)
{
	const auto offset = PlaceOf(record.type, field, kind);
	if (!offset.has_value() || *offset + sizeof(T) > record.values.size())
	{
		return false;
	}
	std::memcpy(record.values.data() + *offset, &value, sizeof(T));
	return true;
}

/// Reads the value of the kind as one value of the game, its bytes added to the record's
template <size_t Size>
bool GetBytes(StreamReader& reader, std::vector<uint8_t>& values)
{
	std::array<uint8_t, Size> bytes {};
	if (!reader.Get(bytes))
	{
		return false;
	}
	values.insert(values.end(), bytes.begin(), bytes.end());
	return true;
}

/// Writes the record's bytes from the offset as one value of the game, and gives the offset after it
template <size_t Size>
size_t PutBytes(StreamWriter& writer, const std::vector<uint8_t>& values, size_t offset)
{
	std::array<uint8_t, Size> bytes {};
	if (offset + Size <= values.size())
	{
		std::memcpy(bytes.data(), values.data() + offset, Size);
	}
	writer.Put(bytes);
	return offset + Size;
}
} // namespace

const RuleDataLayout* openblack::gamesave::RuleDataLayoutOf(uint32_t type)
{
	const auto found = std::ranges::lower_bound(k_Layouts, type, {}, &RuleDataLayout::type);
	return found != k_Layouts.end() && found->type == type ? &*found : nullptr;
}

std::span<const RuleDataLayout> openblack::gamesave::RuleDataLayouts()
{
	return k_Layouts;
}

std::optional<RuleDataRecord> openblack::gamesave::RuleDataRecord::Of(uint32_t type)
{
	const auto* layout = RuleDataLayoutOf(type);
	if (layout == nullptr)
	{
		return std::nullopt;
	}
	RuleDataRecord record;
	record.type = type;
	for (const auto field : layout->fields)
	{
		if (field == RuleField::Ref)
		{
			record.references.push_back(0);
		}
		else
		{
			record.values.resize(record.values.size() + SizeOf(field), 0);
		}
	}
	return record;
}

std::optional<uint8_t> openblack::gamesave::RuleDataRecord::Byte(size_t field) const
{
	return ValueAt<uint8_t>(*this, field, RuleField::Byte);
}

std::optional<uint32_t> openblack::gamesave::RuleDataRecord::Word(size_t field) const
{
	return ValueAt<uint32_t>(*this, field, RuleField::Word);
}

std::optional<float> openblack::gamesave::RuleDataRecord::Float(size_t field) const
{
	return ValueAt<float>(*this, field, RuleField::Word);
}

std::optional<Point> openblack::gamesave::RuleDataRecord::PointAt(size_t field) const
{
	return ValueAt<Point>(*this, field, RuleField::Point);
}

std::optional<std::array<float, 12>> openblack::gamesave::RuleDataRecord::Matrix(size_t field) const
{
	return ValueAt<std::array<float, 12>>(*this, field, RuleField::Matrix);
}

std::optional<ObjectNumber> openblack::gamesave::RuleDataRecord::Reference(size_t field) const
{
	const auto index = PlaceOf(type, field, RuleField::Ref);
	if (!index.has_value() || *index >= references.size())
	{
		return std::nullopt;
	}
	return references[*index];
}

bool openblack::gamesave::RuleDataRecord::SetByte(size_t field, uint8_t value)
{
	return SetValueAt(*this, field, RuleField::Byte, value);
}

bool openblack::gamesave::RuleDataRecord::SetWord(size_t field, uint32_t value)
{
	return SetValueAt(*this, field, RuleField::Word, value);
}

bool openblack::gamesave::RuleDataRecord::SetFloat(size_t field, float value)
{
	return SetValueAt(*this, field, RuleField::Word, value);
}

bool openblack::gamesave::RuleDataRecord::SetPoint(size_t field, const Point& value)
{
	return SetValueAt(*this, field, RuleField::Point, value);
}

bool openblack::gamesave::RuleDataRecord::SetMatrix(size_t field, const std::array<float, 12>& value)
{
	return SetValueAt(*this, field, RuleField::Matrix, value);
}

bool openblack::gamesave::RuleDataRecord::SetReference(size_t field, ObjectNumber value)
{
	const auto index = PlaceOf(type, field, RuleField::Ref);
	if (!index.has_value() || *index >= references.size())
	{
		return false;
	}
	references[*index] = value;
	return true;
}

bool openblack::gamesave::ReadRuleData(StreamReader& reader, RuleDataRecord& record, const ReadNewObject& readNew)
{
	const auto* layout = RuleDataLayoutOf(record.type);
	if (layout == nullptr || !ReadThing(reader, record.thing))
	{
		reader.Fail();
		return false;
	}
	reader.Get(record.persistent.first);
	reader.Get(record.persistent.second);
	record.values.clear();
	record.references.clear();
	for (const auto field : layout->fields)
	{
		bool read = false;
		switch (field)
		{
		case RuleField::Byte:
			read = GetBytes<1>(reader, record.values);
			break;
		case RuleField::Word:
			read = GetBytes<4>(reader, record.values);
			break;
		case RuleField::Point:
			read = GetBytes<12>(reader, record.values);
			break;
		case RuleField::Matrix:
			read = GetBytes<48>(reader, record.values);
			break;
		case RuleField::Ref:
		{
			// a reference may open a new object, whose whole record follows at once
			ObjectNumber number = 0;
			read = !reader.Failed() && ReadReference(reader, number, readNew);
			record.references.push_back(number);
			break;
		}
		}
		if (!read)
		{
			return false;
		}
	}
	return !reader.Failed();
}

void openblack::gamesave::WriteRuleData(StreamWriter& writer, const RuleDataRecord& record, const WriteNewObject& writeNew)
{
	WriteThing(writer, record.type, record.thing);
	writer.Put(record.persistent.first);
	writer.Put(record.persistent.second);
	const auto* layout = RuleDataLayoutOf(record.type);
	if (layout == nullptr)
	{
		writer.Fail();
		return;
	}
	size_t offset = 0;
	size_t reference = 0;
	for (const auto field : layout->fields)
	{
		switch (field)
		{
		case RuleField::Byte:
			offset = PutBytes<1>(writer, record.values, offset);
			break;
		case RuleField::Word:
			offset = PutBytes<4>(writer, record.values, offset);
			break;
		case RuleField::Point:
			offset = PutBytes<12>(writer, record.values, offset);
			break;
		case RuleField::Matrix:
			offset = PutBytes<48>(writer, record.values, offset);
			break;
		case RuleField::Ref:
			WriteReference(writer, reference < record.references.size() ? record.references[reference] : 0, writeNew);
			++reference;
			break;
		}
	}
}
