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

#include <cmath>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/TempleToolTips.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/HelpTextSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "GameClock.h"
#include "Graphics/KeyOrMouseLayout.h"
#include "Gui/ToolTips.h"
#include "Help/HelpSystem.h"
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

TEST(TempleToolTips, DoorsWaitForThePressToBeLetGo)
{
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::Main, .inControl = true, .hoveredDoor = 0};
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE95 - 0xE73);
	// A press held on the room itself, dragging its camera, keeps Move over a door, as one on a control does
	input.pressHeld = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, k_Move);
	// And the creature room's way back waits for it too
	UpdateTempleToolTip(toolTip, {.room = TempleRoom::CreatureCave, .inControl = true, .pressHeld = true, .overWayBack = true});
	EXPECT_NE(toolTip.index, 0xE9B - 0xE73);
}

TEST(TempleToolTips, ARoomsPressLastsUntilLetGo)
{
	EXPECT_TRUE(RoomPressHeld(false, true, false, false, false)); // goes down in the room
	EXPECT_TRUE(RoomPressHeld(true, true, true, false, false));   // still held
	EXPECT_FALSE(RoomPressHeld(true, false, true, false, false)); // let go
	EXPECT_FALSE(RoomPressHeld(true, true, true, true, false));   // the room changed under it
	EXPECT_FALSE(RoomPressHeld(false, true, true, false, false)); // held from before: not the new room's
	EXPECT_FALSE(RoomPressHeld(true, true, true, false, true));   // a dialog takes it
}

TEST(TempleToolTips, MainRoomDoorsInTheGamesOrder)
{
	// Each door's tooltip, the scrolls' wall (6) leaving Move
	constexpr std::array<uint32_t, 8> k_Expected {0xE95, 0xE99, 0xE98, 0xE9A, 0xE97, 0xE96, 0xE7E, 0xE94};
	for (uint32_t door = 0; door < k_Expected.size(); ++door)
	{
		TempleToolTip toolTip = k_FirstTempleToolTip;
		UpdateTempleToolTip(toolTip, {.room = TempleRoom::Main, .inControl = true, .hoveredDoor = door});
		EXPECT_EQ(toolTip.index, k_Expected.at(door) - 0xE73) << door;
	}
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
/// Two more texts, one at the Minimum level's lowest priority and one just under it
constexpr uint32_t k_AtTheMinimum = 0xE82;
constexpr uint32_t k_UnderTheMinimum = 0xE83;

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
		info->toolTips.at(k_AtTheMinimum - help::tooltips::k_First) = {.priority = 0.9f, .displayTime = 1.0f};
		info->toolTips.at(k_UnderTheMinimum - help::tooltips::k_First) = {.priority = std::nextafter(0.9f, 0.0f),
		                                                                  .displayTime = 1.0f};
		// the town's population, kept two seconds after the focus
		info->toolTips.at(0xEFC -
		                  help::tooltips::k_First) = {.priority = 0.5f, .displayTime = 1.0f, .displayTimeAfterFocus = 2.0f};
		// the altar's prayer energy, a text the builder shows at once
		info->toolTips.at(0xEE9 - help::tooltips::k_First) = {.priority = 0.5f, .displayTime = 1.0f};
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

	// the map's clear: the forced text submitted this turn is not kept; the icon stays drawn until the next turn's pass,
	// which deletes it as nothing is submitted
	help::tooltips::Force(k_High, 7.0f);
	help::tooltips::Reset();
	EXPECT_NE(Icon(), nullptr);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(Icon(), nullptr);

	// the low text takes over at once, and the times it was shown are kept: it fades in over 2 s
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 2.0f);
}

TEST_F(HelpToolTipsTest, ANewLandKeepsTheIconDrawnAndTheSameTextKeepsItsIcon)
{
	// the low text shown once: its icon fades in over 1 s
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 1.0f);

	// the map's clear leaves the icon drawn
	help::tooltips::Reset();
	ASSERT_NE(Icon(), nullptr);

	// the same text on the new land's first turn takes over again (now shown twice), and keeps the icon it had: it does
	// not fade in again
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 1.0f);
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

