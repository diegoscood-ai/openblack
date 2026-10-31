/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameMenu.h"

#include <cstddef>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL_keycode.h>

#include "ControlBindings.h"
#include "GameFont.h"
#include "Graphics/DetailLevel.h"
#include "StatisticsBox.h"
#include "TextDatabase.h"

using namespace openblack::gui;

namespace
{
using Look = BigButton::Look;
using LabelSide = BigButton::LabelSide;
using Layout = StaticText::Layout;
constexpr auto k_Big = DialogPainter::k_BigTextSize;
/// The mid text size (22, one more when bigger text is needed): a call, the language is read once the file system exists
int Mid()
{
	return DialogPainter::MidTextSize();
}

// A question dims the box behind it over half a second
constexpr float k_FadeInSeconds = 0.5f;

// The first page's buttons
constexpr int k_ButtonLeft = 180;
constexpr int k_ButtonWidth = 440;
constexpr int k_ButtonHeight = 70;
constexpr int k_FirstButtonTop = 145;
constexpr int k_ButtonSpacing = 80;

// The options' pages: Back at the bottom left, 40 pixel arrows
constexpr glm::ivec2 k_BackPosition {30, 530};
constexpr int k_BackSize = 40;
// The selectors: a 200 x 30 button at x 300 between a left and a right 32 pixel arrow at x 260 / 508, one higher
constexpr int k_SelectorLeft = 300;
constexpr int k_SelectorWidth = 200;
constexpr int k_SelectorHeight = 30;
constexpr int k_SelectorLeftArrow = 260;
constexpr int k_SelectorRightArrow = 508;
constexpr int k_ArrowSize = 32;
constexpr int k_CheckBoxTop = 450;
// The Mods page: a row a mod, its switch's square at x 60 and its line beside it, eight rows from y 120
constexpr size_t k_ModRows = 8;
constexpr int k_ModFirstTop = 120;
constexpr int k_ModRowSpacing = 48;
constexpr int k_ModBoxLeft = 60;
constexpr int k_ModTextLeft = 110;
constexpr int k_ModRowHeight = 32;
// The options' tabs and the Mods page's own place among them
constexpr size_t k_OriginalOptionsTabs = Dialog::k_TabCount;
constexpr size_t k_ModsTab = k_OriginalOptionsTabs;

constexpr std::array k_DetailTexts = {"HELP_TEXT_DIALOG_DETAIL_00", "HELP_TEXT_DIALOG_DETAIL_01", "HELP_TEXT_DIALOG_DETAIL_02",
                                      "HELP_TEXT_DIALOG_DETAIL_03", "HELP_TEXT_DIALOG_DETAIL_04"};
constexpr std::array k_HelpTexts = {"HELP_TEXT_DIALOG_NOHELP", "HELP_TEXT_DIALOG_LOWHELP", "HELP_TEXT_DIALOG_MIDDLEHELP",
                                    "HELP_TEXT_DIALOG_HIGHHELP", "HELP_TEXT_DIALOG_ALLHELP"};
constexpr std::array k_StoryTexts = {"HELP_TEXT_DIALOG_NOTEXT", "HELP_TEXT_DIALOG_STORYTEXT", "HELP_TEXT_DIALOG_ALLTEXT"};
constexpr std::array k_ToolTipTexts = {"HELP_TEXT_DIALOG_NOTOOLTIPS", "HELP_TEXT_DIALOG_MINIMUMTOOLTIPS",
                                       "HELP_TEXT_DIALOG_INTELLIGENTNOTOOLTIPS", "HELP_TEXT_DIALOG_ALLTOOLTIPS"};

using control_bindings::k_Bindings;
using control_bindings::KeyText;
using control_bindings::Mouse;
using control_bindings::MouseTextName;

/// The mice pictures for a wheel mouse: the third row of data/textures/mousehelp.raw, the left button the mirrored
/// right one. The wheel turns over the fourth row.
constexpr int k_MouseRow = 2;
std::optional<std::pair<int, bool>> MouseCell(Mouse mouse)
{
	switch (mouse)
	{
	case Mouse::Left:
		return std::pair((k_MouseRow * 4) + 1, true);
	case Mouse::Right:
		return std::pair((k_MouseRow * 4) + 1, false);
	case Mouse::Both:
		return std::pair((k_MouseRow * 4) + 2, false);
	case Mouse::Middle:
		return std::pair((k_MouseRow * 4) + 3, false);
	case Mouse::WheelUp:
	case Mouse::WheelDown:
		return std::pair(11, false);
	default:
		return std::nullopt;
	}
}
} // namespace

openblack::player_profiles::Settings openblack::gui::ToProfileSettings(const MenuSettings& settings)
{
	return {.toolTips = settings.toolTips,
	        .storyText = settings.storyText,
	        .helpLevel = settings.helpLevel,
	        .creatureHelp = settings.creatureHelp,
	        .readSpeed = settings.helpTextSpeed,
	        .textFromBottom = settings.textFromBottom,
	        .leftHanded = settings.leftHandedHand,
	        .pushScrolling = settings.pushScrolling,
	        .autoSave = settings.autoSave};
}

void openblack::gui::TakeProfileSettings(MenuSettings& settings, const openblack::player_profiles::Settings& kept)
{
	// The tooltip level's selector: none, minimum, intelligent and all, a negative level 400 up
	constexpr int32_t k_ToolTipLevels = 4;
	constexpr int32_t k_ToolTipNegativeOffset = 400;
	settings.toolTips = OptionsIndex(kept.toolTips, k_ToolTipLevels, k_ToolTipNegativeOffset);
	// The story text's: no text, the important texts and all, a negative value 300 up
	constexpr int32_t k_StoryTextChoices = 3;
	constexpr int32_t k_StoryTextNegativeOffset = 300;
	settings.storyText = OptionsIndex(kept.storyText, k_StoryTextChoices, k_StoryTextNegativeOffset);
	// The help level's: no help up to all help, a negative level 500 up. Below -500 the original's own place is still
	// negative and its label is read from outside its list, which can't be reproduced: the place is wrapped as the
	// others are, so the selector always shows one of its five
	constexpr int32_t k_HelpLevels = 5;
	constexpr int32_t k_HelpLevelNegativeOffset = 500;
	settings.helpLevel = OptionsIndex(kept.helpLevel, k_HelpLevels, k_HelpLevelNegativeOffset);
	settings.creatureHelp = kept.creatureHelp;
	settings.helpTextSpeed = kept.readSpeed;
	settings.textFromBottom = kept.textFromBottom;
	settings.leftHandedHand = kept.leftHanded;
	settings.pushScrolling = kept.pushScrolling;
	settings.autoSave = kept.autoSave;
}

