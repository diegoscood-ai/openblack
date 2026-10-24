/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "BoxFades.h"
#include "Common/PlayerProfiles.h"
#include "Common/Zoomer.h"
#include "Dialog.h"
#include "ECS/StatisticsRules.h"

namespace openblack::gui
{

class GameFont;
class StatisticsBox;
class TextDatabase;

/// What the player sets in the game's menu
struct MenuSettings
{
	/// 0 to 1
	float sfxVolume {1.0f};
	float musicVolume {1.0f};
	/// HELP_TEXT_DIALOG_DETAIL_00 to _04, Minimum to Maximum Detail
	int detail {4};
	bool autoSave {true};
	bool pushScrolling {false};
	/// HELP_TEXT_DIALOG_NOHELP to ALLHELP
	int helpLevel {3};
	/// HELP_TEXT_DIALOG_NOTEXT to ALLTEXT
	int storyText {1};
	/// HELP_TEXT_DIALOG_NOTOOLTIPS to ALLTOOLTIPS
	int toolTips {2};
	bool leftHandedHand {false};
	bool creatureHelp {true};
	bool textFromBottom {false};
	/// The Advanced page's HELP_TEXT_DIALOG_HELPTEXTSPEED slider: the help's read speed, which takes effect as the
	/// options' box is left; Game fills it from the help system when it makes the menu, then from the profile
	float helpTextSpeed {0.5f};
	std::u16string creatureName;
	/// One of the 16 player symbols
	int symbol {15};
};

/// What the Mods page shows: the user's choice of 2026-10-21, not the original's. The menu has the page only when it is
/// given this, which the game does only when the mod loader library is there and the mods folder is read
struct MenuMods
{
	struct Mod
	{
		std::u16string label;
		bool enabled {false};
	};
	/// The line above the list: what the loader said about the mods folder
	std::u16string heading;
	std::vector<Mod> mods;
	/// Asked to keep a mod's switch (its index, on or off); when it answers false the switch goes back
	std::function<bool(size_t, bool)> onSwitch;
};

/// A mod's line on the Mods page: its name, then its version and its author when it gives them
[[nodiscard]] std::u16string MenuModLabel(std::u16string_view name, std::u16string_view version, std::u16string_view author);

/// The options of the menu as the player's profile keeps them (the help text speed is the profile's read speed)
[[nodiscard]] player_profiles::Settings ToProfileSettings(const MenuSettings& settings);
/// The menu's options from the player's profile; the sound and the detail aren't kept with it. A tooltip level, story
/// text or help level out of the entries its selector shows is taken at its place on it (OptionsIndex)
void TakeProfileSettings(MenuSettings& settings, const player_profiles::Settings& kept);

/// The place a value kept with the profile takes on one of the options box's selectors of `count` entries: a negative
/// value `negativeOffset` up, then its size, wrapped round. Only the box wraps it: the game takes the value as it is
/// until the box is left
[[nodiscard]] constexpr int32_t OptionsIndex(int32_t value, int32_t count, int32_t negativeOffset)
{
	const int32_t raised = value < 0 ? value + negativeOffset : value;
	return (raised < 0 ? -raised : raised) % count;
}

/// A volume slider shows a main volume (0 to 127) times the float nearest 1/127, as the options box sets it. That is one
/// float step below the volume divided by 127 for eight volumes, which then read back one lower
inline constexpr float k_SliderPerVolume = 1.0f / 127.0f;
[[nodiscard]] constexpr float SliderOfVolume(uint32_t volume)
{
	return static_cast<float>(volume) * k_SliderPerVolume;
}
/// And a slider's place gives the volume times 127, truncated toward zero
[[nodiscard]] constexpr uint32_t VolumeOfSlider(float slider)
{
	return static_cast<uint32_t>(slider * 127.0f);
}

/// The menu Escape brings up during a game, and the options behind it.
///
/// The menu does not take Escape with Shift or Ctrl held (Alt does not count), over a full screen film, while a script
/// holds the wide screen or within 300 ms of the menu closing (a negative difference passes). An open menu always
/// takes it.
struct EscapeState
{
	bool shiftOrCtrl {false};
	bool filmPlaying {false};
	bool scriptWideScreen {false};
	bool menuOpen {false};
	int32_t msSinceMenuClosed {1000}; ///< Now less the time the menu last closed
};
[[nodiscard]] constexpr bool EscapeBlocked(const EscapeState& s)
{
	const bool debounce = s.msSinceMenuClosed >= 0 && s.msSinceMenuClosed <= 300;
	return (s.shiftOrCtrl || s.filmPlaying || s.scriptWideScreen || debounce) && !s.menuOpen;
}

/// What Escape does over a question: a notice is answered by its «OK»; a Yes / No has no Escape button, so on the main
/// page Continue closes the menu and resumes the game, and on the other pages nothing happens
enum class EscapeOverQuestionResult
{
	Answer,
	Continue,
	Nothing,
};
[[nodiscard]] constexpr EscapeOverQuestionResult EscapeOverQuestion(bool notice, bool mainPage)
{
	if (notice)
	{
		return EscapeOverQuestionResult::Answer;
	}
	return mainPage ? EscapeOverQuestionResult::Continue : EscapeOverQuestionResult::Nothing;
}

/// The first page greets the player above five buttons: Continue Game, Start Skirmish Game, Join Online Game,
/// Options and Quit Black & White, with a Main Menu and a Statistics tab. Options opens the options' pages, each a tab
/// of the same box: Options for sound, detail, autosave and push scrolling, Players for the player's profile, Advanced
/// for help, the hand and text and Controls for the keys and buttons of the game's actions. Their Main Menu tab, or
/// Back, goes back to the first page. Given the mods (MenuMods), the options' box has a sixth tab, Mods, after
/// Controls, its six tabs sharing the box's width.
///
/// The menu fades in over half a second and out over a fifth. Quitting first asks whether the player is sure, in a
/// smaller opaque box over the menu with a Yes and a No arrow.
class GameMenu
{
public:
	enum class Action
	{
		None,
		Continue,
		StartSkirmish,
		JoinOnline,
		/// Create New Player: the new player's box is to be shown
		CreatePlayer,
		/// The player picked another profile from the list (GetPickedPlayer), to be played from now on
		PickPlayer,
		/// The player said yes to deleting the profile picked (GetPlayerToDelete); the one to play next is
		/// GetPickedPlayer, none when no profile is left
		DeletePlayer,
		EditTattoo,
		/// The player said yes to starting a new game
		StartNewGame,
		/// The player said yes to the game starting again, as the profile now picked
		Restart,
		RedefineControl,
		Quit,
	};