TEST_F(HelpToolTipsTest, ATownsPopulationIsReadAgainEveryTurnFromTheObjectUnderTheHand)
{
	// 0xEFC "Population: %3.0f/places": the texts are empty here, so the built text is only "/" and the places
	constexpr uint32_t k_Population = 0xEFC;
	std::optional<int32_t> places = 5;
	help::tooltips::SetObjectNumbers([&places](uint32_t text) -> std::optional<help::tooltips::Numbers> {
		if (text != k_Population || !places)
		{
			return std::nullopt;
		}
		return help::tooltips::Numbers {.outOf = *places};
	});
	// submitted once with other numbers: the builder takes this turn's object's
	help::tooltips::Submit(k_Population, -1, 0, false, help::tooltips::Numbers {.outOf = 9});
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->text, u"/5");
	// the next turn, kept by its time after the focus with no submit, it reads the object again
	places = 6;
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->text, u"/6");
	// with no town under the hand the text shows as it is written
	places.reset();
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->text, u"");
}

TEST_F(HelpToolTipsTest, ASpellIconsChargeIsWrittenAfterItsNameAndAHalfRoundsUp)
{
	// "%s %3.0f%%": the texts are empty here, so the built text is a space and the percent; an exact half rounds up, as
	// the game's C library writes it (12.5 shows as 13, 0.5 as 1)
	for (const auto& [percent, shown] : std::vector<std::pair<double, std::u16string>> {
	         {45.0, u"  45%"}, {12.5, u"  13%"}, {0.5, u"   1%"}, {99.5, u" 100%"}, {0.125, u"   0%"}})
	{
		help::tooltips::Submit(k_Low, -1, 0, true, help::tooltips::Numbers {.percentAfter = percent});
		help::tooltips::ProcessTurn();
		ASSERT_NE(Icon(), nullptr);
		EXPECT_EQ(Icon()->text, shown) << percent;
	}
}

TEST_F(HelpToolTipsTest, TheBuildersObjectTextsShowAtOnce)
{
	// the altar's prayer energy, submitted not forced, shows at once as the builder writes it, where another text fades
	// in over the times it was shown
	help::tooltips::Submit(0xEE9, -1, 0, false, help::tooltips::Numbers {.values = {12.0}, .count = 1});
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 0.0f);
	EXPECT_TRUE(help::tooltips::IsShownForced());

	EXPECT_TRUE(help::tooltips::BuilderShowsAtOnce(0xEE8, false));
	EXPECT_TRUE(help::tooltips::BuilderShowsAtOnce(0xECE, false));
	EXPECT_TRUE(help::tooltips::BuilderShowsAtOnce(0xEFF, false));
	// a desire flag's need and a workshop's production only with their number written
	EXPECT_TRUE(help::tooltips::BuilderShowsAtOnce(0xE88, true));
	EXPECT_FALSE(help::tooltips::BuilderShowsAtOnce(0xE88, false));
	EXPECT_TRUE(help::tooltips::BuilderShowsAtOnce(0xEF3, true));
	EXPECT_FALSE(help::tooltips::BuilderShowsAtOnce(0xEF3, false));
	EXPECT_FALSE(help::tooltips::BuilderShowsAtOnce(k_Low, true));
}

