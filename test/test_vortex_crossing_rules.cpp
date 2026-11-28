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
#include <cstdint>

#include <array>
#include <bit>
#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Magic/VortexCrossingRules.h"
#include "Particles/Rules/VortexAttract.h"

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

// The take keeps what the thing's 3D object was, then turns its fade with the distance off and sets its draw importance
// to 10 while it is carried, as the original (docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex)
TEST(VortexCrossing, TheTakeKeepsTheThingsLookThenTurnsItsFadeOffAndItsImportanceToTen)
{
	ecs::Registry registry;
	const auto tree = registry.Create();
	registry.Assign<ecs::components::Tree>(tree, ecs::components::Tree {.type = TreeInfo::Beech, .maxSize = 1.0f});
	registry.Assign<ecs::components::DrawImportance>(tree, ecs::components::DrawImportance {.importance = -0.1f});
	const auto look = particles::vortex_attract::TakeLook(registry, tree, false);
	EXPECT_TRUE(look.wasPaper);
	EXPECT_TRUE(look.wasDisappearing);
	EXPECT_EQ(look.importance, -0.1f);
	EXPECT_TRUE(registry.AllOf<ecs::components::NeverFades>(tree));
	EXPECT_EQ(registry.Get<ecs::components::DrawImportance>(tree).importance, 10.0f);

	// a thing struck in flight lost its paper to the physics start, and one with no importance had 0; one that no
	// longer faded keeps not fading
	const auto thing = registry.Create();
	registry.Assign<ecs::components::NeverFades>(thing);
	const auto struck = particles::vortex_attract::TakeLook(registry, thing, true);
	EXPECT_FALSE(struck.wasPaper);
	EXPECT_FALSE(struck.wasDisappearing);
	EXPECT_EQ(struck.importance, 0.0f);
	EXPECT_EQ(registry.Get<ecs::components::DrawImportance>(thing).importance, 10.0f);
}

namespace
{
/// One step of the spiral as the original game takes it: the thing's place, the velocity it was taken with, its start
/// height, its age, the land under it and the middle (as bits), then its new place and velocity
struct OracleStep
{
	std::array<uint32_t, 12> in;
	std::array<uint32_t, 3> position;
	std::array<uint32_t, 3> velocity;
};

constexpr std::array<OracleStep, 10> k_OracleSteps {{
    {{0x44BD2BD7, 0x4274CCCD, 0x44807D1F, 0x3E9EB852, 0x3F333333, 0xBE6147AE, 0x40600000, 0x3EBD70A4, 0x4226EB85, 0x44BB8000,
      0x42200000, 0x447F0000},
     {0x44BD2B86, 0x4237E79B, 0x44807D4D},
     {0xBE97E00A, 0xC3E45B8B, 0x3E2C800C}},
    {{0x44BA638D, 0x425E0000, 0x447AD550, 0xBFA00000, 0x40066666, 0x3F666666, 0x3F400000, 0x40266666, 0x421FA3D7, 0x44BB8000,
      0x42200000, 0x447F0000},
     {0x44BA6FDA, 0x4247D308, 0x447AD73E},
     {0x4138830C, 0xC326514F, 0x3F67900F}},
    {{0x44BC78A4, 0x423C6666, 0x447F8C29, 0x00000000, 0x00000000, 0x00000000, 0x3F99999A, 0x40DCCCCD, 0x422151EC, 0x44BB8000,
      0x42200000, 0x447F0000},
     {0x44BC688F, 0x42025F00, 0x447FBC55},
     {0xC1713B10, 0xC3D99BCD, 0x41B4A50C}},
    {{0x44BF4CCD, 0x428C0000, 0x4477A666, 0x40800000, 0xC0400000, 0x40200000, 0x41420000, 0x3D4CCCCD, 0x422C0000, 0x44BB8800,
      0x421E0000, 0x447EF000},
     {0x44BF50DA, 0x425E4ACC, 0x4477AB80},
     {0x40730C10, 0xC3D86791, 0x40190C0A}},
    {{0x44B680F6, 0x4231999A, 0x4484823D, 0x3E051EB8, 0x3CA3D70A, 0xBECCCCCD, 0xBE99999A, 0x3F800000, 0x4218CCCD, 0x44BB8000,
      0x42200000, 0x447F0000},
     {0x44B68376, 0x424EB63F, 0x44847A11},
     {0x4016000A, 0x435A56E4, 0xC0F52810}},
    {{0x44BBFCCD, 0x42240000, 0x447E9333, 0x3F000000, 0x3F000000, 0x3F000000, 0x00000000, 0x41100000, 0x42200000, 0x44BB8000,
      0x42200000, 0x447F0000},
     {0x44BC01C1, 0x4134DC50, 0x447ED403},
     {0x4094980A, 0xC45EB8C9, 0x41F30C10}},
    {{0x452A45A3, 0x429BFAA2, 0x44A38CAC, 0xBF068987, 0x40828C31, 0x40AAC1EE, 0x4152BD04, 0x40FF2455, 0x41119B4C, 0x45280B1D,
      0x42D27296, 0x44A08BCD},
     {0x452A299D, 0x42B7F298, 0x44A38D5C},
     {0xC2522D0E, 0x43D1C3C3, 0x3F25000B}},
    {{0x44AA6A2A, 0x42167CB2, 0x45267A76, 0xC04DDB6D, 0xC04CCC42, 0xBE38C729, 0x4186D0CC, 0x404A42E1, 0x3F1D31C2, 0x44AE9DB1,
      0x427B5E2F, 0x452967B7},
     {0x44AA829F, 0x425A5170, 0x452680A9},
     {0x41B76D8C, 0x43FE5DD9, 0x4139FA0C}},
    {{0x44B482F5, 0x428F1929, 0x44F628B0, 0x3FB49F5A, 0x40075259, 0xC0AB4448, 0x41D647CC, 0x4115CA13, 0x43032D4E, 0x44B1E449,
      0x42EF5CA8, 0x44EF7B4C},
     {0x44B44EB5, 0x436F97FC, 0x44F5FB20},
     {0xC243F00D, 0x459D8ABC, 0xC22ADC0B}},
    {{0x449F5CDD, 0x42D28698, 0x4484C916, 0x406B1180, 0x40747ACE, 0x403838F4, 0x40A82E66, 0x40C6ED79, 0x42555668, 0x44A58533,
      0x408B1ACF, 0x44843355},
     {0x449F838B, 0x4300D269, 0x4484AC6F},
     {0x42110C8A, 0x4430B165, 0xC1D6E48E}},
}};
} // namespace

