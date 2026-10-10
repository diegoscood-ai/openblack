/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

#include <GestureFile.h>
#include <gtest/gtest.h>

#include "Magic/Gestures/GestureTemplates.h"

using namespace openblack::gestures;

namespace
{
GestureTemplate MakeTemplate(uint8_t gesture, size_t points, uint32_t junkHighBytes)
{
	GestureTemplate entry;
	for (size_t i = 0; i < points; ++i)
	{
		entry.points.at(i) = {.x = static_cast<float>(i) / 10.0f,
		                      .y = 0.0f,
		                      .z = 0.5f,
		                      .turn = (i % 2 == 0) ? 1.5f : -1.5f,
		                      .direction = static_cast<uint32_t>(i % 8)};
	}
	// Left-over memory after the points is kept as it was
	entry.points.back().x = 123.0f;
	entry.pointCountField = static_cast<uint32_t>(points) | junkHighBytes;
	entry.gestureField = gesture | junkHighBytes;
	entry.positionModeField = 2u | junkHighBytes;
	entry.checkDirection = 1;
	entry.allowMirror = 0;
	entry.checkAspectRatio = 1;
	entry.aspectRatio = 0.75f;
	return entry;
}
} // namespace

TEST(GestureFile, WritesAndReadsBackTheSameTemplates)
{
	GestureFile file;
	file.AddTemplate(MakeTemplate(4, 6, 0xABCD0000u));
	file.AddTemplate(MakeTemplate(5, 3, 0));
	const auto bytes = file.Write();
	ASSERT_EQ(bytes.size(), sizeof(int32_t) + (2 * GestureFile::k_RecordSize));
	ASSERT_EQ(GestureFile::k_RecordSize, 1628u);

	GestureFile read;
	ASSERT_EQ(read.Open(bytes), GestureFileResult::Success);
	ASSERT_EQ(read.GetTemplates().size(), 2u);
	const auto& circle = read.GetTemplates().front();
	EXPECT_EQ(circle.Gesture(), 4);
	EXPECT_EQ(circle.PointCount(), 6u);
	EXPECT_EQ(circle.PositionModeValue(), 2);
	EXPECT_TRUE(circle.ChecksDirection());
	EXPECT_FALSE(circle.AllowsMirror());
	EXPECT_TRUE(circle.ChecksAspectRatio());
	EXPECT_FLOAT_EQ(circle.aspectRatio, 0.75f);
	EXPECT_FLOAT_EQ(circle.Points()[3].x, 0.3f);
	EXPECT_EQ(circle.Points()[5].direction, 5u);
	EXPECT_EQ(read.Write(), bytes);
}

TEST(GestureFile, RefusesFilesOfTheWrongSize)
{
	GestureFile file;
	EXPECT_EQ(file.Open(std::vector<uint8_t> {1, 0}), GestureFileResult::ErrFileTooSmall);
	std::vector<uint8_t> bytes(sizeof(int32_t) + GestureFile::k_RecordSize - 1, 0);
	bytes[0] = 1;
	EXPECT_EQ(file.Open(bytes), GestureFileResult::ErrSizeMismatch);
	EXPECT_TRUE(file.GetTemplates().empty());
}

TEST(GestureFile, RefusesTooManyPoints)
{
	GestureFile file;
	file.AddTemplate(MakeTemplate(4, 3, 0));
	auto bytes = file.Write();
	// The point count's low byte, just after the points
	bytes.at(sizeof(int32_t) + (GestureTemplate::k_MaxPoints * sizeof(TemplatePoint))) = 81;
	GestureFile read;
	EXPECT_EQ(read.Open(bytes), GestureFileResult::ErrTooManyPoints);
}

