/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "ECS/CreatureHandPackets.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Magic/Gestures/GestureTemplates.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "Resources/Loaders.h"
#include "support/CreatureFakes.h"

using namespace openblack;
using namespace openblack::magic::gestures;

namespace
{
constexpr float k_Ratio = 4.0f / 3.0f;

/// Mouse samples along a polyline, `step` pixels apart (a sample per mouse-move message)
void FeedPolyline(GestureSystem& system, const std::vector<glm::vec2>& points, float step)
{
	system.Clear();
	glm::vec2 at = points.front();
	system.AddSample(glm::vec3(at.x, 0.0f, at.y), glm::ivec2(glm::round(at)));
	for (size_t k = 1; k < points.size(); ++k)
	{
		const glm::vec2 to = points[k];
		while (glm::distance(at, to) > step)
		{
			at += glm::normalize(to - at) * step;
			system.AddSample(glm::vec3(at.x, 0.0f, at.y), glm::ivec2(glm::round(at)));
		}
	}
	at = points.back();
	system.AddSample(glm::vec3(at.x, 0.0f, at.y), glm::ivec2(glm::round(at)));
}

/// A template drawn back on a screen: its normalised keypoints x 200 px (z divided by the screen ratio, as the
/// template tool's normaliser multiplied it), mirrored left-right if asked
std::vector<glm::vec2> StrokeOf(const GestureData& tpl, bool mirror)
{
	std::vector<glm::vec2> points;
	for (int k = 0; k < tpl.count; ++k)
	{
		const float x = tpl.samples[k].x * 200.0f;
		points.emplace_back(100.0f + (mirror ? 200.0f - x : x), 100.0f + tpl.samples[k].z * 200.0f / k_Ratio);
	}
	return points;
}

float Length(const std::vector<glm::vec2>& points)
{
	float length = 0.0f;
	for (size_t k = 1; k < points.size(); ++k)
	{
		length += glm::distance(points[k - 1], points[k]);
	}
	return length;
}

/// A stroke spread over about 60 messages (the buffer keeps 80)
bool Recognise(const std::vector<GestureData>& list, const std::vector<glm::vec2>& stroke, Gesture gesture, Result& result)
{
	GestureSystem system;
	FeedPolyline(system, stroke, std::max(5.0f, Length(stroke) / 60.0f));
	const auto input = BuildFromSystem(system, k_Ratio);
	return MatchGesture(list, gesture, input, result, k_Ratio);
}

/// A synthetic template from headings: keypoints at the corners of a unit polyline
GestureData TemplateOf(Gesture gesture, const std::vector<glm::vec2>& corners, bool allowReverse)
{
	GestureData tpl;
	tpl.SetToZero();
	tpl.gesture = gesture;
	tpl.positionMode = 2;
	tpl.checkDirection = true;
	tpl.allowReverse = allowReverse;
	float previous = 0.0f;
	for (size_t k = 0; k < corners.size(); ++k)
	{
		KeySample s;
		s.x = corners[k].x;
		s.z = corners[k].y;
		if (k + 1 < corners.size())
		{
			const auto d = corners[k + 1] - corners[k];
			const float heading = Atan2Positive(d.x, d.y);
			s.direction = Octant(heading);
			s.turn = k == 0 ? 0.0f : WrapDifference(previous, heading);
			previous = heading;
		}
		tpl.Append(s);
	}
	return tpl;
}

std::unique_ptr<InfoConstants> LoadInfo(const char* game)
{
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (data.size() != 0x2C + sizeof(InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	return info;
}

std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

/// The octants on the screen (y down) and the rounding of an exact .5
TEST(Gestures, octant)
{
	EXPECT_EQ(Octant(Atan2Positive(0.0f, -1.0f)), 0u); // up
	EXPECT_EQ(Octant(Atan2Positive(1.0f, 0.0f)), 2u);  // right
	EXPECT_EQ(Octant(Atan2Positive(0.0f, 1.0f)), 4u);  // down
	EXPECT_EQ(Octant(Atan2Positive(-1.0f, 0.0f)), 6u); // left
	EXPECT_EQ(Octant(Atan2Positive(1.0f, -1.0f)), 1u); // up-right
	EXPECT_EQ(RoundHalfDown(0.5f), 0);
	EXPECT_EQ(RoundHalfDown(1.5f), 1);
	EXPECT_EQ(RoundHalfDown(2.5f), 2);
	EXPECT_EQ(RoundHalfDown(0.50001f), 1);
	EXPECT_EQ(RoundHalfDown(7.49f), 7);
	// the turn wraps: +2pi-ish differences come back into (-pi, pi]
	EXPECT_NEAR(WrapDifference(6.0f, 0.2f), 0.2f + glm::two_pi<float>() - 6.0f, 1e-5f);
	EXPECT_NEAR(WrapDifference(0.0f, glm::pi<float>()), glm::pi<float>(), 1e-6f);
}

/// The online corner extraction: a square drawn clockwise gives the start, three corners and the end, with turns of
/// +pi/2 and the right first direction (74 samples: the buffer keeps the last 80, an older start would be lost)
TEST(Gestures, cornersOfASquare)
{
	GestureSystem system;
	FeedPolyline(system, {{100, 100}, {250, 100}, {250, 250}, {100, 250}, {100, 110}}, 8.0f);
	ASSERT_LE(system.Count(), 79);
	const auto data = BuildFromSystem(system, k_Ratio);
	ASSERT_EQ(data.count, 5);
	EXPECT_EQ(data.samples[0].direction, 2u);
	for (int k = 1; k <= 3; ++k)
	{
		EXPECT_NEAR(data.samples[k].turn, glm::half_pi<float>(), 0.15f) << k;
	}
	EXPECT_EQ(data.samples[1].direction, 4u);
	EXPECT_EQ(data.samples[2].direction, 6u);
	EXPECT_EQ(data.samples[3].direction, 0u);
}

/// 70 samples on the same pixel as the one two messages before wipe the buffer (compared with head - 2), and
/// that sample starts it again: the 72nd still sample wipes it
TEST(Gestures, stationaryWipe)
{
	GestureSystem system;
	for (int k = 0; k < 20; ++k)
	{
		system.AddSample(glm::vec3(0.0f), {10 * k, 0});
	}
	EXPECT_EQ(system.Count(), 20);
	for (int k = 0; k < 71; ++k)
	{
		system.AddSample(glm::vec3(0.0f), {500, 0});
	}
	EXPECT_EQ(system.Count(), 80);
	system.AddSample(glm::vec3(0.0f), {500, 0});
	EXPECT_EQ(system.Count(), 1);
}

/// A synthetic square template recognises its own stroke, not a triangle's; a mirrored square only with allowReverse
TEST(Gestures, syntheticTemplates)
{
	std::vector<GestureData> list;
	list.push_back(TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}, true));
	list.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}, false));
	Result result;
	const std::vector<glm::vec2> square {{100, 100}, {300, 100}, {300, 300}, {100, 300}, {100, 110}};
	EXPECT_TRUE(Recognise(list, square, k_Circle, result));
	EXPECT_FALSE(result.reversed);
	EXPECT_FALSE(Recognise(list, square, k_Spiral, result));
	const std::vector<glm::vec2> triangle {{100, 300}, {200, 100}, {300, 300}};
	EXPECT_TRUE(Recognise(list, triangle, k_Spiral, result));
	EXPECT_FALSE(Recognise(list, triangle, k_Circle, result));
	// the square anticlockwise: left, down, right, up
	const std::vector<glm::vec2> mirrored {{300, 100}, {100, 100}, {100, 300}, {300, 300}, {300, 110}};
	EXPECT_TRUE(Recognise(list, mirrored, k_Circle, result));
	EXPECT_TRUE(result.reversed);
	list[0].allowReverse = false;
	EXPECT_FALSE(Recognise(list, mirrored, k_Circle, result));
}