	enum class Page
	{
		Main,
		Options,
		Players,
		Advanced,
		Controls,
		/// The Game Statistics box, from the main menu's Statistics tab
		Statistics,
		/// The mods and their switches, only with the mods given (MenuMods)
		Mods,
		_Count
	};

	/// The menu's alpha behind a question: fade - hold x 0.75, clamped to 0..1; hold is the question's fade-in (0 -> 1
	/// in 0.5 s)
	[[nodiscard]] static float AlphaBehindQuestion(float fade, float hold)
	{
		return std::clamp(fade - std::clamp(hold, 0.0f, 1.0f) * 0.75f, 0.0f, 1.0f);
	}

	/// The page after a question is answered: the kept tab, else from the Options page the main page (the box hides),
	/// else the page stays
	[[nodiscard]] static std::optional<Page> PageAfterAnswer(std::optional<Page> kept, Page current)
	{
		if (kept.has_value())
		{
			return kept;
		}
		if (current == Page::Options)
		{
			return Page::Main;
		}
		return std::nullopt;
	}

	/// The texts are read as the menu is made, the font measures them. playerName fills in the welcome and is the
	/// player's profile.
	GameMenu(const TextDatabase& texts, const GameFont& font, std::u16string_view playerName, MenuSettings settings,
	         std::optional<MenuMods> mods = std::nullopt);
	~GameMenu();

