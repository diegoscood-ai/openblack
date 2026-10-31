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
#include "3D/TempleChallengePictures.h"
#include "3D/TempleChallengeRoom.h"
#include "3D/TempleToolTips.h"

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

TEST(TempleChallengeControls, TheSlotLookedAtIsForgottenOnEnteringAndByTheScroll)
{
	TempleChallengeControls controls;
	controls.Find(k_Names);
	EXPECT_FALSE(controls.GetHighDetailSlot().has_value());
	EXPECT_FALSE(controls.IsPictureLookedAt());
	EXPECT_FALSE(controls.GetLookedAtSlot().has_value());
	// the camera turns to the first picture: it is in high detail and looked at
	controls.LookAt(0);
	EXPECT_EQ(controls.GetHighDetailSlot(), 0u);
	EXPECT_TRUE(controls.IsPictureLookedAt());
	EXPECT_EQ(controls.GetLookedAtSlot(), 0u);
	// sent to the scroll, the picture stays in high detail but is no longer looked at
	controls.LookedAtScroll();
	EXPECT_EQ(controls.GetHighDetailSlot(), 0u);
	EXPECT_FALSE(controls.IsPictureLookedAt());
	EXPECT_FALSE(controls.GetLookedAtSlot().has_value());
	// coming into the room again forgets both
	controls.LookAt(2);
	controls.Enter();
	EXPECT_FALSE(controls.GetHighDetailSlot().has_value());
	EXPECT_FALSE(controls.IsPictureLookedAt());
}

TEST(TempleChallengeControls, TheTooltipsSeeThePicturesAndButtonsOfTheSlotsWithAChallenge)
{
	TempleChallengeControls controls;
	controls.Find(k_Names);
	EXPECT_TRUE(controls.GetToolTipControls().empty());
	// a won challenge in the first slot: its picture and its button (hidden, but seen as done)
	const std::array<Challenge, 1> won {Challenge {.gold = false, .success = 1.0f}};
	controls.SetChallenges(won);
	const auto seen = controls.GetToolTipControls();
	ASSERT_EQ(seen.size(), 2u);
	EXPECT_EQ(seen[0].subMesh, 1u);
	EXPECT_FALSE(seen[0].button);
	EXPECT_EQ(seen[0].place, 0u);
	EXPECT_TRUE(seen[0].done);
	EXPECT_EQ(seen[1].subMesh, 5u);
	EXPECT_TRUE(seen[1].button);
	// an unordered success is done too, any other can still be chosen
	const std::array<Challenge, 2> two {Challenge {.gold = false, .success = std::numeric_limits<float>::quiet_NaN()},
	                                    Challenge {.gold = true, .success = 0.5f}};
	controls.SetChallenges(two);
	const auto both = controls.GetToolTipControls();
	ASSERT_EQ(both.size(), 4u);
	EXPECT_TRUE(both[0].done);
	EXPECT_EQ(both[2].place, 1u);
	EXPECT_FALSE(both[2].done);
}

TEST(TempleChallengeRoom, TellsTheTimeTakenAndTheSuccess)
{
	// Ten turns a second: done after 37,234 turns is an hour, two minutes and three seconds
	const auto done = TimeTaken(1000, 38234, 99999, 100);
	EXPECT_EQ(done.hours, 1u);
	EXPECT_EQ(done.minutes, 2u);
	EXPECT_EQ(done.seconds, 3u);
	// Not done, it counts to now, the odd turns dropped
	const auto going = TimeTaken(1000, std::nullopt, 1609, 100);
	EXPECT_EQ(going.hours, 0u);
	EXPECT_EQ(going.minutes, 1u);
	EXPECT_EQ(going.seconds, 0u);
	EXPECT_EQ(DetailsLine(u"Time Taken:", done, u"Success:", 0.999f), u"Time Taken: 01:02:03  Success: 99%");
	EXPECT_EQ(TextAlpha(1.0f), 255);
	EXPECT_EQ(TextAlpha(0.5f), 127);
	EXPECT_EQ(TextAlpha(0.0f), 0);
}

TEST(TempleChallengeRoom, TheCloseLooksLinesAsTheOriginals)
{
	EXPECT_EQ(TextSize(false), 30.0f);
	EXPECT_EQ(TextSize(true), 33.0f);
	EXPECT_EQ(TitleText(5, 10), 5u);
	EXPECT_EQ(TitleText(10, 10), 0u);
	// Each line centred, the odd pixel dropped
	EXPECT_EQ(LineLeft(800, 300), 250.0f);
	EXPECT_EQ(LineLeft(800, 301), 249.0f);
	// Wider than the screen it goes off the screen to the right
	EXPECT_GT(LineLeft(640, 700), 640.0f);
}

