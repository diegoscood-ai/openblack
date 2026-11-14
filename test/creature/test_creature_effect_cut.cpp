/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The cut a harmful effect leaves on a creature out of a fight: the harm it reads, the wound's kind and column, the line
// it cuts along and the game random draws it makes, and where on the skin's texture a point of a triangle falls

#include <cmath>

#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureEffectCut.h"

using namespace openblack;
using namespace openblack::creature_effect_cut;

namespace
{
/// Plays back fixed draws and keeps what was asked for, "i<n>" for a whole number below n and "r<x>" for a real one
struct Draws
{
	std::vector<uint32_t> integers;
	std::vector<float> reals;
	std::vector<std::string> asked;

	Random Source()
	{
		return {
		    .integer =
		        [this](uint32_t n) {
			        asked.push_back("i" + std::to_string(n));
			        const auto value = integers.front();
			        integers.erase(integers.begin());
			        return value;
		        },
		    .real =
		        [this](float x) {
			        asked.push_back("r" + std::to_string(x));
			        const auto value = reals.front();
			        reals.erase(reals.begin());
			        return value;
		        },
		};
	}
};

Inputs Crushing(float damage)
{
	return {.damage = damage,
	        .burn = 0.0f,
	        .crush = 1.0f,
	        .groin = glm::vec3(100.0f, 20.0f, 50.0f),
	        .size = 1.0f,
	        .markedShare = 0.0f,
	        .from = glm::vec3(0.0f, 3.0f, 0.0f)};
}
} // namespace