	/// Starts fading in on the first page, if it isn't open already
	void Open();
	/// Starts fading out, closing the question about quitting too
	void Close();
	/// Open and taking the player's input, not fading out
	[[nodiscard]] bool IsOpen() const noexcept { return _open; }
	/// On the screen, fading out included
	[[nodiscard]] bool IsVisible() const noexcept { return _open || _fades.Leaving().has_value(); }
	[[nodiscard]] bool IsAskingToQuit() const noexcept { return _question.has_value(); }
	/// Whether the options have the Mods page, and its mods as they are switched now
	[[nodiscard]] bool HasModsPage() const noexcept { return _mods.has_value(); }
	[[nodiscard]] const std::vector<MenuMods::Mod>& GetMods() const;
	[[nodiscard]] Page GetPage() const noexcept { return _page; }
	/// The page shown and the one fading out under it, with their alphas
	[[nodiscard]] const BoxFades<Page>& GetFades() const noexcept { return _fades; }
	/// Inside the temple the options' first tab is the World Room, which closes them, rather than the Main Menu
	void SetInsideTemple(bool inside);
	[[nodiscard]] bool IsInsideTemple() const noexcept { return _insideTemple; }
	/// How a page replaces the one shown: fading out under it (Show, Back, Escape) or gone at once (a tab)
	using PageSwitch = BoxFades<Page>::Switch;
	void ShowPage(Page page, PageSwitch how);
	/// What the Game Statistics box shows as it opens: the figures now and the player at this machine
	using StatisticsSource = std::function<std::pair<statistics::BoxFigures, PlayerNames>()>;
	void SetStatisticsSource(StatisticsSource source) { _statisticsSource = std::move(source); }
	/// The main volumes (0 to 127), the effects' then the music's, which the volume sliders are set from each time the
	/// Options or the Advanced page shows. It is asked with whether the options' box was left just before (a tab from
	/// one of its pages, its own too), whose volumes are to be kept before the shown page's are read
	using VolumeSource = std::function<std::pair<uint32_t, uint32_t>(bool boxLeft)>;
	void SetVolumeSource(VolumeSource source) { _volumeSource = std::move(source); }
	/// The options kept with the player's profile, which the Options and the Advanced pages take again each time either
	/// shows; none without a profile, when the pages keep the menu's own
	using ProfileSource = std::function<std::optional<player_profiles::Settings>()>;
	void SetProfileSource(ProfileSource source) { _profileSource = std::move(source); }
	/// Opens the Game Statistics box with the figures as they are now
	void OpenStatistics();
	[[nodiscard]] const StatisticsBox& GetStatistics() const noexcept { return *_statistics; }

	void Update(float deltaSeconds);

	// Input at points of the dialog space
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// What the player chose by letting go of the button, Quit only once they have said yes
	Action MouseUp(glm::ivec2 point);
	void Wheel(glm::ivec2 point, int steps);
	void TextInput(std::u16string_view text);
	void KeyDown(int key);
	/// Escape backs out of the question, then out of the options, then continues the game
	Action Escape();

	/// Whether a control was clicked since last asked, which the box answers with the menu button sound
	bool TakeClicked() noexcept { return std::exchange(_clicked, false); }
	/// Whether the settings changed since last asked
	bool TakeSettingsChanged() noexcept { return std::exchange(_settingsChanged, false); }
	/// A detail entry chosen on the Options page, once, after the page is left (another page or the menu closed): the
	/// entry it showed before and the one it shows now (graphics::detail_level::LevelAfterOptions)
	struct DetailChange
	{
		int from;
		int to;
	};
	std::optional<DetailChange> TakeDetailChange() noexcept { return std::exchange(_detailChange, std::nullopt); }
	/// Whether the options' box was left since last asked: the Options or the Advanced page left for another page, for
	/// its own tab again, by Escape or by the menu closing. The Advanced page's choices are kept with the profile and
	/// take effect then
	bool TakeOptionsLeft() noexcept { return std::exchange(_optionsLeft, false); }
	[[nodiscard]] const MenuSettings& GetSettings() const noexcept { return _settings; }
	/// The creature help box shows the switch as the game holds it (a script may set it); not a change of the player's
	void ShowCreatureHelp(bool on) noexcept;

