/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// magic::falling_spell (src/Magic/Objects/FallingSpell.h): what the falling spell draws besides the film fall.bik,
// without the GPU: the sparks (init, draw, as sprites), the light bursts (init, draw and their callback), the model
// light kept and put back, the fall.cm2 camera path, and the copy of the creature's tint, glows and clock.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Magic/Objects/FallingSpell.h"
#include "Magic/Objects/FallingSpellCreature.h"

using namespace openblack;
using namespace openblack::magic::falling_spell;
using FallCamera = openblack::magic::falling_spell::Camera; // not openblack::Camera

namespace
{
struct Fake
{
	glm::vec3 light {1.0f, 2.0f, 3.0f};
	std::optional<glm::ivec2> centre;
};

FallingSpell Make(Fake& fake)
{
	FallingSpell::Hooks hooks;
	hooks.light = [&fake]() { return fake.light; };
	hooks.setLight = [&fake](const glm::vec3& position) { fake.light = position; };
	hooks.burstCentre = [&fake](int, int) { return fake.centre; };
	return FallingSpell(std::move(hooks));
}

std::vector<uint8_t> PathBytes(uint32_t duration, const std::vector<float>& xs)
{
	const auto count = static_cast<uint32_t>(xs.size());
	std::vector<uint8_t> bytes(0xC + 0x48 * xs.size(), 0);
	const uint32_t size = static_cast<uint32_t>(bytes.size());
	std::memcpy(bytes.data(), &size, 4);
	std::memcpy(bytes.data() + 4, &duration, 4);
	std::memcpy(bytes.data() + 8, &count, 4);
	for (size_t i = 0; i < xs.size(); ++i)
	{
		std::array<float, 18> key {};
		key[0] = xs[i];        // position x
		key[3] = 2.0f * xs[i]; // focus x
		key[6] = 3.0f * xs[i]; // matrix 0
		std::memcpy(bytes.data() + 0xC + 0x48 * i, key.data(), 0x48);
	}
	return bytes;
}

std::optional<std::filesystem::path> GamePath()
{
	for (const char* name : {"OPENBLACK_TEST_GAME_PATH", "OPENBLACK_TEST_BW_ROOT"})
	{
		if (const char* value = std::getenv(name); value != nullptr && *value != '\0')
		{
			return std::filesystem::path(value);
		}
	}
	return std::nullopt;
}
} // namespace

TEST(FallingSpell, BurstIndicesAreTheFan)
{
	const auto indices = BurstIndices();
	EXPECT_EQ(indices[0], 0);
	EXPECT_EQ(indices[1], 1);
	EXPECT_EQ(indices[2], 3);
	// the last spoke closes on the first rim point (the index wraps at 64)
	EXPECT_EQ(indices[3 * 63], 126);
	EXPECT_EQ(indices[3 * 63 + 1], 127);
	EXPECT_EQ(indices[3 * 63 + 2], 1);
}

TEST(FallingSpell, BurstGeometry)
{
	LightBurst burst;
	burst.radius.fill(1.0f);
	burst.wobble.fill(0.0f);
	burst.phase.fill(0.0f);
	burst.rate.fill(0.0f);
	// shape = 1: k = 0, every spoke reaches the full radius at i pi / 32 + phase (sin to x, cos to y)
	const auto fan = DrawBurst(burst, 100.0f, 50.0f, 10.0f, 0.25f, 1.0f, 0x800020FFu);
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		const auto& centre = fan.vertices.at(static_cast<size_t>(2 * i));
		const auto& rim = fan.vertices.at(static_cast<size_t>(2 * i + 1));
		EXPECT_EQ(centre.pixel, glm::vec2(100.0f, 50.0f));
		const float theta = static_cast<float>(i) * 0.09817477f + 0.25f;
		EXPECT_EQ(rim.pixel.x, static_cast<float>(static_cast<int32_t>(10.0f * std::sin(theta) + 100.0f)));
		EXPECT_EQ(rim.pixel.y, static_cast<float>(static_cast<int32_t>(10.0f * std::cos(theta) + 50.0f)));
		EXPECT_EQ(centre.argb, 0x800020FFu);
		EXPECT_EQ(rim.argb, 0x80000000u); // the rim keeps the alpha only
		EXPECT_FLOAT_EQ(centre.uv.x, 0.5f + 0.25f * static_cast<float>(i) / 64.0f);
		EXPECT_FLOAT_EQ(centre.uv.x, rim.uv.x);
		EXPECT_FLOAT_EQ(centre.uv.y, 0.25f + 0.25f * 0.1f);
	}
	// shape = 0: k = 1 and sin(0) = 0: every spoke has no length, the fan is a point
	const auto none = DrawBurst(burst, 100.0f, 50.0f, 10.0f, 0.0f, 0.0f, 0xFF000000u);
	for (const auto& vertex : none.vertices)
	{
		EXPECT_EQ(vertex.pixel, glm::vec2(100.0f, 50.0f));
	}
}

TEST(FallingSpell, BurstInitRanges)
{
	game_random::testing::ScopedState state;
	LightBurst burst;
	burst.Init();
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		EXPECT_GE(burst.radius.at(i), 0.5f * 0.71428573f);
		EXPECT_LE(burst.radius.at(i), 1.5f * 1.4f);
		EXPECT_GE(std::fabs(burst.rate.at(i)), 2.0f);
		EXPECT_LE(std::fabs(burst.rate.at(i)), 20.0f);
		EXPECT_LE(std::fabs(burst.wobble.at(i)), 0.8f);
		EXPECT_GE(burst.phase.at(i), 0.0f);
		EXPECT_LE(burst.phase.at(i), 6.2831855f);
	}
}