TEST_F(HelpToolTipsTest, AtTheNoneLevelNoTooltipIsShownAndTheHandDoesNotSubmit)
{
	int submitted = 0;
	help::tooltips::SetStateSubmitter([&submitted]() { ++submitted; });
	help::tooltips::Submit(k_High, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(submitted, 1);

	// level 0: the turn deletes the icon before the hand submits, and nothing submitted makes one
	help::tooltips::SetLevel(0);
	help::tooltips::Submit(k_High, -1, 0, true);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(Icon(), nullptr);
	EXPECT_EQ(submitted, 1);
	help::tooltips::Submit(k_Low, -1, 0, true);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(Icon(), nullptr);
	EXPECT_EQ(submitted, 1);
}

TEST_F(HelpToolTipsTest, AtTheMinimumLevelOnlyPrioritiesFromNineTenthsShow)
{
	help::tooltips::SetLevel(1);
	// under 0.9, even just under it and forced, the text is not taken
	help::tooltips::Submit(k_Low, -1, 0, true);
	help::tooltips::Submit(k_UnderTheMinimum, -1, 0, true);
	help::tooltips::ProcessTurn();
	EXPECT_FALSE(help::tooltips::ShownText().has_value());
	EXPECT_EQ(Icon(), nullptr);
	// from 0.9 it shows
	help::tooltips::Submit(k_AtTheMinimum, -1, 0, false);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(help::tooltips::ShownText(), k_AtTheMinimum);
	EXPECT_NE(Icon(), nullptr);
	help::tooltips::Submit(k_High, -1, 1, true);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(help::tooltips::ShownText(), k_High);
	EXPECT_NE(Icon(), nullptr);
}

TEST_F(HelpToolTipsTest, ALowTextKeptAfterItsFocusGoesAtTheMinimumLevel)
{
	// the town's population, priority 0.5, kept two seconds after nobody submits it
	constexpr uint32_t k_Population = 0xEFC;
	help::tooltips::Submit(k_Population, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	// at the Minimum level its next turn deletes it, though its time after the focus is not over
	help::tooltips::SetLevel(1);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(Icon(), nullptr);
}

TEST_F(HelpToolTipsTest, AtTheAllLevelATextShowsAtOnceAndStays)
{
	// shown twice at level 2: the second time it fades in over 2 s
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	help::tooltips::ProcessTurn();
	ASSERT_EQ(Icon(), nullptr);
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 2.0f);
	help::tooltips::ProcessTurn();
	ASSERT_EQ(Icon(), nullptr);

	// at level 3 the same text, shown a third time, has no fade-in
	help::tooltips::SetLevel(3);
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->duration, 0.0f);
	EXPECT_EQ(Icon()->target, 1.0f);

	// fully shown, it stays past its display time of 1 s (25 turns) while it is submitted
	help::input_prompt::Frame(1.0f, 0);
	ASSERT_EQ(Icon()->current, 1.0f);
	for (int turn = 0; turn < 40; ++turn)
	{
		help::tooltips::Submit(k_Low, -1, 0, false);
		help::tooltips::ProcessTurn();
	}
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->target, 1.0f);
	EXPECT_EQ(Icon()->current, 1.0f);
}

TEST_F(HelpToolTipsTest, AtTheIntelligentLevelALowTextFadesOutOnceItsDisplayTimeIsOver)
{
	// the same at level 2: once its 25 turns are over, the fully shown low text fades out in 1 s and stays
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	help::input_prompt::Frame(1.0f, 0);
	ASSERT_EQ(Icon()->current, 1.0f);
	for (int turn = 0; turn < 40; ++turn)
	{
		help::tooltips::Submit(k_Low, -1, 0, false);
		help::tooltips::ProcessTurn();
	}
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->target, 0.0f);
	EXPECT_EQ(Icon()->duration, 1.0f);
}

TEST_F(HelpToolTipsTest, ALevelOutOfTheFourShowsALowTextWithoutTheIntelligentLevelsFadeOut)
{
	// A profile's level 5 is taken as it is: no minimum's filter, and the low text stays past its display time
	help::tooltips::SetLevel(5);
	EXPECT_EQ(help::tooltips::Level(), 5);
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	help::input_prompt::Frame(1.0f, 0);
	ASSERT_EQ(Icon()->current, 1.0f);
	for (int turn = 0; turn < 40; ++turn)
	{
		help::tooltips::Submit(k_Low, -1, 0, false);
		help::tooltips::ProcessTurn();
	}
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->target, 1.0f);
}