/// The selection path with a fake worship icon: the spiral opens the selection only while
/// an icon of that category is requestable, the miracle's gesture then asks for its seed
TEST(Gestures, selectionWithFakeIcon)
{
	std::vector<GestureData> list;
	list.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}, false));
	list.push_back(TemplateOf(9 /*FORK_RIGHT*/, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}, false));
	struct FakeIcons final: IconProvider
	{
		bool available = false;
		int requested = -1;
		[[nodiscard]] bool AnyRequestableIconOfCategory(Gesture category) const override
		{
			return available && category == k_Spiral;
		}
		void ForEachRequestableIcon(Gesture category, const std::function<void(int seedType)>& visit) const override
		{
			if (available && category == k_Spiral)
			{
				visit(4); // SHIELD's row: selection SPIRAL, gesture FORK_RIGHT (9)
			}
		}
		[[nodiscard]] bool IconValidForRequest(int /*seedType*/) const override { return available; }
		void RequestSpell(int seedType) override { requested = seedType; }
	} icons;
	SelectionTables tables;
	tables.selectionGesture = [](int seed) { return seed == 4 ? k_Spiral : k_None; };
	tables.gesture = [](int seed) { return seed == 4 ? Gesture(9) : k_None; };
	tables.gestureStage2 = [](int /*seed*/) { return k_None; };

	Selection selection;
	Result result;
	GestureSystem system;
	auto recognise = [&](Gesture g) {
		const auto input = BuildFromSystem(system, k_Ratio);
		return MatchGesture(list, g, input, result, k_Ratio);
	};
	FeedPolyline(system, {{100, 300}, {200, 100}, {300, 300}}, 8.0f);
	EXPECT_FALSE(selection.Open(k_Spiral, icons, tables)) << "no icon, no selection";
	icons.available = true;
	ASSERT_TRUE(recognise(k_Spiral));
	ASSERT_TRUE(selection.Open(k_Spiral, icons, tables));
	EXPECT_TRUE(selection.open);
	EXPECT_EQ(selection.stage, 1u);
	FeedPolyline(system, {{100, 100}, {250, 100}, {250, 250}, {100, 250}, {100, 110}}, 8.0f);
	const auto outcome = selection.Stage(0.1f, 30.0f, recognise, icons);
	EXPECT_EQ(outcome, Selection::Outcome::Requested);
	EXPECT_EQ(icons.requested, 4);
	EXPECT_FALSE(selection.open);
}

/// The real Data\Gestures.jty and info.dat (OPENBLACK_GAME_PATH): 81 templates of 1628 bytes; each player miracle's
/// stroke is its own gesture and none of the other player miracles' gestures; CIRCLE and STAR drawn mirrored match with
/// reversed = 1
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(Gestures, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto bytes = ReadFile(std::filesystem::path(game) / "Data" / "Gestures.jty");
	ASSERT_EQ(bytes.size(), 4u + 81u * 1628u);
	std::vector<GestureData> list;
	ASSERT_TRUE(LoadTemplates(bytes, list));
	ASSERT_EQ(list.size(), 81u);
	EXPECT_EQ(list[0].gesture, k_Spiral);
	EXPECT_EQ(list[0].count, 26);
	EXPECT_EQ(list[5].gesture, k_Circle);
	EXPECT_TRUE(list[5].allowReverse);
	for (const auto& tpl : list)
	{
		EXPECT_EQ(tpl.positionMode, 2);
	}
	const auto info = LoadInfo(game);
	ASSERT_NE(info, nullptr);
	// the player miracles' gestures (selection SPIRAL)
	std::set<Gesture> playerGestures;
	for (size_t s = 0; s < magic::k_SpellSeedCount; ++s)
	{
		const auto& seed = info->spellSeed[s];
		if (static_cast<Gesture>(seed.selectionGesture) == k_Spiral && seed.gesture != GestureType::None)
		{
			playerGestures.insert(static_cast<Gesture>(seed.gesture));
		}
	}
	std::string names;
	for (const auto g : playerGestures)
	{
		names += " " + std::to_string(g);
	}
	EXPECT_EQ(playerGestures.size(), 14u) << names;
	Result result;
	for (const auto gesture : playerGestures)
	{
		// the first template of that gesture, drawn
		const auto it = std::find_if(list.begin(), list.end(), [gesture](const auto& t) { return t.gesture == gesture; });
		ASSERT_NE(it, list.end());
		const auto stroke = StrokeOf(*it, false);
		EXPECT_TRUE(Recognise(list, stroke, gesture, result)) << "gesture " << int(gesture);
		for (const auto other : playerGestures)
		{
			if (other != gesture)
			{
				EXPECT_FALSE(Recognise(list, stroke, other, result))
				    << "gesture " << int(gesture) << " also matches " << int(other);
			}
		}
	}
	// mirrored strokes: the templates with allowReverse
	for (const Gesture gesture : {k_Circle, Gesture(8) /*STAR*/})
	{
		const auto it = std::find_if(list.begin(), list.end(), [gesture](const auto& t) { return t.gesture == gesture; });
		ASSERT_NE(it, list.end());
		EXPECT_TRUE(Recognise(list, StrokeOf(*it, true), gesture, result)) << "mirrored " << int(gesture);
		EXPECT_TRUE(result.reversed) << int(gesture);
	}
}