TEST(GestureFile, ReadsTheGamesTemplates)
{
	const char* gamePath = std::getenv("OPENBLACK_GAME_PATH");
	if (gamePath == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH is not set";
	}
	const auto path = std::filesystem::path(gamePath) / "Data" / "Gestures.jty";
	if (!std::filesystem::exists(path))
	{
		GTEST_SKIP() << "No gesture templates in the game folder";
	}
	GestureFile file;
	ASSERT_EQ(file.Open(path), GestureFileResult::Success);
	const auto& templates = file.GetTemplates();
	ASSERT_EQ(templates.size(), 81u);
	const auto countOf = [&templates](uint8_t gesture) {
		return std::ranges::count_if(templates, [gesture](const auto& entry) { return entry.Gesture() == gesture; });
	};
	EXPECT_EQ(countOf(4), 1);   // circle
	EXPECT_EQ(countOf(5), 2);   // scribble
	EXPECT_EQ(countOf(13), 10); // heart
	EXPECT_EQ(countOf(15), 1);  // square spiral, the leash gesture
	for (const auto& entry : templates)
	{
		EXPECT_EQ(entry.PositionModeValue(), 2);
		EXPECT_GE(entry.PointCount(), 6u);
		EXPECT_GE(entry.Gesture(), 1);
		EXPECT_LE(entry.Gesture(), 23);
		for (const auto& point : entry.Points())
		{
			EXPECT_LT(point.direction, 8u);
		}
	}
	// Only the circle and the stars may be drawn mirrored
	EXPECT_TRUE(std::ranges::all_of(
	    templates, [](const auto& entry) { return !entry.AllowsMirror() || entry.Gesture() == 4 || entry.Gesture() == 8; }));

	// Written back byte for byte
	std::ifstream stream(path, std::ios::binary);
	const std::vector<uint8_t> original((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	EXPECT_EQ(file.Write(), original);
}

// The recogniser's template list (magic::gestures::LoadTemplates) is read through GestureFile. These tests pin it to
// the parse it replaced, kept below as the reference, field for field and bit for bit.
namespace
{
namespace recogniser = openblack::magic::gestures;

constexpr size_t k_FormerRecordSize = 1628;
constexpr size_t k_FormerSampleSize = 20;
constexpr size_t k_FormerFieldsAt = 1600;

template <class T>
T FormerRead(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

/// The parse the recogniser used before GestureFile, unchanged: nullopt where it returned false
std::optional<std::vector<recogniser::GestureData>> FormerParse(const std::vector<uint8_t>& bytes)
{
	if (bytes.size() < 4)
	{
		return std::nullopt;
	}
	const auto count = FormerRead<uint32_t>(bytes, 0);
	if (bytes.size() < 4 + (static_cast<size_t>(count) * k_FormerRecordSize))
	{
		return std::nullopt;
	}
	std::vector<recogniser::GestureData> out(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		const size_t base = 4 + (static_cast<size_t>(i) * k_FormerRecordSize);
		auto& data = out[i];
		for (size_t s = 0; s < recogniser::k_MaxSamples; ++s)
		{
			const size_t at = base + (s * k_FormerSampleSize);
			data.samples.at(s) = {FormerRead<float>(bytes, at), FormerRead<float>(bytes, at + 4),
			                      FormerRead<float>(bytes, at + 8), FormerRead<float>(bytes, at + 12),
			                      FormerRead<uint32_t>(bytes, at + 16)};
		}
		const size_t fields = base + k_FormerFieldsAt;
		data.count = static_cast<uint8_t>(FormerRead<uint32_t>(bytes, fields));
		data.gesture = static_cast<recogniser::Gesture>(FormerRead<uint32_t>(bytes, fields + 4));
		data.positionMode = static_cast<uint8_t>(FormerRead<uint32_t>(bytes, fields + 8));
		data.checkDirection = FormerRead<uint32_t>(bytes, fields + 12) != 0;
		data.allowReverse = FormerRead<uint32_t>(bytes, fields + 16) != 0;
		data.checkAspect = FormerRead<uint32_t>(bytes, fields + 20) != 0;
		data.aspect = FormerRead<float>(bytes, fields + 24);
	}
	return out;
}

/// Every field of both lists, the floats compared bit for bit
void ExpectSameTemplates(const std::vector<recogniser::GestureData>& expected,
                         const std::vector<recogniser::GestureData>& actual)
{
	ASSERT_EQ(actual.size(), expected.size());
	for (size_t t = 0; t < expected.size(); ++t)
	{
		const auto& a = actual[t];
		const auto& e = expected[t];
		EXPECT_EQ(a.count, e.count) << t;
		EXPECT_EQ(a.gesture, e.gesture) << t;
		EXPECT_EQ(a.positionMode, e.positionMode) << t;
		EXPECT_EQ(a.checkDirection, e.checkDirection) << t;
		EXPECT_EQ(a.allowReverse, e.allowReverse) << t;
		EXPECT_EQ(a.checkAspect, e.checkAspect) << t;
		EXPECT_EQ(std::bit_cast<uint32_t>(a.aspect), std::bit_cast<uint32_t>(e.aspect)) << t;
		for (size_t s = 0; s < recogniser::k_MaxSamples; ++s)
		{
			const auto& as = a.samples.at(s);
			const auto& es = e.samples.at(s);
			EXPECT_EQ(std::bit_cast<uint32_t>(as.x), std::bit_cast<uint32_t>(es.x)) << t << " " << s;
			EXPECT_EQ(std::bit_cast<uint32_t>(as.y), std::bit_cast<uint32_t>(es.y)) << t << " " << s;
			EXPECT_EQ(std::bit_cast<uint32_t>(as.z), std::bit_cast<uint32_t>(es.z)) << t << " " << s;
			EXPECT_EQ(std::bit_cast<uint32_t>(as.turn), std::bit_cast<uint32_t>(es.turn)) << t << " " << s;
			EXPECT_EQ(as.direction, es.direction) << t << " " << s;
		}
	}
}

void PutWord(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

/// A file of `count` records with every byte set: keypoints past the point count, high bytes on the three byte-sized
/// fields, flags of 0, 1 and 2, and aspects with odd bit patterns
std::vector<uint8_t> NoisyFile(uint32_t count)
{
	std::vector<uint8_t> bytes(4 + (static_cast<size_t>(count) * k_FormerRecordSize));
	PutWord(bytes, 0, count);
	uint32_t noise = 2654435769u;
	for (size_t i = 4; i < bytes.size(); ++i)
	{
		noise = (noise * 1664525u) + 1013904223u;
		bytes[i] = static_cast<uint8_t>(noise >> 24);
	}
	for (uint32_t i = 0; i < count; ++i)
	{
		const size_t fields = 4 + (static_cast<size_t>(i) * k_FormerRecordSize) + k_FormerFieldsAt;
		// point counts from 3 to 80, with junk in the upper bytes
		PutWord(bytes, fields, (2779119616u) | (3u + ((i * 7u) % 78u)));
		PutWord(bytes, fields + 4, (16711936u) | ((i % 23u) + 1u));
		PutWord(bytes, fields + 8, (305397760u) | 2u);
		PutWord(bytes, fields + 12, i % 3u);
		PutWord(bytes, fields + 16, (i + 1u) % 3u);
		PutWord(bytes, fields + 20, (i + 2u) % 3u);
		// quiet NaNs with payloads, which must come through unchanged
		PutWord(bytes, fields + 24, 2143289345u + i);
	}
	return bytes;
}
} // namespace

TEST(GestureFile, GivesTheRecogniserTheFormerParseOfSyntheticBytes)
{
	const auto bytes = NoisyFile(12);
	const auto former = FormerParse(bytes);
	ASSERT_TRUE(former.has_value());
	std::vector<recogniser::GestureData> list;
	ASSERT_TRUE(recogniser::LoadTemplates(bytes, list));
	ExpectSameTemplates(*former, list);

	// bytes after the last record are ignored by both
	auto longer = bytes;
	longer.insert(longer.end(), {1, 2, 3, 4, 5});
	ASSERT_TRUE(recogniser::LoadTemplates(longer, list));
	ExpectSameTemplates(*FormerParse(longer), list);

	// short data is refused by both, with an empty list
	for (const size_t size : {size_t {0}, size_t {3}, bytes.size() - 1})
	{
		const std::vector<uint8_t> shorter(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size));
		EXPECT_FALSE(FormerParse(shorter).has_value()) << size;
		EXPECT_FALSE(recogniser::LoadTemplates(shorter, list)) << size;
		EXPECT_TRUE(list.empty()) << size;
	}
	// an empty file is an empty list for both
	const std::vector<uint8_t> empty(4, 0);
	ASSERT_TRUE(FormerParse(empty).has_value());
	EXPECT_TRUE(recogniser::LoadTemplates(empty, list));
	EXPECT_TRUE(list.empty());

	// The one difference: a template with more than 80 keypoints, which the matcher could not index, is now refused
	auto tooMany = bytes;
	PutWord(tooMany, 4 + k_FormerFieldsAt, 81);
	EXPECT_TRUE(FormerParse(tooMany).has_value());
	EXPECT_FALSE(recogniser::LoadTemplates(tooMany, list));
	EXPECT_TRUE(list.empty());
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. Its synthetic twin is the
// test above.
TEST(GestureFile, GivesTheRecogniserTheFormerParseOfTheGamesTemplates)
{
	const char* gamePath = std::getenv("OPENBLACK_GAME_PATH");
	if (gamePath == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH is not set";
	}
	const auto path = std::filesystem::path(gamePath) / "Data" / "Gestures.jty";
	if (!std::filesystem::exists(path))
	{
		GTEST_SKIP() << "No gesture templates in the game folder";
	}
	std::ifstream stream(path, std::ios::binary);
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	const auto former = FormerParse(bytes);
	ASSERT_TRUE(former.has_value());
	ASSERT_EQ(former->size(), 81u);
	std::vector<recogniser::GestureData> list;
	ASSERT_TRUE(recogniser::LoadTemplates(bytes, list));
	ExpectSameTemplates(*former, list);
}
