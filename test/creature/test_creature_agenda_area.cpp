/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureAgendaArea.h"

using namespace openblack;
using namespace openblack::creature_agenda_area;
using creature_mind::Movement;
using creature_mind::Step;

namespace
{
constexpr glm::vec2 k_KeptAt {100.0f, 200.0f};
constexpr float k_Radius = 10.0f;

Step WalkTo(glm::vec2 point, bool main = false)
{
	Step step {.kind = Step::Kind::Move, .main = main};
	step.movement = {.kind = Movement::Kind::ToPoint, .point = point};
	return step;
}

Step Wait(bool main = false)
{
	return Step {.kind = Step::Kind::Wait, .seconds = 1.0f, .main = main};
}
} // namespace

// Only a walk that ends outside the area leaves it; a walk about the creature counts from where it stands
TEST(CreatureAgendaArea, OnlyAWalkThatEndsOutsideTheAreaLeavesIt)
{
	EXPECT_TRUE(LeavesArea(WalkTo({120.0f, 200.0f}), k_KeptAt, k_KeptAt, k_Radius));
	EXPECT_FALSE(LeavesArea(WalkTo({105.0f, 200.0f}), k_KeptAt, k_KeptAt, k_Radius));
	Step nearby {.kind = Step::Kind::Move};
	nearby.movement = {.kind = Movement::Kind::Nearby, .point = {6.0f, 0.0f}};
	EXPECT_FALSE(LeavesArea(nearby, k_KeptAt, k_KeptAt, k_Radius));
	EXPECT_TRUE(LeavesArea(nearby, {105.0f, 200.0f}, k_KeptAt, k_Radius));
	EXPECT_FALSE(LeavesArea(Wait(), k_KeptAt, k_KeptAt, k_Radius));
}

// A forced plan with its own steps loses only the walks that leave the area, and goes on with the rest
TEST(CreatureAgendaArea, AForcedPlanWithItsOwnStepsOnlyLosesTheWalksOut)
{
	const auto kept =
	    WithoutWalksOut({Wait(), WalkTo({130.0f, 200.0f}), WalkTo({105.0f, 200.0f}), Wait()}, k_KeptAt, k_KeptAt, k_Radius);
	ASSERT_EQ(kept.size(), 3u);
	EXPECT_EQ(kept[0].kind, Step::Kind::Wait);
	EXPECT_EQ(kept[1].kind, Step::Kind::Move);
	EXPECT_EQ(kept[2].kind, Step::Kind::Wait);
}

// A main walk left out makes the step before it the main step
TEST(CreatureAgendaArea, AMainWalkLeftOutMakesTheStepBeforeItTheMainStep)
{
	const auto kept = WithoutWalksOut({Wait(), WalkTo({130.0f, 200.0f}, true), Wait()}, k_KeptAt, k_KeptAt, k_Radius);
	ASSERT_EQ(kept.size(), 2u);
	EXPECT_TRUE(kept[0].main);
	EXPECT_FALSE(kept[1].main);
}
