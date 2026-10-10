/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Debug/TestbedFixtures.h"

using namespace openblack;
using namespace openblack::testbed_fixtures;

namespace
{
constexpr float k_Tolerance = 1e-3f;

/// One of every fixture, each as it can be set out, standing at the presses of a demo of three
Fixtures EveryFixture()
{
	Fixtures fixtures;
	fixtures.dispensers.push_back({.magic = MagicType::Heal, .at = DemoPress {.press = 0}});
	fixtures.storms.push_back({.forkSeconds = glm::vec2(1.0f, 4.0f), .sheetSeconds = glm::vec2(2.0f, 2.0f)});
	fixtures.creatures.push_back(
	    {.hold = Hold::Leashed, .leash = LeashType::Good, .knows = {LeashType::Evil, LeashType::Rope}});
	fixtures.villages.push_back({});
	fixtures.temples.push_back({.at = glm::vec2 {0.0f, 40.0f}});
	fixtures.fires.push_back({.what = size_t {0}});
	fixtures.fires.push_back({.what = Where {DemoPress {.press = 2}}});
	fixtures.trees.push_back({.count = 5, .spread = 10.0f});
	fixtures.piles.push_back({});
	fixtures.highlights.push_back({.info = ecs::script_highlight::Info::Gold});
	fixtures.handSeed = MagicType::Fireball;
	fixtures.handDemo = HandDemo {.name = "demo"};
	return fixtures;
}

/// Whether one of the problems mentions the text
bool Mentions(const std::vector<std::string>& problems, const std::string& text)
{
	for (const auto& problem : problems)
	{
		if (problem.find(text) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

/// The problems of the fixtures, which must all be sentences
std::vector<std::string> SentencesOf(const Fixtures& fixtures, std::optional<size_t> pressCount = 3)
{
	auto problems = Problems(fixtures, pressCount);
	for (const auto& problem : problems)
	{
		EXPECT_FALSE(problem.empty());
		EXPECT_EQ(problem.back(), '.') << problem;
	}
	return problems;
}
} // namespace

TEST(TestbedFixtures, NoFixturesIsEmpty)
{
	EXPECT_TRUE(Empty(Fixtures {}));
}

TEST(TestbedFixtures, AnyFixtureIsNotEmpty)
{
	const std::array<void (*)(Fixtures&), 12> adds {
	    [](Fixtures& f) { f.dispensers.emplace_back(); },
	    [](Fixtures& f) { f.storms.emplace_back(); },
	    [](Fixtures& f) { f.creatures.emplace_back(); },
	    [](Fixtures& f) { f.villages.emplace_back(); },
	    [](Fixtures& f) { f.temples.emplace_back(); },
	    [](Fixtures& f) { f.fires.emplace_back(); },
	    [](Fixtures& f) { f.trees.emplace_back(); },
	    [](Fixtures& f) { f.piles.emplace_back(); },
	    [](Fixtures& f) { f.highlights.emplace_back(); },
	    [](Fixtures& f) { f.handSeed = MagicType::Heal; },
	    [](Fixtures& f) { f.handDemo = HandDemo {.name = "demo"}; },
	    [](Fixtures& f) { f.writesGameData = true; },
	};
	for (const auto& add : adds)
	{
		Fixtures fixtures;
		add(fixtures);
		EXPECT_FALSE(Empty(fixtures));
	}
}

TEST(TestbedFixtures, EveryFixtureAsItCanBeSetOutHasNoProblems)
{
	EXPECT_TRUE(SentencesOf(Fixtures {}).empty());
	EXPECT_TRUE(SentencesOf(EveryFixture()).empty());
	// A demo whose presses are not known yet is not held against the fixtures standing at them
	EXPECT_TRUE(SentencesOf(EveryFixture(), std::nullopt).empty());
}

TEST(TestbedFixtures, StormRadiiLifeAndWeatherAreChecked)
{
	auto fixtures = EveryFixture();
	auto& storm = fixtures.storms.front();
	storm.innerRadius = 400.0f;
	storm.outerRadius = 300.0f;
	storm.lifeSeconds = 0.0f;
	storm.rain = 1.5f;
	storm.snow = -0.1f;
	const auto problems = SentencesOf(fixtures);
	EXPECT_EQ(problems.size(), 4u);
	EXPECT_TRUE(Mentions(problems, "inner radius"));
	EXPECT_TRUE(Mentions(problems, "lasts"));
	EXPECT_TRUE(Mentions(problems, "rain"));
	EXPECT_TRUE(Mentions(problems, "snow"));
}

TEST(TestbedFixtures, AStormThatLastsAtAllHasNoLifeProblem)
{
	auto fixtures = EveryFixture();
	fixtures.storms.front().lifeSeconds = -1.0f;
	EXPECT_EQ(SentencesOf(fixtures).size(), 1u);
	fixtures.storms.front().lifeSeconds = 0.01f;
	EXPECT_TRUE(SentencesOf(fixtures).empty());
}

TEST(TestbedFixtures, LightningTimesAreChecked)
{
	auto fixtures = EveryFixture();
	fixtures.storms.front().forkSeconds = glm::vec2(5.0f, 2.0f);
	auto problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "fork lightning"));

	fixtures = EveryFixture();
	fixtures.storms.front().sheetSeconds = glm::vec2(-1.0f, 2.0f);
	problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "sheet lightning"));
	EXPECT_TRUE(Mentions(problems, "negative"));
}