TEST(TempleChallengeRoom, ToolTips)
{
	constexpr uint32_t k_Picture = 10;
	constexpr uint32_t k_Button = 12;
	constexpr uint32_t k_DonePicture = 11;
	const std::array controls {
	    TempleChallengeControl {.subMesh = k_Picture, .button = false, .place = 0, .done = false},
	    TempleChallengeControl {.subMesh = k_DonePicture, .button = false, .place = 1, .done = true},
	    TempleChallengeControl {.subMesh = k_Button, .button = true, .place = 0, .done = false},
	};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::Challenge, .inControl = true, .hoveredSubMesh = k_Picture};
	input.challenges = controls;
	// From afar a picture zooms in
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE75 - 0xE73);
	// Close up to it, it replays its challenge, and off it the camera zooms back out
	input.zoom = 1.0f;
	input.lookingAtScroll = true;
	input.challengeHighDetail = 0;
	input.pictureLookedAt = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE9F - 0xE73);
	input.hoveredSubMesh = std::nullopt;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE8B - 0xE73);
	// A done challenge's picture says nothing close up
	input.challengeHighDetail = 1;
	input.hoveredSubMesh = k_DonePicture;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_FALSE(toolTip.index.has_value());
	EXPECT_EQ(toolTip.action, gui::ToolTipAction::None);
	// The button replays it too
	input.hoveredSubMesh = k_Button;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE9F - 0xE73);
}

TEST(TempleChallengeRoom, ToolTipsInTheOrderOfTheRoomsSubMeshes)
{
	constexpr uint32_t k_Button = 5;
	constexpr uint32_t k_Picture = 10;
	constexpr uint32_t k_DonePicture = 11;
	const std::array controls {
	    TempleChallengeControl {.subMesh = k_Picture, .button = false, .place = 0, .done = false},
	    TempleChallengeControl {.subMesh = k_DonePicture, .button = false, .place = 1, .done = true},
	};
	// Part way in, neither afar nor close, a picture hovered leaves the room's own tooltip
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::Challenge, .inControl = true, .hoveredSubMesh = k_Picture};
	input.challenges = controls;
	input.zoom = 0.5f;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE7E - 0xE73);
	// Close up, a replay button earlier in the mesh than the picture looked at keeps its Replay: the picture after it
	// does not zoom out
	const std::array buttonFirst {
	    TempleChallengeControl {.subMesh = k_Button, .button = true, .place = 0, .done = false},
	    TempleChallengeControl {.subMesh = k_Picture, .button = false, .place = 0, .done = false},
	};
	input.challenges = buttonFirst;
	input.zoom = 1.0f;
	input.challengeHighDetail = 0;
	input.pictureLookedAt = true;
	input.hoveredSubMesh = k_Button;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE9F - 0xE73);
	// Close up with nothing hovered, only the picture looked at zooms out, even a won challenge's
	input.challenges = controls;
	input.challengeHighDetail = 1;
	input.hoveredSubMesh = std::nullopt;
	toolTip = k_FirstTempleToolTip;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE8B - 0xE73);
}

TEST(TempleChallengePictures, ALowDetailPictureForEverySlotAndAHighDetailOneForTheSlotLookedAt)
{
	using temple_challenge_pictures::Steps;
	using temple_challenge_pictures::StepsFor;
	using temple_challenge_pictures::Written;
	// no picture taken yet: nothing
	EXPECT_EQ(StepsFor({}, 0, true), Steps {});
	// the first picture: the low detail one, and the high detail one only for the slot looked at
	EXPECT_EQ(StepsFor({}, 1, false), (Steps {.writeSmall = true}));
	EXPECT_EQ(StepsFor({}, 1, true), (Steps {.writeSmall = true, .writeFull = true}));
	// written already: nothing again until the record has a new picture
	EXPECT_EQ(StepsFor(Written {.small = 1, .full = 1}, 1, true), Steps {});
	EXPECT_EQ(StepsFor(Written {.small = 1, .full = 1}, 2, true), (Steps {.writeSmall = true, .writeFull = true}));
	// another slot looked at: the high detail one goes
	EXPECT_EQ(StepsFor(Written {.small = 1, .full = 1}, 1, false), (Steps {.dropFull = true}));
	EXPECT_EQ(StepsFor(Written {.small = 1}, 1, false), Steps {});
}

TEST(TempleChallengePictures, ALowDetailPictureShownIsReadAgainOnlyAsThePlayerComesIn)
{
	using temple_challenge_pictures::Steps;
	using temple_challenge_pictures::StepsFor;
	using temple_challenge_pictures::Written;
	// A new picture of a slot already shown waits for the next coming into the room
	EXPECT_EQ(StepsFor(Written {.small = 1}, 2, false, false), Steps {});
	EXPECT_EQ(StepsFor(Written {.small = 1}, 2, false, true), (Steps {.writeSmall = true}));
	// A slot not yet shown is written at once
	EXPECT_EQ(StepsFor({}, 1, false, false), (Steps {.writeSmall = true}));
	// The high detail one follows its record's pictures
	EXPECT_EQ(StepsFor(Written {.small = 1, .full = 1}, 2, true, false), (Steps {.writeFull = true}));
}

TEST(TempleChallengeControls, ThePicturesOfTheSlotsWithAChallengeShowTheirRecordsPictures)
{
	TempleChallengeControls controls;
	controls.Find(k_Names);
	EXPECT_TRUE(controls.GetPictures().empty());
	const std::array<Challenge, 1> one {Challenge {.gold = false, .success = 0.5f}};
	controls.SetChallenges(one);
	const auto pictures = controls.GetPictures();
	ASSERT_EQ(pictures.size(), 1u);
	EXPECT_EQ(pictures[0].slot, 0u);
	EXPECT_EQ(pictures[0].subMesh, 1u);
}
