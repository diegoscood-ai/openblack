/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/TempleSaveGameControls.h"
#include "3D/TempleToolTips.h"
#include "Temple/SaveGameRoomRules.h"

using namespace openblack;
using namespace openblack::temple::save_game_room;

namespace
{
constexpr View k_Away {};
constexpr View k_Close {.closeUp = true, .pictureLookedAt = true};
constexpr View k_CloseGameOver {.closeUp = true, .pictureLookedAt = true, .gameOver = true};
constexpr View k_CloseKept {.closeUp = true, .pictureLookedAt = true, .keptInRoom = true};
/// The camera all the way to the scroll, not to a picture
constexpr View k_CloseToScroll {.closeUp = true, .pictureLookedAt = false};

/// The room's mesh's submeshes, by name: a wall, then the pictures and each slot's three buttons
std::vector<std::string> RoomNames()
{
	std::vector<std::string> names {"wall"};
	for (uint32_t slot = 0; slot < k_Slots; ++slot)
	{
		const auto two = [](uint32_t number) { return (number < 10 ? "0" : "") + std::to_string(number); };
		names.push_back("LH_PICTURE_savegame_" + two(slot + 1));
		names.push_back("LH_BUTTON_SAVE" + two(slot));
		names.push_back("LH_BUTTON_LOAD" + two(slot));
		names.push_back("LH_BUTTON_DELETE" + two(slot));
	}
	return names;
}
/// The submesh of a slot's part in RoomNames
uint32_t SubMeshOf(Part part, uint32_t slot)
{
	return 1 + slot * 4 + static_cast<uint32_t>(part);
}
} // namespace

TEST(SaveGameRoom, TheControlsAreFoundByTheirNames)
{
	EXPECT_EQ(ParseControl("LH_PICTURE_savegame_01"), (Control {.part = Part::Picture, .slot = 0}));
	EXPECT_EQ(ParseControl("LH_PICTURE_savegame_21"), (Control {.part = Part::Picture, .slot = 20}));
	EXPECT_EQ(ParseControl("lh_button_save13"), (Control {.part = Part::Save, .slot = 13}));
	EXPECT_EQ(ParseControl("LH_BUTTON_LOAD20"), (Control {.part = Part::Load, .slot = 20}));
	EXPECT_EQ(ParseControl("LH_BUTTON_DELETE00"), (Control {.part = Part::Delete, .slot = 0}));
	EXPECT_FALSE(ParseControl("LH_PICTURE_savegame_00").has_value());
	EXPECT_FALSE(ParseControl("LH_PICTURE_savegame_22").has_value());
	EXPECT_FALSE(ParseControl("LH_BUTTON_SAVE21").has_value());
	EXPECT_FALSE(ParseControl("LH_BUTTON_SAVE1").has_value());
	EXPECT_FALSE(ParseControl("LH_Scroll_Savegame").has_value());
}

TEST(SaveGameRoom, ThePicturesAlwaysShowAndTheButtonsOnlyCloseToAPicture)
{
	for (const auto& view : {k_Away, k_Close, k_CloseGameOver, k_CloseToScroll})
	{
		EXPECT_TRUE(IsShown({.part = Part::Picture, .slot = 20}, view, false));
	}
	for (const auto part : {Part::Save, Part::Load, Part::Delete})
	{
		EXPECT_FALSE(IsShown({.part = part, .slot = 0}, k_Away, true));
		EXPECT_FALSE(IsShown({.part = part, .slot = 0}, k_CloseToScroll, true));
		EXPECT_FALSE(IsShown({.part = part, .slot = 0}, {.closeUp = false, .pictureLookedAt = true}, true));
	}
}

TEST(SaveGameRoom, TheSaveButtonsShowForThePlayersSlotsWhileTheGameIsOn)
{
	EXPECT_TRUE(IsShown({.part = Part::Save, .slot = 0}, k_Close, false));
	EXPECT_TRUE(IsShown({.part = Part::Save, .slot = 13}, k_Close, true));
	EXPECT_FALSE(IsShown({.part = Part::Save, .slot = 14}, k_Close, false));
	EXPECT_FALSE(IsShown({.part = Part::Save, .slot = 19}, k_Close, true));
	EXPECT_FALSE(IsShown({.part = Part::Save, .slot = 3}, k_CloseGameOver, false));
	EXPECT_FALSE(IsShown({.part = Part::Save, .slot = 3}, k_CloseKept, false));
}

