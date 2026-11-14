/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdio>

#include <array>
#include <string>
#include <string_view>
#include <variant>

#include <SavedRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

// The rule data classes read by their layouts: each class's values after its rule, each written as one of the game's
// values (its first byte and its size added to the running sum) in the game's order, and read back as written
// (docs/wiki/engine/saved-games.md#the-particles-rules-data).

using namespace openblack::gamesave;

namespace
{
constexpr StreamStart k_After100 {.objects = 100, .checksum = 0x27000};

/// A class and its values after the rule as the exe's save function writes them: a byte B, a word W, a point P (12
/// bytes), a matrix M (48 bytes) and a reference R
struct ClassLayout
{
	uint32_t type;
	std::string_view fields;
};

constexpr std::array k_Classes {
    ClassLayout {.type = 0xBB, .fields = "WR"},     ClassLayout {.type = 0xBD, .fields = "PPPPWWB"},
    ClassLayout {.type = 0xBE, .fields = "PB"},     ClassLayout {.type = 0xC0, .fields = "PPPPPPPWWWWW"},
    ClassLayout {.type = 0xC1, .fields = "BPPBWW"}, ClassLayout {.type = 0xC2, .fields = "WWWWW"},
    ClassLayout {.type = 0xC3, .fields = "BPWB"},   ClassLayout {.type = 0xC5, .fields = "BWW"},
    ClassLayout {.type = 0xC6, .fields = "BWR"},    ClassLayout {.type = 0xC7, .fields = "B"},
    ClassLayout {.type = 0xC8, .fields = "BBWW"},   ClassLayout {.type = 0xCB, .fields = "BWWW"},
    ClassLayout {.type = 0xCC, .fields = "BP"},     ClassLayout {.type = 0xCD, .fields = "BWWW"},
    ClassLayout {.type = 0xCE, .fields = "B"},      ClassLayout {.type = 0xD0, .fields = "BP"},
    ClassLayout {.type = 0xD1, .fields = "B"},      ClassLayout {.type = 0xD2, .fields = "PWWBWWWWRRBBBB"},
    ClassLayout {.type = 0xD3, .fields = "WWWBWW"}, ClassLayout {.type = 0xD4, .fields = "WWW"},
    ClassLayout {.type = 0xD5, .fields = "WW"},     ClassLayout {.type = 0xD7, .fields = "BPPPPPWWBWB"},
    ClassLayout {.type = 0xD9, .fields = "WBWWB"},  ClassLayout {.type = 0xDB, .fields = "BWR"},
    ClassLayout {.type = 0xDD, .fields = "B"},      ClassLayout {.type = 0xDE, .fields = "P"},
    ClassLayout {.type = 0xDF, .fields = "B"},      ClassLayout {.type = 0xE0, .fields = "WWWWM"},
    ClassLayout {.type = 0xE1, .fields = "B"},      ClassLayout {.type = 0xE2, .fields = "B"},
    ClassLayout {.type = 0xE3, .fields = "W"},      ClassLayout {.type = 0xE4, .fields = "BWWR"},
    ClassLayout {.type = 0xE5, .fields = "W"},      ClassLayout {.type = 0xE6, .fields = "BW"},
    ClassLayout {.type = 0xE7, .fields = "WB"},     ClassLayout {.type = 0xE8, .fields = "WB"},
    ClassLayout {.type = 0xE9, .fields = "BW"},     ClassLayout {.type = 0xEA, .fields = "PWB"},
    ClassLayout {.type = 0xEB, .fields = "WR"},     ClassLayout {.type = 0xEC, .fields = "BR"},
    ClassLayout {.type = 0xEF, .fields = "BWW"},    ClassLayout {.type = 0xF0, .fields = "BWW"},
    ClassLayout {.type = 0xF1, .fields = "B"},      ClassLayout {.type = 0xF2, .fields = "WRBBBWW"},
    ClassLayout {.type = 0xF4, .fields = "W"},      ClassLayout {.type = 0x10E, .fields = "W"},
};

std::string NameOf(const testing::TestParamInfo<ClassLayout>& info)
{
	std::array<char, 16> name {};
	std::snprintf(name.data(), name.size(), "Class%03X", info.param.type);
	return name.data();
}

class RuleDataRecordLayout: public testing::TestWithParam<ClassLayout>
{
};
} // namespace

