/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <initializer_list>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <ObjectStream.h>
#include <VanillaSaveFile.h>
#include <gtest/gtest.h>

// The values, the running sum and the object numbers of the game's world file, on synthetic bytes. The two sums of the
// game object are the ones the original wrote into its saves. See docs/wiki/engine/saved-games.md#the-world-file.

using namespace openblack::gamesave;

namespace
{
template <typename T>
T At(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

/// A value of twelve bytes, as the game's map position: the sum counts it once, by its first byte
struct TwelveBytes
{
	int32_t x {0};
	int32_t z {0};
	float altitude {0.0f};
};

/// The game object's type, the first object of every world file
constexpr uint32_t k_GameType = 0x6A;
} // namespace

TEST(SaveObjectStream, EachValueAddsItsFirstByteAndItsSizeToTheSum)
{
	StreamWriter writer;
	writer.Put(uint32_t {0x1234});
	writer.Put(uint8_t {7});
	writer.Put(TwelveBytes {.x = 0x0102, .z = 0, .altitude = 0.0f});
	const uint32_t sum = (0x34u + 4) + (7u + 1) + (0x02u + 12);
	EXPECT_EQ(writer.Checksum(), sum);
	writer.PutUncounted(99);
	EXPECT_EQ(writer.Checksum(), sum);
	StreamReader reader(writer.Bytes());
	uint32_t word = 0;
	uint8_t byte = 0;
	TwelveBytes coords;
	reader.Get(word);
	reader.Get(byte);
	reader.Get(coords);
	EXPECT_EQ(reader.Checksum(), sum);
}

TEST(SaveObjectStream, AReferenceIsNoneAnEarlierNumberOrTheNextNumber)
{
	StreamWriter writer({.objects = 3, .checksum = 0});
	EXPECT_FALSE(writer.PutReference(k_NoObject));
	EXPECT_FALSE(writer.PutReference(NumberKey(2)));
	EXPECT_TRUE(writer.PutReference(EntityKey(50)));
	EXPECT_FALSE(writer.PutReference(EntityKey(50)));
	EXPECT_TRUE(writer.PutReference(NumberKey(9)));
	const auto bytes = writer.Bytes();
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 4), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 8), 4u);
	EXPECT_EQ(At<uint32_t>(bytes, 12), 4u);
	EXPECT_EQ(At<uint32_t>(bytes, 16), 5u);

	StreamReader reader(bytes, {.objects = 3, .checksum = 0});
	ObjectKey key = 0;
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::None);
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::Earlier);
	EXPECT_EQ(key, NumberKey(2));
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::New);
	EXPECT_EQ(key, NumberKey(4));
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::Earlier);
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::New);

	// A number past the next number is not a stream of the game
	const std::vector<uint8_t> bad {7, 0, 0, 0};
	StreamReader badReader(bad, {.objects = 3, .checksum = 0});
	EXPECT_EQ(badReader.GetReference(key), ReferenceKind::Bad);
	EXPECT_TRUE(badReader.Failed());
}

TEST(SaveObjectStream, TheSumTakesTheLowByteOfEachValueUnsigned)
{
	StreamWriter writer;
	writer.Put(int32_t {-1});
	EXPECT_EQ(writer.Checksum(), 0xFFu + 4);
	writer.Put(1.0f);
	EXPECT_EQ(writer.Checksum(), (0xFFu + 4) + (0x00u + 4));
	writer.Put(uint16_t {0x01FE});
	EXPECT_EQ(writer.Checksum(), (0xFFu + 4) + 4 + (0xFEu + 2));
	const std::vector<uint8_t> golden {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x80, 0x3F, 0xFE, 0x01};
	EXPECT_EQ(writer.Bytes(), golden);
}