TEST(SaveGameRoom, TheLoadAndDeleteButtonsShowForASlotWithASave)
{
	for (const auto part : {Part::Load, Part::Delete})
	{
		EXPECT_FALSE(IsShown({.part = part, .slot = 3}, k_Close, false));
		EXPECT_TRUE(IsShown({.part = part, .slot = 3}, k_Close, true));
		// any slot, the game over or the player kept in the room
		EXPECT_TRUE(IsShown({.part = part, .slot = 19}, k_CloseGameOver, true));
		EXPECT_TRUE(IsShown({.part = part, .slot = 20}, k_CloseKept, true));
	}
}

TEST(SaveGameRoom, APictureLooksCloserThenLoadsOrSaves)
{
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 3}, k_Away, true), Click::LookCloser);
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 3}, k_CloseToScroll, false), Click::LookCloser);
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 3}, k_Close, true), Click::AskLoad);
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 19}, k_Close, true), Click::AskLoad);
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 3}, k_Close, false), Click::AskName);
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 3}, k_CloseKept, false), Click::AskName);
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 3}, k_CloseGameOver, false), Click::Nothing);
	EXPECT_EQ(Clicked({.part = Part::Picture, .slot = 14}, k_Close, false), Click::Nothing);
}

TEST(SaveGameRoom, TheButtonsAskBeforeASaveOverAnotherALoadAndADelete)
{
	EXPECT_EQ(Clicked({.part = Part::Save, .slot = 3}, k_Close, false), Click::AskName);
	EXPECT_EQ(Clicked({.part = Part::Save, .slot = 3}, k_Close, true), Click::AskOverwrite);
	EXPECT_EQ(Clicked({.part = Part::Load, .slot = 3}, k_Close, true), Click::AskLoad);
	EXPECT_EQ(Clicked({.part = Part::Delete, .slot = 3}, k_Close, true), Click::AskDelete);

	EXPECT_FALSE(QuestionOf(Click::AskName).has_value());
	EXPECT_EQ(QuestionOf(Click::AskLoad), k_LoadQuestionText);
	EXPECT_EQ(QuestionOf(Click::AskOverwrite), k_OverwriteQuestionText);
	EXPECT_EQ(QuestionOf(Click::AskDelete), k_DeleteQuestionText);
}

TEST(SaveGameRoom, AYesGoesOnAndANoDoesNothing)
{
	EXPECT_EQ(StepAfter(Click::AskName, false), Step::AskName);
	EXPECT_EQ(StepAfter(Click::AskOverwrite, true), Step::AskName);
	EXPECT_EQ(StepAfter(Click::AskOverwrite, false), Step::Nothing);
	EXPECT_EQ(StepAfter(Click::AskLoad, true), Step::Load);
	EXPECT_EQ(StepAfter(Click::AskLoad, false), Step::Nothing);
	EXPECT_EQ(StepAfter(Click::AskDelete, true), Step::Delete);
	EXPECT_EQ(StepAfter(Click::AskDelete, false), Step::Nothing);
	EXPECT_EQ(StepAfter(Click::Nothing, true), Step::Nothing);
	EXPECT_EQ(StepAfter(Click::LookCloser, true), Step::Nothing);
}

TEST(SaveGameRoom, TheNameBoxStartsEmptyOrWithTheNameSavedOver)
{
	EXPECT_EQ(NameBoxStart(Click::AskName, std::nullopt), u"");
	EXPECT_EQ(NameBoxStart(Click::AskName, u"Old"), u"");
	EXPECT_EQ(NameBoxStart(Click::AskOverwrite, u"Old"), u"Old");
	EXPECT_EQ(NameBoxStart(Click::AskOverwrite, std::u16string(40, u'x')).size(), k_NameLength);
}