namespace
{
constexpr size_t k_JtySampleBytes = 20;
constexpr size_t k_JtyRecordBytes = 1628; // 80 samples, then six u32 fields and the aspect

/// The bytes of a Gestures.jty file holding `templates`: a u32 count, then one record each
std::vector<uint8_t> EncodeTemplates(const std::vector<GestureData>& templates)
{
	std::vector<uint8_t> bytes(4 + templates.size() * k_JtyRecordBytes, 0);
	const auto put = [&bytes](size_t offset, auto value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	put(size_t {0}, static_cast<uint32_t>(templates.size()));
	for (size_t i = 0; i < templates.size(); ++i)
	{
		const auto& tpl = templates[i];
		const size_t base = 4 + i * k_JtyRecordBytes;
		for (size_t s = 0; s < k_MaxSamples; ++s)
		{
			const auto& sample = tpl.samples[s];
			const size_t at = base + s * k_JtySampleBytes;
			put(at, sample.x);
			put(at + 4, sample.y);
			put(at + 8, sample.z);
			put(at + 12, sample.turn);
			put(at + 16, sample.direction);
		}
		const size_t tail = base + k_MaxSamples * k_JtySampleBytes;
		put(tail, static_cast<uint32_t>(tpl.count));
		put(tail + 4, static_cast<uint32_t>(tpl.gesture));
		put(tail + 8, static_cast<uint32_t>(tpl.positionMode));
		put(tail + 12, static_cast<uint32_t>(tpl.checkDirection ? 1 : 0));
		put(tail + 16, static_cast<uint32_t>(tpl.allowReverse ? 1 : 0));
		put(tail + 20, static_cast<uint32_t>(tpl.checkAspect ? 1 : 0));
		put(tail + 24, tpl.aspect);
	}
	return bytes;
}
} // namespace

/// The template loader on a small in-memory Gestures.jty: every field survives the round trip, only the low byte of
/// the byte-sized fields is kept, short data is refused, and the loaded templates recognise strokes as the originals do
TEST(Gestures, realDataSynthetic)
{
	std::vector<GestureData> source;
	source.push_back(TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}, true));
	source.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}, false));
	// a third template with the remaining flags set; no stroke below asks for its gesture
	auto shape = TemplateOf(k_RShape, {{0, 1}, {0, 0}, {1, 0.5f}}, false);
	shape.checkDirection = false;
	shape.checkAspect = true;
	shape.aspect = 1.5f;
	source.push_back(shape);

	auto bytes = EncodeTemplates(source);
	ASSERT_EQ(bytes.size(), 4u + 3u * k_JtyRecordBytes);
	// the file stores the gesture as a u32: set a high byte on the third record, which the loader drops
	const size_t gestureAt = 4 + 2 * k_JtyRecordBytes + k_MaxSamples * k_JtySampleBytes + 4;
	const uint32_t gestureWithHighByte = 256u + k_RShape;
	std::memcpy(bytes.data() + gestureAt, &gestureWithHighByte, sizeof(gestureWithHighByte));

	std::vector<GestureData> list;
	ASSERT_TRUE(LoadTemplates(bytes, list));
	ASSERT_EQ(list.size(), source.size());
	for (size_t t = 0; t < source.size(); ++t)
	{
		EXPECT_EQ(list[t].count, source[t].count) << t;
		EXPECT_EQ(list[t].gesture, source[t].gesture) << t;
		EXPECT_EQ(list[t].positionMode, 2) << t;
		EXPECT_EQ(list[t].checkDirection, source[t].checkDirection) << t;
		EXPECT_EQ(list[t].allowReverse, source[t].allowReverse) << t;
		EXPECT_EQ(list[t].checkAspect, source[t].checkAspect) << t;
		EXPECT_FLOAT_EQ(list[t].aspect, source[t].aspect) << t;
		for (size_t s = 0; s < k_MaxSamples; ++s)
		{
			EXPECT_FLOAT_EQ(list[t].samples[s].x, source[t].samples[s].x) << t << " " << s;
			EXPECT_FLOAT_EQ(list[t].samples[s].y, source[t].samples[s].y) << t << " " << s;
			EXPECT_FLOAT_EQ(list[t].samples[s].z, source[t].samples[s].z) << t << " " << s;
			EXPECT_FLOAT_EQ(list[t].samples[s].turn, source[t].samples[s].turn) << t << " " << s;
			EXPECT_EQ(list[t].samples[s].direction, source[t].samples[s].direction) << t << " " << s;
		}
	}
	EXPECT_EQ(list[0].count, 5);
	EXPECT_TRUE(list[0].allowReverse);
	EXPECT_EQ(list[2].gesture, k_RShape);
	EXPECT_TRUE(list[2].checkAspect);

	// the loaded list recognises the same strokes as the templates it was written from
	Result result;
	const std::vector<glm::vec2> square {{100, 100}, {300, 100}, {300, 300}, {100, 300}, {100, 110}};
	EXPECT_TRUE(Recognise(list, square, k_Circle, result));
	EXPECT_FALSE(result.reversed);
	EXPECT_FALSE(Recognise(list, square, k_Spiral, result));
	const std::vector<glm::vec2> triangle {{100, 300}, {200, 100}, {300, 300}};
	EXPECT_TRUE(Recognise(list, triangle, k_Spiral, result));
	EXPECT_FALSE(Recognise(list, triangle, k_Circle, result));
	const std::vector<glm::vec2> mirrored {{300, 100}, {100, 100}, {100, 300}, {300, 300}, {300, 110}};
	EXPECT_TRUE(Recognise(list, mirrored, k_Circle, result));
	EXPECT_TRUE(result.reversed);

	// short data: under the count, and a count that promises more records than there are
	EXPECT_FALSE(LoadTemplates(std::vector<uint8_t>(3, 0), list));
	EXPECT_TRUE(list.empty());
	bytes.resize(bytes.size() - 1);
	EXPECT_FALSE(LoadTemplates(bytes, list));
	EXPECT_TRUE(list.empty());
	// an empty file is a valid empty list
	EXPECT_TRUE(LoadTemplates(EncodeTemplates({}), list));
	EXPECT_TRUE(list.empty());
}

namespace
{
/// Two templates of no turns at all, the first of 90 keypoints and the second of `secondCount`, with the second's
/// samples' y set to 1, 2, 3... and both flags of allow reverse at `allowReverse`
std::vector<uint8_t> LongTemplates(uint8_t secondCount, uint32_t allowReverse)
{
	std::vector<GestureData> source(2);
	for (auto& tpl : source)
	{
		tpl.gesture = k_Circle;
		tpl.positionMode = 2;
	}
	for (size_t s = 0; s < k_MaxSamples; ++s)
	{
		source[1].samples[s].y = static_cast<float>(s + 1);
	}
	auto bytes = EncodeTemplates(source);
	const auto put = [&bytes](size_t offset, uint32_t value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	for (size_t t = 0; t < 2; ++t)
	{
		const size_t tail = 4 + t * k_JtyRecordBytes + k_MaxSamples * k_JtySampleBytes;
		put(tail, t == 0 ? 90u : secondCount);
		put(tail + 16, allowReverse);
	}
	return bytes;
}
} // namespace

/// A template of more than 80 keypoints (its count is a byte) is kept, and its turns past the 80th are read where the
/// game reads them: the 81st in its own fields (the allow reverse word), the next ones in the next template's object,
/// whose samples start 8 bytes in (a table pointer and a word of 0), so that the 82nd is the next template's first
/// sample's y, and so on. Past the list's last template they cannot be known and are missing
TEST(GestureTemplatesLoader, aTemplateOfMoreThanEightyKeypointsReadsOnIntoTheList)
{
	std::vector<GestureData> list;
	ASSERT_TRUE(LoadTemplates(LongTemplates(83, 0), list));
	ASSERT_EQ(list.size(), 2u);
	EXPECT_EQ(list[0].count, 90);
	ASSERT_EQ(list[0].laterTurns.size(), 10u);
	EXPECT_EQ(std::bit_cast<uint32_t>(list[0].laterTurns[0]), 0u);
	for (size_t k = 1; k < 10; ++k)
	{
		EXPECT_EQ(list[0].laterTurns[k], static_cast<float>(k)) << k;
	}
	EXPECT_EQ(TemplateTurn(list[0], 89).value_or(-1.0f), 9.0f);
	EXPECT_FALSE(TemplateTurn(list[0], 90).has_value());
	// the second: its own fields, then nothing more in the list
	EXPECT_EQ(list[1].count, 83);
	ASSERT_EQ(list[1].laterTurns.size(), 1u);
	EXPECT_EQ(TemplateTurn(list[1], 79).value_or(-1.0f), 0.0f);
	EXPECT_EQ(TemplateTurn(list[1], 80).value_or(-1.0f), 0.0f);
	EXPECT_FALSE(TemplateTurn(list[1], 81).has_value());
	// a set allow reverse word is the 81st turn's bits
	ASSERT_TRUE(LoadTemplates(LongTemplates(83, 1), list));
	EXPECT_EQ(std::bit_cast<uint32_t>(list[0].laterTurns[0]), 1u);
	EXPECT_TRUE(list[0].allowReverse);
}

/// The matcher reads such a template's turns as far as it needs them; a template whose turns run past the list's end
/// is never taken, as what the game would read there cannot be known
TEST(Gestures, aLongTemplateMatchesOnItsLaterTurnsAndOnePastTheListNever)
{
	std::vector<GestureData> list;
	ASSERT_TRUE(LoadTemplates(LongTemplates(83, 0), list));
	// the second's later turns would be 1, 2, ...: give the first the second's samples' y of 0 for this
	auto flat = LongTemplates(83, 0);
	for (size_t s = 0; s < k_MaxSamples; ++s)
	{
		const float zero = 0.0f;
		std::memcpy(flat.data() + 4 + k_JtyRecordBytes + s * k_JtySampleBytes + 4, &zero, sizeof(zero));
	}
	ASSERT_TRUE(LoadTemplates(flat, list));
	GestureData input;
	input.SetToZero();
	input.count = 3; // three keypoints of no turn
	Result result;
	EXPECT_TRUE(Match(list[0], 0, input, result, 1.0f));
	EXPECT_FALSE(Match(list[1], 1, input, result, 1.0f));
	// with the second's samples' y back, the first's 82nd turn is 1 radian, more than the error allowed
	ASSERT_TRUE(LoadTemplates(LongTemplates(83, 0), list));
	EXPECT_FALSE(Match(list[0], 0, input, result, 1.0f));
}

// The resource cache loaders of the gesture assets, on bytes written by the test

TEST(GestureTemplatesLoader, readsTheRecordsOfTheBytes)
{
	std::vector<GestureData> source;
	source.push_back(TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}}, true));
	source.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}}, false));
	const auto list =
	    resources::GestureTemplatesLoader {}(resources::GestureTemplatesLoader::FromBufferTag {}, EncodeTemplates(source));
	ASSERT_NE(list, nullptr);
	ASSERT_EQ(list->size(), 2u);
	EXPECT_EQ((*list)[0].gesture, k_Circle);
	EXPECT_EQ((*list)[0].count, 3);
	EXPECT_TRUE((*list)[0].allowReverse);
	EXPECT_EQ((*list)[1].gesture, k_Spiral);
	EXPECT_EQ((*list)[1].count, 2);
}