// The original's spiral step, read back from the game itself on ten things at several ages, places and heights with
// the effect's own settings: every place and velocity bit for bit (stepping in doubles misses six of them)
TEST(VortexCrossingRules, TheSpiralStepGivesTheOriginalsBits)
{
	const auto f = [](uint32_t value) { return std::bit_cast<float>(value); };
	constexpr uint32_t k_StepBits = 0x3D088880;
	constexpr uint32_t k_InverseBits = 0x41F00010;
	for (size_t i = 0; i < k_OracleSteps.size(); ++i)
	{
		const auto& c = k_OracleSteps.at(i);
		const AttractAtom atom {.position = {f(c.in[0]), f(c.in[1]), f(c.in[2])},
		                        .takenVelocity = {f(c.in[3]), f(c.in[4]), f(c.in[5])},
		                        .startHeight = f(c.in[6]),
		                        .age = f(c.in[7])};
		const float land = f(c.in[8]);
		const glm::vec3 middle {f(c.in[9]), f(c.in[10]), f(c.in[11])};
		const auto step = Attract({}, atom, middle, f(k_StepBits), f(k_InverseBits),
		                          [land](const map_coords::MapCoords& /*at*/) { return land; });
		ASSERT_TRUE(step.has_value()) << i;
		for (glm::length_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(step->position[axis]), c.position.at(static_cast<size_t>(axis)))
			    << i << " place " << axis;
			EXPECT_EQ(std::bit_cast<uint32_t>(step->velocity[axis]), c.velocity.at(static_cast<size_t>(axis)))
			    << i << " velocity " << axis;
		}
	}
}