	/// What the player gave on the Players page for the profile played: its creature's name and its symbol
	struct PlayerEdits
	{
		std::u16string creatureName;
		int symbol {0};
	};
	/// The Players page's edits, once they are to be kept: as the page is left, before what its buttons do, and on
	/// Return or Escape
	std::optional<PlayerEdits> TakePlayerEdits() noexcept { return std::exchange(_playerEdits, std::nullopt); }
	/// The profiles' names in the list's order and the one played. The first time the Players page is shown after
	/// being left, the one played is the one the game was started as: picking another makes Back the Restart arrow.
	void SetPlayers(std::vector<std::u16string> names, std::optional<size_t> current);
	/// The profile picked shows its creature's name, its symbol and its options
	void ShowPlayer(std::u16string_view creatureName, int symbol, const player_profiles::Settings& settings);
	/// Edit Tattoo shows only while the creature can have its tattoo edited
	void SetTattooAvailable(bool available);
	[[nodiscard]] std::optional<size_t> GetPickedPlayer() const noexcept { return _pickedPlayer; }
	[[nodiscard]] const std::u16string& GetPlayerToDelete() const noexcept { return _playerToDelete; }
	/// The player has picked another profile or lost the one played, so leaving the page starts the game again
	[[nodiscard]] bool IsRestartPending() const noexcept { return _mustRestart || _currentPlayer != _originalPlayer; }
	[[nodiscard]] const List& GetPlayers() const noexcept { return *_players; }
	/// The Players page's arrow: Back, or Restart once leaving the page starts the game again
	[[nodiscard]] const std::u16string& GetPlayersBackLabel() const noexcept { return _playersBack->label; }
	/// Where the Players page's square buttons are: Create New Player, Delete Player, Edit Tattoo and Start a New Game
	[[nodiscard]] DialogRect GetPlayerButtonRect(size_t index) const { return _playerButtons.at(index)->rect; }
	[[nodiscard]] bool IsPlayerButtonVisible(size_t index) const { return _playerButtons.at(index)->visible; }
	/// The question or the message up over the menu, empty without one
	[[nodiscard]] const std::u16string& GetQuestionText() const;

	void Draw(const DialogPainter& painter) const;

	// The layout of the first page, for hit tests
	static constexpr size_t k_ButtonCount = 5;
	[[nodiscard]] static DialogRect GetButtonRect(size_t index);
	[[nodiscard]] static constexpr DialogRect GetQuestionRect() { return {.min = {150, 200}, .max = {650, 400}}; }
	[[nodiscard]] const std::u16string& GetWelcome() const noexcept { return _welcome; }
	[[nodiscard]] const std::u16string& GetButtonLabel(size_t index) const { return _buttonLabels.at(index); }
	/// The lines of the Controls page
	[[nodiscard]] const List& GetControls() const noexcept { return *_controls; }

private:
	struct Question
	{
		std::u16string text;
		std::unique_ptr<Dialog> answers;
		Zoomer fade;
	};

	void BuildMain(const TextDatabase& texts);
	void BuildOptions(const TextDatabase& texts);
	void BuildPlayers(const TextDatabase& texts);
	void BuildAdvanced(const TextDatabase& texts);
	void BuildControls(const TextDatabase& texts);
	void BuildMods(const TextDatabase& texts);
	/// The tabs of the options' pages, with selected open
	std::vector<Dialog::Tab> OptionsTabs(const TextDatabase& texts, Page selected);
	/// An options' page: the original's five tabs, or six with the Mods page, `page`'s open
	std::unique_ptr<Dialog> MakeOptionsPage(const TextDatabase& texts, Page page);
	/// A question with a Yes, which settles on the action, and a No
	void Ask(std::u16string text, Action yes = Action::Quit);
	/// A notice: the text and one «OK», which settles on the action
	void Tell(std::u16string text, Action ok = Action::None);
	/// With the detail changed since the Options page opened, HELP_TEXT_DIALOG_VIDEOCHANGE is told, the shown detail
	/// updated and `after` kept for the message's answer, with how it is shown; false when the detail did not change
	bool TellVideoChange(Page after, PageSwitch how);
	/// An options tab: on the Options page, VIDEOCHANGE first when the detail changed
	void SelectOptionsTab(Page page);
	void Settle(Action action);
	/// Leaving the Players page for another while leaving it starts the game again: the player is asked first; true when
	/// they are
	bool AskToRestartFirst(Page page);
	/// The Players page's arrow: Back, or the game starting again
	void PlayersBack();
	void PickPlayer(size_t index);
	/// Yes to deleting the profile picked: which is deleted and which is played after it
	void PrepareDeletion();
	/// The Players page's edits are to be kept
	void KeepPlayerEdits();
	/// Back or Restart, and Edit Tattoo shown or not, the square buttons spread out over the space
	void UpdatePlayersPage();
	void SettingsChanged() { _settingsChanged = true; }
	/// Leaving the Options page: a detail entry other than the one applied last becomes the change to take
	void LeaveOptions();
	/// The Options or the Advanced page shown, its own tab again too: the choices are taken again from the profile and
	/// the volumes, and the controls set to them
	void ShowOptionsBox();
	[[nodiscard]] Dialog& CurrentDialog() const { return DialogOf(_page); }
	[[nodiscard]] Dialog& DialogOf(Page page) const;

