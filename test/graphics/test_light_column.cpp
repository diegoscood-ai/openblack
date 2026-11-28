/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "Graphics/LightColumn.h"

using namespace openblack;
using namespace openblack::graphics::light_column;

TEST(LightColumn, ShowsOnlyForAPlayerWhoseBorderShows)
{
	EXPECT_TRUE(Shown(PlayerNames::PLAYER_ONE, true));
	EXPECT_FALSE(Shown(PlayerNames::PLAYER_ONE, false));
	EXPECT_FALSE(Shown(PlayerNames::NEUTRAL, true));
}

TEST(LightColumn, TheTempleKeepsItsColumnForTheFirstPartOfItsDestruction)
{
	EXPECT_TRUE(TempleShown(false, 30.0f));
	EXPECT_TRUE(TempleShown(true, 21.7f));
	EXPECT_FALSE(TempleShown(true, 21.8f));
}

TEST(LightColumn, TheTemplesStrengthBeatsFrom55To110RoundedDown)
{
	EXPECT_EQ(TempleAlpha(0.0f), 55);
	EXPECT_EQ(TempleAlpha(1.0f), 110);
	EXPECT_EQ(TempleAlpha(0.5f), 82);
	EXPECT_EQ(TempleAlpha(-1.0f), 55);
	EXPECT_EQ(TempleAlpha(2.0f), 110);
	EXPECT_EQ(k_TownAlpha, 20);
}

TEST(LightColumn, TintIsTheOwnersColourAtItsStrength)
{
	const auto tint = Tint(0xFFFF4646u, 20);
	EXPECT_FLOAT_EQ(tint.r, 1.0f);
	EXPECT_FLOAT_EQ(tint.g, 70.0f / 255.0f);
	EXPECT_FLOAT_EQ(tint.b, 70.0f / 255.0f);
	EXPECT_FLOAT_EQ(tint.a, 20.0f / 255.0f);
}

TEST(LightColumn, ItStandsAtItsPointFacingTheEyeAsWideAsItsModelAndStretchedUpright)
{
	const glm::vec3 position {100.0f, 20.0f, 200.0f};
	const glm::vec3 eye {100.0f, 50.0f, 100.0f};
	const auto model = Model(position, eye);
	EXPECT_EQ(glm::vec3(model[3]), position);
	EXPECT_NEAR(glm::length(glm::vec3(model[0])), 1.0f, 1e-5f);
	EXPECT_NEAR(glm::length(glm::vec3(model[1])), 3.0f, 1e-5f);
	EXPECT_NEAR(glm::length(glm::vec3(model[2])), 1.0f, 1e-5f);
	// Its depth points away from the eye, across the ground, and its side is square to that
	const auto away = glm::normalize(glm::vec3(model[2]));
	EXPECT_NEAR(away.x, 0.0f, 1e-5f);
	EXPECT_NEAR(away.y, 0.0f, 1e-5f);
	EXPECT_NEAR(away.z, 1.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(glm::vec3(model[0]), glm::vec3(model[2])), 0.0f, 1e-4f);
}

// A started scroll's glow is drawn as a column, with its own mesh, in its colour at its alpha; a glow whose alpha is 0
// (a gold scroll's between the tops of its pulse) adds nothing and is not drawn
TEST(LightColumn, AScrollsGlowIsAColumnOfItsOwnMeshAndColourUnlessItsAlphaIsNone)
{
	const auto model = Model({10.0f, 2.0f, 30.0f}, {10.0f, 2.0f, 60.0f});
	const auto silver = ScrollGlow(model, 0x14B4DCFFu, 527u);
	ASSERT_TRUE(silver.has_value());
	EXPECT_EQ(silver->model, model);
	EXPECT_EQ(silver->colour, 0xB4DCFFu);
	EXPECT_EQ(silver->alpha, 0x14);
	EXPECT_EQ(silver->mesh, std::optional<entt::id_type>(527u));
	EXPECT_EQ(ScrollGlow(model, 0x50FFFF00u, 527u)->alpha, 0x50);
	EXPECT_FALSE(ScrollGlow(model, 0x00FFFF00u, 527u).has_value());
	// a village centre's own column keeps the column's mesh
	EXPECT_FALSE(Draw {}.mesh.has_value());
}
