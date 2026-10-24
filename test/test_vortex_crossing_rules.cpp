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

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
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