TEST(GestureTemplatesLoader, shortBytesGiveAnEmptyList)
{
	const resources::GestureTemplatesLoader loader;
	const auto list = loader(resources::GestureTemplatesLoader::FromBufferTag {}, std::vector<uint8_t>(3, 0));
	ASSERT_NE(list, nullptr);
	EXPECT_TRUE(list->empty());
}

namespace
{
/// A .cam file: u32 file size, u32 (unused), u32 count, then count points of 6 floats (x, y, z, and 3 unused)
std::vector<uint8_t> EncodeShape(const std::vector<glm::vec3>& points)
{
	std::vector<uint8_t> bytes(12 + points.size() * 24, 0);
	const auto put = [&bytes](size_t offset, auto value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	put(0, static_cast<uint32_t>(bytes.size()));
	put(8, static_cast<uint32_t>(points.size()));
	for (size_t i = 0; i < points.size(); ++i)
	{
		put(12 + i * 24, points[i].x);
		put(12 + i * 24 + 4, points[i].y);
		put(12 + i * 24 + 8, points[i].z);
	}
	return bytes;
}
} // namespace

TEST(GestureShapeLoader, readsAShapeOfTheBytes)
{
	// x (-100..100) -> (x + 100) / 200 and z -> (100 - z) / 200: a square from (0, 1) to (1, 0)
	const auto shape = resources::GestureShapeLoader {}(
	    resources::GestureShapeLoader::FromBufferTag {},
	    EncodeShape({{-100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, -100.0f}, {-100.0f, 0.0f, -100.0f}}));
	ASSERT_NE(shape, nullptr);
	EXPECT_EQ(shape->points.size(), 4u);
	EXPECT_FLOAT_EQ(shape->minX, 0.0f);
	EXPECT_FLOAT_EQ(shape->maxX, 1.0f);
	EXPECT_FLOAT_EQ(shape->minZ, 0.0f);
	EXPECT_FLOAT_EQ(shape->maxZ, 1.0f);
	EXPECT_FLOAT_EQ(shape->aspect, 1.0f);
	EXPECT_FLOAT_EQ(shape->length, 3.0f);
}

TEST(GestureShapeLoader, bytesThatAreNotAShapeGiveNull)
{
	const resources::GestureShapeLoader loader;
	EXPECT_EQ(loader(resources::GestureShapeLoader::FromBufferTag {}, std::vector<uint8_t>(11, 0)), nullptr);
	// a count of 0
	EXPECT_EQ(loader(resources::GestureShapeLoader::FromBufferTag {}, EncodeShape({})), nullptr);
	// a count that promises more points than there are
	auto bytes = EncodeShape({{0.0f, 0.0f, 0.0f}});
	bytes.resize(bytes.size() - 1);
	EXPECT_EQ(loader(resources::GestureShapeLoader::FromBufferTag {}, bytes), nullptr);
}

TEST(PowerUpSystem, ThePassStopsInsideTheCitadel)
{
	// inside the citadel the pass only wipes the gesture buffer
	EXPECT_FALSE(PowerUpPassGoesOn({.insideCitadel = true}));
	EXPECT_TRUE(PowerUpPassGoesOn({.insideCitadel = false}));
}

TEST(PowerUpSystem, EachEarlyOutStopsThePassOnItsOwn)
{
	// out in the world, nothing locked, the interface active and no box: the pass goes on
	EXPECT_TRUE(PowerUpPassGoesOn({}));
	// a creature or an object locked for the hand, an inactive interface, a box holding the mouse: each alone stops it
	EXPECT_FALSE(PowerUpPassGoesOn({.lockedSelect = true}));
	EXPECT_FALSE(PowerUpPassGoesOn({.interfaceActive = false}));
	EXPECT_FALSE(PowerUpPassGoesOn({.boxHasMouse = true}));
	EXPECT_FALSE(
	    PowerUpPassGoesOn({.insideCitadel = true, .lockedSelect = true, .interfaceActive = false, .boxHasMouse = true}));
}

TEST(PowerUpSystem, TheScribbleShakesAHeldLeashOff)
{
	constexpr auto k_Me = PlayerNames::PLAYER_ONE;
	constexpr auto k_Other = PlayerNames::PLAYER_TWO;
	const LeashHand inHand {.holder = k_Me};
	const LeashHand tied {.holder = k_Me, .tied = true};
	const LeashHand othersHand {.holder = k_Other};
	const LeashHand none {};

	HandStatus empty;
	HandStatus holding;
	holding.holdingSomething = true;
	holding.inInfluence = true;
	holding.validToShake = true;

	// an object in the hand comes first, whatever the leash
	EXPECT_EQ(ScribbleCancelFor(holding, true, k_Me, inHand), ScribbleCancel::ShakeHeld);
	// one that can't be shaken out, or outside the influence, leaves nothing waiting: neither the leash nor the icon
	HandStatus stuck = holding;
	stuck.validToShake = false;
	EXPECT_EQ(ScribbleCancelFor(stuck, true, k_Me, inHand), ScribbleCancel::None);
	HandStatus outside = holding;
	outside.inInfluence = false;
	EXPECT_EQ(ScribbleCancelFor(outside, true, k_Me, inHand), ScribbleCancel::None);

	// an empty hand with the leash in it shakes the leash off, before any icon charging, with no influence needed
	EXPECT_FALSE(empty.inInfluence);
	EXPECT_EQ(ScribbleCancelFor(empty, false, k_Me, inHand), ScribbleCancel::ShakeLeash);
	EXPECT_EQ(ScribbleCancelFor(empty, true, k_Me, inHand), ScribbleCancel::ShakeLeash);

	// a tied leash stays, and the icon is next
	EXPECT_EQ(ScribbleCancelFor(empty, true, k_Me, tied), ScribbleCancel::CancelIcon);
	EXPECT_EQ(ScribbleCancelFor(empty, false, k_Me, tied), ScribbleCancel::None);

	// a leash in another player's hand, or none, falls through to the icon
	EXPECT_EQ(ScribbleCancelFor(empty, true, k_Me, othersHand), ScribbleCancel::CancelIcon);
	EXPECT_EQ(ScribbleCancelFor(empty, true, k_Me, none), ScribbleCancel::CancelIcon);
	EXPECT_EQ(ScribbleCancelFor(empty, false, k_Me, none), ScribbleCancel::None);
}

namespace
{
constexpr auto k_Me = PlayerNames::PLAYER_ONE;
constexpr auto k_Other = PlayerNames::PLAYER_TWO;
constexpr Gesture k_SquareSpiral = 15;
constexpr Gesture k_VerticalScribble = 7;
constexpr Gesture k_Heart = 13;
constexpr Gesture k_E = 17;

/// info.dat's leash picker: 25 s, opened by the square spiral; by leash type {E, vertical scribble, E, heart}
LeashSelectionTables PickerTables()
{
	return {.timeOut = 25.0f, .start = k_SquareSpiral, .pickGestures = {k_E, k_VerticalScribble, k_E, k_Heart}};
}

/// The player's creature, knowing the evil, rope and good leashes as given, with its leash in my hand or not on
LeashHand CreatureKnowing(std::array<bool, 3> known, bool inMyHand, LeashType picked = LeashType::Rope)
{
	LeashHand leash {.creature = true, .known = known, .picked = picked};
	if (inMyHand)
	{
		leash.holder = k_Me;
	}
	return leash;
}

/// A recognise that answers yes only for the gestures given, and keeps the order it was asked in
struct FakeRecognise
{
	std::vector<Gesture> drawn;
	std::vector<Gesture> asked;
	[[nodiscard]] std::function<bool(Gesture)> Function()
	{
		return [this](Gesture g) {
			asked.push_back(g);
			return std::ranges::find(drawn, g) != drawn.end();
		};
	}
};

/// A picker that is open, as the square spiral leaves it
LeashSelection OpenPicker()
{
	LeashSelection picker;
	Selection selection;
	picker.Open(selection);
	return picker;
}
} // namespace

TEST(LeashPicker, OpensOnlyWithMoreThanOneLeashKnown)
{
	const std::array<bool, 3> ropeOnly {false, true, false};
	const std::array<bool, 3> ropeAndGood {false, true, true};

	// the square spiral is looked for with no leash on and one leash known, or with the leash in my hand and more
	EXPECT_TRUE(LeashStartWanted(CreatureKnowing(ropeOnly, false), k_Me));
	EXPECT_FALSE(LeashStartWanted(CreatureKnowing(ropeOnly, true), k_Me));
	EXPECT_TRUE(LeashStartWanted(CreatureKnowing(ropeAndGood, true), k_Me));
	EXPECT_FALSE(LeashStartWanted(CreatureKnowing({}, false), k_Me));
	auto othersHand = CreatureKnowing(ropeAndGood, false);
	othersHand.holder = k_Other;
	EXPECT_FALSE(LeashStartWanted(othersHand, k_Me));

	// with one leash known it only puts the leash on: the click is sent, and the picker stays closed
	Selection selection;
	LeashSelection picker;
	EXPECT_TRUE(picker.Start(CreatureKnowing(ropeOnly, false), selection));
	EXPECT_FALSE(picker.open);
	EXPECT_FALSE(picker.pending);

	// with two, the click is sent and the picker opens, waiting for the leash to go on
	EXPECT_TRUE(picker.Start(CreatureKnowing(ropeAndGood, false), selection));
	EXPECT_TRUE(picker.open);
	EXPECT_TRUE(picker.pending);

	// with the leash already in my hand, the picker opens with no click and nothing pending
	LeashSelection again;
	again.timer = 3.0f;
	EXPECT_FALSE(again.Start(CreatureKnowing(ropeAndGood, true), selection));
	EXPECT_TRUE(again.open);
	EXPECT_FALSE(again.pending);
	EXPECT_FLOAT_EQ(again.timer, 0.0f);
}

TEST(LeashPicker, OpeningClosesTheMiracleSelection)
{
	Selection selection;
	selection.open = true;
	LeashSelection picker;
	picker.Start(CreatureKnowing({true, true, true}, true), selection);
	EXPECT_TRUE(picker.open);
	EXPECT_FALSE(selection.open);
}

TEST(LeashPicker, APickSendsTheLeashAndCloses)
{
	auto picker = OpenPicker();
	// the creature knows the rope, which it wears, and the good leash; not the evil one
	const auto leash = CreatureKnowing({false, true, true}, true);
	FakeRecognise recognise {.drawn = {k_Heart}};
	LookingForArray lf {};
	for (auto& entry : lf)
	{
		entry = LookingFor {-1, -1, -1, -1};
	}

	const auto outcome = picker.Stage(0.1f, false, PickerTables(), recognise.Function(), leash, k_Me, false, &lf);

	// the pass sends the good leash as the leash type's packet, for the next turn
	EXPECT_EQ(outcome.kind, LeashSelection::Outcome::Kind::Picked);
	EXPECT_EQ(outcome.leash, LeashType::Good);
	EXPECT_FALSE(picker.open);
	// the scribble first, then only the leashes known and not worn: the evil one is unknown, the rope is worn
	EXPECT_EQ(recognise.asked, (std::vector<Gesture> {k_Scribble, k_Heart}));
	EXPECT_EQ(lf[k_Scribble].type, 0);
	EXPECT_EQ(lf[k_Heart].type, 6);
	EXPECT_EQ(lf[k_Heart].leash, 3);
	EXPECT_EQ(lf[k_VerticalScribble].type, -1);
	EXPECT_EQ(lf[k_E].type, -1);

	// the evil leash, once known, is offered as its own gesture
	auto second = OpenPicker();
	FakeRecognise evil {.drawn = {k_VerticalScribble}};
	const auto knowsEvil = CreatureKnowing({true, true, false}, true);
	const auto picked = second.Stage(0.1f, false, PickerTables(), evil.Function(), knowsEvil, k_Me, false, &lf);
	EXPECT_EQ(picked.leash, LeashType::Evil);
	EXPECT_EQ(lf[k_VerticalScribble].type, 4);
	EXPECT_EQ(lf[k_VerticalScribble].leash, 1);
}

TEST(LeashPicker, AScribbleClosesItBeforeAnyShake)
{
	auto picker = OpenPicker();
	const auto leash = CreatureKnowing({true, true, true}, true);
	FakeRecognise recognise {.drawn = {k_Scribble, k_Heart, k_VerticalScribble}};

	const auto outcome = picker.Stage(0.1f, false, PickerTables(), recognise.Function(), leash, k_Me, false);

	// the picker takes the scribble and the pass ends, so the leash in the hand is not shaken off
	EXPECT_EQ(outcome.kind, LeashSelection::Outcome::Kind::Cancelled);
	EXPECT_FALSE(picker.open);
	EXPECT_EQ(recognise.asked, (std::vector<Gesture> {k_Scribble}));
	EXPECT_EQ(ScribbleCancelFor(HandStatus {}, false, k_Me, leash), ScribbleCancel::ShakeLeash);
}

TEST(LeashPicker, TwentyFiveSecondsIsStrictAndThePassGoesOn)
{
	const auto leash = CreatureKnowing({true, true, true}, true);
	auto picker = OpenPicker();
	picker.timer = 24.5f;
	FakeRecognise nothing;

	// exactly 25 s: still open
	EXPECT_EQ(picker.Stage(0.5f, false, PickerTables(), nothing.Function(), leash, k_Me, false).kind,
	          LeashSelection::Outcome::Kind::GoesOn);
	EXPECT_FLOAT_EQ(picker.timer, 25.0f);
	EXPECT_TRUE(picker.open);

	// past it, closed; that frame still looks for the leashes, and the rest of the pass runs
	EXPECT_EQ(picker.Stage(0.25f, false, PickerTables(), nothing.Function(), leash, k_Me, false).kind,
	          LeashSelection::Outcome::Kind::GoesOn);
	EXPECT_FALSE(picker.open);
	EXPECT_EQ(nothing.asked.size(), 6u); // the scribble and the two leashes not worn, twice

	// and a leash drawn that frame is still picked
	auto late = OpenPicker();
	late.timer = 25.0f;
	FakeRecognise heart {.drawn = {k_Heart}};
	const auto outcome = late.Stage(0.25f, false, PickerTables(), heart.Function(), leash, k_Me, false);
	EXPECT_EQ(outcome.kind, LeashSelection::Outcome::Kind::Picked);
	EXPECT_EQ(outcome.leash, LeashType::Good);
}

TEST(LeashPicker, APausedTimerStands)
{
	const auto leash = CreatureKnowing({true, true, true}, true);
	auto picker = OpenPicker();
	picker.timer = 24.5f;
	FakeRecognise nothing;
	picker.Stage(10.0f, true, PickerTables(), nothing.Function(), leash, k_Me, false);
	EXPECT_FLOAT_EQ(picker.timer, 24.5f);
	EXPECT_TRUE(picker.open);
}

TEST(LeashPicker, NoCreatureClosesItAndEndsThePass)
{
	auto picker = OpenPicker();
	FakeRecognise nothing;
	EXPECT_EQ(picker.Stage(0.1f, false, PickerTables(), nothing.Function(), LeashHand {}, k_Me, false).kind,
	          LeashSelection::Outcome::Kind::NoCreature);
	EXPECT_FALSE(picker.open);
	EXPECT_TRUE(nothing.asked.empty());
}

TEST(LeashPicker, ThePendingLeashSurvivesUntilItsTurn)
{
	Selection selection;
	LeashSelection picker;
	const auto notOnYet = CreatureKnowing({false, true, true}, false);
	ASSERT_TRUE(picker.Start(notOnYet, selection));
	ASSERT_TRUE(picker.pending);

	// until the turn the leash is not on: the picker stays, and looks for nothing, so the rest of the pass runs
	FakeRecognise recognise {.drawn = {k_Heart, k_Scribble}};
	for (int frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(picker.Active(notOnYet, k_Me));
		EXPECT_EQ(picker.Stage(0.1f, false, PickerTables(), recognise.Function(), notOnYet, k_Me, false).kind,
		          LeashSelection::Outcome::Kind::GoesOn);
	}
	EXPECT_TRUE(recognise.asked.empty());

	// the click's packet handler clears the pending leash before it works the leash key
	ecs::creature_hand_packets::ApplyLeashClick(nullptr, k_Me, &picker);
	EXPECT_FALSE(picker.pending);
	// the leash went on: the picker stays while it is in my hand
	EXPECT_TRUE(picker.Active(CreatureKnowing({false, true, true}, true), k_Me));
	// had it not gone on, the picker closes
	EXPECT_FALSE(picker.Active(notOnYet, k_Me));
	EXPECT_FALSE(picker.open);
}

TEST(LeashPicker, ClosesWithoutACreatureOrWithTheLeashOff)
{
	auto picker = OpenPicker();
	EXPECT_FALSE(picker.Active(LeashHand {}, k_Me));
	auto off = OpenPicker();
	EXPECT_FALSE(off.Active(CreatureKnowing({true, true, true}, false), k_Me));
	// a tied leash is still on, so the picker stays
	auto tied = OpenPicker();
	auto leash = CreatureKnowing({true, true, true}, true);
	leash.tied = true;
	EXPECT_TRUE(tied.Active(leash, k_Me));
}

TEST(LeashPicker, AChargingSeedOrTheMiracleSelectionClosesIt)
{
	const auto leash = CreatureKnowing({true, true, true}, true);

	// a charging seed closes both
	Selection selection;
	selection.open = true;
	auto picker = OpenPicker();
	EXPECT_EQ(FirstStage(selection, picker, true, true, leash, k_Me), GestureStage::PowerUp);
	EXPECT_FALSE(selection.open);
	EXPECT_FALSE(picker.open);

	// the miracle selection open with the hand ready closes the picker
	selection.open = true;
	picker = OpenPicker();
	EXPECT_EQ(FirstStage(selection, picker, false, true, leash, k_Me), GestureStage::Selection);
	EXPECT_TRUE(selection.open);
	EXPECT_FALSE(picker.open);

	// a hand that is not ready closes the miracle selection, and the picker runs
	selection.open = true;
	picker = OpenPicker();
	EXPECT_EQ(FirstStage(selection, picker, false, false, leash, k_Me), GestureStage::LeashPicker);
	EXPECT_FALSE(selection.open);
	EXPECT_TRUE(picker.open);

	// neither open: no stage
	picker.open = false;
	EXPECT_EQ(FirstStage(selection, picker, false, true, leash, k_Me), GestureStage::None);
}

TEST(LeashPicker, TheCreaturesFightWithItsControlsUpComesFirstAndClosesBoth)
{
	const auto leash = CreatureKnowing({true, true, true}, true);

	// fighting with the fight controls up: both close, before a charging seed is looked at
	Selection selection;
	selection.open = true;
	auto picker = OpenPicker();
	EXPECT_EQ(FirstStage(selection, picker, true, true, leash, k_Me, {.fighting = true, .controlsUp = true}),
	          GestureStage::Fight);
	EXPECT_FALSE(selection.open);
	EXPECT_FALSE(picker.open);

	// the move button held: the path is wiped and nothing closes
	selection.open = true;
	picker = OpenPicker();
	EXPECT_EQ(FirstStage(selection, picker, false, true, leash, k_Me, {.fighting = true, .controlsUp = true, .moveHeld = true}),
	          GestureStage::FightMoveHeld);
	EXPECT_TRUE(selection.open);
	EXPECT_TRUE(picker.open);

	// fighting with the controls down, or the controls up without a fight: the usual stages
	EXPECT_EQ(FirstStage(selection, picker, false, true, leash, k_Me, {.fighting = true}), GestureStage::Selection);
	EXPECT_FALSE(picker.open);
	picker = OpenPicker();
	selection.open = false;
	EXPECT_EQ(FirstStage(selection, picker, false, true, leash, k_Me, {.controlsUp = true, .moveHeld = true}),
	          GestureStage::LeashPicker);
	EXPECT_TRUE(picker.open);
}

TEST(LeashPicker, TheFightIsTheLocalPlayersCreaturesDuel)
{
	using test::creature_loop_fakes::FakeFight;
	using test::creature_loop_fakes::FakeLeash;
	test::creature_loop_fakes::CallLog log;
	auto& leashes = static_cast<FakeLeash&>(Locator::leashSystem::emplace<FakeLeash>(log));
	auto& fights = static_cast<FakeFight&>(Locator::creatureFightSystem::emplace<FakeFight>(log));
	const auto mine = static_cast<entt::entity>(4);
	const auto other = static_cast<entt::entity>(9);

	// no creature: no fight, even with another creature duelling
	fights.duelling = other;
	EXPECT_FALSE(CurrentFight().fighting);

	// the player's creature not duelling, then duelling; with no camera watching it the fight controls stay down
	leashes.playersCreature = mine;
	EXPECT_FALSE(CurrentFight().fighting);
	fights.duelling = mine;
	const auto fight = CurrentFight();
	EXPECT_TRUE(fight.fighting);
	EXPECT_FALSE(fight.controlsUp);
	EXPECT_FALSE(fight.moveHeld);

	Locator::creatureFightSystem::reset();
	Locator::leashSystem::reset();
}

TEST(LeashPicker, ASquareWaveStartsTheSecretScriptOnlyOnTheFirstSinglePlayerLand)
{
	// the first land of a single player game that is not a playground
	EXPECT_TRUE(SecretScriptLand(1, false, false));
	EXPECT_FALSE(SecretScriptLand(2, false, false));
	EXPECT_FALSE(SecretScriptLand(0, false, false));
	EXPECT_FALSE(SecretScriptLand(1, true, false));
	EXPECT_FALSE(SecretScriptLand(1, false, true));

	const auto leash = CreatureKnowing({true, true, true}, true);
	LookingForArray lf {};
	for (auto& entry : lf)
	{
		entry = LookingFor {-1, -1, -1, -1};
	}
	auto picker = OpenPicker();
	FakeRecognise wave {.drawn = {k_SquareWave}};
	const auto outcome = picker.Stage(0.1f, false, PickerTables(), wave.Function(), leash, k_Me, true, &lf);
	EXPECT_TRUE(outcome.startSecretScript);
	// the square wave is not shown among the gestures looked for; the leashes still are
	EXPECT_EQ(lf[k_SquareWave].type, -1);
	EXPECT_EQ(lf[k_VerticalScribble].type, 4);
	EXPECT_EQ(lf[k_Heart].type, 6);

	// elsewhere it is not even looked for
	auto elsewhere = OpenPicker();
	FakeRecognise other {.drawn = {k_SquareWave}};
	const auto ignored = elsewhere.Stage(0.1f, false, PickerTables(), other.Function(), leash, k_Me, false);
	EXPECT_FALSE(ignored.startSecretScript);
	EXPECT_EQ(other.asked, (std::vector<Gesture> {k_Scribble, k_VerticalScribble, k_Heart}));
}

TEST(LeashPicker, ThePassGoesOnAfterTheSquareWave)
{
	const auto leash = CreatureKnowing({true, true, true}, true);
	auto picker = OpenPicker();
	FakeRecognise recognise {.drawn = {k_SquareWave, k_Heart}};
	const auto outcome = picker.Stage(0.1f, false, PickerTables(), recognise.Function(), leash, k_Me, true);

	// the rest of the pass runs, the picker stays open, and the wiped buffer picks no leash
	EXPECT_EQ(outcome.kind, LeashSelection::Outcome::Kind::GoesOn);
	EXPECT_TRUE(outcome.startSecretScript);
	EXPECT_TRUE(picker.open);
	EXPECT_EQ(recognise.asked, (std::vector<Gesture> {k_Scribble, k_SquareWave}));

	// a scribble comes first: it closes the picker, and the square wave is not looked for
	auto cancelled = OpenPicker();
	FakeRecognise both {.drawn = {k_Scribble, k_SquareWave}};
	const auto closed = cancelled.Stage(0.1f, false, PickerTables(), both.Function(), leash, k_Me, true);
	EXPECT_EQ(closed.kind, LeashSelection::Outcome::Kind::Cancelled);
	EXPECT_FALSE(closed.startSecretScript);
}

TEST(LeashPicker, ASquareWaveOutsideThePickerDoesNothing)
{
	const auto leash = CreatureKnowing({true, true, true}, true);

	// a closed picker runs no stage of its own
	Selection selection;
	LeashSelection closed;
	EXPECT_EQ(FirstStage(selection, closed, false, true, leash, k_Me), GestureStage::None);

	// an open picker with the leash not in my hand looks for nothing
	auto picker = OpenPicker();
	FakeRecognise wave {.drawn = {k_SquareWave}};
	const auto off = CreatureKnowing({true, true, true}, false);
	const auto outcome = picker.Stage(0.1f, false, PickerTables(), wave.Function(), off, k_Me, true);
	EXPECT_EQ(outcome.kind, LeashSelection::Outcome::Kind::GoesOn);
	EXPECT_FALSE(outcome.startSecretScript);
	EXPECT_TRUE(wave.asked.empty());
}

namespace
{
/// Three samples and a template of position mode 2, and the box PacketFromResult takes from them
struct ModeTwo
{
	ModeTwo()
	{
		system.AddSample(glm::vec3(1.0f), {100, 200});
		system.AddSample(glm::vec3(1.0f), {161, 250});
		system.AddSample(glm::vec3(1.0f), {220, 330});
		list[0].gesture = k_Circle;
		list[0].positionMode = 2;
		int first = 0;
		int last = 0;
		system.KeypointIndices(result.start, result.end, first, last);
		const auto box = system.Box(first, last);
		centre = {box.minX + (box.maxX - box.minX + 1.0f) * 0.5f, box.minZ + (box.maxZ - box.minZ + 1.0f) * 0.5f};
		width = std::max(box.maxX - box.minX + 1.0f, box.maxZ - box.minZ + 1.0f);
	}

