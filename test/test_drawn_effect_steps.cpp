/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Particles/DrawnEffectSteps.h"

using namespace openblack;

namespace
{
constexpr auto k_A = entt::entity {1};
constexpr auto k_B = entt::entity {2};
constexpr auto k_C = entt::entity {3};
constexpr auto k_D = entt::entity {4};
constexpr auto k_Other = entt::entity {9};
} // namespace

TEST(DrawnEffectSteps, TheListedThingsComeInTheListsOrderThenTheThingsOffTheBlocks)
{
	// The list holds C and A of these things and another thing; D is not on the land's blocks; B is on them, not listed
	const std::vector list {k_C, k_Other, k_A};
	const std::vector things {k_A, k_B, k_C, k_D};
	const auto order = particles::drawn_steps::DrawOrder(list, things, true, [](entt::entity thing) { return thing != k_D; });
	EXPECT_EQ(order, (std::vector {k_C, k_A, k_D}));
}

TEST(DrawnEffectSteps, WithoutACameraOrALandEveryThingComesInItsOwnOrder)
{
	const std::vector<entt::entity> list;
	const std::vector things {k_B, k_A};
	const auto order = particles::drawn_steps::DrawOrder(list, things, false, [](entt::entity) { return true; });
	EXPECT_EQ(order, things);
}

TEST(DrawnEffectSteps, TheThingsOffTheDrawOrderAreLeftOut)
{
	// B is on the land's blocks but not on the list, so the frame does not draw it, and its effects do not show
	const std::vector list {k_C, k_Other, k_A};
	const std::vector things {k_A, k_B, k_C, k_D};
	const auto order = particles::drawn_steps::DrawOrder(list, things, true, [](entt::entity thing) { return thing != k_D; });
	EXPECT_EQ(particles::drawn_steps::LeftOut(things, order), (std::vector {k_B}));
	EXPECT_TRUE(particles::drawn_steps::LeftOut(things, things).empty());
}