TEST(CreatureEffectCut, TheHarmIsKeptToNoneToOne)
{
	EXPECT_FLOAT_EQ(KeptDamage(-0.5f), 0.0f);
	EXPECT_FLOAT_EQ(KeptDamage(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	EXPECT_FLOAT_EQ(KeptDamage(0.25f), 0.25f);
	EXPECT_FLOAT_EQ(KeptDamage(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(KeptDamage(3.0f), 1.0f);
}

TEST(CreatureEffectCut, TheColumnIsTheDrawLessTwoLessTheDamagesSevenLessSevenKeptToTheAtlas)
{
	// the whole harm: 7 - 7 = 0, so the draw less 2
	EXPECT_EQ(Column(0, 1.0f), 0);
	EXPECT_EQ(Column(2, 1.0f), 0);
	EXPECT_EQ(Column(4, 1.0f), 2);
	// half: 3.5 - 7 = -3.5, cut to -3, so the draw plus 1
	EXPECT_EQ(Column(0, 0.5f), 1);
	EXPECT_EQ(Column(4, 0.5f), 5);
	// a tenth: 0.7 - 7 = -6.3, cut to -6, so the draw plus 4, never past the last column
	EXPECT_EQ(Column(3, 0.1f), 7);
	EXPECT_EQ(Column(4, 0.1f), 7);
}

TEST(CreatureEffectCut, ACrushCutsFromWhereTheEffectCameOfTheFirstKindBelowHalfAndTheSecondFromHalf)
{
	Draws draws {.integers = {3}};
	const auto light = Plan(Crushing(0.25f), draws.Source());
	ASSERT_TRUE(light.has_value());
	EXPECT_EQ(light->type, k_LowDamageKind);
	EXPECT_EQ(light->from, glm::vec3(0.0f, 3.0f, 0.0f));
	EXPECT_EQ(light->direction, glm::vec3(100.0f, 17.0f, 50.0f));
	// a quarter: 1.75 - 7 = -5.25, cut to -5: 3 - 2 + 5
	EXPECT_EQ(light->column, 6);
	EXPECT_EQ(draws.asked, std::vector<std::string>({"i5"}));

	Draws again {.integers = {0}};
	const auto deep = Plan(Crushing(0.5f), again.Source());
	ASSERT_TRUE(deep.has_value());
	EXPECT_EQ(deep->type, k_HighDamageKind);
	EXPECT_EQ(deep->column, 1);
}

TEST(CreatureEffectCut, ALineTooShortCutsNothingAndDrawsNothing)
{
	auto inputs = Crushing(1.0f);
	inputs.from = inputs.groin + glm::vec3(0.05f, 0.05f, 0.05f);
	Draws draws;
	EXPECT_FALSE(Plan(inputs, draws.Source()).has_value());
	EXPECT_TRUE(draws.asked.empty());
}

TEST(CreatureEffectCut, AnEffectThatCrushesNoMoreThanItBurnsMarksFromAboutTheGroinByChance)
{
	// burning as much as it crushes: the kind first (the rare one on a draw of 0), then the odds, from the share of the room
	// taken: 0.5 x 140 + 10 = 80
	auto inputs = Crushing(1.0f);
	inputs.burn = 1.0f;
	inputs.markedShare = 0.5f;
	inputs.size = 2.0f;
	Draws draws {.integers = {0, 0, 4}, .reals = {1.0f, 2.0f, 3.0f}};
	const auto cut = Plan(inputs, draws.Source());
	ASSERT_TRUE(cut.has_value());
	EXPECT_EQ(cut->type, k_RareGroinKind);
	// 30 high at size 2: an eighth (3.75) each side across, a half (15) each side up
	EXPECT_EQ(draws.asked, std::vector<std::string>({"i3", "i80", "r" + std::to_string(7.5f), "r" + std::to_string(30.0f),
	                                                 "r" + std::to_string(7.5f), "i5"}));
	EXPECT_FLOAT_EQ(cut->from.x, 1.0f + 100.0f - 3.75f);
	EXPECT_FLOAT_EQ(cut->from.y, 2.0f + 20.0f - 15.0f);
	EXPECT_FLOAT_EQ(cut->from.z, 3.0f + 50.0f - 3.75f);
	EXPECT_EQ(cut->direction, inputs.groin - cut->from);
	EXPECT_EQ(cut->column, 2);

	// a draw of the kind but 0 gives the first kind; one of the odds but 0 marks nothing, and nothing more is drawn
	Draws first {.integers = {1, 0, 0}, .reals = {0.0f, 0.0f, 0.0f}};
	const auto plain = Plan(inputs, first.Source());
	ASSERT_TRUE(plain.has_value());
	EXPECT_EQ(plain->type, k_GroinKind);
	Draws none {.integers = {0, 7}};
	EXPECT_FALSE(Plan(inputs, none.Source()).has_value());
	EXPECT_EQ(none.asked, std::vector<std::string>({"i3", "i80"}));
}

TEST(CreatureEffectCut, ATexelIsTheCoordinateAlongTheTriangleTimes256CutAndKeptToTheSkin)
{
	// 0.25 + 0.5 x 0.5 + 0.25 x 0.25 = 0.5625 -> 144
	EXPECT_EQ(Texel(0.25f, 0.75f, 0.5f, 0.5f, 0.25f), 144);
	EXPECT_EQ(Texel(0.0f, 0.0f, 0.0f, 0.5f, 0.5f), 0);
	EXPECT_EQ(Texel(1.0f, 1.0f, 1.0f, 0.0f, 0.0f), 255);
	// below the texture: a negative number read as a large unsigned one, the last texel
	EXPECT_EQ(Texel(-0.1f, -0.1f, -0.1f, 0.0f, 0.0f), 255);
	EXPECT_EQ(Texel(0.002f, 0.002f, 0.002f, 0.0f, 0.0f), 0);
}

// A creature that has just caught fire is cut three times about its groin: each a point drawn in its box (an eighth of
// its standing height each side across, a half up), then for a line long enough its kind (the rare one when the draw of
// 3 is 0) and its column (a draw of 8); a point on the groin itself cuts nothing and draws no more
TEST(CreatureEffectCut, CatchingFireCutsThreeTimesAboutTheGroin)
{
	Draws draws {.integers = {0, 7, 2, 3}, .reals = {1.0f, 2.0f, 3.0f, 1.875f, 7.5f, 1.875f, 0.0f, 0.0f, 0.0f}};
	const auto cuts = CaughtFire(glm::vec3(100.0f, 20.0f, 50.0f), 1.0f, draws.Source());
	ASSERT_EQ(cuts.size(), 2u);
	EXPECT_EQ(cuts[0].from, glm::vec3(99.125f, 14.5f, 51.125f));
	EXPECT_EQ(cuts[0].direction, glm::vec3(100.0f, 20.0f, 50.0f) - cuts[0].from);
	EXPECT_EQ(cuts[0].type, k_RareGroinKind);
	EXPECT_EQ(cuts[0].column, 7u);
	EXPECT_EQ(cuts[1].from, glm::vec3(98.125f, 12.5f, 48.125f));
	EXPECT_EQ(cuts[1].type, k_GroinKind);
	EXPECT_EQ(cuts[1].column, 3u);
	const std::vector<std::string> k_Asked {"r3.750000", "r15.000000", "r3.750000", "i3",        "i8",
	                                        "r3.750000", "r15.000000", "r3.750000", "r3.750000", "r15.000000",
	                                        "r3.750000", "i3",         "i8"};
	EXPECT_EQ(draws.asked, k_Asked);
}