TEST_P(RuleDataRecordLayout, EachValueIsOneOfTheGamesValuesInTheGamesOrderAndReadsBack)
{
	const auto& [type, fields] = GetParam();
	auto made = RuleDataRecord::Of(type);
	ASSERT_TRUE(made.has_value());
	auto& record = *made;
	record.thing = {.extra = 3, .checksum = 0, .destroyed = 0, .flags = 0x10};
	record.persistent = {.first = 22, .second = 7};

	// What the game writes: the part every object writes, the rule, then each value on its own
	StreamWriter expected(k_After100);
	WriteThing(expected, type, record.thing);
	expected.Put(uint32_t {22});
	expected.Put(uint32_t {7});
	for (size_t i = 0; i < fields.size(); ++i)
	{
		const auto at = static_cast<float>(i);
		switch (fields[i])
		{
		case 'B':
		{
			const auto value = static_cast<uint8_t>(0x11 * (i + 1));
			ASSERT_TRUE(record.SetByte(i, value));
			expected.Put(value);
			break;
		}
		case 'W':
		{
			const auto value = 0x01020300U + static_cast<uint32_t>(i);
			ASSERT_TRUE(record.SetWord(i, value));
			expected.Put(value);
			break;
		}
		case 'P':
		{
			const Point value {at, 0.5f, -at};
			ASSERT_TRUE(record.SetPoint(i, value));
			expected.Put(value);
			break;
		}
		case 'M':
		{
			std::array<float, 12> value {};
			for (size_t j = 0; j < value.size(); ++j)
			{
				value.at(j) = at + static_cast<float>(j) * 0.25f;
			}
			ASSERT_TRUE(record.SetMatrix(i, value));
			expected.Put(value);
			break;
		}
		case 'R':
		{
			const auto number = static_cast<ObjectNumber>(40 + i);
			ASSERT_TRUE(record.SetReference(i, number));
			WriteReference(expected, number, nullptr);
			break;
		}
		default:
			FAIL() << "a layout of the test has a value of no kind";
		}
	}

	StreamWriter writer(k_After100);
	Write(writer, SavedRecord {record}, nullptr);
	EXPECT_EQ(writer.Bytes(), expected.Bytes());
	EXPECT_EQ(writer.Checksum(), expected.Checksum());

	StreamReader reader(writer.Bytes(), k_After100);
	uint32_t readType = 0;
	uint32_t extra = 0;
	reader.Get(readType);
	reader.Get(extra);
	EXPECT_TRUE(IsKnownType(readType));
	SavedRecord read;
	ASSERT_TRUE(ReadRecord(reader, readType, extra, read, nullptr));
	EXPECT_EQ(reader.Position(), writer.Bytes().size());
	EXPECT_EQ(TypeOf(read), type);
	const auto* data = std::get_if<RuleDataRecord>(&read);
	ASSERT_NE(data, nullptr);
	EXPECT_EQ(data->thing.extra, 3u);
	EXPECT_EQ(data->thing.flags, 0x10);
	EXPECT_EQ(data->persistent, record.persistent);
	EXPECT_EQ(data->values, record.values);
	EXPECT_EQ(data->references, record.references);
	EXPECT_FALSE(TypeName(type).starts_with("class "));
}

INSTANTIATE_TEST_SUITE_P(EachClass, RuleDataRecordLayout, testing::ValuesIn(k_Classes), NameOf);

TEST(RuleDataRecords, TheTableHoldsTheClassesOfTheTestOnly)
{
	ASSERT_EQ(RuleDataLayouts().size(), k_Classes.size());
	for (size_t i = 0; i < k_Classes.size(); ++i)
	{
		EXPECT_EQ(RuleDataLayouts()[i].type, k_Classes.at(i).type);
		EXPECT_EQ(RuleDataLayouts()[i].fields.size(), k_Classes.at(i).fields.size());
	}
	// the flying collections of the tornado write themselves as its debris collections: no save holds 0xD6
	EXPECT_FALSE(IsKnownType(0xD6));
	EXPECT_FALSE(RuleDataRecord::Of(0xD6).has_value());
	// a class with a record of its own keeps it
	EXPECT_EQ(RuleDataLayoutOf(VortexAttractAtomDataRecord::k_Type), nullptr);
	EXPECT_FALSE(IsKnownType(k_RuleDataRecordType));
}

TEST(RuleDataRecords, AValueIsReadOnlyAsItsKind)
{
	auto record = RuleDataRecord::Of(0xC0);
	ASSERT_TRUE(record.has_value());
	EXPECT_EQ(record->values.size(), 7u * 12 + 5 * 4);
	EXPECT_TRUE(record->references.empty());
	EXPECT_FALSE(record->Byte(0).has_value());
	EXPECT_FALSE(record->SetWord(0, 1));
	ASSERT_TRUE(record->SetFloat(11, 1.5f));
	EXPECT_EQ(record->Float(11), 1.5f);
	EXPECT_EQ(record->Word(11), 0x3FC00000u);
	EXPECT_FALSE(record->Word(12).has_value());
	EXPECT_EQ(TypeName(0xC0), "mana path atom data");
}

TEST(RuleDataRecords, AReferenceToANewObjectReadsItsRecordAtOnce)
{
	auto record = RuleDataRecord::Of(0xEB);
	ASSERT_TRUE(record.has_value());
	// the reference is the next new object's number: its record follows the reference
	ASSERT_TRUE(record->SetReference(1, 101));
	StreamWriter writer(k_After100);
	bool wroteNew = false;
	Write(writer, SavedRecord {*record},
	      [&wroteNew](StreamWriter& /*stream*/, ObjectNumber number) { wroteNew = number == 101; });
	EXPECT_TRUE(wroteNew);

	StreamReader reader(writer.Bytes(), k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	bool readNew = false;
	SavedRecord read;
	ASSERT_TRUE(ReadRecord(reader, type, extra, read, [&readNew](StreamReader& /*stream*/, ObjectNumber number) {
		readNew = number == 101;
		return true;
	}));
	EXPECT_TRUE(readNew);
	EXPECT_EQ(std::get<RuleDataRecord>(read).Reference(1), 101u);
}
