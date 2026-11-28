/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/ECS/DrawList/DrawSteps.h: the effects the original moves on only inside their object's Draw are stepped on the
// frames the object draw list ran and called that Draw (and, where the step waits for it, found the object on the
// screen); the debug switch steps them every frame. Those stepped in their Z object's callback go in the Z sorter's
// drain order. Fake lists: nothing here reads the locator.

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/DrawList/DrawSteps.h"

using namespace openblack::ecs;

namespace
{
entt::entity E(uint32_t id)
{
	return static_cast<entt::entity>(id);
}
} // namespace

TEST(DrawSteps, AnEffectStepsOnlyWhenTheListRanAndItsObjectsDrawFoundIt)
{
	EXPECT_TRUE(draw_steps::Steps(false, true, true, true));
	// the list did not run this frame (the temple, a film): it stands still
	EXPECT_FALSE(draw_steps::Steps(false, false, true, true));
	// the list did not reach its object
	EXPECT_FALSE(draw_steps::Steps(false, true, false, true));
	// its object's Draw found it off the screen
	EXPECT_FALSE(draw_steps::Steps(false, true, true, false));
	// the debug switch: every frame, as before
	EXPECT_TRUE(draw_steps::Steps(true, false, false, false));
}

TEST(DrawSteps, TheFramesListStepsTheObjectsItDrewOnTheScreenOrDrewAtAll)
{
	const std::vector drawn {E(1), E(2), E(3)};
	const std::vector onScreen {E(1), E(3)};
	const draw_steps::ListFrame list {.everyFrame = false, .ran = true, .drawn = drawn, .onScreen = onScreen};
	EXPECT_TRUE(list.StepsOnScreen(E(1)));
	EXPECT_FALSE(list.StepsOnScreen(E(2))); // drawn, off the screen
	EXPECT_TRUE(list.StepsDrawn(E(2)));     // a Draw that steps it on the screen or not
	EXPECT_FALSE(list.StepsDrawn(E(4)));    // not reached
	EXPECT_FALSE(list.StepsOnScreen(E(4)));

	// a frame the list did not run keeps the last frame's draws: they step nothing
	const draw_steps::ListFrame notRun {.everyFrame = false, .ran = false, .drawn = drawn, .onScreen = onScreen};
	EXPECT_FALSE(notRun.StepsOnScreen(E(1)));
	EXPECT_FALSE(notRun.StepsDrawn(E(1)));

	// with the switch, everything, drawn or not
	const draw_steps::ListFrame everyFrame {.everyFrame = true, .ran = false};
	EXPECT_TRUE(everyFrame.StepsOnScreen(E(4)));
	EXPECT_TRUE(everyFrame.StepsDrawn(E(4)));
}

TEST(DrawSteps, TheDrainCallsTheZObjectsBackFarToNearTheFirstQueuedFirstAmongEqualKeys)
{
	const glm::vec3 camera {0.0f, 0.0f, 0.0f};
	const std::vector<glm::vec3> points {{10.0f, 0.0f, 0.0f}, {0.0f, 30.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, {20.0f, 0.0f, 0.0f}};
	EXPECT_EQ(draw_steps::DrainOrder(points, camera), (std::vector<size_t> {1, 3, 0, 2}));
	EXPECT_TRUE(draw_steps::DrainOrder({}, camera).empty());
}
