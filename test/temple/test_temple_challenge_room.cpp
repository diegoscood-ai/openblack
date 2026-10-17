/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstring>

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/TempleChallengeControls.h"
#include "3D/TempleChallengeRoom.h"

using namespace openblack;
using namespace openblack::temple_challenge_room;

namespace
{
float FromBits(uint32_t bits)
{
	float value = 0.0f;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}
uint32_t Bits(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}
} // namespace

TEST(TempleChallengeRoom, TheCameraBlendStepsAsTheOriginals)
{
	// the original's answers for a frame of 16 and of 33 ms (the frame's ms x 0.001f), from 0, 0.5, 0.99, 1 and 1.2
	struct Row
	{
		uint32_t dt;
		float blend;
		uint32_t falling;
		uint32_t rising;
	};
	constexpr Row k_Rows[] = {
	    {0x3C83126Fu, 0.0f, 0x00000000u, 0x3C449BA6u},  {0x3C83126Fu, 0.5f, 0x3EFA43FEu, 0x3F03126Fu},
	    {0x3C83126Fu, 0.99f, 0x3F7A92A3u, 0x3F800000u}, {0x3C83126Fu, 1.0f, 0x3F7D21FFu, 0x3F800000u},
	    {0x3C83126Fu, 1.2f, 0x3F7D21FFu, 0x3F800000u},  {0x3D072B02u, 0.0f, 0x00000000u, 0x3CCAC083u},
	    {0x3D072B02u, 0.5f, 0x3EF42C3Du, 0x3F065604u},  {0x3D072B02u, 0.99f, 0x3F7786C2u, 0x3F800000u},
	    {0x3D072B02u, 1.0f, 0x3F7A161Eu, 0x3F800000u},  {0x3D072B02u, 1.2f, 0x3F7A161Eu, 0x3F800000u},
	};
	for (const auto& row : k_Rows)
	{
		EXPECT_EQ(Bits(BlendStep(row.blend, false, FromBits(row.dt))), row.falling) << row.blend;
		EXPECT_EQ(Bits(BlendStep(row.blend, true, FromBits(row.dt))), row.rising) << row.blend;
	}
	// no time: as it was, but held to 1
	EXPECT_EQ(Bits(BlendStep(0.99f, false, 0.0f)), 0x3F7D70A4u);
	EXPECT_EQ(BlendStep(1.2f, false, 0.0f), 1.0f);
	EXPECT_EQ(BlendStep(1.2f, true, 0.0f), 1.0f);
	// unordered: falling becomes 0, rising stays unordered
	EXPECT_EQ(Bits(BlendStep(std::numeric_limits<float>::quiet_NaN(), false, 0.016f)), 0u);
	EXPECT_TRUE(std::isnan(BlendStep(std::numeric_limits<float>::quiet_NaN(), true, 0.016f)));
}

TEST(TempleChallengeRoom, TheControlsAreFoundByTheirNamesInAnyCase)
{
	// as the room's mesh names them
	EXPECT_EQ(ControlOf("LH_PICTURE_challenge_01"), (ControlSlot {.control = Control::Picture, .slot = 0}));
	EXPECT_EQ(ControlOf("LH_BUTTON_challenge_21"), (ControlSlot {.control = Control::Button, .slot = 20}));
	EXPECT_EQ(ControlOf("LH_SCROLL_CHALLENGE"), (ControlSlot {.control = Control::Scroll, .slot = 0}));
	// the three frames, the silver and gold ones not taken for the bronze one
	EXPECT_EQ(ControlOf("LH_FRAME_CHALLENGE_07"), (ControlSlot {.control = Control::Frame, .slot = 6}));
	EXPECT_EQ(ControlOf("lh_frame_challenge_silver_07"), (ControlSlot {.control = Control::SilverFrame, .slot = 6}));
	EXPECT_EQ(ControlOf("LH_Frame_Challenge_Gold_12"), (ControlSlot {.control = Control::GoldFrame, .slot = 11}));
	// not controls: the room's other submeshes, a slot past the 21st, or a name only beginning like one
	EXPECT_FALSE(ControlOf("LH_Wall").has_value());
	EXPECT_FALSE(ControlOf("LH_Picture_Challenge_22").has_value());
	EXPECT_FALSE(ControlOf("LH_Picture_Challenge_00").has_value());
	EXPECT_FALSE(ControlOf("LH_Picture_Challenge_011").has_value());
	EXPECT_FALSE(ControlOf("LH_Scroll_Challenge_01").has_value());
}

TEST(TempleChallengeRoom, ASlotWithoutAChallengeShowsOnlyItsBronzeFrame)
{
	const std::optional<Challenge> none;
	EXPECT_TRUE(Visible(Control::Frame, none));
	EXPECT_FALSE(Visible(Control::Picture, none));
	EXPECT_FALSE(Visible(Control::SilverFrame, none));
	EXPECT_FALSE(Visible(Control::GoldFrame, none));
	EXPECT_FALSE(Visible(Control::Button, none));
	EXPECT_TRUE(Visible(Control::Scroll, none));
}