TEST_F(HelpToolTipsTest, UnderAScriptsWideScreenWithAnOwnerNoTooltipIsShown)
{
	ecs::systems::HelpTextSystem texts;
	help::Start(texts, help::HelpSystem::Info {8, 5}, {}, {});
	int submitted = 0;
	help::tooltips::SetStateSubmitter([&submitted]() { ++submitted; });

	// the wide screen with no owner leaves the tooltip
	help::Get()->SetWideScreen(1, 0);
	help::tooltips::Submit(k_High, -1, 0, false);
	help::tooltips::ProcessTurn();
	EXPECT_NE(Icon(), nullptr);
	EXPECT_EQ(submitted, 1);

	// a script's wide screen deletes it, and the hand does not submit
	help::Get()->SetWideScreen(0, 0);
	help::Get()->SetWideScreen(1, 7);
	help::tooltips::Submit(k_High, -1, 0, true);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(Icon(), nullptr);
	EXPECT_EQ(submitted, 1);

	// once it ends the tooltip comes back
	help::Get()->SetWideScreen(0, 0);
	help::tooltips::Submit(k_High, -1, 0, true);
	help::tooltips::ProcessTurn();
	EXPECT_NE(Icon(), nullptr);
	EXPECT_EQ(submitted, 2);
	help::Shutdown();
}

// A tooltip whose action has no button (-1) is shown as words only: animation 0, no click and the row with no
// picture (3)
TEST_F(HelpToolTipsTest, ATooltipWithNoButtonIsShownAsWordsOnly)
{
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	ASSERT_NE(Icon(), nullptr);
	EXPECT_EQ(Icon()->row, 3);
	EXPECT_EQ(Icon()->animType, 0);
	EXPECT_EQ(Icon()->clickType, 0);
	EXPECT_TRUE(Icon()->keyName.empty());
}

TEST(ToolTips, TheIconsSizeIsATwentyFifthOfTheScreensHeight)
{
	// S = H / 25, cut to whole pixels
	EXPECT_EQ(help::tooltips::IconSize(480), 19);
	EXPECT_EQ(help::tooltips::IconSize(600), 24);
	EXPECT_EQ(help::tooltips::IconSize(768), 30);
	EXPECT_EQ(help::tooltips::IconSize(1080), 43);
	EXPECT_EQ(help::tooltips::IconSize(24), 0);
}

TEST(InputPromptIcons, TheBoxAtTheHandIsKeptOnTheScreen)
{
	using graphics::key_or_mouse::BoxAtTheHand;
	// the hand's point cut to whole pixels, plus the offsets
	EXPECT_EQ(BoxAtTheHand({100.7f, 50.2f}, 0, 0, 30, 1024, 768), glm::ivec2(100, 50));
	EXPECT_EQ(BoxAtTheHand({100.7f, 50.2f}, 5, -8, 30, 1024, 768), glm::ivec2(105, 42));
	// never off the top or the left
	EXPECT_EQ(BoxAtTheHand({-5.0f, -3.0f}, 0, 0, 30, 1024, 768), glm::ivec2(0, 0));
	EXPECT_EQ(BoxAtTheHand({-0.5f, 2.0f}, 0, -4, 30, 1024, 768), glm::ivec2(0, 0));
	// nor past the right or the bottom: the box's size S stays inside
	EXPECT_EQ(BoxAtTheHand({1020.0f, 760.0f}, 0, 0, 30, 1024, 768), glm::ivec2(994, 738));
	EXPECT_EQ(BoxAtTheHand({994.0f, 738.0f}, 0, 0, 30, 1024, 768), glm::ivec2(994, 738));
	EXPECT_EQ(BoxAtTheHand({995.0f, 739.0f}, 0, 0, 30, 1024, 768), glm::ivec2(994, 738));
}