TEST(FallingSpell, LightKeptAndPutBack)
{
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	EXPECT_TRUE(spell.IsActive());
	spell.Draw(16, 640, 480);
	EXPECT_EQ(fake.light, k_CreatureLight);
	spell.Close();
	EXPECT_FALSE(spell.IsActive());
	EXPECT_EQ(fake.light, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(FallingSpell, TheCreaturesTintTakesThreeLocalDrawsOnlyWhileItIsDrawn)
{
	Fake fake;
	std::vector<int32_t> ranges;
	FallingSpell::Hooks hooks;
	hooks.setLight = [&fake](const glm::vec3& position) { fake.light = position; };
	bool creature = true;
	hooks.hasCreature = [&creature]() { return creature; };
	hooks.localRand = [&ranges](int32_t range) {
		ranges.push_back(range);
		return 0u;
	};
	FallingSpell spell(std::move(hooks));
	spell.Init(std::nullopt);
	// drawn: red of 32, green of 8, blue of 4, in that order, with the sparks still off
	spell.AdvanceCreature(1000);
	spell.Draw(16, 640, 480);
	EXPECT_FALSE(spell.SparksOn());
	EXPECT_EQ(ranges, (std::vector<int32_t> {32, 8, 4}));
	ASSERT_TRUE(spell.CreatureTintNow().has_value());
	EXPECT_EQ(*spell.CreatureTintNow(), TintOf(CreatureTintLevel(1000), Flicker {}));
	// hidden: no draws and no tint
	ranges.clear();
	spell.AdvanceCreature(20000);
	spell.Draw(16, 640, 480);
	EXPECT_TRUE(ranges.empty());
	EXPECT_FALSE(spell.CreatureTintNow().has_value());
	// no creature: the same
	creature = false;
	spell.AdvanceCreature(30000);
	spell.Draw(16, 640, 480);
	EXPECT_TRUE(ranges.empty());
	EXPECT_FALSE(spell.CreatureTintNow().has_value());
	// closed: none
	creature = true;
	spell.Draw(16, 640, 480);
	spell.Close();
	EXPECT_FALSE(spell.CreatureTintNow().has_value());
}

TEST(FallingSpell, SparksInit)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	EXPECT_FALSE(spell.SparksOn());
	for (const auto& spark : spell.Sparks())
	{
		EXPECT_GE(spark.x, -0.1f);
		EXPECT_LE(spark.x, 1.1f);
		EXPECT_GE(spark.depth, 5.0f);
		EXPECT_LE(spark.depth, 25.0f);
		EXPECT_GE(spark.size, 4.0f);
		EXPECT_LE(spark.size, 8.0f);
		EXPECT_FLOAT_EQ(spark.shrink, 2.0f - spark.y);
		EXPECT_EQ(spark.age, 0.0f);
		const uint32_t v = (spark.rgb >> 8) & 0xFFu;
		EXPECT_GE(v, 16u);
		EXPECT_LE(v, 100u);
		EXPECT_EQ(spark.rgb >> 16, std::min(2 * v, 0xFFu));
		EXPECT_EQ(spark.rgb & 0xFFu, v / 3);
	}
}

TEST(FallingSpell, SparksOnlyAfterTheUpdateSetsThem)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	spell.Draw(100, 640, 480);
	EXPECT_TRUE(spell.SparkQuads(640, 480, 1.0f).empty());
	EXPECT_EQ(spell.Sparks()[0].age, 0.0f); // Draw skips them
}

TEST(FallingSpell, SparksOnScreen)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	spell.StartSparks();
	const auto before = spell.Sparks();
	spell.Draw(100, 640, 480);
	EXPECT_TRUE(spell.SparksOn());
	const auto quads = spell.SparkQuads(640, 480, 1.0f);
	ASSERT_EQ(quads.size(), static_cast<size_t>(k_SparkCount));
	const float tanHalf = std::tan(k_FallFov * 0.5f);
	const float age = 100.0f * 0.0013f;
	// each quad is centred on its spark's truncated pixel, half width size' 320 / (T depth) on both axes
	for (int i = 0; i < k_SparkCount; ++i)
	{
		const auto& spark = before.at(static_cast<size_t>(i));
		const auto& sprite = spell.Sprites().at(static_cast<size_t>(i));
		EXPECT_FLOAT_EQ(spell.Sparks().at(static_cast<size_t>(i)).age, age);
		EXPECT_EQ(sprite.argb >> 24, 0xFFu); // 255 - 32 x 0 (the age before the step)
		EXPECT_FLOAT_EQ(sprite.size, ((spark.size - spark.shrink * age * 0.5f) + 1.0f) * 0.75f);
		EXPECT_FLOAT_EQ(sprite.angle, spark.spin * age + static_cast<float>(i));
		EXPECT_EQ(sprite.cell, 1); // trunc(0.13 x 8)
		bool found = false;
		for (const auto& quad : quads)
		{
			const glm::vec2 centre = (quad[0].pixel + quad[2].pixel) * 0.5f;
			const glm::vec2 want(static_cast<float>(static_cast<int32_t>(640.0f * spark.x)),
			                     static_cast<float>(static_cast<int32_t>(480.0f * spark.y)));
			if (glm::length(centre - want) < 0.01f && quad[0].argb == sprite.argb)
			{
				found = true;
				const float half = sprite.size * 320.0f / (tanHalf * spark.depth);
				EXPECT_NEAR(glm::length(quad[1].pixel - quad[0].pixel), 2.0f * half, 0.01f);
				EXPECT_NEAR(glm::length(quad[3].pixel - quad[0].pixel), 2.0f * half, 0.01f);
			}
		}
		EXPECT_TRUE(found) << i;
		// y moves by (spin + 1) (1 - y) dt 0.1
		EXPECT_FLOAT_EQ(spell.Sparks().at(static_cast<size_t>(i)).y,
		                spark.y + (spark.spin + 1.0f) * (1.0f - spark.y) * age * 0.1f);
	}
	// far to near (sorted by |pos - camera|^2): the keys of the quads' sprites never grow
	float last = 1e30f;
	for (const auto& quad : quads)
	{
		const glm::vec2 centre = (quad[0].pixel + quad[2].pixel) * 0.5f;
		for (int i = 0; i < k_SparkCount; ++i)
		{
			const auto& spark = before.at(static_cast<size_t>(i));
			const glm::vec2 want(static_cast<float>(static_cast<int32_t>(640.0f * spark.x)),
			                     static_cast<float>(static_cast<int32_t>(480.0f * spark.y)));
			if (glm::length(centre - want) < 0.01f)
			{
				const auto& sprite = spell.Sprites().at(static_cast<size_t>(i));
				const float key = glm::dot(sprite.position, sprite.position);
				EXPECT_LE(key, last * 1.0001f);
				last = key;
				break;
			}
		}
	}
	// the near plane: past every depth nothing is drawn
	EXPECT_TRUE(spell.SparkQuads(640, 480, 30.0f).empty());
}