TEST(TempleChallengeRoom, AChallengeShowsItsPictureItsFrameAndItsButtonWhileItCanBeChosen)
{
	const Challenge silver {.gold = false, .success = 0.5f};
	EXPECT_TRUE(Visible(Control::Picture, silver));
	EXPECT_TRUE(Visible(Control::SilverFrame, silver));
	EXPECT_FALSE(Visible(Control::GoldFrame, silver));
	EXPECT_FALSE(Visible(Control::Frame, silver));
	EXPECT_TRUE(Visible(Control::Button, silver));
	const Challenge gold {.gold = true, .success = 0.0f};
	EXPECT_TRUE(Visible(Control::GoldFrame, gold));
	EXPECT_FALSE(Visible(Control::SilverFrame, gold));
	// won (exactly 1, or unordered): no button, the picture and frame stay
	for (const float won : {1.0f, std::numeric_limits<float>::quiet_NaN()})
	{
		const Challenge done {.gold = false, .success = won};
		EXPECT_FALSE(Visible(Control::Button, done));
		EXPECT_TRUE(Visible(Control::Picture, done));
		EXPECT_TRUE(Visible(Control::SilverFrame, done));
	}
	// just under 1: still chosen
	EXPECT_TRUE(Visible(Control::Button, Challenge {.gold = false, .success = 0.99999994f}));
}

namespace
{
/// The challenge room's mesh, in part: its scroll, two slots' controls and a wall
const std::vector<std::string> k_Names = {
    "LH_Wall",
    "LH_PICTURE_challenge_01",
    "LH_FRAME_CHALLENGE_01",
    "LH_FRAME_CHALLENGE_SILVER_01",
    "LH_FRAME_CHALLENGE_GOLD_01",
    "LH_BUTTON_challenge_01",
    "LH_PICTURE_challenge_02",
    "LH_FRAME_CHALLENGE_02",
    "LH_BUTTON_challenge_02",
    "LH_SCROLL_CHALLENGE",
};
} // namespace

TEST(TempleChallengeControls, OnlyTheControlsOfTheChallengesFoundAreShown)
{
	TempleChallengeControls controls;
	controls.Find(k_Names);
	// nothing found yet: only the bronze frames show
	EXPECT_EQ(controls.GetHidden(), (std::vector<uint32_t> {1, 3, 4, 5, 6, 8}));
	// a gold challenge in the first slot, won: its picture and gold frame, no button
	const std::array<Challenge, 1> won {Challenge {.gold = true, .success = 1.0f}};
	controls.SetChallenges(won);
	EXPECT_EQ(controls.GetHidden(), (std::vector<uint32_t> {2, 3, 5, 6, 8}));
	EXPECT_FALSE(controls.ChallengeOf(1).has_value());
	// the controls shown glow; the scroll is the scrolls'
	EXPECT_TRUE(controls.IsControl(1));
	EXPECT_TRUE(controls.IsControl(4));
	EXPECT_TRUE(controls.IsControl(7));
	EXPECT_FALSE(controls.IsControl(5));
	EXPECT_FALSE(controls.IsControl(9));
	EXPECT_FALSE(controls.IsControl(0));
}

TEST(TempleChallengeControls, ALetGoOverTheSamePictureOrButtonIsItsClick)
{
	TempleChallengeControls controls;
	controls.Find(k_Names);
	const std::array<Challenge, 1> one {Challenge {.gold = false, .success = 0.5f}};
	controls.SetChallenges(one);
	std::optional<TempleChallengeControls::Click> click;
	// press and let go on the first picture
	EXPECT_TRUE(controls.Hold(true, 1u, click));
	EXPECT_FALSE(click.has_value());
	EXPECT_TRUE(controls.Hold(true, 1u, click));
	EXPECT_TRUE(controls.Hold(false, 1u, click));
	ASSERT_TRUE(click.has_value());
	EXPECT_EQ(*click, (TempleChallengeControls::Click {.control = Control::Picture, .slot = 0, .subMesh = 1}));
	// the button, let go elsewhere: no click, but the press was taken
	click.reset();
	EXPECT_TRUE(controls.Hold(true, 5u, click));
	EXPECT_TRUE(controls.Hold(false, 0u, click));
	EXPECT_FALSE(click.has_value());
	// a frame shown takes the press, but its let-go is no click
	EXPECT_TRUE(controls.Hold(true, 3u, click));
	EXPECT_TRUE(controls.Hold(false, 3u, click));
	EXPECT_FALSE(click.has_value());
	// the scroll (the scrolls' own), a hidden frame or picture, or the wall take no press
	for (const uint32_t subMesh : {9u, 2u, 6u, 0u})
	{
		EXPECT_FALSE(controls.Hold(true, subMesh, click)) << subMesh;
		EXPECT_FALSE(controls.Hold(false, subMesh, click)) << subMesh;
	}
	EXPECT_FALSE(click.has_value());
	// a press held from before it was over the button takes nothing
	EXPECT_FALSE(controls.Hold(true, 0u, click));
	EXPECT_FALSE(controls.Hold(true, 5u, click));
	EXPECT_FALSE(controls.Hold(false, 5u, click));
	EXPECT_FALSE(click.has_value());
}