	const TextDatabase& _texts;
	const GameFont& _font;
	std::u16string _playerName;
	std::u16string _welcome;
	std::array<std::u16string, k_ButtonCount> _buttonLabels;
	std::u16string _yes;
	std::u16string _no;
	/// HELP_TEXT_DIALOG_QUIT_QUESTION, asked by the first page, and HELP_TEXT_DIALOG_AREYOUSUREQUIT by the options
	std::u16string _quitQuestion;
	std::u16string _quitGameQuestion;
	std::u16string _accept;      ///< HELP_TEXT_REQUESTER_BOXES_04
	std::u16string _videoChange; ///< HELP_TEXT_DIALOG_VIDEOCHANGE
	std::u16string _back;
	std::u16string _restart;
	/// HELP_TEXT_DIALOG_RESTART_WARNING, asked before the game starts again, and HELP_TEXT_DIALOG_RESTART, told when it
	/// must
	std::u16string _restartQuestion;
	std::u16string _restartNotice;
	/// HELP_TEXT_DIALOG_AERYOUSUREDELETE, with the player's name for its %s
	std::u16string _deleteQuestion;
	std::u16string _newGameQuestion;
	/// The detail when the page opened or the player was last told (the current one is _settings.detail)
	int _detailShown {0};
	/// The detail entry the game runs with, and the change the player made to it, until the game takes it
	int _detailApplied {0};
	std::optional<DetailChange> _detailChange;
	std::array<std::u16string, 5> _detailLabels;
	std::array<std::u16string, 5> _helpLabels;
	std::array<std::u16string, 3> _storyLabels;
	std::array<std::u16string, 4> _toolTipLabels;
	MenuSettings _settings;
	std::optional<MenuMods> _mods;
	std::array<std::unique_ptr<Dialog>, static_cast<size_t>(Page::_Count)> _pages;
	/// The Game Statistics box, its own page's dialog
	std::unique_ptr<StatisticsBox> _statistics;
	StatisticsSource _statisticsSource;
	VolumeSource _volumeSource;
	Slider* _sfxSlider {nullptr};
	Slider* _musicSlider {nullptr};
	ProfileSource _profileSource;
	/// Set the Options and the Advanced pages' boxes, selectors and slider to show the menu's choices
	std::vector<std::function<void()>> _showOptionsChoices;
	std::vector<std::function<void()>> _showAdvancedChoices;
	/// The options' first tab, outside the temple and inside it
	std::u16string _mainMenuTab;
	std::u16string _worldRoomTab;
	bool _insideTemple {false};
	List* _controls {nullptr};
	CheckBox* _creatureHelpBox {nullptr};
	List* _players {nullptr};
	EditBox* _creatureNameBox {nullptr};
	SymbolPicture* _symbolPicture {nullptr};
	BigButton* _playersBack {nullptr};
	std::array<BigButton*, 4> _playerButtons {};
	std::vector<std::u16string> _playerNames;
	std::optional<size_t> _currentPlayer;
	/// The profile the game was started as, by its name, and its place in the list
	std::u16string _originalName;
	std::optional<size_t> _originalPlayer;
	/// The profile the game was started as was deleted: leaving the page starts the game again
	bool _mustRestart {false};
	bool _tattooAvailable {false};
	std::optional<size_t> _pickedPlayer;
	std::u16string _playerToDelete;
	std::optional<PlayerEdits> _playerEdits;
	/// The wheel's mice on the Controls page turn over
	std::vector<std::pair<size_t, bool>> _wheelMice;
	float _wheelTime {0.0f};

	Page _page {Page::Main};
	bool _open {false};
	/// The page shown and the one fading out under it
	BoxFades<Page> _fades;
	std::optional<Question> _question;
	Action _action {Action::None};
	bool _clicked {false};
	bool _answered {false};
	std::optional<Page> _pageAfterTell;                  ///< The page the VIDEOCHANGE answer goes to (or Back's hide)
	PageSwitch _switchAfterTell {PageSwitch::CrossFade}; ///< How: a kept tab replaces, Back's hide fades
	bool _settingsChanged {false};
	bool _optionsLeft {false};
};

} // namespace openblack::gui
