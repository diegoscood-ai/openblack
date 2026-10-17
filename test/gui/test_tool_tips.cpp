/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The interface's tooltips are the help system's: what they submit, in the help system's terms; and the tooltip the
// temple shows in each of its rooms (raffclar's tests); and the help system's tooltip's turn, with fake info.dat
// values

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "3D/TempleToolTips.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "GameClock.h"
#include "Gui/ToolTips.h"
#include "Help/InputPromptIcon.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::gui;

namespace
{
constexpr uint32_t k_Move = 0xE7E - 0xE73;
constexpr uint32_t k_ZoomIn = 0xE75 - 0xE73;
constexpr uint32_t k_Scroll = 0xEE2 - 0xE73;
constexpr uint32_t k_ZoomOut = 0xE8B - 0xE73;
constexpr uint32_t k_WorldRoom = 0xE9B - 0xE73;
} // namespace

TEST(ToolTips, AreTheHelpSystemsTooltipTexts)
{
	EXPECT_EQ(ToolTips::k_Count, 170u);
	// The first is HELP_TEXT_TOOLTIP_01, the help system's first tooltip
	EXPECT_EQ(ToolTips::TextOf(0), openblack::help::tooltips::k_First);
	EXPECT_EQ(ToolTips::TextOf(ToolTips::k_Count - 1), openblack::help::tooltips::k_First + 169u);
}

TEST(ToolTips, ShowTheButtonOfTheirAction)
{
	// The left button's binding selects, the right button's applies, and a tooltip of just words shows none
	EXPECT_EQ(ToolTips::BindingOf(ToolTipAction::Select), 1);
	EXPECT_EQ(ToolTips::BindingOf(ToolTipAction::Apply), 2);
	EXPECT_EQ(ToolTips::BindingOf(ToolTipAction::None), -1);
}

TEST(ToolTips, ArrowsAreTheHandsOwn)
{
	// The hand's tooltips ask for the same arrows: up and down, or all four
	EXPECT_EQ(ToolTipArrows::k_UpDown, 0x300u);
	EXPECT_EQ(ToolTipArrows::k_All, 0xF00u);
	EXPECT_EQ(ToolTipArrows::k_None, 0u);
}

TEST(TempleToolTips, MainRoomMovesZoomsAndLeadsThroughDoors)
{
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::Main, .inControl = true};
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_Move);
	EXPECT_EQ(toolTip.arrows, ToolTipArrows::k_All);

	input.overPool = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_ZoomIn);
	EXPECT_EQ(toolTip.arrows, ToolTipArrows::k_None);

	input.overPool = false;
	input.hoveredDoor = 1;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE99 - 0xE73);
	// The scroll's wall leaves Move
	input.hoveredDoor = 6;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_Move);
}

TEST(TempleToolTips, ScrollsZoomInScrollAndZoomOut)
{
	const std::array scrolls = {TempleScrolls::Control {.subMesh = 9, .focused = true}};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::Challenge, .inControl = true, .hoveredSubMesh = 9, .scrolls = scrolls};
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xEE5 - 0xE73);

	input.zoom = 1.0f;
	input.lookingAtScroll = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_Scroll);
	EXPECT_EQ(toolTip.arrows, ToolTipArrows::k_UpDown);

	input.hoveredSubMesh = std::nullopt;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_ZoomOut);
}

TEST(TempleToolTips, CreatureRoomKeepsTheScrollsArrows)
{
	const std::array scrolls = {TempleScrolls::Control {.subMesh = 3, .focused = false},
	                            TempleScrolls::Control {.subMesh = 4, .focused = true}};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::CreatureCave, .inControl = true, .scrolls = scrolls};
	// An unfocused scroll hovered shows its title, which isn't a tooltip
	input.hoveredSubMesh = 3;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_FALSE(toolTip.index.has_value());

	input.hoveredSubMesh = 4;
	input.zoom = 1.0f;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_Scroll);

	// Moving off, the arrows stay up and down
	input.hoveredSubMesh = std::nullopt;
	input.zoom = 0.0f;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_Move);
	EXPECT_EQ(toolTip.arrows, ToolTipArrows::k_UpDown);

	input.overWayBack = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_WorldRoom);
}

TEST(TempleToolTips, CreatureRoomTargets)
{
	constexpr uint32_t k_ExitTemple = 0xE98 - 0xE73;
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::CreatureCave, .inControl = true};
	input.caveTarget = CreatureCaveTargets::Target::Exit;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_ExitTemple);

	input.caveTarget = CreatureCaveTargets::Target::Belts;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_ZoomIn);

	// Zoomed to one, the camera zooms back wherever the cursor is
	input.zoomingToCaveTarget = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_ZoomOut);
	input.caveTarget = std::nullopt;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_ZoomOut);
}

TEST(TempleToolTips, NoneOnTheWayIn)
{
	TempleToolTip toolTip = k_FirstTempleToolTip;
	UpdateTempleToolTip(toolTip, {.room = TempleRoom::Main, .inControl = false});
	EXPECT_FALSE(toolTip.index.has_value());
	EXPECT_EQ(toolTip.action, ToolTipAction::None);
	// The options room leaves it so
	UpdateTempleToolTip(toolTip, {.room = TempleRoom::Options, .inControl = true});
	EXPECT_FALSE(toolTip.index.has_value());
}