TEST(SaveGameRoom, LoadingTheLandBeforeOrAfterFirstSavesIntoTheOther)
{
	EXPECT_EQ(LoadStepsOf(14), (LoadSteps {.saveFirstInto = 18, .load = 14}));
	EXPECT_EQ(LoadStepsOf(18), (LoadSteps {.saveFirstInto = 14, .load = 18}));
	EXPECT_EQ(LoadStepsOf(3), (LoadSteps {.saveFirstInto = std::nullopt, .load = 3}));
	EXPECT_EQ(LoadStepsOf(19), (LoadSteps {.saveFirstInto = std::nullopt, .load = 19}));
}

TEST(SaveGameRoom, TheLandFoldersListedAndDeleted)
{
	EXPECT_FALSE(LandFolderOf(3, 2).has_value());
	EXPECT_FALSE(LandFolderOf(19, 2).has_value());
	EXPECT_FALSE(LandFolderOf(20, 2).has_value());
	EXPECT_EQ(LandFolderOf(15, 2), 2u);
	EXPECT_EQ(LandFolderOf(17, 5), 5u);
	// the land before past the first land, the slot's own folder on it
	EXPECT_EQ(LandFolderOf(14, 2), 1u);
	EXPECT_FALSE(LandFolderOf(14, 1).has_value());
	// the land after on the lands 1 to 4, the slot's own folder on the last
	EXPECT_EQ(LandFolderOf(18, 1), 2u);
	EXPECT_EQ(LandFolderOf(18, 4), 5u);
	EXPECT_FALSE(LandFolderOf(18, 5).has_value());
	EXPECT_FALSE(LandFolderOf(18, 0).has_value());
}