std::u16string openblack::gui::MenuModLabel(std::u16string_view name, std::u16string_view version, std::u16string_view author)
{
	std::u16string label(name);
	if (!version.empty())
	{
		label.append(u" ").append(version);
	}
	if (!author.empty())
	{
		label.append(u" (").append(author).append(u")");
	}
	return label;
}

GameMenu::GameMenu(const TextDatabase& texts, const GameFont& font, std::u16string_view playerName, MenuSettings settings,
                   std::optional<MenuMods> mods)
    : _texts(texts)
    , _font(font)
    , _playerName(playerName)
    , _yes(texts.Get("HELP_TEXT_REQUESTER_BOXES_01"))
    , _no(texts.Get("HELP_TEXT_REQUESTER_BOXES_02"))
    , _quitQuestion(texts.Get("HELP_TEXT_DIALOG_QUIT_QUESTION"))
    , _quitGameQuestion(texts.Get("HELP_TEXT_DIALOG_AREYOUSUREQUIT"))
    , _accept(texts.Get("HELP_TEXT_REQUESTER_BOXES_04"))
    , _videoChange(texts.Get("HELP_TEXT_DIALOG_VIDEOCHANGE"))
    , _back(texts.Get("HELP_TEXT_DIALOG_BACK"))
    , _restart(texts.Get("HELP_TEXT_FRONT_END_12"))
    , _restartQuestion(texts.Get("HELP_TEXT_DIALOG_RESTART_WARNING"))
    , _restartNotice(texts.Get("HELP_TEXT_DIALOG_RESTART"))
    , _deleteQuestion(texts.Get("HELP_TEXT_DIALOG_AERYOUSUREDELETE"))
    , _newGameQuestion(texts.Get("HELP_TEXT_DIALOG_ADDITION_20"))
    , _settings(std::move(settings))
    , _mods(std::move(mods))
{
	const auto read = [&texts](auto& labels, const auto& names) {
		for (size_t i = 0; i < labels.size(); ++i)
		{
			labels.at(i) = texts.Get(names.at(i));
		}
	};
	read(_detailLabels, k_DetailTexts);
	read(_helpLabels, k_HelpTexts);
	read(_storyLabels, k_StoryTexts);
	read(_toolTipLabels, k_ToolTipTexts);
	_settings.detail = std::clamp(_settings.detail, 0, static_cast<int>(_detailLabels.size()) - 1);
	_detailShown = _settings.detail;
	_detailApplied = _settings.detail;
	_settings.helpLevel = std::clamp(_settings.helpLevel, 0, static_cast<int>(_helpLabels.size()) - 1);
	_settings.storyText = std::clamp(_settings.storyText, 0, static_cast<int>(_storyLabels.size()) - 1);
	_settings.toolTips = std::clamp(_settings.toolTips, 0, static_cast<int>(_toolTipLabels.size()) - 1);
	if (_settings.creatureName.empty())
	{
		_settings.creatureName = _playerName;
	}
	_mainMenuTab = texts.Get("HELP_TEXT_DIALOG_ADDITION_88");
	_worldRoomTab = texts.Get("HELP_TEXT_ROOM_WORLD_TITLE");

	BuildMain(texts);
	BuildOptions(texts);
	BuildPlayers(texts);
	BuildAdvanced(texts);
	BuildControls(texts);
	if (_mods)
	{
		BuildMods(texts);
	}
	// The Game Statistics box's first tab and its Back arrow return to the main menu
	_statistics = std::make_unique<StatisticsBox>(texts, font, [this] { ShowPage(Page::Main, PageSwitch::CrossFade); });
}

GameMenu::~GameMenu() = default;

DialogRect GameMenu::GetButtonRect(size_t index)
{
	const auto top = k_FirstButtonTop + (k_ButtonSpacing * static_cast<int>(index));
	return {.min = {k_ButtonLeft, top}, .max = {k_ButtonLeft + k_ButtonWidth, top + k_ButtonHeight}};
}