	GestureSystem system;
	std::vector<GestureData> list = std::vector<GestureData>(1);
	Result result {.gesture = k_Circle, .start = 0, .end = 1};
	glm::vec2 centre {0.0f};
	float width {0.0f};
};
} // namespace

TEST(GesturePacket, ModeTwoTakesTheLandUnderTheCentresPixelAndTheEdgePointAtItsDistance)
{
	const ModeTwo two;
	const glm::vec3 camera(1000.0f, 300.0f, 900.0f);
	const glm::vec3 land(1100.0f, 20.0f, 1200.0f);
	std::optional<glm::ivec2> landPixel;
	std::optional<std::pair<glm::ivec2, float>> along;
	Projection projection;
	projection.screenToLand = [&](glm::ivec2 pixel) {
		landPixel = pixel;
		return std::optional(land);
	};
	projection.alongPixel = [&](glm::ivec2 pixel, float length) {
		along = {pixel, length};
		return camera + glm::vec3(length * 0.6f, length * -0.8f, 0.0f);
	};
	projection.cameraPosition = camera;
	projection.yawAxis = {0.8f, 0.6f};
	const auto packet = PacketFromResult(two.list, two.system, two.result, projection);
	// the centre's pixel, truncated
	ASSERT_TRUE(landPixel.has_value());
	EXPECT_EQ(*landPixel, glm::ivec2(two.centre));
	// the distance from the camera, summed z, y, x, and the edge pixel half the box's width to the right
	const glm::vec3 dp = land - camera;
	const float distance = std::sqrt((dp.z * dp.z + dp.y * dp.y) + dp.x * dp.x);
	ASSERT_TRUE(along.has_value());
	EXPECT_EQ(along->first, glm::ivec2(glm::vec2(two.centre.x + two.width * 0.5f, two.centre.y)));
	EXPECT_EQ(along->second, distance);
	// the point is the land; the size is 1.05 times the two points' distance apart along the camera's yaw
	EXPECT_EQ(packet.position, land);
	const glm::vec3 dq = (camera + glm::vec3(distance * 0.6f, distance * -0.8f, 0.0f)) - camera;
	const float alongQ = dq.x * 0.8f + dq.z * 0.6f;
	const float alongP = dp.x * 0.8f + dp.z * 0.6f;
	EXPECT_EQ(packet.size, std::abs(alongQ - alongP) * 1.05f);
}

TEST(GesturePacket, ModeTwoOffTheLandKeepsTheCameraPositionAtDistanceZero)
{
	const ModeTwo two;
	const glm::vec3 camera(1000.0f, 300.0f, 900.0f);
	std::optional<float> length;
	Projection projection;
	projection.screenToLand = [](glm::ivec2) { return std::optional<glm::vec3>(); };
	projection.alongPixel = [&](glm::ivec2, float l) {
		length = l;
		return camera;
	};
	projection.cameraPosition = camera;
	const auto packet = PacketFromResult(two.list, two.system, two.result, projection);
	EXPECT_EQ(packet.position, camera);
	ASSERT_TRUE(length.has_value());
	EXPECT_EQ(*length, 0.0f);
	EXPECT_EQ(packet.size, 0.0f);
}

namespace
{
/// A table of gesture icons with nothing looked for, as each pass starts it
LookingForArray FreshTable()
{
	LookingForArray lf;
	lf.fill(LookingFor {0, -1, -1, -1});
	return lf;
}

/// Worship icons that can always be asked, for the selection's stages
struct AlwaysIcons final: IconProvider
{
	int requested = -1;
	[[nodiscard]] bool AnyRequestableIconOfCategory(Gesture /*category*/) const override { return true; }
	void ForEachRequestableIcon(Gesture /*category*/, const std::function<void(int seedType)>& visit) const override
	{
		visit(4);
	}
	[[nodiscard]] bool IconValidForRequest(int /*seedType*/) const override { return true; }
	void RequestSpell(int seedType) override { requested = seedType; }
};

/// The selection opened by the spiral on seed 4, whose gesture is FORK_RIGHT (9)
Selection OpenSelection(AlwaysIcons& icons)
{
	SelectionTables tables;
	tables.selectionGesture = [](int seed) { return seed == 4 ? k_Spiral : k_None; };
	tables.gesture = [](int seed) { return seed == 4 ? Gesture(9) : k_None; };
	tables.gestureStage2 = [](int /*seed*/) { return k_None; };
	Selection selection;
	selection.Open(k_Spiral, icons, tables);
	return selection;
}
} // namespace

TEST(PowerUpStage, AScribbleWithAPowerUpAskedForPowersDownAndKeepsTheSeed)
{
	auto lf = FreshTable();
	FakeRecognise drawn {.drawn = {k_Scribble}};
	const auto outcome = PowerUpStage(Gesture(1), {Gesture(1), Gesture(2), k_None}, 6, true, drawn.Function(), lf);
	EXPECT_EQ(outcome.kind, PowerUpStageOutcome::Kind::PoweredDown);
	// its icon shows the power down with the seed; nothing is looked for after it
	EXPECT_EQ(lf[k_Scribble].type, 0xD);
	EXPECT_EQ(lf[k_Scribble].seed, 6);
	EXPECT_EQ(lf[k_Scribble].powerUp, -1);
	EXPECT_EQ(drawn.asked, (std::vector<Gesture> {k_Scribble}));
}

TEST(PowerUpStage, AScribbleWithNoPowerUpAskedForShakesTheSeedOutWhereverTheHandIs)
{
	auto lf = FreshTable();
	FakeRecognise drawn {.drawn = {k_Scribble}};
	// no influence enters the stage: the seed goes back to its icon even outside the player's influence
	const auto outcome = PowerUpStage(k_None, {Gesture(1), k_None, k_None}, 6, true, drawn.Function(), lf);
	EXPECT_EQ(outcome.kind, PowerUpStageOutcome::Kind::ShakenOut);
	EXPECT_EQ(lf[k_Scribble].type, 0);
	EXPECT_EQ(lf[k_Scribble].seed, -1);
}

TEST(PowerUpStage, EveryLevelButTheOneAskedForIsLookedForInOrder)
{
	auto lf = FreshTable();
	FakeRecognise nothing;
	const std::array<Gesture, 3> powerUps {Gesture(1), Gesture(2), Gesture(3)};
	EXPECT_EQ(PowerUpStage(Gesture(1), powerUps, 6, true, nothing.Function(), lf).kind, PowerUpStageOutcome::Kind::None);
	EXPECT_EQ(nothing.asked, (std::vector<Gesture> {k_Scribble, Gesture(2), Gesture(3)}));
	// each one's icon: 0xE + the level, with the seed and the level
	EXPECT_EQ(lf[1].type, 0);
	EXPECT_EQ(lf[2].type, 0xF);
	EXPECT_EQ(lf[2].seed, 6);
	EXPECT_EQ(lf[2].powerUp, 1);
	EXPECT_EQ(lf[3].type, 0x10);
	EXPECT_EQ(lf[3].powerUp, 2);

	FakeRecognise level2 {.drawn = {Gesture(3)}};
	const auto outcome = PowerUpStage(Gesture(1), powerUps, 6, true, level2.Function(), lf);
	EXPECT_EQ(outcome.kind, PowerUpStageOutcome::Kind::PoweredUp);
	EXPECT_EQ(outcome.level, 2);
	EXPECT_EQ(outcome.gesture, Gesture(3));
}

TEST(PowerUpStage, APowerUpIsActedOnOnlyForASeedFromAnIcon)
{
	auto lf = FreshTable();
	FakeRecognise drawn {.drawn = {Gesture(2)}};
	EXPECT_EQ(PowerUpStage(k_None, {Gesture(2), k_None, k_None}, 6, false, drawn.Function(), lf).kind,
	          PowerUpStageOutcome::Kind::None);
}

TEST(RepeatStage, RIsLookedForOnlyWithTheHandReadyAndNothingElseOpen)
{
	constexpr Gesture k_R = k_RShape;
	EXPECT_TRUE(RepeatLookedFor(false, k_R, false, true, true));
	EXPECT_FALSE(RepeatLookedFor(true, k_R, false, true, true)) << "the creature fights";
	EXPECT_FALSE(RepeatLookedFor(false, k_None, false, true, true)) << "no R in the tables";
	EXPECT_FALSE(RepeatLookedFor(false, k_R, true, true, true)) << "the selection is open";
	EXPECT_FALSE(RepeatLookedFor(false, k_R, false, false, true)) << "the hand is not ready";
	EXPECT_FALSE(RepeatLookedFor(false, k_R, false, true, false)) << "nothing to repeat";
}

TEST(RepeatStage, TheRShowsTheLastMiracleAndRepeatsItWhenDrawn)
{
	auto lf = FreshTable();
	FakeRecognise nothing;
	EXPECT_FALSE(RepeatStage(k_RShape, 7, nothing.Function(), lf));
	EXPECT_EQ(lf[k_RShape].type, 0xA);
	EXPECT_EQ(lf[k_RShape].seed, 7);
	FakeRecognise drawn {.drawn = {k_RShape}};
	EXPECT_TRUE(RepeatStage(k_RShape, 7, drawn.Function(), lf));
}

TEST(Selection, ThirtySecondsCloseItThoughThatStageStillLooks)
{
	AlwaysIcons icons;
	auto selection = OpenSelection(icons);
	ASSERT_TRUE(selection.open);
	FakeRecognise nothing;
	// the paused frames add nothing
	EXPECT_EQ(selection.Stage(0.0f, 30.0f, nothing.Function(), icons), Selection::Outcome::None);
	selection.timer = 30.0f;
	EXPECT_EQ(selection.Stage(0.0f, 30.0f, nothing.Function(), icons), Selection::Outcome::None);
	EXPECT_TRUE(selection.open) << "30 s is not past the time out";
	EXPECT_EQ(selection.Stage(0.05f, 30.0f, nothing.Function(), icons), Selection::Outcome::None);
	EXPECT_FALSE(selection.open);

	// the stage that closes it still takes the miracle's gesture
	selection = OpenSelection(icons);
	selection.timer = 30.0f;
	FakeRecognise fork {.drawn = {Gesture(9)}};
	EXPECT_EQ(selection.Stage(0.05f, 30.0f, fork.Function(), icons), Selection::Outcome::Requested);
	EXPECT_EQ(icons.requested, 4);
}

TEST(Selection, AScribbleClosesIt)
{
	AlwaysIcons icons;
	auto selection = OpenSelection(icons);
	auto lf = FreshTable();
	FakeRecognise drawn {.drawn = {k_Scribble, Gesture(9)}};
	EXPECT_EQ(selection.Stage(0.1f, 30.0f, drawn.Function(), icons, &lf), Selection::Outcome::Cancelled);
	EXPECT_FALSE(selection.open);
	EXPECT_EQ(icons.requested, -1) << "the scribble comes before the miracle's gesture";
	EXPECT_EQ(drawn.asked, (std::vector<Gesture> {k_Scribble}));
}

TEST(GestureBuffer, OffTheLandTheNewestLandPointIsKept)
{
	GestureSystem system;
	// nothing recorded yet: a sample off the land is not taken
	system.AddSampleAtLastWorld({10, 20});
	EXPECT_EQ(system.Count(), 0);
	const glm::vec3 land(100.0f, 5.0f, 200.0f);
	system.AddSample(land, {10, 20});
	system.AddSampleAtLastWorld({40, 25});
	ASSERT_EQ(system.Count(), 2);
	EXPECT_EQ(system.At(1).world, land);
	EXPECT_FLOAT_EQ(system.At(1).sx, 40.0f);
	EXPECT_FLOAT_EQ(system.At(1).sz, 25.0f);
}

TEST(PowerUpSystem, AMovingCameraWipesThePathUnlessItShakes)
{
	EXPECT_FALSE(CameraMoveWipesPath(false, false, false));
	EXPECT_TRUE(CameraMoveWipesPath(true, false, false));
	// a camera shake going on keeps the path, however the camera moves
	EXPECT_FALSE(CameraMoveWipesPath(true, true, false));
	// so does a camera mode that lets the player gesture while it moves
	EXPECT_FALSE(CameraMoveWipesPath(true, false, true));
}