// The glow behind an icon or its words: a 9-slice whose corners are squares of half the box's height, centred on the
// box's corners, over the soft blob of atmos.raw
TEST(InputPromptIcons, TheGlowPanelIsANineSliceWithCornersOfHalfTheBoxsHeight)
{
	using graphics::key_or_mouse::PanelSlices;
	// a box 30 high: b = 7
	const auto slices = PanelSlices(100, 200, 160, 230);
	const auto& topLeft = slices.at(0);
	EXPECT_EQ(topLeft.x0, 93.0f);
	EXPECT_EQ(topLeft.y0, 193.0f);
	EXPECT_EQ(topLeft.x1, 107.0f);
	EXPECT_EQ(topLeft.y1, 207.0f);
	EXPECT_EQ(topLeft.u0, 0.0f);
	EXPECT_EQ(topLeft.v0, 0.25f);
	EXPECT_EQ(topLeft.u1, 0.078125f);
	EXPECT_EQ(topLeft.v1, 0.328125f);
	const auto& middle = slices.at(4);
	EXPECT_EQ(middle.x0, 107.0f);
	EXPECT_EQ(middle.x1, 153.0f);
	EXPECT_EQ(middle.y0, 207.0f);
	EXPECT_EQ(middle.y1, 223.0f);
	EXPECT_EQ(middle.u0, 0.078125f);
	EXPECT_EQ(middle.u1, 0.16796875f);
	EXPECT_EQ(middle.v0, 0.328125f);
	EXPECT_EQ(middle.v1, 0.41796875f);
	const auto& bottomRight = slices.at(8);
	EXPECT_EQ(bottomRight.x1, 167.0f);
	EXPECT_EQ(bottomRight.y1, 237.0f);
	EXPECT_EQ(bottomRight.u1, 0.24609375f);
	EXPECT_EQ(bottomRight.v1, 0.49609375f);
	// the quarter is cut towards zero: a box 10 high has b = 2, one turned upside down b = -2
	EXPECT_EQ(PanelSlices(0, 0, 40, 10).at(0).x0, -2.0f);
	EXPECT_EQ(PanelSlices(0, 10, 40, 0).at(0).x0, 2.0f);
}

TEST(InputPromptIcons, TheTooltipGoesRightOfTheHandAndLeftOfItNearTheRightOfTheScreen)
{
	using graphics::key_or_mouse::Layout;
	// the tooltip's align (0x16) picks the side; a screen 900 wide: past 600 it goes left of the hand, under 300 right
	constexpr uint32_t k_Align = 0x16;
	constexpr int32_t k_Width = 900;
	constexpr int32_t k_S = 30;
	constexpr float k_Total = 100.5f;
	int32_t side = 0;
	// in the middle third the side stays as it was: right of the hand, starting S / 2 left of it
	auto out = Layout(500, k_Total, k_S, k_Align, k_Width, side);
	EXPECT_EQ(out.x, 485);
	EXPECT_FALSE(out.rightAligned);
	EXPECT_EQ(Layout(600, k_Total, k_S, k_Align, k_Width, side).x, 585);
	EXPECT_EQ(side, 0);
	// past two thirds: left of it, ending S / 2 right of the hand, cut to whole pixels
	out = Layout(601, k_Total, k_S, k_Align, k_Width, side);
	EXPECT_EQ(side, 1);
	EXPECT_EQ(out.x, 515);
	EXPECT_TRUE(out.rightAligned);
	// back in the middle third it stays left
	out = Layout(500, k_Total, k_S, k_Align, k_Width, side);
	EXPECT_EQ(out.x, 414);
	EXPECT_TRUE(out.rightAligned);
	EXPECT_EQ(Layout(300, k_Total, k_S, k_Align, k_Width, side).x, 214);
	EXPECT_EQ(side, 1);
	// under a third it goes right again
	out = Layout(299, k_Total, k_S, k_Align, k_Width, side);
	EXPECT_EQ(side, 0);
	EXPECT_EQ(out.x, 284);
	EXPECT_FALSE(out.rightAligned);
	// an odd S: half of it cut to a whole pixel
	EXPECT_EQ(Layout(100, k_Total, 31, k_Align, k_Width, side).x, 85);
}