TEST(TestbedFixtures, ALeashedCreatureNeedsALeash)
{
	auto fixtures = EveryFixture();
	fixtures.creatures.front().leash = LeashType::None;
	EXPECT_EQ(SentencesOf(fixtures).size(), 1u);
	// Free or penned, it needs none
	fixtures.creatures.front().hold = Hold::Free;
	EXPECT_TRUE(SentencesOf(fixtures).empty());
	fixtures.creatures.front().hold = Hold::Penned;
	EXPECT_TRUE(SentencesOf(fixtures).empty());
}

TEST(TestbedFixtures, KnownLeashesHaveAKind)
{
	auto fixtures = EveryFixture();
	fixtures.creatures.front().knows.push_back(LeashType::None);
	const auto problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "Creature 0 knows a leash of no kind"));
}

TEST(TestbedFixtures, EachPlayerHasOneTemple)
{
	auto fixtures = EveryFixture();
	// Another player's temple is another temple
	fixtures.temples.push_back({.at = glm::vec2 {200.0f, 0.0f}, .owner = PlayerNames::PLAYER_TWO});
	EXPECT_TRUE(SentencesOf(fixtures).empty());
	// A second of the first player's is not, nor is a temple of nobody's
	fixtures.temples.push_back({.at = glm::vec2 {-200.0f, 0.0f}});
	fixtures.temples.push_back({.at = glm::vec2 {0.0f, -200.0f}, .owner = PlayerNames::NEUTRAL});
	const auto problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 2u);
	EXPECT_TRUE(Mentions(problems, "Temple 2 is a second temple of player 0"));
	EXPECT_TRUE(Mentions(problems, "Temple 3 belongs to no player"));
}

TEST(TestbedFixtures, ATempleAtAPressNeedsTheDemo)
{
	auto fixtures = EveryFixture();
	fixtures.temples.front().at = DemoPress {.press = 5};
	const auto problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "Temple 0"));
}

TEST(TestbedFixtures, ACreatureInItsTemplesPenNeedsThatTemple)
{
	auto fixtures = EveryFixture();
	fixtures.creatures.push_back({.hold = Hold::TemplePen});
	// The first player's temple is among the fixtures
	EXPECT_TRUE(SentencesOf(fixtures).empty());
	fixtures.creatures.push_back({.owner = PlayerNames::PLAYER_TWO, .hold = Hold::TemplePen});
	const auto problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "Creature 2 is held in its temple's pen, but player 1 has no temple"));
}