TEST(FallingSpell, SparksFadeAndStop)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	spell.StartSparks();
	// 255 / 32 = 7.97 of age: 6131 ms at 0.0013 a ms
	for (int frame = 0; frame < 6100 / 50; ++frame)
	{
		spell.Draw(50, 640, 480);
	}
	EXPECT_TRUE(spell.SparksOn());
	for (int frame = 0; frame < 10; ++frame)
	{
		spell.Draw(50, 640, 480);
	}
	EXPECT_FALSE(spell.SparksOn()); // none seen
	EXPECT_TRUE(spell.SparkQuads(640, 480, 1.0f).empty());
}

TEST(FallingSpell, BurstsNeedStateTwoAndACentre)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	fake.centre = glm::ivec2(320, 240);
	spell.FinishFrame(100, 1, 640, 480);
	EXPECT_TRUE(spell.Bursts().empty());
	fake.centre.reset();
	spell.FinishFrame(100, 2, 640, 480);
	EXPECT_TRUE(spell.Bursts().empty());
	EXPECT_EQ(spell.BurstAlpha(), 0.0f); // no step without the centre
	fake.centre = glm::ivec2(320, 240);
	spell.FinishFrame(1000, 2, 640, 480);
	ASSERT_EQ(spell.Bursts().size(), 4u);
	EXPECT_FLOAT_EQ(spell.BurstAlpha(), 0.07f);
	EXPECT_FLOAT_EQ(spell.BurstShape(), 0.175f);
	EXPECT_FLOAT_EQ(spell.BurstGrow(), 1.0f);
	EXPECT_EQ(spell.Bursts()[0].vertices[0].argb, (17u << 24) | 0x0020FF40u); // trunc(0.07 x 255) = 17
	EXPECT_EQ(spell.Bursts()[3].vertices[0].argb, (17u << 24) | 0x00FF4040u);
	for (int i = 0; i < 2; ++i)
	{
		spell.FinishFrame(1000, 2, 640, 480);
	}
	EXPECT_FLOAT_EQ(spell.BurstShape(), 0.525f);
	EXPECT_FLOAT_EQ(spell.BurstGrow(), 4.5f); // past 0.5: + 5 x 0.7
}

TEST(FallingSpell, CameraPathFraction)
{
	// 4 keys over 10 ms: step 3.333, its integer part 3
	const auto path = CameraPath::Parse(PathBytes(10, {0.0f, 1.0f, 2.0f, 3.0f}));
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path->keys.size(), 4u);
	EXPECT_EQ(path->At(0).position.x, 0.0f);
	// t = 5: key 3 x 5 / 10 = 1, k = trunc(1.5) = 1, remainder 5 - 3 = 2, f = 0.6
	EXPECT_FLOAT_EQ(path->At(5).position.x, 1.0f * 0.4f + 2.0f * 0.6f);
	EXPECT_FLOAT_EQ(path->At(5).focus.x, 2.0f * 1.6f);
	EXPECT_FLOAT_EQ(path->At(5).matrix[0], 3.0f * 1.6f);
	// past the duration: duration - 1 = 9, key 2, k = 2, remainder 3, f = 0.9
	EXPECT_FLOAT_EQ(path->At(50).position.x, 2.0f * 0.1f + 3.0f * 0.9f);
	EXPECT_FALSE(CameraPath::Parse({1, 2, 3}).has_value());
	EXPECT_FALSE(CameraPath::Parse(PathBytes(10, {})).has_value());
}

TEST(FallingSpell, CameraPathDriftsLikeTheOriginal)
{
	// fall.cm2's shape: 1449 keys over 48333 ms, the key's index as its x
	std::vector<float> xs(1449);
	for (size_t i = 0; i < xs.size(); ++i)
	{
		xs[i] = static_cast<float>(i);
	}
	const auto path = CameraPath::Parse(PathBytes(48333, xs));
	ASSERT_TRUE(path.has_value());
	// t = 48000: key 1438, k = trunc(48000 / 33.379) = 1438, remainder 48000 - 1438 x 33 = 546, f = 16.36
	const float step = 48333.0f / 1448.0f;
	const float f = 546.0f / step;
	EXPECT_GT(f, 16.0f);
	EXPECT_FLOAT_EQ(path->At(48000).position.x, 1438.0f * (1.0f - f) + 1439.0f * f);
}

TEST(FallingSpell, CameraFollowsTheFilm)
{
	Fake fake;
	auto spell = Make(fake);
	spell.Init(CameraPath::Parse(PathBytes(10, {0.0f, 1.0f, 2.0f, 3.0f})));
	spell.UpdateCamera(5);
	ASSERT_TRUE(spell.CameraNow().has_value());
	EXPECT_FLOAT_EQ(spell.CameraNow()->position.x, (0.4f + 1.2f) * k_PathScale);
	EXPECT_FLOAT_EQ(spell.CameraNow()->focus.x, 3.2f * k_PathScale);
	EXPECT_FLOAT_EQ(spell.CameraNow()->matrix[0], 4.8f); // the matrix is not scaled
	EXPECT_FLOAT_EQ(spell.CameraNow()->fov, k_FallFov);
	spell.Close();
	EXPECT_FALSE(spell.CameraNow().has_value());
}

TEST(FallingSpell, CameraAppliedEveryUpdateAndGivenBackAtClose)
{
	FallingSpell::Hooks hooks;
	std::vector<std::optional<FallCamera>> applied;
	hooks.applyCamera = [&applied](const std::optional<FallCamera>& camera) { applied.push_back(camera); };
	FallingSpell spell(std::move(hooks));
	spell.UpdateCamera(5); // not open: nothing
	EXPECT_TRUE(applied.empty());
	spell.Init(CameraPath::Parse(PathBytes(10, {0.0f, 1.0f, 2.0f, 3.0f})));
	spell.UpdateCamera(5);
	spell.UpdateCamera(6);
	ASSERT_EQ(applied.size(), 2u);
	ASSERT_TRUE(applied[1].has_value());
	EXPECT_FLOAT_EQ(applied[1]->position.x, spell.CameraNow()->position.x);
	EXPECT_FLOAT_EQ(applied[1]->fov, k_FallFov);
	spell.Close();
	ASSERT_EQ(applied.size(), 3u);
	EXPECT_FALSE(applied[2].has_value()); // the game camera back
	spell.Close();                        // closed: nothing
	EXPECT_EQ(applied.size(), 3u);
}