namespace
{
/// A high priority text, shown 2 s, and a low priority one, shown 1 s, neither kept once nobody submits it
constexpr uint32_t k_High = 0xE80;
constexpr uint32_t k_Low = 0xE81;

/// The help system's tooltip with its own script state, clock, camera help and info.dat values, and no renderer,
/// texts or files: every text is empty
class HelpToolTipsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::scriptState::emplace<ecs::systems::ScriptState>();
		Locator::time::emplace<ecs::systems::TimeSystem>();
		Locator::cameraHelpSystem::emplace<ecs::systems::CameraHelpSystem>();
		Locator::rendererInterface::reset();
		Locator::helpTextSystem::reset();
		Locator::filesystem::reset();
		Locator::resources::reset();
		auto info = std::make_unique<InfoConstants>();
		info->toolTips.at(k_High - help::tooltips::k_First) = {.priority = 0.95f, .displayTime = 2.0f};
		info->toolTips.at(k_Low - help::tooltips::k_First) = {.priority = 0.5f, .displayTime = 1.0f};
		Locator::infoConstants::reset(info.release());
	}

	/// The tooltip's icon, none when it is not shown
	[[nodiscard]] static const help::input_prompt::InputPromptIcon* Icon()
	{
		const auto icons = help::input_prompt::All();
		return icons.empty() ? nullptr : &icons.front();
	}

private:
	test::RestoreService<Locator::scriptState> _scriptState;
	test::RestoreService<Locator::time> _time;
	test::RestoreService<Locator::cameraHelpSystem> _cameraHelp;
	test::RestoreService<Locator::rendererInterface> _renderer;
	test::RestoreService<Locator::helpTextSystem> _helpTexts;
	test::RestoreService<Locator::filesystem> _filesystem;
	test::RestoreService<Locator::resources> _resources;
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(HelpToolTipsTest, ANewLandStartsWithNoTooltipAndKeepsTheShowCounts)
{
	// the low text shown once: its icon fades in over 1 s
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 1.0f);

	// a high text forced every turn takes over at once, and its display time keeps the low one out. Every text is
	// empty here and an icon is kept while its words, button and anchor are the same, so the high text is anchored
	// apart for its own icon to be made
	help::tooltips::Submit(k_High, -1, 1, true);
	help::tooltips::ProcessTurn();
	help::tooltips::Submit(k_High, -1, 1, true);
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 0.0f);

	// the map's clear: the forced text submitted this turn is not kept, and nothing shows until a text is submitted
	help::tooltips::Force(k_High, 7.0f);
	help::tooltips::Reset();
	EXPECT_EQ(Icon(), nullptr);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(Icon(), nullptr);

	// the low text takes over at once, and the times it was shown are kept: it fades in over 2 s
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 2.0f);
}

TEST_F(HelpToolTipsTest, PausedOutsideTheCitadelTheTurnDeletesTheIconAndInsideKeepsIt)
{
	int submitted = 0;
	help::tooltips::SetStateSubmitter([&submitted]() { ++submitted; });
	help::tooltips::Submit(k_High, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(submitted, 1);

	// the pause itself leaves the icon as it is: only a turn deletes it
	game_clock::Pause(true);
	EXPECT_NE(Icon(), nullptr);

	// inside the citadel the temple's turn goes on: the hand submits and the icon is kept
	game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);
	help::tooltips::Submit(k_High, -1, 0, false);
	help::tooltips::ProcessTurn();
	EXPECT_NE(Icon(), nullptr);
	EXPECT_EQ(submitted, 2);

	// a turn paused outside it deletes the icon, and the hand does not submit
	game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	help::tooltips::Submit(k_High, -1, 0, false);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(Icon(), nullptr);
	EXPECT_EQ(submitted, 2);
}

TEST(InputPromptIcons, AreNotDrawnUnderAnActiveBoxWhileTheirFadeGoesOn)
{
	const auto handle = help::input_prompt::Create({.start = 0.0f, .target = 1.0f, .duration = 2.0f, .text = u"tip"});
	std::vector<graphics::InputPromptDraw> copy;

	// under a box the frame's copy is empty, and both updates still advance the fade
	help::input_prompt::Frame(0.25f, 0);
	help::input_prompt::Frame(0.25f, 1);
	help::input_prompt::Snapshot(copy, true);
	EXPECT_TRUE(copy.empty());
	const auto* icon = help::input_prompt::Get(handle);
	ASSERT_NE(icon, nullptr);
	EXPECT_EQ(icon->elapsed, 0.5f);
	const auto underTheBox = icon->passAlpha;
	EXPECT_GT(underTheBox.at(1), 0.0f);

	// once the box is gone the icon is drawn at once, with the alpha its fade reached
	help::input_prompt::Snapshot(copy, false);
	ASSERT_FALSE(copy.empty());
	EXPECT_EQ(copy.front().text, u"tip");
	EXPECT_EQ(copy.front().alpha, underTheBox);

	help::input_prompt::Destroy(handle);
}