TEST(InputPromptIcons, WithoutTheSideBitTheBoxIsRightAlignedOnlyWhenAskedAndTheSideIsKept)
{
	using graphics::key_or_mouse::Layout;
	int32_t side = 1;
	// align & 1: it ends at the anchor, cut toward zero
	auto out = Layout(500, 100.5f, 30, 1, 900, side);
	EXPECT_EQ(out.x, 399);
	EXPECT_TRUE(out.rightAligned);
	EXPECT_EQ(Layout(10, 100.5f, 30, 1, 900, side).x, -90);
	// neither bit: it starts at the anchor
	out = Layout(800, 100.5f, 30, 0, 900, side);
	EXPECT_EQ(out.x, 800);
	EXPECT_FALSE(out.rightAligned);
	EXPECT_EQ(side, 1);
	// with the side bit the side wins over align & 1
	side = 0;
	out = Layout(500, 100.5f, 30, 3, 900, side);
	EXPECT_EQ(out.x, 485);
	EXPECT_FALSE(out.rightAligned);
}

TEST(InputPromptIcons, TheMousePictureLightsTheButtonToPressInItsRow)
{
	using graphics::key_or_mouse::MousePictureCell;
	// The original's two jump tables over click types 0 to 16: 0 the row's plain mouse, 2, 3 and 4 the row's cell 1, 2
	// or 3 while lit (else its plain one), 8 and 16 the wheel, and 1, 5 to 7, 9 to 15 and anything else the right
	// button's cell mirrored, the left button
	constexpr int32_t k_Row = 2;
	EXPECT_EQ(MousePictureCell(0, k_Row, true, 0).cell, 8);
	EXPECT_EQ(MousePictureCell(0, k_Row, false, 0).cell, 8);
	EXPECT_EQ(MousePictureCell(2, k_Row, true, 0).cell, 9);
	EXPECT_EQ(MousePictureCell(2, k_Row, false, 0).cell, 8);
	EXPECT_EQ(MousePictureCell(3, k_Row, true, 0).cell, 10);
	EXPECT_EQ(MousePictureCell(3, k_Row, false, 0).cell, 8);
	EXPECT_EQ(MousePictureCell(4, k_Row, true, 0).cell, 11);
	EXPECT_EQ(MousePictureCell(4, k_Row, false, 0).cell, 8);
	for (const int32_t clickType : {0, 2, 3, 4, 8, 16})
	{
		EXPECT_FALSE(MousePictureCell(clickType, k_Row, true, 0).mirrored) << clickType;
	}
	for (const int32_t clickType : {1, 5, 6, 7, 9, 12, 15, 17, -1})
	{
		const auto left = MousePictureCell(clickType, k_Row, true, 0);
		EXPECT_TRUE(left.mirrored) << clickType;
		EXPECT_EQ(left.cell, 9) << clickType;
		EXPECT_EQ(MousePictureCell(clickType, k_Row, false, 0).cell, 8) << clickType;
	}
	// The wheel's own frames on the last row, whatever the row asked: down from 15 to 12, up from 12 to 15, resting on
	// 11 the fifth tenth
	EXPECT_EQ(MousePictureCell(8, 0, true, 0).cell, 15);
	EXPECT_EQ(MousePictureCell(8, 1, false, 3).cell, 12);
	EXPECT_EQ(MousePictureCell(8, k_Row, true, 4).cell, 11);
	EXPECT_EQ(MousePictureCell(16, 0, true, 0).cell, 12);
	EXPECT_EQ(MousePictureCell(16, 1, false, 3).cell, 15);
	EXPECT_EQ(MousePictureCell(16, k_Row, true, 4).cell, 11);
	// A negative row gives a negative cell, which draws nothing
	EXPECT_LT(MousePictureCell(2, -1, true, 0).cell, 0);
}

