/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "GameFrameOrder.h"

using namespace openblack::game_frame;

namespace
{
/// Records the steps that the frame does, in the order that it does them
class FrameRecorder
{
public:
	void operator()(Step step) { _steps.push_back(step); }
	[[nodiscard]] const std::vector<Step>& Steps() const { return _steps; }
	/// Where the step is in the record
	[[nodiscard]] std::ptrdiff_t IndexOf(Step step) const
	{
		return std::distance(_steps.begin(), std::ranges::find(_steps, step));
	}
	[[nodiscard]] bool Before(Step first, Step second) const { return IndexOf(first) < IndexOf(second); }

private:
	std::vector<Step> _steps;
};

FrameRecorder PlayOneFrame()
{
	FrameRecorder recorder;
	PlayFrame([&recorder](Step step) {
		recorder(step);
		return true;
	});
	return recorder;
}
} // namespace

TEST(GameFrameOrder, EachStepOnce)
{
	const auto frame = PlayOneFrame();
	ASSERT_EQ(frame.Steps().size(), static_cast<size_t>(Step::_Count));
	for (size_t i = 0; i < static_cast<size_t>(Step::_Count); ++i)
	{
		EXPECT_EQ(std::ranges::count(frame.Steps(), static_cast<Step>(i)), 1) << i;
	}
}

TEST(GameFrameOrder, FollowsTheGamesStages)
{
	EXPECT_TRUE(InGameOrder(k_Frame));
	// a step out of the stages' order is caught
	constexpr std::array k_Swapped = {Place {.step = Step::Hand, .stage = Stage::PreDraw},
	                                  Place {.step = Step::Camera, .stage = Stage::Camera}};
	EXPECT_FALSE(InGameOrder(k_Swapped));
}

TEST(GameFrameOrder, AStepThatReturnsFalseEndsTheFrame)
{
	std::vector<Step> steps;
	EXPECT_FALSE(PlayFrame([&steps](Step step) {
		steps.push_back(step);
		return step != Step::Turns;
	}));
	ASSERT_FALSE(steps.empty());
	EXPECT_EQ(steps.back(), Step::Turns);
}

// The order the frame has always had: the table plays it as it was written in Game::Update
TEST(GameFrameOrder, OpenblacksOrder)
{
	const auto frame = PlayOneFrame();
	const std::vector<Step> k_Expected = {
	    Step::FrameClock,   Step::Input,     Step::StopCheck,      Step::DebugGui,      Step::Editor, Step::Turns,
	    Step::SendCommands, Step::FrameTime, Step::Camera,         Step::Interface,     Step::Video,  Step::FrameEffects,
	    Step::TextShot,     Step::HandDemo,  Step::ObjectDrawList, Step::DrawnFrameEnd, Step::Draw3D, Step::Advisors,
	    Step::InputPrompts, Step::Hand,      Step::Audio,
	};
	EXPECT_EQ(frame.Steps(), k_Expected);
	// the turns before the camera, and the camera before the hand
	EXPECT_TRUE(frame.Before(Step::Turns, Step::Camera));
	EXPECT_TRUE(frame.Before(Step::Camera, Step::Hand));
}