TEST(FallingSpell, WorldToCameraTransposesAndNormalisesTheRows)
{
	// (a0, a3, -a6, a1, a4, -a7, a2, a5, -a8), each row normalised, translation -(column . position)
	FallCamera camera;
	camera.position = {1.0f, 2.0f, 3.0f};
	camera.matrix = {2.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 4.0f, 7.0f, 8.0f, 9.0f};
	const glm::mat4 view = WorldToCamera(camera);
	EXPECT_EQ(view[0], glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(view[1], glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
	EXPECT_EQ(view[2], glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
	EXPECT_EQ(view[3], glm::vec4(-1.0f, -2.0f, 3.0f, 1.0f)); // m9..m11 of the path are not read
	EXPECT_EQ(glm::vec3(view * glm::vec4(camera.position, 1.0f)), glm::vec3(0.0f));
	// rows: right +z, up +y, back +x: a point ahead (-x) is at +z in camera space, +z is to the right
	camera.position = glm::vec3(0.0f);
	camera.matrix = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	const glm::mat4 turned = WorldToCamera(camera);
	EXPECT_EQ(glm::vec3(turned * glm::vec4(-5.0f, 0.0f, 0.0f, 1.0f)), glm::vec3(0.0f, 0.0f, 5.0f));
	EXPECT_EQ(glm::vec3(turned * glm::vec4(0.0f, 0.0f, 2.0f, 1.0f)), glm::vec3(2.0f, 0.0f, 0.0f));
}

// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(FallingSpell, RealFallCm2)
{
	const auto root = GamePath();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	std::ifstream file(*root / "Data" / "Spells" / "fall" / "fall.cm2", std::ios::binary);
	ASSERT_TRUE(file.good());
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	EXPECT_EQ(bytes.size(), 104340u);
	const auto path = CameraPath::Parse(bytes);
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path->duration, 48333u);
	EXPECT_EQ(path->keys.size(), 1449u);
	// the first key, bytes 12..20 of the file
	EXPECT_FLOAT_EQ(path->keys[0].position.x, 8.2829723f);
	EXPECT_EQ(path->At(0).position, path->keys[0].position);
}

/// The checks of RealFallCm2 on a small path built here: the header's duration and key count, each key read in
/// order, and the start of the path is its first key
TEST(FallingSpell, RealFallCm2Synthetic)
{
	const std::vector<float> xs {0.5f, 1.5f, 2.5f};
	const auto bytes = PathBytes(40, xs);
	// a 12-byte header and 72 bytes per key
	EXPECT_EQ(bytes.size(), 0xCu + 0x48u * xs.size());
	const auto path = CameraPath::Parse(bytes);
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path->duration, 40u);
	ASSERT_EQ(path->keys.size(), xs.size());
	for (size_t i = 0; i < xs.size(); ++i)
	{
		EXPECT_FLOAT_EQ(path->keys[i].position.x, xs[i]) << i;
		EXPECT_FLOAT_EQ(path->keys[i].focus.x, 2.0f * xs[i]) << i;
		EXPECT_FLOAT_EQ(path->keys[i].matrix[0], 3.0f * xs[i]) << i;
	}
	EXPECT_EQ(path->At(0).position, path->keys[0].position);
}

namespace
{
/// The camera path's 18 output floats (position, focus, matrix) as the original gives them, by their bits
struct PathCase
{
	bool longPath; ///< fall.cm2's shape (1449 keys over 48 333 ms) or the small one (7 keys over 200 ms)
	uint32_t ms;
	std::array<uint32_t, 18> bits;
};
// clang-format off
constexpr auto k_PathCases = std::to_array<PathCase>({
    {false, 0u, {0xC0400000u, 0xC0380000u, 0xC0300000u, 0xC0280000u, 0xC0200000u, 0xC0180000u, 0xC0100000u, 0xC0080000u, 0xC0000000u, 0xBFF00000u, 0xBFE00000u, 0xBFD00000u, 0xBFC00000u, 0xBFB00000u, 0xBFA00000u, 0xBF900000u, 0xBF800000u, 0xBF600000u}},
    {false, 1u, {0xC03BAE15u, 0xC033AE15u, 0xC02BAE15u, 0xC023AE15u, 0xC01BAE15u, 0xC013AE15u, 0xC00BAE15u, 0xC003AE14u, 0xBFF75C29u, 0xBFE75C29u, 0xBFD75C29u, 0xBFC75C2Au, 0xBFB75C29u, 0xBFA75C29u, 0xBF975C2Au, 0xBF875C29u, 0xBF6EB852u, 0xBF4EB852u}},
    {false, 16u, {0xBFF5C28Fu, 0xBFE5C28Eu, 0xBFD5C28Fu, 0xBFC5C290u, 0xBFB5C28Fu, 0xBFA5C28Eu, 0xBF95C28Fu, 0xBF85C290u, 0xBF6B851Eu, 0xBF4B851Eu, 0xBF2B851Eu, 0xBF0B851Eu, 0xBED70A3Cu, 0xBE970A3Cu, 0xBE2E1478u, 0xBD3851E0u, 0x3DA3D710u, 0x3E51EB88u}},
    {false, 33u, {0xBF45C28Fu, 0xBF25C28Eu, 0xBF05C28Fu, 0xBECB851Eu, 0xBE8B851Eu, 0xBE170A3Cu, 0xBCB851E0u, 0x3DD1EB88u, 0x3E68F5C4u, 0x3EB47AE2u, 0x3EF47AE2u, 0x3F1A3D70u, 0x3F3A3D71u, 0x3F5A3D72u, 0x3F7A3D71u, 0x3F8D1EB8u, 0x3F9D1EB8u, 0x3FAD1EB9u}},
    {false, 34u, {0xBF2EB852u, 0xBF0EB853u, 0xBEDD70A4u, 0xBE9D70A4u, 0xBE3AE148u, 0xBD6B8520u, 0x3D8A3D71u, 0x3E451EB9u, 0x3EA28F5Cu, 0x3EE28F5Cu, 0x3F1147AEu, 0x3F3147AFu, 0x3F5147AEu, 0x3F7147AEu, 0x3F88A3D7u, 0x3F98A3D8u, 0x3FA8A3D8u, 0x3FB8A3D7u}},
    {false, 50u, {0x3ECB851Eu, 0x3F05C290u, 0x3F25C28Fu, 0x3F45C28Eu, 0x3F65C28Fu, 0x3F82E148u, 0x3F92E148u, 0x3FA2E147u, 0x3FB2E148u, 0x3FC2E148u, 0x3FD2E147u, 0x3FE2E147u, 0x3FF2E148u, 0x400170A4u, 0x400970A4u, 0x401170A4u, 0x401970A4u, 0x402170A4u}},
    {false, 99u, {0x406E8F5Cu, 0x40768F5Cu, 0x407E8F5Cu, 0x408347AFu, 0x408747AEu, 0x408B47AEu, 0x408F47AEu, 0x409347AEu, 0x409747AEu, 0x409B47AEu, 0x409F47AEu, 0x40A347AEu, 0x40A747AEu, 0x40AB47AEu, 0x40AF47AEu, 0x40B347AEu, 0x40B747AEu, 0x40BB47AEu}},
    {false, 100u, {0x407451ECu, 0x407C51ECu, 0x408228F6u, 0x408628F6u, 0x408A28F6u, 0x408E28F6u, 0x409228F6u, 0x409628F6u, 0x409A28F6u, 0x409E28F6u, 0x40A228F7u, 0x40A628F6u, 0x40AA28F6u, 0x40AE28F7u, 0x40B228F6u, 0x40B628F6u, 0x40BA28F6u, 0x40BE28F6u}},
    {false, 150u, {0x40E6E148u, 0x40EAE148u, 0x40EEE148u, 0x40F2E148u, 0x40F6E148u, 0x40FAE148u, 0x40FEE148u, 0x410170A4u, 0x410370A4u, 0x410570A4u, 0x410770A4u, 0x410970A4u, 0x410B70A4u, 0x410D70A4u, 0x410F70A4u, 0x411170A4u, 0x411370A4u, 0x411570A4u}},
    {false, 166u, {0x4104B852u, 0x4106B852u, 0x4108B852u, 0x410AB852u, 0x410CB852u, 0x410EB852u, 0x4110B852u, 0x4112B852u, 0x4114B852u, 0x4116B852u, 0x4118B852u, 0x411AB852u, 0x411CB852u, 0x411EB852u, 0x4120B852u, 0x4122B851u, 0x4124B852u, 0x4126B852u}},
    {false, 199u, {0x4128B852u, 0x412AB852u, 0x412CB852u, 0x412EB852u, 0x4130B851u, 0x4132B852u, 0x4134B852u, 0x4136B852u, 0x4138B852u, 0x413AB852u, 0x413CB852u, 0x413EB852u, 0x4140B852u, 0x4142B851u, 0x4144B852u, 0x4146B852u, 0x4148B852u, 0x414AB852u}},
    {false, 200u, {0x41280000u, 0x412A0000u, 0x412C0000u, 0x412E0000u, 0x41300000u, 0x41320000u, 0x41340000u, 0x41360000u, 0x41380000u, 0x413A0000u, 0x413C0000u, 0x413E0000u, 0x41400000u, 0x41420000u, 0x41440000u, 0x41460000u, 0x41480000u, 0x414A0000u}},
    {false, 201u, {0x4128B852u, 0x412AB852u, 0x412CB852u, 0x412EB852u, 0x4130B851u, 0x4132B852u, 0x4134B852u, 0x4136B852u, 0x4138B852u, 0x413AB852u, 0x413CB852u, 0x413EB852u, 0x4140B852u, 0x4142B851u, 0x4144B852u, 0x4146B852u, 0x4148B852u, 0x414AB852u}},
    {false, 250u, {0x4128B852u, 0x412AB852u, 0x412CB852u, 0x412EB852u, 0x4130B851u, 0x4132B852u, 0x4134B852u, 0x4136B852u, 0x4138B852u, 0x413AB852u, 0x413CB852u, 0x413EB852u, 0x4140B852u, 0x4142B851u, 0x4144B852u, 0x4146B852u, 0x4148B852u, 0x414AB852u}},
    {false, 4294967295u, {0x4128B852u, 0x412AB852u, 0x412CB852u, 0x412EB852u, 0x4130B851u, 0x4132B852u, 0x4134B852u, 0x4136B852u, 0x4138B852u, 0x413AB852u, 0x413CB852u, 0x413EB852u, 0x4140B852u, 0x4142B851u, 0x4144B852u, 0x4146B852u, 0x4148B852u, 0x414AB852u}},
    {true, 0u, {0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {true, 2900u, {0x42AFB702u, 0x432FB702u, 0xC2AFB702u, 0x422FB702u, 0x422FB702u, 0x422FB702u, 0x42AFB702u, 0x432FB702u, 0x4383C942u, 0x43AFB702u, 0x43DBA4C5u, 0x4403C942u, 0x4419C022u, 0x442FB702u, 0x4445ADE4u, 0x445BA4C5u, 0x44719BA5u, 0x4483C942u}},
    {true, 3000u, {0x42B5C65Au, 0x4335C65Au, 0xC2B5C65Au, 0x4235C65Au, 0x4235C65Au, 0x4235C65Au, 0x42B5C65Au, 0x4335C65Au, 0x438854C3u, 0x43B5C65Au, 0x43E337F1u, 0x440854C3u, 0x441F0D8Eu, 0x4435C65Au, 0x444C7F25u, 0x446337F1u, 0x4479F0BDu, 0x448854C3u}},
    {true, 10000u, {0x43977E06u, 0x44177E06u, 0xC3977E06u, 0x43177E06u, 0x43177E06u, 0x43177E06u, 0x43977E06u, 0x44177E06u, 0x44633D08u, 0x44977E06u, 0x44BD5D84u, 0x44E33D08u, 0x45048E44u, 0x45177E06u, 0x452A6DC6u, 0x453D5D84u, 0x45504D44u, 0x45633D08u}},
    {true, 33333u, {0x447C7D10u, 0x44FC7D10u, 0xC47C7D10u, 0x43FC7D10u, 0x43FC7D10u, 0x43FC7D10u, 0x447C7D10u, 0x44FC7D10u, 0x453D5DC0u, 0x457C7D10u, 0x459DCE28u, 0x45BD5DC0u, 0x45DCED60u, 0x45FC7D10u, 0x460E0658u, 0x461DCE28u, 0x462D95F8u, 0x463D5DC0u}},
    {true, 48000u, {0x44B5CB70u, 0x4535CB70u, 0xC4B5CB70u, 0x4435CB70u, 0x4435CB70u, 0x4435CB70u, 0x44B5CB70u, 0x4535CB70u, 0x458858A0u, 0x45B5CB70u, 0x45E33E50u, 0x460858A0u, 0x461F1200u, 0x4635CB70u, 0x464C84E0u, 0x46633E50u, 0x4679F7C0u, 0x468858A0u}},
    {true, 48332u, {0x44B70D00u, 0x45370D00u, 0xC4B70D00u, 0x44370D00u, 0x44370D00u, 0x44370D00u, 0x44B70D00u, 0x45370D00u, 0x458949C0u, 0x45B70D00u, 0x45E4D040u, 0x460949C0u, 0x46202B60u, 0x46370D00u, 0x464DEEA0u, 0x4664D040u, 0x467BB1F0u, 0x468949C0u}},
    {true, 48333u, {0x44B50000u, 0x45350000u, 0xC4B50000u, 0x44350000u, 0x44350000u, 0x44350000u, 0x44B50000u, 0x45350000u, 0x4587C000u, 0x45B50000u, 0x45E24000u, 0x4607C000u, 0x461E6000u, 0x46350000u, 0x464BA000u, 0x46624000u, 0x4678E000u, 0x4687C000u}},
    {true, 60000u, {0x44B70D00u, 0x45370D00u, 0xC4B70D00u, 0x44370D00u, 0x44370D00u, 0x44370D00u, 0x44B70D00u, 0x45370D00u, 0x458949C0u, 0x45B70D00u, 0x45E4D040u, 0x460949C0u, 0x46202B60u, 0x46370D00u, 0x464DEEA0u, 0x4664D040u, 0x467BB1F0u, 0x468949C0u}},
});
// clang-format on

std::vector<uint8_t> KeysBytes(uint32_t duration, const std::vector<std::array<float, 18>>& keys)
{
	const auto count = static_cast<uint32_t>(keys.size());
	std::vector<uint8_t> bytes(0xC + 0x48 * keys.size(), 0);
	const uint32_t size = static_cast<uint32_t>(bytes.size());
	std::memcpy(bytes.data(), &size, 4);
	std::memcpy(bytes.data() + 4, &duration, 4);
	std::memcpy(bytes.data() + 8, &count, 4);
	for (size_t i = 0; i < keys.size(); ++i)
	{
		std::memcpy(bytes.data() + 0xC + 0x48 * i, keys[i].data(), 0x48);
	}
	return bytes;
}

/// 7 keys over 200 ms: float j of key i is (18 i + j) / 8 - 3, every value exact
std::vector<std::array<float, 18>> SmallKeys()
{
	std::vector<std::array<float, 18>> keys(7);
	for (size_t i = 0; i < keys.size(); ++i)
	{
		for (size_t j = 0; j < 18; ++j)
		{
			keys[i][j] = static_cast<float>(18 * i + j) * 0.125f - 3.0f;
		}
	}
	return keys;
}

/// 1449 keys: position (i, 2i, -i), focus (i / 2) x 3, matrix k = i (k + 1)
std::vector<std::array<float, 18>> LongKeys()
{
	std::vector<std::array<float, 18>> keys(1449);
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const auto f = static_cast<float>(i);
		keys[i] = {f, 2.0f * f, -f, 0.5f * f, 0.5f * f, 0.5f * f};
		for (size_t k = 0; k < 12; ++k)
		{
			keys[i][6 + k] = static_cast<float>(i * (k + 1));
		}
	}
	return keys;
}

/// The original's draw of the copy at a film time with a local seed: whether it is hidden, its colour and specular,
/// the seed after, and the power each glow is given (k_NoGlow: none)
constexpr uint32_t k_NoGlow = 0xFFFFFFFFu;
struct TintCase
{
	int32_t ms;
	uint32_t seed;
	bool hidden;
	uint32_t colour;
	uint32_t specular;
	uint32_t seedAfter;
	uint32_t first;
	uint32_t third;
};
// clang-format off
constexpr auto k_TintCases = std::to_array<TintCase>({
    {0, 0x0088F89Fu, false, 0xFFFFFFFFu, 0xFF000000u, 0x47184E08u, k_NoGlow, k_NoGlow},
    {1, 0x00000001u, false, 0xFFFFFFFFu, 0xFF000000u, 0x9610FC57u, k_NoGlow, k_NoGlow},
    {5000, 0x0088F89Fu, false, 0xFFDECEBEu, 0xFF0D0300u, 0x47184E08u, k_NoGlow, k_NoGlow},
    {10000, 0x12345678u, false, 0xFFBE9D7Cu, 0xFF1E0700u, 0x8CE90B59u, k_NoGlow, k_NoGlow},
    {10000, 0xDEADBEEFu, false, 0xFFBE9D7Cu, 0xFF1B0401u, 0x024604BBu, k_NoGlow, k_NoGlow},
    {16350, 0x0088F89Fu, false, 0xFF945F29u, 0xFF2D0A00u, 0x47184E08u, k_NoGlow, k_NoGlow},
    {17000, 0x7FFFFFFFu, false, 0xFF905921u, 0xFF1B0702u, 0x0900039Bu, 0x40133334u, k_NoGlow},
    {19000, 0x12345678u, false, 0xFF834506u, 0xFF390D00u, 0x8CE90B59u, 0x3FACCCCDu, 0x400CCCCDu},
    {19449, 0xDEADBEEFu, false, 0xFF804101u, 0xFF350802u, 0x024604BBu, k_NoGlow, 0x3FE0418Au},
    {19450, 0x00000001u, false, 0xFFFFFFFFu, 0xFF000000u, 0x9610FC57u, k_NoGlow, 0x3FE00000u},
    {19500, 0x0088F89Fu, false, 0xFFFEFEFEu, 0xFF000000u, 0x47184E08u, k_NoGlow, 0x3FD9999Au},
    {19549, 0x12345678u, false, 0xFFFDFCFCu, 0xFF000000u, 0x8CE90B59u, k_NoGlow, 0x3FD374BCu},
    {19550, 0x0088F89Fu, true, 0x00000000u, 0x00000000u, 0x0088F89Fu, k_NoGlow, k_NoGlow},
    {27350, 0x00000001u, true, 0x00000000u, 0x00000000u, 0x00000001u, k_NoGlow, k_NoGlow},
    {27351, 0xDEADBEEFu, false, 0xFFFFFFFFu, 0xFF000000u, 0x024604BBu, k_NoGlow, k_NoGlow},
    {32000, 0x0088F89Fu, false, 0xFFFFFFFFu, 0xFF000000u, 0x47184E08u, 0x3FD9999Au, k_NoGlow},
    {37499, 0x7FFFFFFFu, false, 0xFFFFFFFFu, 0xFF000000u, 0x0900039Bu, k_NoGlow, 0x403FDF3Cu},
    {45000, 0x12345678u, false, 0xFFFFFFFFu, 0xFF000000u, 0x8CE90B59u, k_NoGlow, k_NoGlow},
    {-1, 0x0088F89Fu, false, 0xFFFFFFFFu, 0xFF000000u, 0x47184E08u, k_NoGlow, k_NoGlow},
    {-20000, 0x00000001u, false, 0xFFFFFFFFu, 0xFF000000u, 0x9610FC57u, k_NoGlow, k_NoGlow},
});
// clang-format on
} // namespace

/// The path's key at a film time, bit for bit with the original's sampler: the drifting fraction, the clamp past the
/// duration (an unsigned compare: 0xFFFFFFFF too) and the last key held
TEST(FallingSpell, CameraPathMatchesTheOriginalBits)
{
	const auto small = CameraPath::Parse(KeysBytes(200, SmallKeys()));
	const auto big = CameraPath::Parse(KeysBytes(48333, LongKeys()));
	ASSERT_TRUE(small.has_value());
	ASSERT_TRUE(big.has_value());
	for (const auto& c : k_PathCases)
	{
		const auto key = (c.longPath ? *big : *small).At(c.ms);
		std::array<float, 18> got {key.position.x, key.position.y, key.position.z, key.focus.x, key.focus.y, key.focus.z};
		std::copy(key.matrix.begin(), key.matrix.end(), got.begin() + 6);
		for (size_t i = 0; i < got.size(); ++i)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(got[i]), c.bits[i]) << (c.longPath ? "long " : "small ") << c.ms << " #" << i;
		}
	}
}