TEST(SaveGameRoom, TheSlotsFoldersAreFoundInTheOrderOfTheirNames)
{
	EXPECT_EQ(FolderOrder(),
	          (std::vector<uint32_t> {0, 1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 2, 20, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(SaveGameRoom, TheListTakesANewSaveLastAndASaveOverAnotherInItsPlace)
{
	std::vector<Entry> list {{.slot = 1, .date = u"a", .name = u"One"}, {.slot = 103, .date = u"b", .name = u"Three"}};
	EXPECT_TRUE(HasSave(list, 3));
	EXPECT_FALSE(HasSave(list, 2));
	ASSERT_NE(SaveOf(list, 3), nullptr);
	EXPECT_EQ(SaveOf(list, 3)->name, u"Three");

	AfterSave(list, {.slot = 2, .date = u"c", .name = u"Two"}, false);
	ASSERT_EQ(list.size(), 3u);
	EXPECT_EQ(list.back().name, u"Two");

	AfterSave(list, {.slot = 1, .date = u"d", .name = u"New"}, true);
	ASSERT_EQ(list.size(), 3u);
	EXPECT_EQ(list.front(), (Entry {.slot = 1, .date = u"d", .name = u"New"}));

	AfterDelete(list, 3);
	ASSERT_EQ(list.size(), 2u);
	EXPECT_FALSE(HasSave(list, 3));
	EXPECT_TRUE(StartsLoadHelp(false));
	EXPECT_FALSE(StartsLoadHelp(true));
}

TEST(SaveGameRoom, TheControlsHideTheirButtonsAwayFromThePictures)
{
	const auto names = RoomNames();
	TempleSaveGameControls controls;
	controls.Find(names);
	controls.SetSaves({{.slot = 3, .date = u"d", .name = u"Three"}});
	// away, only the pictures show: every button is hidden
	EXPECT_EQ(controls.GetHidden(k_Away).size(), static_cast<size_t>(k_Slots) * 3);
	EXPECT_FALSE(controls.IsControl(0, k_Away));
	EXPECT_TRUE(controls.IsControl(SubMeshOf(Part::Picture, 20), k_Away));
	// close to a picture: the save buttons of slots 0 to 13 and slot 3's load and delete show
	const auto hidden = controls.GetHidden(k_Close);
	EXPECT_EQ(hidden.size(), static_cast<size_t>(k_Slots) * 3 - (k_LastPlayerSlot + 1) - 2);
	EXPECT_TRUE(controls.IsControl(SubMeshOf(Part::Save, 13), k_Close));
	EXPECT_FALSE(controls.IsControl(SubMeshOf(Part::Save, 14), k_Close));
	EXPECT_TRUE(controls.IsControl(SubMeshOf(Part::Load, 3), k_Close));
	EXPECT_TRUE(controls.IsControl(SubMeshOf(Part::Delete, 3), k_Close));
	EXPECT_FALSE(controls.IsControl(SubMeshOf(Part::Load, 4), k_Close));
}

TEST(SaveGameRoom, AControlIsClickedAsThePressIsLetGoOverIt)
{
	const auto names = RoomNames();
	TempleSaveGameControls controls;
	controls.Find(names);
	std::optional<TempleSaveGameControls::Click> click;
	const auto picture = SubMeshOf(Part::Picture, 5);
	EXPECT_TRUE(controls.Hold(true, picture, k_Away, click));
	EXPECT_TRUE(controls.IsHeld());
	EXPECT_TRUE(controls.Hold(false, picture, k_Away, click));
	ASSERT_TRUE(click.has_value());
	EXPECT_EQ(click->control, (Control {.part = Part::Picture, .slot = 5}));
	EXPECT_EQ(click->subMesh, picture);

	// let go elsewhere, nothing is clicked
	click.reset();
	controls.Hold(true, picture, k_Away, click);
	controls.Hold(false, SubMeshOf(Part::Picture, 6), k_Away, click);
	EXPECT_FALSE(click.has_value());

	// a hidden button takes no press
	EXPECT_FALSE(controls.Hold(true, SubMeshOf(Part::Save, 5), k_Away, click));
	controls.Hold(false, SubMeshOf(Part::Save, 5), k_Away, click);
	EXPECT_FALSE(click.has_value());

	// a button shown, close to a picture
	EXPECT_TRUE(controls.Hold(true, SubMeshOf(Part::Save, 5), k_Close, click));
	controls.Hold(false, SubMeshOf(Part::Save, 5), k_Close, click);
	ASSERT_TRUE(click.has_value());
	EXPECT_EQ(click->control, (Control {.part = Part::Save, .slot = 5}));
}

TEST(SaveGameRoom, TheCameraSentToAPictureUntilTheScrollOrTheRoomAgain)
{
	TempleSaveGameControls controls;
	EXPECT_FALSE(controls.GetLookedAtSlot().has_value());
	controls.LookAt(4);
	EXPECT_EQ(controls.GetLookedAtSlot(), 4u);
	controls.LookedAtScroll();
	EXPECT_FALSE(controls.GetLookedAtSlot().has_value());
	controls.LookAt(2);
	controls.Enter();
	EXPECT_FALSE(controls.GetLookedAtSlot().has_value());
}

TEST(SaveGameRoom, ASlotWithNoSaveShowsTheEmptyPicturesHalfByTheAlignment)
{
	constexpr UvRect k_Good {.u0 = 0.5f, .v0 = 0.0f, .u1 = 1.0f, .v1 = 1.0f};
	constexpr UvRect k_Evil {.u0 = 0.0f, .v0 = 0.0f, .u1 = 0.5f, .v1 = 1.0f};
	EXPECT_EQ(EmptyPicture(0.3f), k_Good);
	// neither good nor evil shows the good half
	EXPECT_EQ(EmptyPicture(0.0f), k_Good);
	EXPECT_EQ(EmptyPicture(-0.01f), k_Evil);
	EXPECT_EQ(PictureOf(4, false, true, std::nullopt, -1.0f),
	          (SlotPicture {.sheet = PictureSheet::Empty, .file = 0, .uv = k_Evil}));
	// even the slot shown in high detail, with no save
	EXPECT_EQ(PictureOf(4, false, true, 4u, 1.0f), (SlotPicture {.sheet = PictureSheet::Empty, .file = 0, .uv = k_Good}));
	// and a slot with a save while the player is in another room
	EXPECT_EQ(PictureOf(4, true, false, std::nullopt, 1.0f).sheet, PictureSheet::Empty);
}

TEST(SaveGameRoom, ASlotWithASaveShowsItsCellOfItsSharedFile)
{
	// eight small pictures a file, in four rows of two
	EXPECT_EQ(PictureOf(0, true, true, std::nullopt, 0.0f),
	          (SlotPicture {.sheet = PictureSheet::Small, .file = 0, .uv = {.u0 = 0.0f, .v0 = 0.0f, .u1 = 0.5f, .v1 = 0.25f}}));
	EXPECT_EQ(PictureOf(13, true, true, std::nullopt, 0.0f),
	          (SlotPicture {.sheet = PictureSheet::Small, .file = 1, .uv = {.u0 = 0.5f, .v0 = 0.5f, .u1 = 1.0f, .v1 = 0.75f}}));
	EXPECT_EQ(PictureOf(20, true, true, 3u, 0.0f),
	          (SlotPicture {.sheet = PictureSheet::Small, .file = 2, .uv = {.u0 = 0.0f, .v0 = 0.5f, .u1 = 0.5f, .v1 = 0.75f}}));
	// the slot shown in high detail, its half of the pair's file of large pictures
	EXPECT_EQ(PictureOf(13, true, true, 13u, 0.0f),
	          (SlotPicture {.sheet = PictureSheet::Large, .file = 6, .uv = {.u0 = 0.0f, .v0 = 0.5f, .u1 = 1.0f, .v1 = 1.0f}}));
	EXPECT_EQ(PictureOf(20, true, true, 20u, 0.0f),
	          (SlotPicture {.sheet = PictureSheet::Large, .file = 10, .uv = {.u0 = 0.0f, .v0 = 0.0f, .u1 = 1.0f, .v1 = 0.5f}}));
}

TEST(SaveGameRoom, ASaveReadsTheSmallPicturesAgainAndTheLargeOneOfTheSlotInHighDetail)
{
	EXPECT_EQ(PicturesAfterSave(5, std::nullopt), (PicturesToRead {.smallPictures = true, .largePicture = false}));
	EXPECT_EQ(PicturesAfterSave(5, 6u), (PicturesToRead {.smallPictures = true, .largePicture = false}));
	EXPECT_EQ(PicturesAfterSave(5, 5u), (PicturesToRead {.smallPictures = true, .largePicture = true}));
}

TEST(SaveGameRoom, ThePicturesAreTheSlotsPictureSubmeshes)
{
	TempleSaveGameControls controls;
	controls.Find(RoomNames());
	const auto pictures = controls.GetPictures();
	ASSERT_EQ(pictures.size(), k_Slots);
	for (uint32_t slot = 0; slot < k_Slots; ++slot)
	{
		EXPECT_EQ(pictures[slot].slot, slot);
		EXPECT_EQ(pictures[slot].subMesh, SubMeshOf(Part::Picture, slot));
	}
}

TEST(SaveGameRoom, ALongNameIsCutWithDots)
{
	EXPECT_EQ(ToolTipName(u"Land One"), u"Land One");
	EXPECT_EQ(ToolTipName(u"123456789012345"), u"123456789012345");
	EXPECT_EQ(ToolTipName(u"1234567890123456"), u"1234567890123...");
	EXPECT_EQ(ToolTipName(u"1234567890123456789"), u"1234567890123...");
}

TEST(SaveGameRoom, APictureChoosesItsTooltip)
{
	PictureToolTipInput input {.slot = 4,
	                           .hovered = true,
	                           .zoom = 0.0f,
	                           .pictureLookedAt = false,
	                           .lookedAt = std::nullopt,
	                           .buttonHovered = false,
	                           .saveName = u"Ogre",
	                           .saveNumbered = true,
	                           .gameOver = false};
	// From afar, a named save zooms in with its name, an unnamed one or an empty slot without
	auto chosen = PictureToolTipOf(input);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(chosen->text, k_ZoomInNamedText);
	EXPECT_EQ(chosen->name, u"Ogre");
	input.saveName = u"";
	EXPECT_EQ(PictureToolTipOf(input)->text, k_ZoomInText);
	input.saveName = std::nullopt;
	input.saveNumbered = false;
	EXPECT_EQ(PictureToolTipOf(input)->text, k_ZoomInText);
	// Half way there, nothing
	input.zoom = 0.5f;
	EXPECT_FALSE(PictureToolTipOf(input).has_value());
	// Close up as the picture looked at: an empty slot saves, a save loads
	input.zoom = 1.0f;
	input.pictureLookedAt = true;
	input.lookedAt = 4;
	EXPECT_EQ(PictureToolTipOf(input)->text, k_SaveText);
	input.saveName = u"Ogre";
	input.saveNumbered = true;
	EXPECT_EQ(PictureToolTipOf(input)->text, k_LoadText);
	// A button hovered before it leaves it be
	input.buttonHovered = true;
	EXPECT_FALSE(PictureToolTipOf(input).has_value());
	// Not hovered, it zooms out
	input.buttonHovered = false;
	input.hovered = false;
	EXPECT_EQ(PictureToolTipOf(input)->text, k_ZoomOutText);
	// An empty slot that the player cannot save into, or any once the game is over, says nothing close up
	input.hovered = true;
	input.saveName = std::nullopt;
	input.saveNumbered = false;
	input.slot = 15;
	input.lookedAt = 15;
	EXPECT_FALSE(PictureToolTipOf(input).has_value());
	input.slot = 4;
	input.lookedAt = 4;
	input.gameOver = true;
	EXPECT_FALSE(PictureToolTipOf(input).has_value());
}

TEST(SaveGameRoom, ThePictureCloseUpGoesByTheSavesWholeNumber)
{
	// the room's list goes by a save's last two digits, the close-up tooltip by its whole number
	const std::vector<Entry> numbered {{.slot = 103, .date = u"", .name = u"Second"}};
	EXPECT_TRUE(HasSave(numbered, 3));
	EXPECT_FALSE(HasSaveNumbered(numbered, 3));
	EXPECT_TRUE(HasSaveNumbered(numbered, 103));
}

TEST(SaveGameRoom, TheLoadButtonKeepsTheArrowsBeforeIt)
{
	const std::vector<TempleSaveGameControls::Placed> controls {
	    {.subMesh = 1, .control = {.part = Part::Load, .slot = 0}},
	    {.subMesh = 2, .control = {.part = Part::Save, .slot = 0}},
	};
	const std::vector<Entry> saves {{.slot = 0, .date = u"", .name = u"A"}};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::SaveGame, .inControl = true, .hoveredSubMesh = 1};
	input.saveControls = controls;
	input.saves = saves;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE9Du - 0xE73u);
	EXPECT_EQ(toolTip.arrows, gui::ToolTipArrows::k_All);
	input.hoveredSubMesh = 2;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE9Cu - 0xE73u);
	EXPECT_EQ(toolTip.arrows, gui::ToolTipArrows::k_None);
}

TEST(SaveGameRoom, ANamedZoomGivesTheSavesNameCut)
{
	// the first save of the slot by its last two digits names the picture
	const std::vector<TempleSaveGameControls::Placed> controls {{.subMesh = 5, .control = {.part = Part::Picture, .slot = 3}}};
	const std::vector<Entry> saves {{.slot = 103, .date = u"", .name = u"A long name for it"},
	                                {.slot = 3, .date = u"", .name = u"Short"}};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::SaveGame, .inControl = true, .hoveredSubMesh = 5};
	input.saveControls = controls;
	input.saves = saves;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xEE6u - 0xE73u);
	EXPECT_EQ(toolTip.name, u"A long name f...");
	// the next turn over nothing names nothing
	input.hoveredSubMesh = std::nullopt;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE7Eu - 0xE73u);
	EXPECT_TRUE(toolTip.name.empty());
}