namespace
{
/// A take's matrix (three rows, then the place, as bits) and the particle the original makes of it: its rows and size
struct OracleAtomStart
{
	std::array<uint32_t, 12> matrix;
	std::array<uint32_t, 9> rows;
	uint32_t scale;
};

constexpr std::array<OracleAtomStart, 8> k_OracleAtomStarts {{
    {{0x3FAA1FDB, 0x3E8F92E7, 0xBF2F6E1B, 0x3F1037EA, 0xBFA5FB6F, 0x3F0FDF32, 0xBEF6025B, 0xBF3EA492, 0xBF9E4A5F, 0x44DD334D,
      0x42EAF435, 0x447069A8},
     {0x3F5FA369, 0x3E3CBC6D, 0xBEE69CE8, 0x3EBD9558, 0xBF5A316D, 0x3EBD20B8, 0xBEA1B254, 0xBEFA9C68, 0xBF5014FB},
     0x3FC2BE0C},
    {{0xBFD11AD2, 0x3E752889, 0x3EA4C639, 0xBE8B2BC1, 0x3EA9D78B, 0xBFD0334B, 0xBE9705AB, 0xBFD0D9E8, 0xBE9122A5, 0x449D501A,
      0x419112A7, 0x451DC1C8},
     {0xBF789D96, 0x3E11BDA3, 0x3E43E8A6, 0xBE2577B2, 0x3E49EF19, 0xBF778A50, 0xBE338EE0, 0xBF785068, 0xBE2C8F19},
     0x3FD750CD},
    {{0x3FFD631C, 0xBF0D5D9B, 0xBF34221A, 0x3F20B780, 0x4004CD1D, 0x3E0D12DE, 0x3F231A2B, 0xBEA865FD, 0x40033C13, 0x45056CE7,
      0x42F6399F, 0x445F6F0E},
     {0x3F694A81, 0xBE822769, 0xBEA5D8CB, 0x3E93F869, 0x3F74899C, 0x3D81E299, 0x3E962AA6, 0xBE1B0AF0, 0x3F71A725},
     0x400B06B2},
    {{0xBDBD92C1, 0x3E96257D, 0x3E15B5EC, 0x3EA56C02, 0x3DDB2BDB, 0xBC257678, 0xBD5FD670, 0x3E0B3E59, 0xBE9D5D1D, 0x448A1B71,
      0x40C08750, 0x44CF7AC2},
     {0xBE8B2FC5, 0x3F5C7A62, 0x3EDBD68E, 0x3F72E8AB, 0x3EA0EAF4, 0xBCF2F807, 0xBE245807, 0x3ECC77D1, 0xBF671379},
     0x3EAE5647},
    {{0xBEC7566A, 0x3F90ABC5, 0xBF63731B, 0x3F31A150, 0x3F7432C5, 0x3F68CF76, 0x3FA12AD6, 0xBE346A9E, 0xBF46A009, 0x44DAADDA,
      0x43047D6B, 0x44CD6A64},
     {0xBE85D59C, 0x3F424354, 0xBF18B571, 0x3EEE8531, 0x3F23F429, 0x3F1C4ED5, 0x3F586A07, 0xBDF24303, 0xBF055B2A},
     0x3FBEA5C7},
    {{0x3F0AAAA5, 0xBF66718A, 0xBCFFF79B, 0x3F661711, 0x3F09AD81, 0x3DB2E2BE, 0xBD70ED98, 0xBD92E9C1, 0x3F860210, 0x450DD863,
      0x427C38C6, 0x44864546},
     {0x3F03EEC8, 0xBF5B40BB, 0xBCF3897E, 0x3F5AEAA7, 0x3F02FDEF, 0x3DAA32E3, 0xBD653A72, 0xBD8BC75F, 0x3F7F0039},
     0x3F86887A},
    {{0xBEFE1879, 0x3D4D403D, 0x3F7557D5, 0xBF7577B4, 0xBD9081F2, 0xBEFC55E8, 0x3D22BDE3, 0xBF89D90B, 0x3D9D7568, 0x45238758,
      0x429A9AFC, 0x4534F1B2},
     {0xBEEB2F55, 0x3D3DF9B5, 0x3F631572, 0xBF6332F2, 0xBD85C0B9, 0xBEE98E4C, 0x3D16A142, 0xBF7F2D69, 0x3D91BD6F},
     0x3F8A4ACE},
    {{0x400CD3AE, 0xBEA94557, 0x3FA918E2, 0x3FAE4778, 0x3EF8898E, 0xC0095E24, 0x3CD84BDC, 0x40214B11, 0x3F16334F, 0x44D15EFB,
      0x43441261, 0x44BAB1F3},
     {0x3F59AEFA, 0xBE02D34F, 0x3F02B0F3, 0x3F06B23D, 0x3E4016AE, 0xBF545627, 0x3C272B99, 0x3F7951C5, 0x3E682C35},
     0x40259D79},
}};
} // namespace