/// The copy's tint and its three local draws a frame, as the original's draw gives them
TEST(FallingSpell, CreatureTintMatchesTheOriginal)
{
	for (const auto& c : k_TintCases)
	{
		uint32_t seed = c.seed;
		const auto tint = CreatureTintAt(
		    c.ms, [&seed](int32_t range) { return game_random::SeededRandom(static_cast<uint32_t>(range), seed); });
		EXPECT_EQ(CreatureHidden(c.ms), c.hidden) << c.ms;
		EXPECT_EQ(tint.has_value(), !c.hidden) << c.ms;
		if (tint.has_value())
		{
			EXPECT_EQ(tint->colour, c.colour) << c.ms;
			EXPECT_EQ(tint->specular, c.specular) << c.ms;
		}
		// hidden: no draw
		EXPECT_EQ(seed, c.seedAfter) << c.ms;
	}
}

/// The tint's level and colours at the ends of its ramps
TEST(FallingSpell, CreatureTintLevel)
{
	EXPECT_EQ(CreatureTintLevel(0), 0);
	EXPECT_EQ(CreatureTintLevel(-500), 0);
	EXPECT_EQ(CreatureTintLevel(10000), 131); // 2 550 000 / 19 450
	EXPECT_EQ(CreatureTintLevel(19449), 254);
	EXPECT_EQ(CreatureTintLevel(19450), 0); // the second ramp starts again
	EXPECT_EQ(CreatureTintLevel(27349), 254);
	EXPECT_EQ(CreatureTintLevel(27350), 0);
	EXPECT_EQ(TintOf(255, {.red = 31, .green = 7, .blue = 3}), (CreatureTint {.colour = 0xFF804000u, .specular = 0xFF3E0E02u}));
	EXPECT_EQ(TintOf(0, {.red = 31, .green = 7, .blue = 3}), CreatureTint {});
}

