/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/HandActionPoint.h"
#include "ECS/HandCursorDepth.h"
#include "ECS/Registry.h"

using namespace openblack::ecs::hand_cursor_depth;
namespace hand_action_point = openblack::ecs::hand_action_point;

TEST(HandCursorDepth, TheDepthIsAlongTheCamerasForward)
{
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(ViewDepth(glm::vec3(0.0f, 0.0f, 10.0f), forward), 10.0f);
	// off the centre, the depth is shorter than the distance
	EXPECT_FLOAT_EQ(ViewDepth(glm::vec3(10.0f, 0.0f, 10.0f), forward), 10.0f);
}

TEST(HandCursorDepth, TheLandCountsItsMarginFurther)
{
	EXPECT_TRUE(LandBeforeObject(10.0f, 12.31f));
	EXPECT_FALSE(LandBeforeObject(10.0f, 12.3f)); // equal is not before
	EXPECT_FALSE(LandBeforeObject(10.0f, 12.0f));
}

TEST(HandCursorDepth, OffTheCentreTheMarginWeighsMoreThanAlongTheRay)
{
	// A ray at 45 degrees: the land at 49.716 and the object at 52.306 along the ray. Along the ray the land (with
	// its margin) would win; in depth in front of the camera it does not
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);
	const glm::vec3 ray = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f));
	EXPECT_TRUE(49.716f + k_LandMargin < 52.306f);
	EXPECT_FALSE(LandBeforeObject(ViewDepth(ray * 49.716f, forward), ViewDepth(ray * 52.306f, forward)));
	// straight ahead the two agree
	EXPECT_TRUE(LandBeforeObject(ViewDepth(forward * 49.716f, forward), ViewDepth(forward * 52.306f, forward)));
}

TEST(HandActionPoint, WithNoActionItIsTheFramesCollide)
{
	const glm::vec3 object(1.0f, 2.0f, 3.0f);
	const glm::vec3 land(4.0f, 5.0f, 6.0f);
	const glm::vec3 previous(7.0f, 8.0f, 9.0f);
	// the object or the land under the cursor, whatever the land, the action's object and the point were
	const auto point = hand_action_point::ActionPoint(
	    {.collide = object, .land = land, .actionObject = land, .previous = previous, .actionInProgress = false});
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(*point, object);
}

TEST(HandActionPoint, WithNoActionNothingUnderTheCursorClearsIt)
{
	// the cleared point, never the point the last frame left nor the hand's own
	const auto point = hand_action_point::ActionPoint(
	    {.collide = std::nullopt, .land = std::nullopt, .previous = glm::vec3(7.0f, 8.0f, 9.0f), .actionInProgress = false});
	EXPECT_FALSE(point.has_value());
}

TEST(HandActionPoint, AnActionKeepsItsObject)
{
	const glm::vec3 grabbed(1.0f, 2.0f, 3.0f);
	const auto point = hand_action_point::ActionPoint({.collide = glm::vec3(10.0f),
	                                                   .land = glm::vec3(20.0f),
	                                                   .actionObject = grabbed,
	                                                   .previous = glm::vec3(30.0f),
	                                                   .actionInProgress = true});
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(*point, grabbed);
}

TEST(HandActionPoint, AnActionWithoutAnObjectTakesTheLandNotAnotherObject)
{
	// another object under the cursor does not become the action's
	const glm::vec3 land(4.0f, 5.0f, 6.0f);
	const auto point = hand_action_point::ActionPoint(
	    {.collide = glm::vec3(10.0f), .land = land, .previous = glm::vec3(30.0f), .actionInProgress = true});
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(*point, land);
}

TEST(HandActionPoint, AnActionOverTheSkyKeepsThePoint)
{
	const glm::vec3 previous(7.0f, 8.0f, 9.0f);
	const auto kept = hand_action_point::ActionPoint({.previous = previous, .actionInProgress = true});
	ASSERT_TRUE(kept.has_value());
	EXPECT_EQ(*kept, previous);
	// a point cleared before the action started stays cleared
	EXPECT_FALSE(hand_action_point::ActionPoint({.actionInProgress = true}).has_value());
}

TEST(HandActionPoint, TheObjectIsTheOneUnderTheCursorWithNoAction)
{
	const auto kept = std::optional(entt::entity {3});
	const auto under = std::optional(entt::entity {7});
	EXPECT_EQ(hand_action_point::CollideObject(kept, under, false), under);
	EXPECT_FALSE(hand_action_point::CollideObject(kept, std::nullopt, false).has_value());
}

TEST(HandActionPoint, AnActionKeepsTheObjectWhateverIsUnderTheCursor)
{
	// the land grip over another object, over the land, or with nothing kept
	const auto kept = std::optional(entt::entity {3});
	EXPECT_EQ(hand_action_point::CollideObject(kept, entt::entity {7}, true), kept);
	EXPECT_EQ(hand_action_point::CollideObject(kept, std::nullopt, true), kept);
	EXPECT_FALSE(hand_action_point::CollideObject(std::nullopt, entt::entity {7}, true).has_value());
}

TEST(HandActionPoint, ADestroyedObjectIsNoLongerTheInterfaces)
{
	// a kept object burnt, eaten or deleted by a script during the action
	openblack::ecs::Registry registry;
	const auto kept = registry.Create();
	const auto other = registry.Create();
	EXPECT_EQ(hand_action_point::ExistingObject(kept, registry), kept);
	registry.Destroy(kept);
	EXPECT_FALSE(hand_action_point::ExistingObject(kept, registry).has_value());
	EXPECT_EQ(hand_action_point::ExistingObject(other, registry), other);
	EXPECT_FALSE(hand_action_point::ExistingObject(std::nullopt, registry).has_value());
}

TEST(HandActionPoint, ThePickPassesOverAnAbodeNotYetStarted)
{
	// the game's draw collision takes only what the hand can interact with: an abode at 0 built is passed over, so the
	// cursor finds what lies behind it, the land
	EXPECT_FALSE(hand_action_point::Interactable(true, true, 0.0f));
	EXPECT_TRUE(hand_action_point::Interactable(true, true, 0.4f));
	EXPECT_TRUE(hand_action_point::Interactable(true, true, 1.0f));
	// any other available object is taken, an unavailable one never
	EXPECT_TRUE(hand_action_point::Interactable(true, false, 0.0f));
	EXPECT_FALSE(hand_action_point::Interactable(false, false, 1.0f));
	EXPECT_FALSE(hand_action_point::Interactable(false, true, 1.0f));
}