// The particle a take makes from the matrix it hands over, read back from the game itself on eight turned and scaled
// matrices: the place as handed, the size and every cell of the rows bit for bit
TEST(VortexCrossingRules, AParticleStartsFromItsMatrixAsTheOriginal)
{
	const auto f = [](uint32_t value) { return std::bit_cast<float>(value); };
	for (size_t i = 0; i < k_OracleAtomStarts.size(); ++i)
	{
		const auto& c = k_OracleAtomStarts.at(i);
		const auto& m = c.matrix;
		const glm::mat3 rows({f(m[0]), f(m[1]), f(m[2])}, {f(m[3]), f(m[4]), f(m[5])}, {f(m[6]), f(m[7]), f(m[8])});
		const glm::vec3 place {f(m[9]), f(m[10]), f(m[11])};
		const auto start = AtomFromMatrix(rows, place);
		EXPECT_EQ(std::bit_cast<uint32_t>(start.scale), c.scale) << i;
		EXPECT_EQ(start.position, place) << i;
		for (glm::length_t row = 0; row < 3; ++row)
		{
			for (glm::length_t cell = 0; cell < 3; ++cell)
			{
				EXPECT_EQ(std::bit_cast<uint32_t>(start.rows[row][cell]), c.rows.at(static_cast<size_t>(row * 3 + cell)))
				    << i << " row " << row << " cell " << cell;
			}
		}
	}
}

