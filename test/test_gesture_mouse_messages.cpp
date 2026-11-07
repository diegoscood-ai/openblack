/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mouse messages the gesture samples come from: a move per frame in which the cursor moved, the input thread's own
// message every 40 ms of a still mouse while the window is active (a look every 300 ms while it is not), each giving
// the ms since the one before, and the handler that sends the position on once their sum is past 28 ms

#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "Magic/Gestures/GestureMouseMessages.h"

namespace messages = openblack::magic::gestures::mouse_messages;

namespace
{
std::vector<float> Frame(messages::Clock& clock, float ms, bool moved, bool active = true)
{
	std::vector<float> out;
	messages::Frame(clock, ms, moved, active, out);
	return out;
}
} // namespace

TEST(GestureMouseMessages, TheFirstMessageGivesNoTimeAndAMovingMouseGivesItsFramesTime)
{
	messages::Clock clock;
	EXPECT_EQ(Frame(clock, 16.0f, true), std::vector<float>({0.0f}));
	EXPECT_EQ(Frame(clock, 16.0f, true), std::vector<float>({16.0f}));
	EXPECT_EQ(Frame(clock, 20.0f, true), std::vector<float>({20.0f}));
}

TEST(GestureMouseMessages, AStillMouseGivesAMessageEveryFortyMilliseconds)
{
	messages::Clock clock;
	(void)Frame(clock, 10.0f, true);
	EXPECT_TRUE(Frame(clock, 20.0f, false).empty());
	// the wait that began with the move ends 40 ms after it
	EXPECT_EQ(Frame(clock, 20.0f, false), std::vector<float>({40.0f}));
	EXPECT_TRUE(Frame(clock, 30.0f, false).empty());
	EXPECT_EQ(Frame(clock, 10.0f, false), std::vector<float>({40.0f}));
	// a long frame holds several
	EXPECT_EQ(Frame(clock, 100.0f, false), std::vector<float>({40.0f, 40.0f}));
}

TEST(GestureMouseMessages, AMoveGivesTheTimeSinceTheThreadsLastMessageAndStartsItsWaitAgain)
{
	messages::Clock clock;
	(void)Frame(clock, 10.0f, true);
	EXPECT_EQ(Frame(clock, 50.0f, false), std::vector<float>({40.0f}));
	EXPECT_EQ(Frame(clock, 15.0f, true), std::vector<float>({25.0f}));
	EXPECT_TRUE(Frame(clock, 39.0f, false).empty());
	EXPECT_EQ(Frame(clock, 1.0f, false), std::vector<float>({40.0f}));
}

TEST(GestureMouseMessages, AMoveAtTheEndOfAFrameComesAfterTheThreadsMessageOfThatFrame)
{
	messages::Clock clock;
	(void)Frame(clock, 10.0f, true);
	(void)Frame(clock, 30.0f, false);
	EXPECT_EQ(Frame(clock, 30.0f, true), std::vector<float>({40.0f, 20.0f}));
}

TEST(GestureMouseMessages, AnInactiveWindowGetsNoneAndTheThreadLooksAgainEveryThreeHundredMilliseconds)
{
	messages::Clock clock;
	(void)Frame(clock, 10.0f, true);
	// the wait ends at 40 ms with the window inactive: a sleep of 300 ms, then another
	EXPECT_TRUE(Frame(clock, 40.0f, false, false).empty());
	EXPECT_EQ(clock.thread, messages::Thread::Sleeping);
	EXPECT_TRUE(Frame(clock, 299.0f, false, false).empty());
	EXPECT_TRUE(Frame(clock, 1.0f, false, false).empty());
	// active again: the message waits for the end of the sleep, then the thread waits 40 ms
	EXPECT_TRUE(Frame(clock, 299.0f, false, true).empty());
	EXPECT_EQ(Frame(clock, 1.0f, false, true), std::vector<float>({640.0f}));
	EXPECT_EQ(clock.thread, messages::Thread::Waiting);
	EXPECT_EQ(Frame(clock, 40.0f, false, true), std::vector<float>({40.0f}));
}

TEST(GestureMouseMessages, AMoveDoesNotWakeASleepingThread)
{
	messages::Clock clock;
	(void)Frame(clock, 10.0f, true);
	(void)Frame(clock, 40.0f, false, false); // asleep, 300 ms to go
	EXPECT_EQ(Frame(clock, 100.0f, true, true), std::vector<float>({140.0f}));
	EXPECT_EQ(Frame(clock, 200.0f, false, true), std::vector<float>({200.0f}));
}

TEST(GestureMouseMessages, TheHandlerSendsThePositionOncePastTwentyEightMillisecondsAndStartsAgainFromNothing)
{
	messages::Clock clock;
	EXPECT_FALSE(messages::Handle(clock, 0.0f));
	EXPECT_FALSE(messages::Handle(clock, -5.0f));
	EXPECT_FALSE(messages::Handle(clock, 28.0f)); // not past
	EXPECT_TRUE(messages::Handle(clock, 1.0f));
	EXPECT_EQ(clock.handlerMs, 0.0f);
	EXPECT_FALSE(messages::Handle(clock, 20.0f));
	EXPECT_TRUE(messages::Handle(clock, 20.0f)); // 40: back to 0, nothing kept
	EXPECT_EQ(clock.handlerMs, 0.0f);
	EXPECT_TRUE(messages::Handle(clock, 40.0f)); // each still message alone
}

TEST(GestureMouseMessages, AStillMouseFeedsASampleEveryFortyMilliseconds)
{
	messages::Clock clock;
	int samples = 0;
	const auto run = [&](float ms, bool moved) {
		for (const float m : Frame(clock, ms, moved))
		{
			samples += messages::Handle(clock, m) ? 1 : 0;
		}
	};
	run(10.0f, true);
	for (int frame = 0; frame < 300; ++frame)
	{
		run(10.0f, false); // 3 s
	}
	EXPECT_EQ(samples, 75);
}

TEST(GestureMouseMessages, AFrameTimeThatIsNotANumberCountsAsNone)
{
	messages::Clock clock;
	(void)Frame(clock, 10.0f, true);
	EXPECT_TRUE(Frame(clock, std::numeric_limits<float>::quiet_NaN(), false).empty());
	EXPECT_TRUE(Frame(clock, std::numeric_limits<float>::infinity(), false).empty());
	EXPECT_EQ(clock.threadElapsed, 0.0f);
}