TEST_F(HelpToolTipsTest, ATextNobodySubmitsLingersItsTimeAfterTheFocus)
{
	// the time after the focus in game turns, as the original counts it: trunc(1 / 0.4 x time x 10), each step in single
	// precision, so 2 s are 50 turns and 0.3 s are 7; it counts down only on the turns nobody submits the text
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->toolTips.at(k_High - help::tooltips::k_First).displayTimeAfterFocus = 0.3f;
	Locator::infoConstants::reset(info.release());
	constexpr uint32_t k_Population = 0xEFC;
	help::tooltips::Submit(k_Population, -1, 0, false);
	help::tooltips::ProcessTurn();
	for (int turn = 1; turn <= 50; ++turn)
	{
		help::tooltips::ProcessTurn();
		EXPECT_EQ(help::tooltips::ShownText(), std::optional(k_Population)) << turn;
	}
	help::tooltips::ProcessTurn();
	EXPECT_FALSE(help::tooltips::ShownText().has_value());

	help::tooltips::Submit(k_High, -1, 0, false);
	help::tooltips::ProcessTurn();
	for (int turn = 1; turn <= 7; ++turn)
	{
		help::tooltips::ProcessTurn();
		EXPECT_EQ(help::tooltips::ShownText(), std::optional(k_High)) << turn;
	}
	help::tooltips::ProcessTurn();
	EXPECT_FALSE(help::tooltips::ShownText().has_value());
	// a text with no time after the focus goes on the first turn nobody submits it
	help::tooltips::Submit(k_Low, -1, 0, false);
	help::tooltips::ProcessTurn();
	EXPECT_EQ(help::tooltips::ShownText(), std::optional(k_Low));
	help::tooltips::ProcessTurn();
	EXPECT_FALSE(help::tooltips::ShownText().has_value());
}

TEST(InputPromptIcons, TheAnchorIsTheHandsOwnPoint)
{
	// A projection of its own: in front of the eye at z >= 0, the point a little off whole pixels
	const auto project = [](const glm::vec3& p) -> std::optional<glm::vec3> {
		return p.z < 0.0f ? std::nullopt : std::optional(glm::vec3(p.x + 0.7f, p.y + 0.2f, 0.5f));
	};
	// The hand's point on the screen, cut to whole pixels, in the temple as on the land
	EXPECT_EQ(help::input_prompt::HandAnchor(glm::vec3(12.0f, 30.0f, 1.0f), project), glm::vec2(12.0f, 30.0f));
	// Nowhere when it can't be projected, or without a hand
	EXPECT_FALSE(help::input_prompt::HandAnchor(glm::vec3(0.0f, 0.0f, -1.0f), project).has_value());
	EXPECT_FALSE(help::input_prompt::HandAnchor(std::nullopt, project).has_value());
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

TEST(InputPromptIcons, TheZoomOnControlShowsBothButtons)
{
	int32_t animType = -2;
	int32_t clickType = -2;
	int32_t row = -2;
	std::u16string keyName = u"stale";
	// the zoom on control (6) has no button of its own, and the select and action controls are the left and right
	// buttons, so its picture is the mouse with both buttons, before its key
	ASSERT_TRUE(help::input_prompt::ResolveAction(6, animType, clickType, row, keyName));
	EXPECT_EQ(animType, 0);
	EXPECT_EQ(clickType, 3);
	EXPECT_EQ(row, help::input_prompt::MouseRow());
	EXPECT_TRUE(keyName.empty());

	// the select control keeps its own button
	ASSERT_TRUE(help::input_prompt::ResolveAction(1, animType, clickType, row, keyName));
	EXPECT_EQ(clickType, 1);
	// and a key-only control still shows its key
	ASSERT_TRUE(help::input_prompt::ResolveAction(0, animType, clickType, row, keyName));
	EXPECT_EQ(animType, -1);
	EXPECT_EQ(clickType, 0);
}