TEST(TestbedFixtures, VillagersNeedHuts)
{
	auto fixtures = EveryFixture();
	fixtures.villages.front().huts = 0;
	auto problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "no huts for them"));

	fixtures.villages.front().villagers = 0;
	problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "no huts"));

	// Huts with nobody in them are a village
	fixtures.villages.front().huts = 3;
	EXPECT_TRUE(SentencesOf(fixtures).empty());
}

TEST(TestbedFixtures, MiraclesNeedASeed)
{
	auto fixtures = EveryFixture();
	fixtures.dispensers.front().magic = MagicType::None;
	fixtures.handSeed = MagicType::None;
	auto problems = SentencesOf(fixtures);
	EXPECT_EQ(problems.size(), 2u);
	EXPECT_TRUE(Mentions(problems, "Dispenser 0"));
	EXPECT_TRUE(Mentions(problems, "hand seed"));

	// A creature spell that does nothing in the game has no seed to give
	fixtures = EveryFixture();
	fixtures.dispensers.front().magic = MagicType::CreatureSpellFat;
	fixtures.handSeed = MagicType::CreatureSpellThin;
	problems = SentencesOf(fixtures);
	EXPECT_EQ(problems.size(), 2u);
	EXPECT_TRUE(Mentions(problems, "no seed"));
}

TEST(TestbedFixtures, CountsOfNothingAreChecked)
{
	auto fixtures = EveryFixture();
	fixtures.trees.front().count = 0;
	fixtures.piles.front().amount = 0;
	const auto problems = SentencesOf(fixtures);
	EXPECT_EQ(problems.size(), 2u);
	EXPECT_TRUE(Mentions(problems, "Trees 0"));
	EXPECT_TRUE(Mentions(problems, "Pile 0"));
}

TEST(TestbedFixtures, PressesNeedAHandDemoThatHasThem)
{
	auto fixtures = EveryFixture();
	// Presses 0 and 2 of a demo of two: the one at 2 is missing
	auto problems = SentencesOf(fixtures, 2);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "Fire 1"));
	EXPECT_TRUE(Mentions(problems, "only 2 presses"));

	// Without a demo neither press is there
	fixtures.handDemo.reset();
	problems = SentencesOf(fixtures);
	EXPECT_EQ(problems.size(), 2u);
	EXPECT_TRUE(Mentions(problems, "no hand demo"));
}

TEST(TestbedFixtures, AHighlightAtAPressNeedsAHandDemo)
{
	Fixtures fixtures;
	fixtures.highlights.push_back({.at = DemoPress {.press = 0}});
	const auto problems = SentencesOf(fixtures);
	ASSERT_EQ(problems.size(), 1u);
	EXPECT_TRUE(Mentions(problems, "Highlight 0"));
	EXPECT_TRUE(Mentions(problems, "no hand demo"));
}

TEST(TestbedFixtures, AHandDemoNeedsAName)
{
	auto fixtures = EveryFixture();
	fixtures.handDemo->name = {};
	EXPECT_EQ(SentencesOf(fixtures).size(), 1u);
}

TEST(TestbedFixtures, WritingTheGameDataIsTheRunnersToCheck)
{
	auto fixtures = EveryFixture();
	fixtures.writesGameData = true;
	EXPECT_TRUE(SentencesOf(fixtures).empty());
}

TEST(TestbedFixtures, AnOffsetIsFromTheMiddle)
{
	const auto point = Resolve(glm::vec2(3.0f, -4.0f), glm::vec2(100.0f, 200.0f), {});
	ASSERT_TRUE(point.has_value());
	EXPECT_FLOAT_EQ(point->x, 103.0f);
	EXPECT_FLOAT_EQ(point->y, 196.0f);
}

TEST(TestbedFixtures, APressIsWhereTheDemoPressedMovedByItsNudge)
{
	const std::vector<glm::vec2> presses {{10.0f, 20.0f}, {30.0f, 40.0f}};
	const auto point = Resolve(DemoPress {.press = 1, .nudge = glm::vec2(1.0f, -2.0f)}, glm::vec2(500.0f), presses);
	ASSERT_TRUE(point.has_value());
	EXPECT_FLOAT_EQ(point->x, 31.0f);
	EXPECT_FLOAT_EQ(point->y, 38.0f);
	EXPECT_FALSE(Resolve(DemoPress {.press = 2}, glm::vec2(500.0f), presses).has_value());
	EXPECT_FALSE(Resolve(DemoPress {.press = 0}, glm::vec2(500.0f), {}).has_value());
}

