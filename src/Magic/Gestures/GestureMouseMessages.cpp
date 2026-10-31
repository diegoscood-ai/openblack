/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureMouseMessages.h"

#include <cmath>

using namespace openblack::magic::gestures;

namespace
{
/// A message now: its ms is the time since the one before (0 for the very first)
void Send(mouse_messages::Clock& clock, std::vector<float>& messages)
{
	messages.push_back(clock.anyMessage ? clock.sinceMessage : 0.0f);
	clock.anyMessage = true;
	clock.sinceMessage = 0.0f;
}
} // namespace

void mouse_messages::Frame(Clock& clock, float frameMs, bool moved, bool threadMaySend, std::vector<float>& messages)
{
	// (openblack guard) a frame time that is not a number, or below 0, counts as none
	float left = std::isfinite(frameMs) && frameMs > 0.0f ? frameMs : 0.0f;
	while (true)
	{
		const float span = clock.thread == Thread::Waiting ? k_StillWaitMs : k_InactiveSleepMs;
		if (clock.threadElapsed + left < span)
		{
			clock.threadElapsed += left;
			clock.sinceMessage += left;
			break;
		}
		// the wait or the sleep ends within the frame: the thread looks whether it may send
		const float step = span - clock.threadElapsed;
		left -= step;
		clock.sinceMessage += step;
		clock.threadElapsed = 0.0f;
		if (threadMaySend)
		{
			Send(clock, messages);
			clock.thread = Thread::Waiting;
		}
		else
		{
			clock.thread = Thread::Sleeping;
		}
	}
	if (moved)
	{
		Send(clock, messages);
		// the move wakes a waiting thread, which waits again; a sleeping one sleeps on
		if (clock.thread == Thread::Waiting)
		{
			clock.threadElapsed = 0.0f;
		}
	}
}

bool mouse_messages::Handle(Clock& clock, float ms)
{
	if (ms <= 0.0f)
	{
		return false;
	}
	clock.handlerMs += ms;
	if (clock.handlerMs > k_SampleMs)
	{
		clock.handlerMs = 0.0f;
		return true;
	}
	return false;
}