// One step of the spiral as the original's rule takes it on its 24-bit FPU, run on the v1.42 exe through the oracle (random
// settings, places, velocities, ages and steps on a flat land; 20 of the 24 cases are ones the step in double missed)
// (docs/wiki/world/vortex.md#what-is-ported-and-what-is-pending)
TEST(VortexCrossingRules, TheSpiralStepGivesTheOriginalsBitsForAnySettings)
{
	struct Case
	{
		std::array<uint32_t, 8> settings;
		std::array<uint32_t, 3> origin;
		std::array<uint32_t, 3> position;
		std::array<uint32_t, 3> taken;
		uint32_t startHeight;
		uint32_t age;
		bool indestructible;
		uint32_t dt;
		uint32_t invDt;
		uint32_t land;
		std::array<uint32_t, 6> out;
	};
	constexpr std::array k_Cases = {
	    Case {{0x3F0501B9u, 0x409E66D1u, 0x4031CF30u, 0x3E9EC0DFu, 0x3FD9A8FDu, 0x3F9CA9B4u, 0xC03E1519u, 0x422C3B10u},
	          {0x44191F2Bu, 0x42AD7551u, 0x4431643Eu},
	          {0x440CD7D8u, 0x42D4E416u, 0x443B3277u},
	          {0xC170C449u, 0xC1312087u, 0x40A31D51u},
	          0x4217A229u,
	          0x4013BD05u,
	          false,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x40DF98CCu,
	          {0x440C182Eu, 0x40F1E234u, 0x44396CE0u, 0xC26F9480u, 0xC4F73770u, 0xC30DBF30u}},
	    Case {{0x3FA5BB54u, 0x4110B212u, 0x3F80A054u, 0x3EE21911u, 0x3F8828EEu, 0x401F0CC3u, 0x3FA9A778u, 0x42427AE4u},
	          {0x45170BD9u, 0x4294F582u, 0x4505F3BCu},
	          {0x4513C467u, 0x42894D9Du, 0x4503BF2Du},
	          {0x40E6E977u, 0xC0395D1Bu, 0xC0EDE441u},
	          0x41BB613Au,
	          0x3FE807C8u,
	          true,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x42D1B2C6u,
	          {0x4514542Fu, 0x42D745CAu, 0x45036018u, 0x4333BA00u, 0x4442EC70u, 0xC2EDB480u}},
	    Case {{0x3ECED137u, 0x418B9062u, 0x4015A4DEu, 0x4028D355u, 0x400F8440u, 0x3F80CC01u, 0x41EA72EFu, 0x415DADCFu},
	          {0x44DB4BCAu, 0x43176D9Eu, 0x446EFD08u},
	          {0x44DB2169u, 0x43102C36u, 0x447408EEu},
	          {0x4129534Au, 0x403AF248u, 0x41704E49u},
	          0x4148CC64u,
	          0x4031FEE1u,
	          false,
	          0x3D088880u,
	          0x41F0000Fu,
	          0x412507F5u,
	          {0x44D94830u, 0x4295960Au, 0x4471EEADu, 0xC3DDD2C6u, 0xC5021644u, 0xC37C4E88u}},
	    Case {{0x3E861EACu, 0x41C70E75u, 0x407B96AAu, 0x40006395u, 0x3F972FA1u, 0x3FDE3D61u, 0xBED4C4F6u, 0x40C22EDCu},
	          {0x45554ABAu, 0x4301EF58u, 0x4501FBE4u},
	          {0x45588B4Au, 0x43164D35u, 0x4504C5A3u},
	          {0x4150BD47u, 0xC138EED3u, 0xC11ED362u},
	          0x413B7FA8u,
	          0x3F764FF5u,
	          false,
	          0x3D088889u,
	          0x41EFFFFFu,
	          0x42A34E4Bu,
	          {0x455803B2u, 0x42A93DC9u, 0x4505359Bu, 0xC37E3CFFu, 0xC4F64DADu, 0x4351F0FFu}},
	    Case {{0x3FA13A02u, 0x4012FCDDu, 0x4045BE17u, 0x402D038Du, 0x400383F7u, 0x401EDB3Fu, 0x415162D5u, 0x4286122Au},
	          {0x4543E81Au, 0x41D138A2u, 0x446EE096u},
	          {0x4543FC5Au, 0x429A7F83u, 0x44772C27u},
	          {0x408AF32Fu, 0x4130AA39u, 0xC160205Cu},
	          0x40B5320Bu,
	          0x401E7D6Bu,
	          true,
	          0x3DCCCCCDu,
	          0x41200000u,
	          0x424396DCu,
	          {0x45438C73u, 0x428D3081u, 0x4475FDC1u, 0xC28BE0C0u, 0xC2851614u, 0xC23CFFC0u}},
	    Case {{0x3FA915C2u, 0x41E25AC8u, 0x3FB44C1Fu, 0x3FDC7DD5u, 0x4035D55Du, 0x402353F5u, 0xBE4D1A11u, 0x4161F244u},
	          {0x44E44B56u, 0x416825C4u, 0x4498BD52u},
	          {0x44DDE41Fu, 0x424D7D34u, 0x449CFFA3u},
	          {0x417E18D0u, 0xC15D2777u, 0x410A5115u},
	          0x41D34837u,
	          0x3F126916u,
	          false,
	          0x3D088880u,
	          0x41F0000Fu,
	          0x4203C0B1u,
	          {0x44DDDF85u, 0x42584D64u, 0x449CE19Cu, 0xC08A0C09u, 0x42A232DAu, 0xC1E1348Eu}},
	    Case {{0x3E2F8A79u, 0x41CE11E9u, 0x3F3BB19Cu, 0x402687F2u, 0x3FBC3B81u, 0x3F9326E9u, 0x4165B746u, 0x4295001Au},
	          {0x44A2F289u, 0x41CEC27Au, 0x45020BECu},
	          {0x449F0621u, 0x41BC0D71u, 0x44FF03CFu},
	          {0xC18FE0E5u, 0xC13EDE4Au, 0xC0F0A654u},
	          0x41433416u,
	          0x40426E7Au,
	          true,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x4222503Cu,
	          {0x44A39743u, 0x42859B00u, 0x44FDC370u, 0x4436AD50u, 0x44587B1Au, 0xC3483B60u}},
	    Case {{0x3F9B9014u, 0x41EEB3A7u, 0x3F211B4Bu, 0x3E1D242Cu, 0x3FCED38Eu, 0x403C111Du, 0x414FF8B8u, 0x41BB6860u},
	          {0x44E62555u, 0x4303AA00u, 0x45192516u},
	          {0x44E87E54u, 0x431FE0A4u, 0x451C0F71u},
	          {0x41967FFEu, 0xC0F609A6u, 0xC13648BAu},
	          0x4112EC22u,
	          0x3F4B6437u,
	          false,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x4272D191u,
	          {0x44E7D4FAu, 0x42A1F901u, 0x451C118Au, 0xC2D3B080u, 0xC4C53A59u, 0x4027D000u}},
	    Case {{0x3F9D19A7u, 0x4198E433u, 0x406C9B61u, 0x4034FE69u, 0x3FDE7181u, 0x400DBF56u, 0xC0512C76u, 0x426FB4ABu},
	          {0x44E7929Au, 0x4316889Au, 0x4518178Eu},
	          {0x44E45DA4u, 0x430FF645u, 0x451B4AF7u},
	          {0xC16E8550u, 0xBF8E6ADFu, 0xC0C81C8Eu},
	          0x413E92F1u,
	          0x403D313Cu,
	          false,
	          0x3D088889u,
	          0x41EFFFFFu,
	          0x4273B9BEu,
	          {0x44E29690u, 0x42732B5Eu, 0x451A7427u, 0xC3D5515Fu, 0xC51BF16Du, 0xC3C962FFu}},
	    Case {{0x3ECAC8F0u, 0x416C0FDAu, 0x4035D406u, 0x3EE4FE63u, 0x40000FF6u, 0x3ED22A21u, 0x414856B4u, 0x4283C624u},
	          {0x4506728Eu, 0x42B531C6u, 0x44BB5017u},
	          {0x4508644Fu, 0x42DD089Eu, 0x44BC0796u},
	          {0xC123C903u, 0xC15031F5u, 0x400F09A4u},
	          0x414C581Bu,
	          0x3FBC9284u,
	          false,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x43051675u,
	          {0x4507EE6Cu, 0x43180413u, 0x44BDD680u, 0xC3135BC0u, 0x444F7ED4u, 0x4390A920u}},
	    Case {{0x3FAA6DB0u, 0x416857F6u, 0x3FA92D15u, 0x3F510422u, 0x4038F12Bu, 0x400B12EDu, 0x40B8491Bu, 0x40D44A2Bu},
	          {0x44F95DC9u, 0x4306E485u, 0x44DC0186u},
	          {0x44F5B9A6u, 0x432B9B86u, 0x44E26224u},
	          {0xC12EDB5Fu, 0xC19516BDu, 0xC0CF4B43u},
	          0x41869404u,
	          0x402EBCB1u,
	          true,
	          0x3D088889u,
	          0x41EFFFFFu,
	          0x42DDBD20u,
	          {0x44F481A0u, 0x43051676u, 0x44E117E6u, 0xC39242CFu, 0xC49072FBu, 0xC39ACD0Fu}},
	    Case {{0x3E0F436Du, 0x40C80655u, 0x3F3850AEu, 0x3FED9AD2u, 0x3F9BB0A3u, 0x3F91A751u, 0x41E30DB1u, 0x41044D7Cu},
	          {0x452B3505u, 0x4309EA5Cu, 0x454C8AF3u},
	          {0x4529B00Au, 0x43326CEEu, 0x454D4270u},
	          {0x41439F11u, 0x418EE046u, 0xC18B17FFu},
	          0x420429B6u,
	          0x3EDBABD9u,
	          false,
	          0x3D088880u,
	          0x41F0000Fu,
	          0x430F162Bu,
	          {0x4529AF2Bu, 0x434084F2u, 0x454D304Cu, 0xBFD1100Du, 0x43D36849u, 0xC2080E09u}},
	    Case {{0x3EF3A04Bu, 0x40808A5Eu, 0x3F94EDC5u, 0x403AFB7Au, 0x3FEA9DDCu, 0x40337CB5u, 0x410073E4u, 0x428BEB4Au},
	          {0x44E6EAEEu, 0x424FF565u, 0x45311543u},
	          {0x44ED9A6Du, 0x424593B6u, 0x4531CDDDu},
	          {0x4099888Eu, 0xC134B4F9u, 0xC0A80D95u},
	          0x40B4F3F4u,
	          0x3F50DF33u,
	          true,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x42F46C51u,
	          {0x44ED94BBu, 0x43041106u, 0x4531D673u, 0xC063D000u, 0x44CEAE3Du, 0x412BB800u}},
	    Case {{0x3F70FCC9u, 0x40C92170u, 0x4009F125u, 0x3FDEAFC5u, 0x3EA232FCu, 0x4034FCEDu, 0x3EF41A15u, 0x41FF864Cu},
	          {0x446D19B1u, 0x43422372u, 0x45382F30u},
	          {0x4463E0D4u, 0x43760243u, 0x453AC0C2u},
	          {0x40DC7C02u, 0x40D6E84Du, 0xC0E1053Au},
	          0x41797ED1u,
	          0x3FE955E7u,
	          false,
	          0x3D088889u,
	          0x41EFFFFFu,
	          0x42C2B557u,
	          {0x44635F71u, 0x42C4E556u, 0x453A8417u, 0xC272999Fu, 0xC58A569Eu, 0xC2E3813Fu}},
	    Case {{0x3EFE6A93u, 0x40FB4CE4u, 0x3FEE5DF4u, 0x3F95328Cu, 0x3FCE151Fu, 0x3F335686u, 0xC09C1229u, 0x429DEBAFu},
	          {0x44ECFA39u, 0x42B2BA41u, 0x45133B9Du},
	          {0x44F1C311u, 0x4309EC04u, 0x45158FD4u},
	          {0xC07F1FA4u, 0xC18A857Au, 0xC0B5061Fu},
	          0x4169D00Cu,
	          0x404D625Au,
	          false,
	          0x3DCCCCCDu,
	          0x41200000u,
	          0x40C320BAu,
	          {0x44EF0FDFu, 0x40A432B0u, 0x45163AC7u, 0xC357FFA0u, 0xC4A5FD0Au, 0x42D5AFC0u}},
	    Case {{0x3F51CDC1u, 0x419A2BD4u, 0x3FE12E59u, 0x40286B7Fu, 0x3FE09399u, 0x3FE97CD7u, 0x41CF1C1Bu, 0x414D87D3u},
	          {0x45596DD2u, 0x42FBE916u, 0x44D2589Eu},
	          {0x455BA959u, 0x43067CF0u, 0x44D9B421u},
	          {0x40460AFCu, 0xC0B2E0D4u, 0x41295E7Cu},
	          0x418D87B6u,
	          0x3F34FF8Au,
	          false,
	          0x3DCCCCCDu,
	          0x41200000u,
	          0x4231D47Eu,
	          {0x455B1F12u, 0x42B267CCu, 0x44DA18DBu, 0xC2ACD8C0u, 0xC3E26D32u, 0x41FBD100u}},
	    Case {{0x3DF665F1u, 0x41D5E31Eu, 0x404F62DFu, 0x400B2D90u, 0x3E5F0148u, 0x40241F4Fu, 0x41A8A709u, 0x421F9465u},
	          {0x452A543Du, 0x42B4FEB1u, 0x44933B0Cu},
	          {0x45275E61u, 0x42C18435u, 0x448C501Bu},
	          {0xC0D28A19u, 0x411FC752u, 0x40F9BD64u},
	          0x420740DBu,
	          0x403630F0u,
	          true,
	          0x3D088880u,
	          0x41F0000Fu,
	          0x42CDB859u,
	          {0x4528D1C7u, 0x43185F01u, 0x448AADD0u, 0x442E17DBu, 0x44D08C6Du, 0xC3C41334u}},
	    Case {{0x3F51C3F8u, 0x3F3031D7u, 0x3F17C24Fu, 0x4037C6FCu, 0x3F5ACEEAu, 0x402B5948u, 0x41B4FA07u, 0x4209780Bu},
	          {0x450CFFF3u, 0x42E214F3u, 0x447DA8D8u},
	          {0x45097F25u, 0x42DDBF32u, 0x4480A8C8u},
	          {0xC15870D8u, 0x4198C545u, 0x41007936u},
	          0x3F9E0DC0u,
	          0x3F0DB94Eu,
	          false,
	          0x3DCCCCCDu,
	          0x41200000u,
	          0x42D11A31u,
	          {0x450976C0u, 0x430A2684u, 0x4480AF6Fu, 0xC0A7E400u, 0x43886297u, 0x40050C00u}},
	    Case {{0x3FAA7768u, 0x40EC7ECFu, 0x40073E4Fu, 0x3FD21BFEu, 0x40140086u, 0x4013BC57u, 0x418CF6C7u, 0x41F9175Du},
	          {0x44B8FF61u, 0x41F885D6u, 0x453D5517u},
	          {0x44BB6DD8u, 0x42920248u, 0x453ADAA1u},
	          {0xC01CAD53u, 0x412EFF9Fu, 0x404AACB3u},
	          0x40A15A5Fu,
	          0x3FEC8D9Fu,
	          false,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x42984F9Fu,
	          {0x44BC123Eu, 0x42D72336u, 0x453B3285u, 0x42CD7F80u, 0x442CD253u, 0x42DBBA00u}},
	    Case {{0x3EE02DE1u, 0x41B61E12u, 0x40592432u, 0x3FF1F42Du, 0x400E6BFDu, 0x403B7A72u, 0x41A27C1Au, 0x4248DE58u},
	          {0x44C13CAEu, 0x423CF86Fu, 0x45527613u},
	          {0x44BD9E0Bu, 0x42D02E4Au, 0x45562C55u},
	          {0xC156A7B0u, 0x40CA1C9Fu, 0xC142EC6Au},
	          0x40C13A9Bu,
	          0x3F17E0FAu,
	          false,
	          0x3D088889u,
	          0x41EFFFFFu,
	          0x42827A16u,
	          {0x44BD70DAu, 0x42A28FD4u, 0x45561CFBu, 0xC22977BFu, 0xC42B123Au, 0xC1E645FFu}},
	    Case {{0x3E3E2BDDu, 0x4107694Du, 0x403C232Eu, 0x3E93B643u, 0x400FD2E1u, 0x3F889033u, 0x4173A65Fu, 0x42605F04u},
	          {0x44E59D83u, 0x430F5358u, 0x454591EFu},
	          {0x44E35204u, 0x43472BC4u, 0x45447C6Du},
	          {0x408DFA1Eu, 0xBE812ADFu, 0xC13458D7u},
	          0x4137F4D9u,
	          0x403D0562u,
	          false,
	          0x3D088880u,
	          0x41F0000Fu,
	          0x41416527u,
	          {0x44E42D01u, 0x421601F9u, 0x45442D18u, 0x434D4D3Du, 0xC597909Bu, 0xC314BF69u}},
	    Case {{0x3F3389D2u, 0x4185AAF6u, 0x4065E0C3u, 0x401E7556u, 0x4027A043u, 0x3F7AC5B0u, 0x411890E9u, 0x41FF4340u},
	          {0x45450941u, 0x433F8BD6u, 0x446E30D0u},
	          {0x45429B98u, 0x4345C881u, 0x446630D6u},
	          {0xBF19FB4Du, 0x406427FBu, 0xC117D798u},
	          0x3E27AC8Bu,
	          0x3FD68028u,
	          false,
	          0x3D088889u,
	          0x41EFFFFFu,
	          0x430EF6F6u,
	          {0x45431EF8u, 0x431CBAD0u, 0x44647222u, 0x437653FFu, 0xC499F357u, 0xC351645Fu}},
	    Case {{0x3F868E3Du, 0x417B4FDCu, 0x402A573Du, 0x4003E71Au, 0x3EB3CDE1u, 0x402DFF0Fu, 0x41B26437u, 0x428D2D4Eu},
	          {0x4534D9E9u, 0x429CF39Au, 0x44D41DF8u},
	          {0x4531E0B3u, 0x42E1C08Au, 0x44CD8D00u},
	          {0xC18A72E2u, 0xC13A643Du, 0xC158203Fu},
	          0x4159A264u,
	          0x3E575984u,
	          true,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x42A0FC51u,
	          {0x4531E3D5u, 0x42E0F138u, 0x44CD6886u, 0x407AA000u, 0xC1019340u, 0xC1B66200u}},
	    Case {{0x3FB6865Fu, 0x4194D78Du, 0x3F3F00B5u, 0x3F33FBECu, 0x3FA070CBu, 0x3FFCF8EFu, 0x41E387F3u, 0x4248AF0Bu},
	          {0x44F04E8Au, 0x41B890CDu, 0x44F58689u},
	          {0x44F77961u, 0x423ACB1Fu, 0x44F2B40Cu},
	          {0xC163C3CAu, 0x411FCA93u, 0x4119D325u},
	          0x419928B6u,
	          0x40312AA2u,
	          false,
	          0x3D4CCCCDu,
	          0x41A00000u,
	          0x430EA5D9u,
	          {0x44F76A51u, 0x43497735u, 0x44F6542Eu, 0xC116A000u, 0x45417588u, 0x44110550u}},
	};
	const auto f = [](uint32_t bits) { return std::bit_cast<float>(bits); };
	const auto bits = [](float value) { return std::bit_cast<uint32_t>(value); };
	for (const auto& c : k_Cases)
	{
		const AttractSettings settings {.radialSpeed = f(c.settings[0]),
		                                .thetaSpeed = f(c.settings[1]),
		                                .deletionRadius = f(c.settings[2]),
		                                .minRadius = f(c.settings[3]),
		                                .blendTime = f(c.settings[4]),
		                                .blendTimeVelocity = f(c.settings[5]),
		                                .yOffset = f(c.settings[6]),
		                                .rZeroHeight = f(c.settings[7])};
		const AttractAtom atom {.position = {f(c.position[0]), f(c.position[1]), f(c.position[2])},
		                        .takenVelocity = {f(c.taken[0]), f(c.taken[1]), f(c.taken[2])},
		                        .startHeight = f(c.startHeight),
		                        .age = f(c.age),
		                        .indestructible = c.indestructible};
		const float land = f(c.land);
		const auto step = Attract(settings, atom, {f(c.origin[0]), f(c.origin[1]), f(c.origin[2])}, f(c.dt), f(c.invDt),
		                          [land](const map_coords::MapCoords& /*at*/) { return land; });
		ASSERT_TRUE(step.has_value());
		const std::array got {bits(step->position.x), bits(step->position.y), bits(step->position.z),
		                      bits(step->velocity.x), bits(step->velocity.y), bits(step->velocity.z)};
		EXPECT_EQ(got, c.out);
	}
}
