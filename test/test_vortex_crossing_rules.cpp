/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a way out of a land does with what it takes (Magic/VortexCrossingRules.h; raffclar's cases, PR #382): the pot a
// pile gives up, and the spiral that draws a thing into the middle (UR_VortexAttract)

#include <cmath>

#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Enums.h"
#include "Magic/VortexCrossingRules.h"

using namespace openblack;
using namespace openblack::vortex::crossing;

namespace
{
float FlatGround(const map_coords::MapCoords& /*at*/)
{
	return 10.0f;
}

float SeaLevel(const map_coords::MapCoords& /*at*/)
{
	return 0.0f;
}

constexpr float k_Dt = 0.1f;
constexpr float k_InvDt = 1.0f / k_Dt;
} // namespace

TEST(VortexCrossing, APileGivesUpAPotOfASmallerSize)
{
	EXPECT_FLOAT_EQ(PileShare(0.0f), 0.3f);
	EXPECT_FLOAT_EQ(PileShare(k_PileShareRandom), 0.42f);
	EXPECT_EQ(k_PileTake, 1000u);
}

TEST(VortexCrossing, TheRulesOwnSettings)
{
	// the vortex's effect file sets none of them, so these are what it runs with
	const AttractSettings settings;
	EXPECT_EQ(settings.radialSpeed, 1.0f);
	EXPECT_EQ(settings.thetaSpeed, 1.0f);
	EXPECT_EQ(settings.deletionRadius, 4.0f);
	EXPECT_EQ(settings.minRadius, 3.0f);
	EXPECT_EQ(settings.blendTime, 8.0f);
	EXPECT_EQ(settings.blendTimeVelocity, 1.0f);
	EXPECT_EQ(settings.yOffset, -80.0f);
	EXPECT_EQ(settings.rZeroHeight, 10.0f);
}

TEST(VortexCrossing, AThingJustTakenStaysWhereItWas)
{
	const AttractAtom atom {.position = {40.0f, 12.0f, 0.0f}, .startHeight = 2.0f, .age = 0.0f};
	const auto step = Attract({}, atom, glm::vec3(0.0f), k_Dt, k_InvDt, FlatGround);
	ASSERT_TRUE(step.has_value());
	EXPECT_FLOAT_EQ(step->position.x, 40.0f);
	EXPECT_FLOAT_EQ(step->position.y, 12.0f);
	EXPECT_FLOAT_EQ(step->position.z, 0.0f);
	EXPECT_FLOAT_EQ(step->velocity.y, 0.0f);
}

TEST(VortexCrossing, ABlendedThingSpiralsInOntoTheFunnel)
{
	const AttractAtom atom {.position = {40.0f, 50.0f, 0.0f}, .startHeight = 2.0f, .age = 8.0f};
	const auto step = Attract({}, atom, glm::vec3(0.0f), k_Dt, k_InvDt, SeaLevel);
	ASSERT_TRUE(step.has_value());
	// a tenth of its distance gone, swung round by 3 / 36 of a radian, and on the funnel: sqrt(36 x 640) - 80 above
	// the land
	const float angle = 3.0f / 36.0f;
	EXPECT_NEAR(step->position.x, 36.0f * std::cos(angle), 1e-4f);
	EXPECT_NEAR(step->position.z, 36.0f * std::sin(angle), 1e-4f);
	EXPECT_NEAR(step->position.y, std::sqrt(36.0f * 640.0f) - 80.0f, 1e-3f);
}

TEST(VortexCrossing, AtTheMiddleItIsGoneOnceBlendedIn)
{
	const AttractSettings settings;
	const glm::vec3 middle(0.0f);
	// inside the deletion radius and fully blended: gone
	EXPECT_FALSE(Attract(settings, {.position = {3.5f, 0.0f, 0.0f}, .age = 8.0f}, middle, k_Dt, k_InvDt, FlatGround));
	// half blended, it stays, drawn in by half the step
	const auto waiting = Attract(settings, {.position = {3.5f, 0.0f, 0.0f}, .age = 4.0f}, middle, k_Dt, k_InvDt, FlatGround);
	ASSERT_TRUE(waiting.has_value());
	EXPECT_NEAR(glm::length(glm::vec2(waiting->position.x, waiting->position.z)), 3.325f, 1e-3f);
	// never nearer than the least radius
	const auto least = Attract(settings, {.position = {3.0f, 0.0f, 0.0f}, .age = 4.0f}, middle, k_Dt, k_InvDt, FlatGround);
	ASSERT_TRUE(least.has_value());
	EXPECT_NEAR(glm::length(glm::vec2(least->position.x, least->position.z)), 3.0f, 1e-3f);
	// one that can't be destroyed goes from half way
	EXPECT_FALSE(Attract(settings, {.position = {3.5f, 0.0f, 0.0f}, .age = 4.0f, .indestructible = true}, middle, k_Dt, k_InvDt,
	                     FlatGround));
	// outside it, it stays whatever its age
	EXPECT_TRUE(Attract(settings, {.position = {30.0f, 0.0f, 0.0f}, .age = 20.0f}, middle, k_Dt, k_InvDt, FlatGround));
}

TEST(VortexCrossing, TheVelocityIsTheStepItTook)
{
	const AttractAtom atom {.position = {20.0f, 5.0f, 10.0f}, .startHeight = 5.0f, .age = 2.0f};
	const auto step = Attract({}, atom, glm::vec3(0.0f), k_Dt, k_InvDt, SeaLevel);
	ASSERT_TRUE(step.has_value());
	const auto moved = (step->position - atom.position) * k_InvDt;
	EXPECT_NEAR(step->velocity.x, moved.x, 1e-3f);
	EXPECT_NEAR(step->velocity.y, moved.y, 1e-3f);
	EXPECT_NEAR(step->velocity.z, moved.z, 1e-3f);
}

// A way into a land brings out 30 villagers in all, those of the crossing file counted, and while it still has the file
// or fewer than 30 out its fade-out waits, as the original (docs/wiki/world/vortex.md#vortex-details)
TEST(VortexCrossing, ThirtyVillagersComeOutInAllAndTheFadeWaitsForThem)
{
	EXPECT_EQ(k_ArrivingVillagers, 30u);
	EXPECT_TRUE(StillBringsOut(true, 0));
	EXPECT_TRUE(StillBringsOut(true, 45));
	EXPECT_TRUE(StillBringsOut(false, 0));
	EXPECT_TRUE(StillBringsOut(false, 29));
	EXPECT_FALSE(StillBringsOut(false, 30));
	EXPECT_FALSE(StillBringsOut(false, 31));
}

// Given no town, it takes the nearest town within 200, as the original (docs/wiki/world/vortex.md#vortex-details)
TEST(VortexCrossing, AWayInGivenNoTownTakesTheNearestWithinTwoHundred)
{
	EXPECT_EQ(k_ArrivalTownReach, 200.0f);
}

// A new villager is of its town's tribe, Norse without one; a housewife on a draw below a half, else a forester,
// fisherman, farmer, shepherd or leader by a draw out of five, never a trader, as the original
// (docs/wiki/world/vortex.md#vortex-details)
TEST(VortexCrossing, NewVillagersAreOfTheTownsTribeHalfOfThemHousewivesAndNeverATrader)
{
	EXPECT_EQ(NewVillagerTribe(std::nullopt), Tribe::NORSE);
	EXPECT_EQ(NewVillagerTribe(Tribe::CELTIC), Tribe::CELTIC);
	EXPECT_EQ(NewVillagerTribe(Tribe::TIBETAN), Tribe::TIBETAN);
	EXPECT_TRUE(IsHousewife(0.0f));
	EXPECT_TRUE(IsHousewife(0.4999f));
	EXPECT_FALSE(IsHousewife(0.5f));
	EXPECT_FALSE(IsHousewife(0.99f));
	EXPECT_EQ(k_NewVillagerJobs, 5u);
	EXPECT_EQ(NewVillagerRow(Tribe::NORSE, true, 0), static_cast<int32_t>(VillagerInfo::NorseHousewifeFemale));
	EXPECT_EQ(NewVillagerRow(Tribe::NORSE, false, 0), static_cast<int32_t>(VillagerInfo::NorseForesterMale));
	EXPECT_EQ(NewVillagerRow(Tribe::NORSE, false, 1), static_cast<int32_t>(VillagerInfo::NorseFishermanMale));
	EXPECT_EQ(NewVillagerRow(Tribe::NORSE, false, 2), static_cast<int32_t>(VillagerInfo::NorseFarmerMale));
	EXPECT_EQ(NewVillagerRow(Tribe::NORSE, false, 3), static_cast<int32_t>(VillagerInfo::NorseShepherdMale));
	EXPECT_EQ(NewVillagerRow(Tribe::NORSE, false, 4), static_cast<int32_t>(VillagerInfo::NorseLeaderMale));
	EXPECT_EQ(NewVillagerRow(Tribe::CELTIC, true, 0), static_cast<int32_t>(VillagerInfo::CelticHousewifeFemale));
	EXPECT_EQ(NewVillagerRow(Tribe::CELTIC, false, 4), static_cast<int32_t>(VillagerInfo::CelticLeaderMale));
	// the highest job drawn is the leader, the row before the trader's
	EXPECT_LT(NewVillagerRow(Tribe::NORSE, false, k_NewVillagerJobs - 1), static_cast<int32_t>(VillagerInfo::NorseTraderMale));
}

// Every fifth new villager, counted with itself, is a child of 1 to 9 and the others adults of 16 to 21, as the
// original (docs/wiki/world/vortex.md#bringing-them-out-the-out-vortex)
TEST(VortexCrossing, EveryFifthNewVillagerIsAChildAndTheOthersAdults)
{
	for (const uint32_t count : {5u, 10u, 30u})
	{
		const auto draw = NewVillagerAgeDraw(count);
		EXPECT_EQ(draw.range, 9u) << count;
		EXPECT_EQ(draw.youngest, 1u) << count;
	}
	for (const uint32_t count : {1u, 4u, 6u, 29u})
	{
		const auto draw = NewVillagerAgeDraw(count);
		EXPECT_EQ(draw.range, 6u) << count;
		EXPECT_EQ(draw.youngest, 16u) << count;
	}
}
