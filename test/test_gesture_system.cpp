/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// GestureSystem: once a frame it runs the gestures' frame pass with the frame's game time, and on a new land the
// new-land pass, after dropping the events waiting. Events put in by hand are handed out once, oldest first. The circle
// drawn for the seed in the hand is remembered for five seconds. The getters and ForgetPath read and change the hand
// magic state the test services put in, empty, around each test.

#define LOCATOR_IMPLEMENTATIONS

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/GestureSystem.h"
#include "Magic/Gestures/PowerUpSystem.h"

using openblack::ecs::systems::GestureEvent;
using openblack::ecs::systems::GestureSystem;

namespace
{
/// A gesture system whose passes write down what they were called with
struct RecordedPasses
{
	std::vector<std::string> calls;
	std::vector<float> frameSeconds;

	GestureSystem Make()
	{
		return GestureSystem({
		    .frame =
		        [this](float seconds) {
			        calls.emplace_back("frame");
			        frameSeconds.push_back(seconds);
		        },
		    .newLand = [this] { calls.emplace_back("newLand"); },
		});
	}
};
} // namespace

TEST(GestureSystem, updateRunsTheFramePassWithTheFramesGameTime)
{
	RecordedPasses passes;
	auto gestures = passes.Make();

	gestures.Update({.seconds = 0.025f});
	gestures.Update({.seconds = 0.0f});

	EXPECT_EQ(passes.calls, (std::vector<std::string> {"frame", "frame"}));
	ASSERT_EQ(passes.frameSeconds.size(), 2u);
	EXPECT_FLOAT_EQ(passes.frameSeconds[0], 0.025f);
	EXPECT_FLOAT_EQ(passes.frameSeconds[1], 0.0f);
}

TEST(GestureSystem, resetDropsTheWaitingEventsAndRunsTheNewLandPass)
{
	RecordedPasses passes;
	auto gestures = passes.Make();
	gestures.Inject({.kind = GestureEvent::Kind::Shake});

	gestures.Reset();

	EXPECT_EQ(passes.calls, (std::vector<std::string> {"newLand"}));
	EXPECT_TRUE(gestures.TakeEvents().empty());
}

TEST(GestureSystem, injectedEventsAreHandedOutOnceOldestFirst)
{
	RecordedPasses passes;
	auto gestures = passes.Make();
	gestures.Inject({.kind = GestureEvent::Kind::Circle, .radius = 30.0f});
	gestures.Inject({.kind = GestureEvent::Kind::PowerUp, .powerUpLevel = 1});

	const auto events = gestures.TakeEvents();

	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].kind, GestureEvent::Kind::Circle);
	EXPECT_FLOAT_EQ(events[0].radius, 30.0f);
	EXPECT_EQ(events[1].kind, GestureEvent::Kind::PowerUp);
	EXPECT_EQ(events[1].powerUpLevel, 1);
	EXPECT_TRUE(gestures.TakeEvents().empty());
	// Nothing put in by the gesture passes themselves
	EXPECT_TRUE(passes.calls.empty());
}

TEST(GestureSystem, withoutPassesNothingRuns)
{
	GestureSystem gestures(GestureSystem::Passes {});
	gestures.Update({.seconds = 0.03f});
	gestures.Reset();
	EXPECT_TRUE(gestures.TakeEvents().empty());
}

TEST(GestureSystem, theCircleIsRememberedForFiveSeconds)
{
	using openblack::magic::gestures::CircleSecondsLeft;
	openblack::magic::gestures::InterfaceGestures state;
	EXPECT_FLOAT_EQ(CircleSecondsLeft(state), 0.0f);

	state.circlePending = true;
	state.circleTimer = 0.0f;
	EXPECT_FLOAT_EQ(CircleSecondsLeft(state), 5.0f);
	state.circleTimer = 1.5f;
	EXPECT_FLOAT_EQ(CircleSecondsLeft(state), 3.5f);
	// Past its life it is forgotten on the next pass; until then nothing is left
	state.circleTimer = 5.25f;
	EXPECT_FLOAT_EQ(CircleSecondsLeft(state), 0.0f);

	state.circlePending = false;
	state.circleTimer = 1.0f;
	EXPECT_FLOAT_EQ(CircleSecondsLeft(state), 0.0f);
}

TEST(GestureSystem, theCircleLeftIsReadFromTheHandMagicState)
{
	GestureSystem gestures(GestureSystem::Passes {});
	EXPECT_FLOAT_EQ(gestures.GetCircleSecondsLeft(), 0.0f);

	auto& state = openblack::magic::gestures::State();
	state.circlePending = true;
	state.circleTimer = 2.0f;
	EXPECT_FLOAT_EQ(gestures.GetCircleSecondsLeft(), 3.0f);
}

TEST(GestureSystem, forgetPathEmptiesTheHandsPath)
{
	GestureSystem gestures(GestureSystem::Passes {});
	auto& path = openblack::magic::gestures::State().system;
	path.AddSample({1.0f, 0.0f, 2.0f}, {10, 20});
	path.AddSample({3.0f, 0.0f, 4.0f}, {30, 40});
	ASSERT_EQ(path.Count(), 2u);

	gestures.ForgetPath();

	EXPECT_EQ(path.Count(), 0u);
	EXPECT_EQ(path.Head(), 0u);
}

TEST(GestureSystem, withoutAWindowTheScreenIsSquare)
{
	GestureSystem gestures(GestureSystem::Passes {});
	EXPECT_FLOAT_EQ(gestures.GetScreenAspect(), 1.0f);
}

TEST(GestureSystem, aGestureIsExpectedWhileTheSelectionIsOpenWithTheHandReady)
{
	namespace gestures = openblack::magic::gestures;
	GestureSystem system(GestureSystem::Passes {});
	EXPECT_FALSE(system.IsGesturing());
	EXPECT_FALSE(gestures::IsGesturing());

	gestures::State().selection.open = true;
	EXPECT_FALSE(system.IsGesturing());

	gestures::SetHandStatus({.handReady = true});
	EXPECT_TRUE(system.IsGesturing());

	gestures::State().selection.open = false;
	EXPECT_FALSE(system.IsGesturing());
}

TEST(GestureSystem, aGestureIsExpectedWhileASeedIsHeldWithItsTrailShown)
{
	namespace gestures = openblack::magic::gestures;
	GestureSystem system(GestureSystem::Passes {});
	// The held entity is no seed in a registry, so not a charging one; the trail flag decides before any seed lookup
	gestures::SetHandStatus({.heldSeed = static_cast<entt::entity>(7)});
	gestures::State().showHeldGestureTrail = true;
	EXPECT_TRUE(system.IsGesturing());

	gestures::SetHandStatus({});
	EXPECT_FALSE(system.IsGesturing());
}
