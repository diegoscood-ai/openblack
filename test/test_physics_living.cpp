/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's cases (0222f9f5, test_physics_living.cpp) on our physics::living rules. His shelter rule takes the
// shield's and the flyer's positions; ours takes the distance between them, which the villagers work out across the
// map with the game's distance in metres, as here.

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "Physics/LivingRules.h"

using namespace openblack;

namespace
{
/// Whether a villager sheltering under a shield of `radius` at `shield` sees a flyer at `flyer`
bool SeesFlyer(glm::vec3 shield, float radius, glm::vec3 flyer)
{
	return physics::living::ShelterSeesFlyer(gutils::GetDistanceInMetres(shield, flyer), radius);
}
} // namespace

TEST(PhysicsLiving, AShelteredVillagerSeesOnlyWhatFliesInsideItsShield)
{
	const glm::vec3 shield(0.0f, 0.0f, 0.0f);
	// Heights don't count, only the way across the map
	EXPECT_TRUE(SeesFlyer(shield, 20.0f, glm::vec3(10.0f, 50.0f, 0.0f)));
	// At the radius or beyond it the flyer is outside
	EXPECT_FALSE(SeesFlyer(shield, 20.0f, glm::vec3(20.0f, 0.0f, 0.0f)));
	EXPECT_FALSE(SeesFlyer(shield, 20.0f, glm::vec3(0.0f, 0.0f, 30.0f)));
}