/// The hand glows' power in their windows, as the original's draw gives them
TEST(FallingSpell, GlowPowerMatchesTheOriginal)
{
	for (const auto& c : k_TintCases)
	{
		if (c.hidden)
		{
			continue;
		}
		for (const auto& [glow, bits] : {std::pair {FallGlow::First, c.first}, std::pair {FallGlow::Third, c.third}})
		{
			const auto power = GlowPower(glow, c.ms);
			EXPECT_EQ(power.has_value(), bits != k_NoGlow) << c.ms;
			if (power.has_value())
			{
				EXPECT_EQ(std::bit_cast<uint32_t>(*power), bits) << c.ms;
			}
		}
	}
	// a window's first ms gives nothing, its last the power before the end
	EXPECT_FALSE(GlowPower(FallGlow::First, 16350).has_value());
	EXPECT_EQ(GlowPower(FallGlow::First, 17350), 3.0f);
	EXPECT_EQ(GlowPower(FallGlow::First, 19349), 1.002f);
	EXPECT_FALSE(GlowPower(FallGlow::First, 19350).has_value());
	EXPECT_FALSE(GlowPower(FallGlow::First, 32650).has_value());
	EXPECT_EQ(GlowPower(FallGlow::Third, 18200), 3.0f);
	EXPECT_FALSE(GlowPower(FallGlow::Third, 20200).has_value());
	EXPECT_FALSE(GlowPower(FallGlow::Third, 37500).has_value());
}