namespace
{
constexpr uint32_t k_ScrollSubMesh = 100;
/// The tooltip index of a text
constexpr uint32_t Index(uint32_t text)
{
	return text - 0xE73;
}
} // namespace

TEST(SaveGameRoom, ToolTipsOfThePicturesAndButtons)
{
	TempleSaveGameControls controls;
	controls.Find(RoomNames());
	const std::vector<Entry> saves {{.slot = 2, .date = u"", .name = u"Land one"}, {.slot = 4, .date = u"", .name = u""}};
	const std::vector<TempleScrolls::Control> scrolls {{.subMesh = k_ScrollSubMesh, .focused = false}};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::SaveGame, .inControl = true};
	input.saveControls = controls.GetControls();
	input.saves = saves;
	input.scrolls = scrolls;

	// Moving about, with nothing hovered
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE7E));
	// From afar a picture zooms in, naming its save when it has a name
	input.hoveredSubMesh = SubMeshOf(Part::Picture, 2);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xEE6));
	EXPECT_EQ(toolTip.name, u"Land one");
	input.hoveredSubMesh = SubMeshOf(Part::Picture, 4);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE75));
	EXPECT_TRUE(toolTip.name.empty());
	// Half way there, nothing of its own
	input.zoom = 0.5f;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE7E));

	// Close up to the picture the camera was sent to: it loads its save, or saves into the slot
	input.zoom = 1.0f;
	input.lookingAtScroll = true;
	input.pictureLookedAt = true;
	input.saveLookedAt = 2;
	input.hoveredSubMesh = SubMeshOf(Part::Picture, 2);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE9D));
	input.saveLookedAt = 3;
	input.hoveredSubMesh = SubMeshOf(Part::Picture, 3);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE9C));
	// a land's own slot, or once the game is over, an empty picture says nothing of its own
	input.saveLookedAt = 15;
	input.hoveredSubMesh = SubMeshOf(Part::Picture, 15);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE7E));
	input.saveLookedAt = 3;
	input.hoveredSubMesh = SubMeshOf(Part::Picture, 3);
	input.gameOver = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE7E));
	input.gameOver = false;
	// off it, the camera zooms back out
	input.hoveredSubMesh = std::nullopt;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE8B));

	// The buttons say what they do
	input.hoveredSubMesh = SubMeshOf(Part::Save, 3);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE9C));
	input.hoveredSubMesh = SubMeshOf(Part::Load, 3);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE9D));
	input.hoveredSubMesh = SubMeshOf(Part::Delete, 3);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE9E));
	// the save button of a land's own slot says nothing, so the picture looked at zooms back out
	input.hoveredSubMesh = SubMeshOf(Part::Save, 15);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE8B));

	// Close up to the scroll, a picture hovered zooms in still, and holds back the scroll's zoom out
	input.pictureLookedAt = false;
	input.saveLookedAt = std::nullopt;
	input.hoveredSubMesh = std::nullopt;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE8B));
	input.hoveredSubMesh = SubMeshOf(Part::Picture, 4);
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE75));
	// the scroll hovered close up can be dragged
	input.hoveredSubMesh = k_ScrollSubMesh;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xEE2));
}

TEST(SaveGameRoom, AButtonHoveredKeepsThePicturesAfterItFromChoosing)
{
	// a slot's buttons before its picture in the mesh's order
	const std::vector<TempleSaveGameControls::Placed> controls {
	    {.subMesh = 1, .control = {.part = Part::Load, .slot = 0}},
	    {.subMesh = 2, .control = {.part = Part::Picture, .slot = 0}},
	};
	const std::vector<Entry> saves {{.slot = 0, .date = u"", .name = u"Mine"}};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::SaveGame, .inControl = true, .zoom = 1.0f, .lookingAtScroll = true};
	input.pictureLookedAt = true;
	input.saveLookedAt = 0;
	input.saveControls = controls;
	input.saves = saves;
	// the load button hovered: the picture after it neither zooms out nor says its own
	input.hoveredSubMesh = 1;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE9D));
	// nothing hovered, the picture zooms out
	input.hoveredSubMesh = std::nullopt;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, Index(0xE8B));
	// none on the way in
	input.inControl = false;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_FALSE(toolTip.index.has_value());
}