std::vector<Dialog::Tab> GameMenu::OptionsTabs(const TextDatabase& texts, Page selected)
{
	// Main Menu, Options, Players, Advanced and Controls. Inside the temple the first is the World Room, which hides the
	// options without showing the main menu.
	(void)selected;
	return {
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_88")),
	     .onSelect =
	         [this] {
		         if (_insideTemple)
		         {
			         if (!AskToRestartFirst(Page::Main))
			         {
				         Close();
			         }
		         }
		         else
		         {
			         SelectOptionsTab(Page::Main);
		         }
	         }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_OPTIONS")),
	     .onSelect = [this] { SelectOptionsTab(Page::Options); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_PLAYERLIST")),
	     .onSelect = [this] { SelectOptionsTab(Page::Players); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_FRONT_END_06")),
	     .onSelect = [this] { SelectOptionsTab(Page::Advanced); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_CONTROLOPTIONS")),
	     .onSelect = [this] { SelectOptionsTab(Page::Controls); }},
	};
}

std::unique_ptr<Dialog> GameMenu::MakeOptionsPage(const TextDatabase& texts, Page page)
{
	auto tabs = OptionsTabs(texts, page);
	if (!_mods)
	{
		return std::make_unique<Dialog>(std::move(tabs), static_cast<size_t>(page));
	}
	// The Mods tab after the original's five, the six sharing the box's width (the game has no text for it)
	tabs.push_back({.label = u"Mods", .onSelect = [this] { SelectOptionsTab(Page::Mods); }});
	const auto open = page == Page::Mods ? k_ModsTab : static_cast<size_t>(page);
	if (open < tabs.size())
	{
		tabs.at(open).onSelect = nullptr;
	}
	return std::make_unique<Dialog>(std::move(tabs), open, Dialog::k_Box);
}

const std::vector<MenuMods::Mod>& GameMenu::GetMods() const
{
	static const std::vector<MenuMods::Mod> k_None;
	return _mods ? _mods->mods : k_None;
}

void GameMenu::BuildMods(const TextDatabase& texts)
{
	// The mods of the mods folder, a switch each: the user's choice of 2026-10-21, not a page of the original
	auto& page = *(_pages[static_cast<size_t>(Page::Mods)] = MakeOptionsPage(texts, Page::Mods));
	page.Add<StaticText>(DialogRect {.min = {50, 60}, .max = {750, 100}}, _mods->heading, Layout::Wrapped);
	const auto shown = std::min(_mods->mods.size(), k_ModRows);
	for (size_t i = 0; i < shown; ++i)
	{
		const int top = k_ModFirstTop + (static_cast<int>(i) * k_ModRowSpacing);
		const auto& mod = _mods->mods.at(i);
		auto& box = page.Add<CheckBox>(_font, glm::ivec2(k_ModBoxLeft, top), std::u16string {}, mod.enabled);
		box.onChange = [this, i, &box](bool checked) {
			auto& entry = _mods->mods.at(i);
			if (_mods->onSwitch && _mods->onSwitch(i, checked))
			{
				entry.enabled = checked;
			}
			else
			{
				box.SetChecked(entry.enabled);
			}
		};
		page.Add<StaticText>(DialogRect {.min = {k_ModTextLeft, top}, .max = {750, top + k_ModRowHeight}}, mod.label,
		                     Layout::Left, DialogPainter::MidTextSize());
	}
	if (_mods->mods.size() > shown)
	{
		const int top = k_ModFirstTop + (static_cast<int>(shown) * k_ModRowSpacing);
		page.Add<StaticText>(DialogRect {.min = {k_ModTextLeft, top}, .max = {750, top + k_ModRowHeight}},
		                     u"+ " + ToUtf16(std::to_string(_mods->mods.size() - shown)), Layout::Left,
		                     DialogPainter::MidTextSize());
	}

	page.Add<BigButton>(_font, k_BackPosition, k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_BACK")), LabelSide::Right,
	                    Look::LeftArrow)
	    .onClick = [this] { ShowPage(Page::Main, PageSwitch::CrossFade); };
}

void GameMenu::BuildMain(const TextDatabase& texts)
{
	// 0 HELP_TEXT_DIALOG_ADDITION_88, 1 HELP_TEXT_PAUSE_STATS_151, 2 the multiplayer database's
	// HELP_TEXT_MULTIPLAYER_MAIN_TAB (in every game), 3 and 4 without a label. the third tab does nothing yet
	std::vector<Dialog::Tab> tabs {
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_88")), .onSelect = {}},
	    {.label = std::u16string(texts.Get("HELP_TEXT_PAUSE_STATS_151")), .onSelect = [this] { OpenStatistics(); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_MULTIPLAYER_MAIN_TAB")), .onSelect = {}},
	};
	auto& page = *(_pages[static_cast<size_t>(Page::Main)] = std::make_unique<Dialog>(std::move(tabs), 0));

	_welcome = texts.Get("HELP_TEXT_DIALOG_WELCOME");
	if (const auto found = _welcome.find(u"%s"); found != std::u16string::npos)
	{
		_welcome.replace(found, 2, _playerName);
	}
	page.Add<StaticText>(DialogRect {.min = {50, 65}, .max = {750, 155}}, _welcome, Layout::Wrapped);

	// "Leave Skirmish Game" (HELP_TEXT_DIALOG_ADDITION_170) during a skirmish and "Exit Online Game."
	// (HELP_TEXT_FRONT_END_03) during a multiplayer game
	const std::array<std::pair<const char*, std::function<void()>>, k_ButtonCount> buttons = {{
	    {"HELP_TEXT_DIALOG_CONTINUEGAME", [this] { Settle(Action::Continue); }},
	    {"HELP_TEXT_DIALOG_ADDITION_169", [this] { Settle(Action::StartSkirmish); }},
	    {"HELP_TEXT_DIALOG_JOINGONLINE", [this] { Settle(Action::JoinOnline); }},
	    {"HELP_TEXT_DIALOG_OPTIONS", [this] { ShowPage(Page::Options, PageSwitch::CrossFade); }},
	    {"HELP_TEXT_DIALOG_QUIT", [this] { Ask(_quitQuestion); }},
	}};
	for (size_t i = 0; i < k_ButtonCount; ++i)
	{
		_buttonLabels.at(i) = texts.Get(buttons.at(i).first);
		page.Add<Button>(GetButtonRect(i), _buttonLabels.at(i)).onClick = buttons.at(i).second;
	}
}

void GameMenu::BuildOptions(const TextDatabase& texts)
{
	// Sound, detail, autosave and push scrolling
	auto& page = *(_pages[static_cast<size_t>(Page::Options)] = MakeOptionsPage(texts, Page::Options));
	_showOptionsChoices.clear();
	page.Add<StaticText>(DialogRect {.min = {50, 60}, .max = {750, 100}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_SOUNDANDVIDEOCONTROL")), Layout::Wrapped);

	auto& sfx = page.Add<Slider>(DialogRect {.min = {250, 140}, .max = {550, 170}},
	                             std::u16string(texts.Get("HELP_TEXT_DIALOG_MASTERVOLUME")), _settings.sfxVolume);
	sfx.onChange = [this](float value) {
		_settings.sfxVolume = value;
		SettingsChanged();
	};
	auto& music = page.Add<Slider>(DialogRect {.min = {250, 200}, .max = {550, 230}},
	                               std::u16string(texts.Get("HELP_TEXT_DIALOG_MUSICVOLUME")), _settings.musicVolume);
	music.onChange = [this](float value) {
		_settings.musicVolume = value;
		SettingsChanged();
	};
	_sfxSlider = &sfx;
	_musicSlider = &music;

	page.Add<StaticText>(DialogRect {.min = {50, 280}, .max = {750, 320}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_VIDEOCONTROL")), Layout::Centre);
	// The detail: the button and the right arrow step it on, the left arrow back. The original says a changed detail only
	// takes effect the next time the game starts (HELP_TEXT_DIALOG_VIDEOCHANGE); openblack keeps no settings for a next
	// start, so the game takes it as the page is left (TakeDetailChange)
	auto& detail = page.Add<Button>(
	    DialogRect {.min = {k_SelectorLeft, 350}, .max = {k_SelectorLeft + k_SelectorWidth, 350 + k_SelectorHeight}},
	    _detailLabels.at(static_cast<size_t>(_settings.detail)), Mid());
	const auto stepDetail = [this, &detail](int step) {
		const auto count = static_cast<int>(_detailLabels.size());
		_settings.detail = (_settings.detail + step + count) % count;
		detail.label = _detailLabels.at(static_cast<size_t>(_settings.detail));
		SettingsChanged();
	};
	detail.onClick = [stepDetail] { stepDetail(1); };
	page.Add<BigButton>(_font, glm::ivec2(k_SelectorLeftArrow, 349), k_ArrowSize, u"", LabelSide::Right, Look::LeftArrow)
	    .onClick = [stepDetail] { stepDetail(-1); };
	page.Add<BigButton>(_font, glm::ivec2(k_SelectorRightArrow, 349), k_ArrowSize, u"", LabelSide::Right, Look::RightArrow)
	    .onClick = [stepDetail] { stepDetail(1); };

	auto& autoSave = page.Add<CheckBox>(_font, glm::ivec2(285, k_CheckBoxTop),
	                                    std::u16string(texts.Get("HELP_TEXT_DIALOG_AUTOSAVE")), _settings.autoSave);
	autoSave.onChange = [this](bool checked) {
		_settings.autoSave = checked;
		SettingsChanged();
	};
	auto& pushScrolling =
	    page.Add<CheckBox>(_font, glm::ivec2(485, k_CheckBoxTop), std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_116")),
	                       _settings.pushScrolling);
	pushScrolling.onChange = [this](bool checked) {
		_settings.pushScrolling = checked;
		_pushScrollChanged = true;
		SettingsChanged();
	};
	_showOptionsChoices.emplace_back([this, &autoSave, &pushScrolling] {
		autoSave.SetChecked(_settings.autoSave);
		pushScrolling.SetChecked(_settings.pushScrolling);
	});

	page.Add<BigButton>(_font, k_BackPosition, k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_BACK")), LabelSide::Right,
	                    Look::LeftArrow)
	    .onClick = [this] {
		// A changed detail is told about first; the tell keeps no tab, so its answer hides the box: the main page
		if (!TellVideoChange(Page::Main, PageSwitch::CrossFade))
		{
			ShowPage(Page::Main, PageSwitch::CrossFade);
		}
	};
	page.Add<BigButton>(_font, glm::ivec2(730, 530), k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_QUIT")),
	                    LabelSide::Left, Look::RightArrow)
	    .onClick = [this] { Ask(_quitGameQuestion); };
}

void GameMenu::BuildPlayers(const TextDatabase& texts)
{
	// The player's profile
	auto& page = *(_pages[static_cast<size_t>(Page::Players)] = MakeOptionsPage(texts, Page::Players));
	page.Add<StaticText>(DialogRect {.min = {100, 60}, .max = {700, 120}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_SELECT_A_PLAYER")), Layout::Wrapped);

	// The profiles, the one played picked; picking another plays as it from now on. Until the game gives the list, the
	// player is the only one
	auto& players = page.Add<List>(_font, DialogRect {.min = {200, 120}, .max = {600, 280}}, Mid(), true, false, true);
	players.SetItems({{.text = _playerName, .mouseCell = std::nullopt, .mouseMirrored = false}});
	players.SetSelection(0);
	players.onClick = [this, &players] {
		if (const auto picked = players.GetSelection(); picked.has_value())
		{
			PickPlayer(*picked);
		}
	};
	_players = &players;
	_playerNames = {_playerName};
	_currentPlayer = 0;
	_originalPlayer = 0;
	_originalName = _playerName;

	page.Add<StaticText>(DialogRect {.min = {170, 295}, .max = {360, 359}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_PLAYER")), Layout::Right);
	page.Add<StaticText>(DialogRect {.min = {440, 295}, .max = {630, 359}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_SYMBOL")), Layout::Left);
	auto& symbol = page.Add<SymbolPicture>(DialogRect {.min = {368, 295}, .max = {432, 359}}, _settings.symbol);
	_symbolPicture = &symbol;
	symbol.onChange = [this](int value) {
		_settings.symbol = value;
		SettingsChanged();
	};

	page.Add<StaticText>(DialogRect {.min = {0, 370}, .max = {190, 410}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_CREATURENAME")), Layout::Right, Mid());
	// The edit box takes 29 characters
	auto& name = page.Add<EditBox>(_font, DialogRect {.min = {200, 370}, .max = {600, 410}}, _settings.creatureName, 29);
	_creatureNameBox = &name;
	name.onChange = [this](const std::u16string& text) {
		_settings.creatureName = text;
		SettingsChanged();
	};

	// The square buttons; UpdatePlayersPage spreads them out
	const std::array<std::pair<const char*, std::function<void()>>, 4> buttons = {{
	    {"HELP_TEXT_DIALOG_CREATENEWPLAYER",
	     [this] {
		     KeepPlayerEdits();
		     Settle(Action::CreatePlayer);
	     }},
	    {"HELP_TEXT_DIALOG_DELETEPLAYER",
	     [this] {
		     // Asks about the profile picked, by its name
		     if (!_currentPlayer.has_value())
		     {
			     return;
		     }
		     auto question = _deleteQuestion;
		     if (const auto found = question.find(u"%s"); found != std::u16string::npos)
		     {
			     question.replace(found, 2, _playerNames.at(*_currentPlayer));
		     }
		     Ask(std::move(question), Action::DeletePlayer);
	     }},
	    {"HELP_TEXT_DIALOG_EDITTATOO",
	     [this] {
		     KeepPlayerEdits();
		     Settle(Action::EditTattoo);
	     }},
	    {"HELP_TEXT_DIALOG_STARTGAME", [this] { Ask(_newGameQuestion, Action::StartNewGame); }},
	}};
	for (size_t i = 0; i < buttons.size(); ++i)
	{
		auto& button = page.Add<BigButton>(_font, glm::ivec2(0, k_CheckBoxTop), 25,
		                                   std::u16string(texts.Get(buttons.at(i).first)), LabelSide::Below, Look::Square);
		button.onClick = buttons.at(i).second;
		_playerButtons.at(i) = &button;
	}

	_playersBack = &page.Add<BigButton>(_font, k_BackPosition, k_BackSize, _back, LabelSide::Right, Look::LeftArrow);
	_playersBack->onClick = [this] { PlayersBack(); };
	UpdatePlayersPage();
}

void GameMenu::UpdatePlayersPage()
{
	// Back becomes Restart once leaving the page starts the game again, and Edit Tattoo hides then too
	const bool restart = IsRestartPending();
	_playersBack->label = restart ? _restart : _back;
	const bool tattoo = _tattooAvailable && !restart;
	_playerButtons.at(2)->visible = tattoo;
	// The square buttons spread out over the page, three or four of them
	constexpr std::array k_ThreeLefts = {188, 388, 0, 588};
	constexpr std::array k_FourLefts = {138, 305, 471, 638};
	for (size_t i = 0; i < _playerButtons.size(); ++i)
	{
		const auto left = tattoo ? k_FourLefts.at(i) : k_ThreeLefts.at(i);
		auto& rect = _playerButtons.at(i)->rect;
		rect = {.min = {left, rect.min.y}, .max = {left + rect.Width(), rect.max.y}};
	}
}

void GameMenu::SetPlayers(std::vector<std::u16string> names, std::optional<size_t> current)
{
	std::vector<List::Item> items;
	items.reserve(names.size());
	for (const auto& name : names)
	{
		items.push_back({.text = name, .mouseCell = std::nullopt, .mouseMirrored = false});
	}
	_players->SetItems(std::move(items));
	_playerNames = std::move(names);
	_currentPlayer = current.has_value() && *current < _playerNames.size() ? current : std::nullopt;
	_players->SetSelection(_currentPlayer);
	// The profile the game was started as may have gone: leaving the page then starts the game again
	const auto original = std::ranges::find(_playerNames, _originalName);
	_originalPlayer = original != _playerNames.end() && !_originalName.empty()
	                      ? std::optional(static_cast<size_t>(original - _playerNames.begin()))
	                      : std::nullopt;
	if (_page != Page::Players)
	{
		// Shown afresh, the page starts from the profile played
		_originalPlayer = _currentPlayer;
		_originalName = _currentPlayer.has_value() ? _playerNames.at(*_currentPlayer) : std::u16string {};
		_mustRestart = false;
	}
	UpdatePlayersPage();
}

void GameMenu::ShowPlayer(std::u16string_view creatureName, int symbol, const player_profiles::Settings& settings)
{
	_settings.creatureName = creatureName;
	_settings.symbol = symbol;
	_creatureNameBox->SetText(creatureName);
	_symbolPicture->SetSymbol(symbol);
	TakeProfileSettings(_settings, settings);
	// The other pages show the profile's options: they are made again, inside the temple with its first tab
	const auto page = _page;
	BuildOptions(_texts);
	BuildAdvanced(_texts);
	if (_insideTemple)
	{
		for (const auto rebuilt : {Page::Options, Page::Advanced})
		{
			_pages.at(static_cast<size_t>(rebuilt))->SetTabLabel(0, _worldRoomTab);
		}
	}
	_page = page;
}

void GameMenu::SetTattooAvailable(bool available)
{
	_tattooAvailable = available;
	UpdatePlayersPage();
}

void GameMenu::PickPlayer(size_t index)
{
	if (index >= _playerNames.size())
	{
		return;
	}
	// The edits are kept with the profile played before another is
	KeepPlayerEdits();
	const bool changed = _currentPlayer != index;
	_currentPlayer = index;
	UpdatePlayersPage();
	if (changed)
	{
		_pickedPlayer = index;
		Settle(Action::PickPlayer);
	}
}

void GameMenu::PrepareDeletion()
{
	if (!_currentPlayer.has_value())
	{
		return;
	}
	_playerToDelete = _playerNames.at(*_currentPlayer);
	auto remaining = _playerNames;
	remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(*_currentPlayer));
	// Deleting the profile the game was started as forgets it: the first left is played, and leaving the page then
	// starts the game again. With none left the new player's box asks for one.
	if (_playerToDelete == _originalName)
	{
		_originalName.clear();
		_mustRestart = !remaining.empty();
	}
	const auto original = std::ranges::find(remaining, _originalName);
	if (original != remaining.end() && !_originalName.empty())
	{
		_pickedPlayer = static_cast<size_t>(original - remaining.begin());
	}
	else
	{
		_pickedPlayer = remaining.empty() ? std::nullopt : std::optional<size_t>(0);
	}
}

void GameMenu::KeepPlayerEdits()
{
	_playerEdits = PlayerEdits {.creatureName = _settings.creatureName, .symbol = _settings.symbol};
}

void GameMenu::PlayersBack()
{
	KeepPlayerEdits();
	if (_mustRestart)
	{
		Tell(_restartNotice, Action::Restart);
	}
	else if (IsRestartPending())
	{
		Ask(_restartQuestion, Action::Restart);
	}
	else
	{
		ShowPage(Page::Main, PageSwitch::CrossFade);
	}
}

bool GameMenu::AskToRestartFirst(Page page)
{
	if (_page != Page::Players || page == Page::Players || !IsRestartPending())
	{
		return false;
	}
	KeepPlayerEdits();
	Ask(_restartQuestion, Action::Restart);
	return true;
}

const std::u16string& GameMenu::GetQuestionText() const
{
	static const std::u16string k_None;
	return _question.has_value() ? _question->text : k_None;
}

void GameMenu::BuildAdvanced(const TextDatabase& texts)
{
	// Help, the hand and text
	auto& page = *(_pages[static_cast<size_t>(Page::Advanced)] = MakeOptionsPage(texts, Page::Advanced));
	_showAdvancedChoices.clear();
	page.Add<StaticText>(DialogRect {.min = {50, 60}, .max = {750, 100}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_HELPCONTROLS")), Layout::Wrapped);

	// Three selectors, the button and the right arrow stepping them on and the left arrow back
	// Each steps the page's own choice; the choices are kept with the player's profile and take effect as the options'
	// box is left (TakeOptionsLeft, Game::ApplyPlayerSettings)
	const auto addSelector = [this, &page](int top, int& value, auto& labels) {
		auto& button = page.Add<Button>(
		    DialogRect {.min = {k_SelectorLeft, top}, .max = {k_SelectorLeft + k_SelectorWidth, top + k_SelectorHeight}},
		    labels.at(static_cast<size_t>(value)), Mid());
		const auto step = [this, &button, &value, &labels](int by) {
			const auto count = static_cast<int>(labels.size());
			value = (value + by + count) % count;
			button.label = labels.at(static_cast<size_t>(value));
			SettingsChanged();
		};
		button.onClick = [step] { step(1); };
		page.Add<BigButton>(_font, glm::ivec2(k_SelectorLeftArrow, top - 1), k_ArrowSize, u"", LabelSide::Right,
		                    Look::LeftArrow)
		    .onClick = [step] { step(-1); };
		page.Add<BigButton>(_font, glm::ivec2(k_SelectorRightArrow, top - 1), k_ArrowSize, u"", LabelSide::Right,
		                    Look::RightArrow)
		    .onClick = [step] { step(1); };
		_showAdvancedChoices.emplace_back([&button, &value, &labels] { button.label = labels.at(static_cast<size_t>(value)); });
	};
	addSelector(150, _settings.helpLevel, _helpLabels);
	addSelector(215, _settings.storyText, _storyLabels);
	addSelector(280, _settings.toolTips, _toolTipLabels);
	// The help text speed slider
	auto& speed = page.Add<Slider>(DialogRect {.min = {250, 365}, .max = {550, 395}},
	                               std::u16string(texts.Get("HELP_TEXT_DIALOG_HELPTEXTSPEED")), _settings.helpTextSpeed);
	speed.onChange = [this](float value) {
		_settings.helpTextSpeed = value;
		SettingsChanged();
	};
	_showAdvancedChoices.emplace_back([this, &speed] { speed.SetValue(_settings.helpTextSpeed); });

	const std::array<std::tuple<int, const char*, bool*>, 3> boxes = {{
	    {385, "HELP_TEXT_DIALOG_CREATUREHELP", &_settings.creatureHelp},
	    {185, "HELP_TEXT_DIALOG_LEFTHANDED", &_settings.leftHandedHand},
	    {585, "HELP_TEXT_DIALOG_ADDITION_126", &_settings.textFromBottom},
	}};
	for (const auto& [x, label, setting] : boxes)
	{
		auto& box = page.Add<CheckBox>(_font, glm::ivec2(x, k_CheckBoxTop), std::u16string(texts.Get(label)), *setting);
		// Apple's clang can't capture a structured binding: it is captured through a copy of its own
		box.onChange = [this, setting = setting](bool checked) {
			*setting = checked;
			SettingsChanged();
		};
		_showAdvancedChoices.emplace_back([&box, setting = setting] { box.SetChecked(*setting); });
		if (setting == &_settings.creatureHelp)
		{
			_creatureHelpBox = &box;
		}
	}

	page.Add<BigButton>(_font, k_BackPosition, k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_BACK")), LabelSide::Right,
	                    Look::LeftArrow)
	    .onClick = [this] { ShowPage(Page::Main, PageSwitch::CrossFade); };
}

void GameMenu::BuildControls(const TextDatabase& texts)
{
	// The keys and buttons of the game's actions
	auto& page = *(_pages[static_cast<size_t>(Page::Controls)] = MakeOptionsPage(texts, Page::Controls));
	page.Add<StaticText>(DialogRect {.min = {100, 60}, .max = {700, 160}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_CONTROLTITLE")), Layout::Wrapped);

	// "<action> : <key> <mouse>", the mouse's name after a gap of five spaces for its picture
	std::vector<List::Item> items;
	for (size_t i = 0; i < k_Bindings.size(); ++i)
	{
		const auto& binding = k_Bindings.at(i);
		std::u16string text(texts.Get(binding.name));
		text += u" : ";
		text += KeyText(binding);
		text += u" ";
		const auto mouseName = MouseTextName(binding.mouse);
		if (!mouseName.empty())
		{
			text += u"     (" + std::u16string(texts.Get(mouseName)) + u")";
		}
		const auto cell = MouseCell(binding.mouse);
		items.push_back({.text = std::move(text),
		                 .mouseCell = cell ? std::optional(cell->first) : std::nullopt,
		                 .mouseMirrored = cell && cell->second});
		if (binding.mouse == Mouse::WheelUp || binding.mouse == Mouse::WheelDown)
		{
			_wheelMice.emplace_back(i, binding.mouse == Mouse::WheelUp);
		}
	}
	// Halfway between the big and the mid text size
	auto& list =
	    page.Add<List>(_font, DialogRect {.min = {100, 120}, .max = {700, 440}}, (k_Big + Mid()) / 2, true, true, false);
	list.SetItems(std::move(items));
	// TODO(raffclar): redefining an action, by double clicking it
	_controls = &list;

	// Load Defaults asks first, with its own label as the question, and a yes puts every binding back
	auto loadDefaults = std::u16string(texts.Get("HELP_TEXT_DIALOG_LOADDEFAULTS"));
	page.Add<Button>(DialogRect {.min = {260, 530}, .max = {540, 570}}, loadDefaults, Mid()).onClick =
	    [this, question = loadDefaults] { Ask(question, Action::LoadDefaultControls); };

	page.Add<BigButton>(_font, k_BackPosition, k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_BACK")), LabelSide::Right,
	                    Look::LeftArrow)
	    .onClick = [this] { ShowPage(Page::Main, PageSwitch::CrossFade); };
}

void GameMenu::Ask(std::u16string text, Action yes)
{
	// A question with a Yes and a No: the question in the middle of a 500 by 200 box
	auto answers = std::make_unique<Dialog>();
	// The big text size (36 stored, decremented before the first measure), made smaller while the wrapped text is 125
	// or more high
	answers->Add<StaticText>(DialogRect {.min = {200, 220}, .max = {600, 350}}, text, Layout::Wrapped, k_Big).messageBox = true;
	// Yes (10002, the quit) at (200, 338) with a left arrow and No (10003) at (568, 338) with a right arrow, both 32.
	// Label sides: 2 the label centred under the button, 1 / 3 beside it on the left (right-justified at x0), 0 on the
	// right
	answers->Add<BigButton>(_font, glm::ivec2(200, 338), k_ArrowSize, _yes, LabelSide::Right, Look::LeftArrow).onClick =
	    [this, yes] { Settle(yes); };
	answers->Add<BigButton>(_font, glm::ivec2(568, 338), k_ArrowSize, _no, LabelSide::Left, Look::RightArrow).onClick = [this] {
		Settle(Action::None);
	};
	_question = Question {.text = std::move(text), .answers = std::move(answers), .fade = Zoomer()};
	_question->fade.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_FadeInSeconds);
	CurrentDialog().Reset();
}

void GameMenu::Tell(std::u16string text, Action ok)
{
	auto answers = std::make_unique<Dialog>();
	// The big text size (36 stored, decremented before the first measure), made smaller while the wrapped text is 125
	// or more high
	answers->Add<StaticText>(DialogRect {.min = {200, 220}, .max = {600, 350}}, text, Layout::Wrapped, k_Big).messageBox = true;
	// One 150 x 32 "OK" button
	answers->Add<Button>(DialogRect {.min = {325, 338}, .max = {475, 370}}, _accept, Mid()).onClick = [this, ok] {
		Settle(ok);
	};
	_question = Question {.text = std::move(text), .answers = std::move(answers), .fade = Zoomer()};
	_question->fade.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_FadeInSeconds);
	CurrentDialog().Reset();
}

void GameMenu::Settle(Action action)
{
	switch (action)
	{
	case Action::DeletePlayer:
		PrepareDeletion();
		[[fallthrough]];
	case Action::StartNewGame:
	case Action::Restart:
		KeepPlayerEdits();
		break;
	default:
		break;
	}
	_action = action;
	// The question goes once the click on its answer is over
	_answered = _question.has_value();
}

bool GameMenu::TellVideoChange(Page after, PageSwitch how)
{
	if (_settings.detail == _detailShown)
	{
		return false;
	}
	_detailShown = _settings.detail;
	_pageAfterTell = after;
	_switchAfterTell = how;
	Tell(_videoChange);
	return true;
}

void GameMenu::SelectOptionsTab(Page page)
{
	// On the Players page another tab, while leaving it starts the game again, asks first
	if (AskToRestartFirst(page))
	{
		return;
	}
	// On the Options page another tab than its own tells VIDEOCHANGE first, the tab kept. A tab hides every box before
	// it shows its page, so the page left goes at once, and the page fades in anew, its own tab too
	if (_page == Page::Options && page != Page::Options && TellVideoChange(page, PageSwitch::Replace))
	{
		return;
	}
	ShowPage(page, PageSwitch::Replace);
}

void GameMenu::LeaveOptions()
{
	// Maximum Detail is reported even when unchanged, since it sets the top level from the default 4
	if (_settings.detail != _detailApplied || _settings.detail >= graphics::detail_level::k_MenuTopEntry)
	{
		_detailChange = DetailChange {.from = _detailApplied, .to = _settings.detail};
		_detailApplied = _settings.detail;
	}
}

void GameMenu::ShowPage(Page page, PageSwitch how)
{
	// Leaving the options' box, for its own tab again too: a tab hides every box before it shows its page
	if (_page == Page::Options || _page == Page::Advanced)
	{
		_optionsLeft = true;
	}
	CurrentDialog().Reset();
	if (_page == Page::Options && page != Page::Options)
	{
		LeaveOptions();
	}
	if (_page == Page::Players && page != Page::Players)
	{
		KeepPlayerEdits();
	}
	if (page == Page::Players && _page != Page::Players)
	{
		// Shown afresh, the page starts from the profile played
		_originalPlayer = _currentPlayer;
		_originalName = _currentPlayer.has_value() ? _playerNames.at(*_currentPlayer) : std::u16string {};
		_mustRestart = false;
		UpdatePlayersPage();
	}
	_page = page;
	if (page == Page::Options)
	{
		_detailShown = _settings.detail; // each time it opens
		_pageAfterTell.reset();
	}
	if (page == Page::Options || page == Page::Advanced)
	{
		ShowOptionsBox();
	}
	CurrentDialog().Reset();
	_fades.Show(page, how);
	_profileShown = true;
}

void GameMenu::ShowOptionsBox()
{
	// The volume sliders are set again from the main volumes, so a slider dragged between two of its steps shows the
	// step the volume took. A leave just before keeps the volumes first, as the box left is done with before this one
	// shows
	if (_volumeSource)
	{
		const auto [sfx, music] = _volumeSource(_optionsLeft);
		_settings.sfxVolume = SliderOfVolume(sfx);
		_settings.musicVolume = SliderOfVolume(music);
		_sfxSlider->SetValue(_settings.sfxVolume);
		_musicSlider->SetValue(_settings.musicVolume);
	}
	// And the choices are taken again from the player's profile; the left-handed box stays as the hand is drawn. When the
	// box was just left, from one of its pages or its own tab, that leave keeps these very choices with the profile
	// first, so they stay
	if (_profileSource && !_optionsLeft)
	{
		if (const auto kept = _profileSource(); kept.has_value())
		{
			const bool leftHanded = _settings.leftHandedHand;
			TakeProfileSettings(_settings, *kept);
			_settings.leftHandedHand = leftHanded;
		}
	}
	for (const auto& show : _showOptionsChoices)
	{
		show();
	}
	for (const auto& show : _showAdvancedChoices)
	{
		show();
	}
}

void GameMenu::SetInsideTemple(bool inside)
{
	if (_insideTemple == inside)
	{
		return;
	}
	_insideTemple = inside;
	for (const auto page : {Page::Options, Page::Players, Page::Advanced, Page::Controls, Page::Mods})
	{
		if (const auto& dialog = _pages.at(static_cast<size_t>(page)); dialog != nullptr)
		{
			dialog->SetTabLabel(0, inside ? _worldRoomTab : _mainMenuTab);
		}
	}
}

void GameMenu::ShowCreatureHelp(bool on) noexcept
{
	_settings.creatureHelp = on;
	if (_creatureHelpBox != nullptr)
	{
		_creatureHelpBox->SetChecked(on);
	}
}

void GameMenu::Open()
{
	if (_open)
	{
		return;
	}
	_open = true;
	_question.reset();
	_page = Page::Main;
	CurrentDialog().Reset();
	// Over the game no box was shown: the first page fades in from nothing
	_fades.Show(Page::Main, PageSwitch::Replace);
	_profileShown = true;
}

void GameMenu::Close()
{
	if (_page == Page::Players)
	{
		KeepPlayerEdits();
	}
	if (_page == Page::Options)
	{
		LeaveOptions();
	}
	// The menu closing over the options' box leaves it
	if (_open && (_page == Page::Options || _page == Page::Advanced))
	{
		_optionsLeft = true;
	}
	_open = false;
	_question.reset();
	CurrentDialog().Reset();
	_fades.Hide();
}

Dialog& GameMenu::DialogOf(Page page) const
{
	return page == Page::Statistics ? _statistics->GetDialog() : *_pages.at(static_cast<size_t>(page));
}

void GameMenu::OpenStatistics()
{
	auto [figures, local] =
	    _statisticsSource ? _statisticsSource() : std::pair<statistics::BoxFigures, PlayerNames> {{}, PlayerNames::PLAYER_ONE};
	_statistics->Open(std::move(figures), local, false);
	// The main page's tab: every box hidden, then the statistics shown
	ShowPage(Page::Statistics, PageSwitch::Replace);
}

void GameMenu::Update(float deltaSeconds)
{
	_fades.Update(deltaSeconds);
	_statistics->Update(deltaSeconds);
	if (_question)
	{
		_question->fade.Update(deltaSeconds);
	}
	if (_open)
	{
		CurrentDialog().Update(_question ? 0.0f : deltaSeconds);
		if (_question)
		{
			_question->answers->Update(deltaSeconds);
		}
	}

	// The wheel turns over a picture every tenth of a second, resting one in five
	_wheelTime += deltaSeconds;
	const auto frame = static_cast<int>(_wheelTime * 10.0f) % 5;
	for (const auto& [index, up] : _wheelMice)
	{
		// The wheel turns the four frames one way scrolling up and the other scrolling down, then rests
		auto cell = 11;
		if (frame < 4)
		{
			cell = up ? 12 + frame : 15 - frame;
		}
		_controls->SetMouseCell(index, cell);
	}
}

void GameMenu::MouseMove(glm::ivec2 point)
{
	if (_open)
	{
		(_question ? *_question->answers : CurrentDialog()).MouseMove(point);
	}
}

void GameMenu::MouseDown(glm::ivec2 point)
{
	if (_open)
	{
		(_question ? *_question->answers : CurrentDialog()).MouseDown(point);
	}
}

GameMenu::Action GameMenu::MouseUp(glm::ivec2 point)
{
	if (!_open)
	{
		return Action::None;
	}
	if ((_question ? *_question->answers : CurrentDialog()).MouseUp(point))
	{
		_clicked = true;
	}
	if (std::exchange(_answered, false))
	{
		// the box back at once whole, as the question goes
		_question.reset();
		_fades.SnapShown();
		// the box's return: the kept tab, or from Options the main page (the options box hides)
		const auto how = _pageAfterTell.has_value() ? _switchAfterTell : PageSwitch::CrossFade;
		if (const auto page = PageAfterAnswer(std::exchange(_pageAfterTell, std::nullopt), _page); page.has_value())
		{
			ShowPage(*page, how);
		}
	}
	return std::exchange(_action, Action::None);
}

void GameMenu::Wheel(glm::ivec2 point, int steps)
{
	if (_open && !_question)
	{
		CurrentDialog().Wheel(point, steps);
	}
}

void GameMenu::TextInput(std::u16string_view text)
{
	if (_open && !_question)
	{
		CurrentDialog().TextInput(text);
	}
}

void GameMenu::KeyDown(int key)
{
	if (_open && !_question)
	{
		if (_page == Page::Players && (key == SDLK_RETURN || key == SDLK_KP_ENTER))
		{
			KeepPlayerEdits();
		}
		CurrentDialog().KeyDown(key);
	}
}

GameMenu::Action GameMenu::Escape()
{
	if (!_open)
	{
		return Action::None;
	}
	if (_question)
	{
		switch (EscapeOverQuestion(_pageAfterTell.has_value(), _page == Page::Main))
		{
		case EscapeOverQuestionResult::Answer: // the notice's «OK»
			_question.reset();
			_fades.SnapShown();
			ShowPage(*std::exchange(_pageAfterTell, std::nullopt), _switchAfterTell);
			return Action::None;
		case EscapeOverQuestionResult::Continue: // Continue: the menu closes and the game goes on
			_question.reset();
			return Action::Continue;
		case EscapeOverQuestionResult::Nothing:
			break;
		}
		return Action::None;
	}
	if (_page == Page::Players)
	{
		KeepPlayerEdits();
		// Leaving the page would start the game again: Escape does nothing
		if (IsRestartPending())
		{
			return Action::None;
		}
	}
	// Escape hides the box shown, back to the one under it. The first page does not let Escape out of it: the menu
	// stays open and only its Continue goes back to the game
	if (_page != Page::Main)
	{
		ShowPage(Page::Main, PageSwitch::CrossFade);
	}
	return Action::None;
}

void GameMenu::Draw(const DialogPainter& painter) const
{
	// The page left fades out under the one shown, taking no input
	if (const auto leaving = _fades.Leaving(); leaving.has_value() && leaving->second > 0.0f)
	{
		painter.SetAlpha(leaving->second);
		DialogOf(leaving->first).Draw(painter, false);
	}
	const auto shown = _fades.Shown();
	const auto fade = shown.has_value() ? shown->second : 0.0f;
	if (fade <= 0.0f)
	{
		painter.SetAlpha(1.0f);
		return;
	}
	// The menu dims to a quarter while a question holds it
	painter.SetAlpha(_question ? AlphaBehindQuestion(fade, _question->fade.value) : fade);
	CurrentDialog().Draw(painter, _open && !_question);
	if (_question)
	{
		// An opaque box over the menu, fading in by itself, whatever the menu's own fade
		painter.SetAlpha(std::clamp(_question->fade.value, 0.0f, 1.0f));
		painter.DrawBackground(GetQuestionRect(), glm::vec3(1.0f), true, DialogPainter::Edges::All);
		_question->answers->Draw(painter, _open);
	}
	painter.SetAlpha(1.0f);
}