/// The copy's clock: only forward steps advance it
TEST(FallingSpell, CreatureAdvance)
{
	EXPECT_EQ(CreatureAdvance(k_CreatureStartMs, 141), 41);
	EXPECT_FALSE(CreatureAdvance(141, 141).has_value());
	EXPECT_FALSE(CreatureAdvance(141, 100).has_value());
}

/// The copy's animations: at its size's rate, truncated, until its time is the film's, then the film's ms
TEST(FallingSpell, CreatureAnimationMs)
{
	// 1.6 less half the size times 0.85: 1.175 at size 1, 1.3875 at size 0.5, 0.75 at size 2
	EXPECT_EQ(CreatureAnimationMs(1, 1.0f, false), 1);
	EXPECT_EQ(CreatureAnimationMs(1, 2.0f, false), 0);
	EXPECT_EQ(CreatureAnimationMs(1000, 1.0f, false), 1175);
	EXPECT_EQ(CreatureAnimationMs(1000, 0.5f, false), 1387);
	EXPECT_EQ(CreatureAnimationMs(1000, 2.0f, false), 750);
	for (const float size : {0.5f, 1.0f, 2.0f})
	{
		EXPECT_EQ(CreatureAnimationMs(k_CreatureFirstStepMs, size, true), k_CreatureFirstStepMs);
		EXPECT_EQ(CreatureAnimationMs(41, size, true), 41);
	}
}