TEST(TestbedFixtures, NoHutsStandNowhere)
{
	EXPECT_TRUE(HutOffsets(Village {.huts = 0}).empty());
}

TEST(TestbedFixtures, HutsStandOnARingClearOfEachOtherAndOfThePit)
{
	for (size_t huts = 1; huts <= 24; ++huts)
	{
		const auto offsets = HutOffsets(Village {.huts = huts, .seed = 7});
		ASSERT_EQ(offsets.size(), huts);
		const float radius = glm::length(offsets.front());
		EXPECT_GE(radius, k_MinHutRing - k_Tolerance);
		for (size_t i = 0; i < offsets.size(); ++i)
		{
			EXPECT_NEAR(glm::length(offsets[i]), radius, k_Tolerance);
			for (size_t j = i + 1; j < offsets.size(); ++j)
			{
				EXPECT_GE(glm::distance(offsets[i], offsets[j]), k_HutSpacing - k_Tolerance) << huts << " huts";
			}
		}
	}
}

TEST(TestbedFixtures, TheSmallestRingFitsFewHuts)
{
	const auto offsets = HutOffsets(Village {.huts = 3});
	EXPECT_NEAR(glm::length(offsets.front()), k_MinHutRing, k_Tolerance);
}

TEST(TestbedFixtures, TheFirstHutStandsDueNorthUnturned)
{
	const auto offsets = HutOffsets(Village {.huts = 4, .seed = 0});
	ASSERT_EQ(offsets.size(), 4u);
	EXPECT_NEAR(offsets[0].x, 0.0f, k_Tolerance);
	EXPECT_GT(offsets[0].y, 0.0f);
	// Then clockwise: east, south, west
	EXPECT_GT(offsets[1].x, 0.0f);
	EXPECT_LT(offsets[2].y, 0.0f);
	EXPECT_LT(offsets[3].x, 0.0f);
}

TEST(TestbedFixtures, TheSeedTurnsTheRingTheSameWayEachTime)
{
	const auto unturned = HutOffsets(Village {.huts = 5, .seed = 0});
	const auto turned = HutOffsets(Village {.huts = 5, .seed = 3});
	EXPECT_GT(glm::distance(unturned.front(), turned.front()), 0.1f);
	EXPECT_NEAR(glm::length(unturned.front()), glm::length(turned.front()), k_Tolerance);
	const auto again = HutOffsets(Village {.huts = 5, .seed = 3});
	for (size_t i = 0; i < turned.size(); ++i)
	{
		EXPECT_EQ(turned[i], again[i]);
	}
}

TEST(TestbedFixtures, ASpreadOfNothingIsAllAtTheMiddle)
{
	for (const auto& offset : SpreadOffsets(4, 0.0f))
	{
		EXPECT_EQ(offset, glm::vec2(0.0f));
	}
	EXPECT_TRUE(SpreadOffsets(0, 10.0f).empty());
	const auto one = SpreadOffsets(1, 10.0f);
	ASSERT_EQ(one.size(), 1u);
	EXPECT_EQ(one.front(), glm::vec2(0.0f));
}

TEST(TestbedFixtures, ASpreadStaysWithinItsRadiusApartAndTheSame)
{
	constexpr float k_Spread = 12.0f;
	const auto offsets = SpreadOffsets(30, k_Spread);
	ASSERT_EQ(offsets.size(), 30u);
	for (size_t i = 0; i < offsets.size(); ++i)
	{
		EXPECT_LE(glm::length(offsets[i]), k_Spread + k_Tolerance);
		for (size_t j = i + 1; j < offsets.size(); ++j)
		{
			EXPECT_GT(glm::distance(offsets[i], offsets[j]), 0.5f);
		}
	}
	EXPECT_EQ(offsets, SpreadOffsets(30, k_Spread));
}