TEST(SaveObjectStream, AnObjectsHeadKeepsTheSumOfWhatCameBefore)
{
	// The head every record starts with: the type, the extra word, the running sum, the destroyed word and the flags
	StreamWriter writer({.objects = 247, .checksum = 0x27000});
	writer.Put(uint32_t {0x4F});
	writer.Put(uint32_t {0});
	writer.PutChecksum();
	writer.Put(uint32_t {0});
	writer.Put(uint8_t {0});
	const uint32_t stored = 0x27000u + (0x4Fu + 4) + (0u + 4);
	const auto& bytes = writer.Bytes();
	ASSERT_EQ(bytes.size(), 17u);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0x4Fu);
	EXPECT_EQ(At<uint32_t>(bytes, 8), stored);
	// The stored sum then counts itself, as any other value
	EXPECT_EQ(writer.Checksum(), stored + ((stored & 0xFFu) + 4) + 4 + 1);

	StreamReader reader(bytes, {.objects = 247, .checksum = 0x27000});
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	const auto expected = reader.Checksum();
	uint32_t read = 0;
	reader.Get(read);
	EXPECT_EQ(read, expected);
}

TEST(SaveObjectStream, AReadPastTheEndFailsAndEveryReadAfterIt)
{
	const std::vector<uint8_t> bytes {1, 2, 3};
	StreamReader reader(bytes);
	uint32_t word = 0;
	EXPECT_FALSE(reader.Get(word));
	EXPECT_TRUE(reader.Failed());
	EXPECT_EQ(reader.Left(), 0u);
	uint8_t byte = 0;
	EXPECT_FALSE(reader.Get(byte));
	EXPECT_EQ(reader.Checksum(), 0u);
}

TEST(SaveVanillaFile, TheStartHasTheRevisionAndTheLand)
{
	const auto bytes = WriteVanillaFraming(".\\DATA\\LANDSCAPE\\Land1.lnd");
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 4), 17u);
	const auto framing = ReadVanillaFraming(bytes);
	ASSERT_TRUE(framing.has_value());
	EXPECT_EQ(framing->revision, k_VanillaRevision);
	EXPECT_EQ(framing->landscape, ".\\DATA\\LANDSCAPE\\Land1.lnd");
	EXPECT_EQ(framing->body, bytes.size());
}

TEST(SaveVanillaFile, TheGameObjectKeepsTheSumTheGameWroteForEachLand)
{
	// The game object's record follows the start at once, with no reference: it is the stream's first object, so the
	// first object it writes gets the number 2. The game wrote 0xD83 into its saves of Land 2 and 0xD82 into Land 1's.
	for (const auto& [landscape, sum] :
	     {std::pair {".\\DATA\\LANDSCAPE\\Land2.lnd", 0xD83u}, std::pair {".\\DATA\\LANDSCAPE\\Land1.lnd", 0xD82u}})
	{
		const auto start = WriteVanillaFraming(landscape);
		EXPECT_EQ(start.size(), 0x38u);
		const auto framing = ReadVanillaFraming(start);
		ASSERT_TRUE(framing.has_value());
		StreamWriter writer({.objects = 1, .checksum = framing->checksum});
		writer.Put(k_GameType);
		writer.Put(uint32_t {0});
		writer.PutChecksum();
		EXPECT_EQ(At<uint32_t>(writer.Bytes(), 8), sum) << landscape;
		EXPECT_TRUE(writer.PutReference(EntityKey(1)));
		EXPECT_EQ(At<uint32_t>(writer.Bytes(), 12), 2u);
	}
}

TEST(SaveVanillaFile, AStartWithNoTextOrACutTextIsNotAWorldFile)
{
	auto bytes = WriteVanillaFraming(".\\DATA\\LANDSCAPE\\Land1.lnd");
	EXPECT_FALSE(ReadVanillaFraming(std::span(bytes).first(bytes.size() - 1)).has_value());
	bytes[4] = 0;
	EXPECT_FALSE(ReadVanillaFraming(bytes).has_value());
	EXPECT_FALSE(ReadVanillaFraming(std::vector<uint8_t>(3)).has_value());
}