namespace
{
/// What the spell asked of the falling creature, in order: 'm' made, 'f' freed, and each advance's ms
struct CreatureLog
{
	std::vector<char> calls;
	std::vector<int32_t> advances;
};

FallingSpell MakeWithCreature(Fake& fake, CreatureLog& log)
{
	FallingSpell::Hooks hooks;
	hooks.light = [&fake]() { return fake.light; };
	hooks.setLight = [&fake](const glm::vec3& position) { fake.light = position; };
	hooks.burstCentre = [&fake](int, int) { return fake.centre; };
	hooks.makeCreature = [&log]() { log.calls.push_back('m'); };
	hooks.freeCreature = [&log]() { log.calls.push_back('f'); };
	hooks.advanceCreature = [&log](int32_t ms) {
		log.calls.push_back('a');
		log.advances.push_back(ms);
	};
	return FallingSpell(std::move(hooks));
}
} // namespace

/// Init makes the falling creature and starts its clock at 100 ms; each update moves it on by the film's ms since the
/// last one when that is more than 0; Close frees it, once
TEST(FallingSpell, TheFallingCreatureIsMadeAtInitMovedOnByTheFilmAndFreedAtClose)
{
	const game_random::testing::ScopedState state;
	Fake fake;
	CreatureLog log;
	auto spell = MakeWithCreature(fake, log);

	// nothing before Init
	spell.AdvanceCreature(500);
	EXPECT_TRUE(log.calls.empty());

	spell.Init(std::nullopt);
	EXPECT_EQ(log.calls, std::vector<char>({'m'}));
	EXPECT_EQ(spell.CreatureMs(), k_CreatureStartMs);

	// the first update moves it on by the film's ms less 100, even without a camera path
	spell.AdvanceCreature(141);
	// no time, or back in time: not moved, though the last update's ms are taken
	spell.AdvanceCreature(141);
	spell.AdvanceCreature(120);
	EXPECT_EQ(spell.CreatureMs(), 120);
	spell.AdvanceCreature(160);
	EXPECT_EQ(log.advances, std::vector<int32_t>({41, 40}));

	// Init over an open one closes it first: freed, then made again with its clock back at 100
	spell.Init(std::nullopt);
	EXPECT_EQ(log.calls, std::vector<char>({'m', 'a', 'a', 'f', 'm'}));
	EXPECT_EQ(spell.CreatureMs(), k_CreatureStartMs);

	spell.Close();
	spell.Close();
	spell.AdvanceCreature(1000);
	EXPECT_EQ(log.calls, std::vector<char>({'m', 'a', 'a', 'f', 'm', 'f'}));
}

namespace
{
/// What each Draw asked of the falling creature, in order: 'l' the model light, 'p' made ready to be drawn, 's' / 'n'
/// the body that sparkles or nobody, 'e' its eyes placed
FallingSpell MakeDrawingCreature(Fake& fake, std::vector<char>& calls)
{
	FallingSpell::Hooks hooks;
	hooks.light = [&fake]() { return fake.light; };
	hooks.setLight = [&fake, &calls](const glm::vec3& position) {
		fake.light = position;
		calls.push_back('l');
	};
	hooks.burstCentre = [&fake](int, int) { return fake.centre; };
	hooks.prepareCreature = [&calls]() { calls.push_back('p'); };
	hooks.setCreatureSparkling = [&calls](bool sparkling) { calls.push_back(sparkling ? 's' : 'n'); };
	hooks.placeCreatureEyes = [&calls]() { calls.push_back('e'); };
	return FallingSpell(std::move(hooks));
}
} // namespace

/// Each Draw, after the light: the creature made ready to be drawn, hidden or not, then the body that sparkles by its
/// last update's film ms, the creature outside 19 550 .. 27 350 ms (its eyes placed then) and nobody inside
TEST(FallingSpell, EachDrawPreparesTheCreatureThenSetsWhoSparklesAndPlacesItsEyesOutsideItsHiddenWindow)
{
	const game_random::testing::ScopedState state;
	Fake fake;
	std::vector<char> calls;
	auto spell = MakeDrawingCreature(fake, calls);

	// nothing while it is closed
	spell.Draw(16, 640, 480);
	EXPECT_TRUE(calls.empty());

	spell.Init(std::nullopt);
	const std::vector<char> shown {'l', 'p', 's', 'e'};
	const std::vector<char> hidden {'l', 'p', 'n'};
	for (const auto& [filmMs, expected] : std::vector<std::pair<int32_t, std::vector<char>>> {
	         {141, shown},
	         {k_CreatureHiddenFromMs - 1, shown},
	         {k_CreatureHiddenFromMs, hidden},
	         {23000, hidden},
	         {k_CreatureHiddenToMs, hidden},
	         {k_CreatureHiddenToMs + 1, shown},
	         {40000, shown},
	     })
	{
		calls.clear();
		spell.AdvanceCreature(filmMs);
		// the sparks are off: the creature's part comes before their early return
		ASSERT_FALSE(spell.SparksOn());
		spell.Draw(16, 640, 480);
		EXPECT_EQ(calls, expected) << "film ms " << filmMs;
	}

	// with the sparks on, the same, and in a hidden frame
	spell.StartSparks();
	calls.clear();
	spell.AdvanceCreature(20000);
	spell.Draw(16, 640, 480);
	EXPECT_EQ(calls, hidden);

	// closed: the kept light back, and no more
	spell.Close();
	calls.clear();
	spell.Draw(16, 640, 480);
	EXPECT_TRUE(calls.empty());
}
