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

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "Canvas.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureStatusPanel.h"
#include "Dialog.h"
#include "DialogPainter.h"
#include "GameFont.h"
#include "GameMenu.h"
#include "MenuBackdrop.h"
#include "NewPlayerBox.h"
#include "SkipBox.h"
#include "StoryHistoryBox.h"
#include "TattooEditorDialog.h"
#include "TextDatabase.h"
#include "ToolTips.h"

union SDL_Event;

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui
{

/// Every dialog put away at once, as the bars coming in do. No box hears of it but through its own close: the menu
/// closes as it does when it is left, keeping what its page changed, and fades out. The tattoo editor's box and the story
/// history's buttons go too, but their owners show them again while the creature or the scroll is looked at, and the
/// tattoos stay as they are. Nothing brings the menu back when the bars go
void HideAllDialogs(GameMenu& menu, TattooEditorDialog& tattooEditor, StoryHistoryBox& storyHistory);

/// The game's own interface, drawn over the scene: for now the menu that Escape brings up.
///
/// It loads what Black & White's dialogs are made of: the texts of the info scripts, the font j0 and the front end
/// atlas, and takes the mouse and keyboard from the game while a dialog is open.
class GameInterface
{
public:
	/// Null when the game's files for it are missing
	/// The key the constructor takes: only GameInterface makes one, so only Create makes a GameInterface, through
	/// std::make_unique
	class Key
	{
		friend class GameInterface;
		Key() = default;
	};

	static std::unique_ptr<GameInterface> Create(std::u16string_view playerName, MenuSettings settings);
	GameInterface(Key key, TextDatabase texts, GameFont font, std::shared_ptr<graphics::Texture2D> atlas,
	              std::unique_ptr<graphics::Texture2D> fontTexture, std::shared_ptr<graphics::Texture2D> symbols,
	              std::shared_ptr<graphics::Texture2D> mice, std::u16string_view playerName, MenuSettings settings);
	~GameInterface();

	/// Takes Escape, and every key press and mouse event while the menu is open. True when the event was taken.
	bool ProcessEvent(const SDL_Event& event, glm::u16vec2 resolution);
	/// What the player last chose in the menu, once
	GameMenu::Action TakeAction();
	/// Whether the menu's settings changed since last asked
	bool TakeSettingsChanged() { return _menu->TakeSettingsChanged(); }

	void Update(float deltaSeconds);
	/// The open dialogs and, over them, the pointer at the mouse; over a debug window the pointer is drawn too
	void Draw(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds, bool overDebugWindow);

	[[nodiscard]] GameMenu& GetMenu() noexcept { return *_menu; }
	/// The tattoo editor's dialog, open while the tattoo editor is
	[[nodiscard]] TattooEditorDialog& GetTattooEditor() noexcept { return *_tattooEditor; }
	/// The buttons of the story history in the temple's library
	[[nodiscard]] StoryHistoryBox& GetStoryHistoryBox() noexcept { return *_storyHistoryBox; }
	[[nodiscard]] const StoryHistoryBox& GetStoryHistoryBox() const noexcept { return *_storyHistoryBox; }
	/// Every dialog put away at once, as the bars coming in do (gui::HideAllDialogs)
	void HideAllDialogs() { gui::HideAllDialogs(*_menu, *_tattooEditor, *_storyHistoryBox); }
	/// The skip tutorial box at the start of a game: shown until answered
	void ShowSkipBox();
	/// (test hook) the SkipBox answered as its OK would
	void AnswerSkipBox(int32_t answer)
	{
		if (IsSkipBoxShown())
		{
			_skipBox.reset();
			_skipAnswer = answer;
		}
	}
	[[nodiscard]] bool IsSkipBoxShown() const noexcept { return _skipBox.has_value() && _skipBox->IsVisible(); }
	/// The game's own message box over play: the text and an OK, 500 by 200 in the middle of the screen, as the game
	/// tells a save it could not write. It takes the mouse and the keys pressed until its OK is clicked, Escape does not
	/// close it, and the game goes on under it; a new message replaces the one shown
	void ShowGameMessage(std::u16string text);
	/// The game's message that a save could not be written, "An error has occurred!"
	void ShowSaveProblem() { ShowGameMessage(std::u16string(_texts.Get("HELP_TEXT_DIALOG_ERRORHASOCCURED"))); }
	[[nodiscard]] bool IsGameMessageShown() const noexcept { return _gameMessage.has_value(); }
	/// Whether a box with a background panel takes the input: the menu while it is open (not while it fades out) or
	/// the SkipBox. The fight and creature panels and the input prompt icons are not drawn under it
	[[nodiscard]] bool IsBoxActive() const noexcept
	{
		return _menu->IsOpen() || IsSkipBoxShown() || IsGameMessageShown() || _newPlayerBox->IsActive() ||
		       _boxOverGameFade.has_value();
	}

	/// Asks a new player for their name, over the front end's swirling backdrop; Cancel shows only when someone has a
	/// profile. checkName says whether a name may be taken
	void ShowNewPlayerBox(bool canCancel, NewPlayerBox::CheckName checkName);
	/// The new player's box during play, from the Players page: the screen goes to the front end's backdrop as on a
	/// first start
	void ShowNewPlayerBoxOverGame(bool canCancel, NewPlayerBox::CheckName checkName);
	/// The new player's box is up over the game, or its screen is fading
	[[nodiscard]] bool IsNewPlayerScreenOverGame() const noexcept { return _boxOverGameFade.has_value(); }
	/// The new player's box takes the mouse and the keyboard; true when it took the event
	bool ProcessNewPlayerBoxEvent(const SDL_Event& event, glm::u16vec2 resolution);
	[[nodiscard]] NewPlayerBox& GetNewPlayerBox() noexcept { return *_newPlayerBox; }
	/// The new player's screen: the backdrop, swirling on by the frame's milliseconds, the box and the pointer, under
	/// black of an alpha
	void DrawNewPlayerScreen(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds, float frameMilliseconds,
	                         uint8_t black);
	/// The menu as the player's profile has it: greeting them, with their creature's name, symbol and options
	void SetPlayerProfile(std::u16string_view name, std::u16string_view creatureName, int symbol,
	                      const player_profiles::Settings& kept);
	/// The SkipBox's answer once given (its OK), then nothing
	std::optional<int32_t> TakeSkipAnswer() { return std::exchange(_skipAnswer, std::nullopt); }
	/// Whether the Escape key opened the menu over the game since this was last asked
	bool TakeMenuOpenedByKey() noexcept { return std::exchange(_menuOpenedByKey, false); }
	[[nodiscard]] const TextDatabase& GetTexts() const noexcept { return _texts; }
	/// The dialogs' font, which the temple's scrolls are written in too
	[[nodiscard]] const GameFont& GetFont() const noexcept { return _font; }
	/// The font's glyphs, their coverage in the texture's one channel
	[[nodiscard]] const graphics::Texture2D& GetFontTexture() const { return _font.GetTexture(); }

	/// Words shown over the screen, wrapped across the dialogs' 800 by 600 and 60 high from its top, as the temple's
	/// future room shows them
	struct Message
	{
		std::u16string text;
		float alpha;
	};
	void SetMessage(std::optional<Message> message) { _message = std::move(message); }

	/// Lines of words centred at the top of the screen, in black and again in yellow just right of and below, as the
	/// temple's challenge room titles the picture it looks at closely
	struct Caption
	{
		std::vector<std::u16string> lines;
		/// From 0 to 255
		uint8_t alpha;
	};
	void SetCaption(std::optional<Caption> caption) { _caption = std::move(caption); }

	/// What the hand shows for what it is over
	[[nodiscard]] ToolTips& GetToolTips() noexcept { return _toolTips; }
	/// The creature's status panel shown this frame, or none. It shows its reward row when the values have a reward,
	/// and near the top of the screen without one, as while the camera follows a creature.
	void SetCreaturePanel(std::optional<creature_panel::Values> values) { _creaturePanel = values; }
	/// The fight's panel shown this frame, or none: each fighter's name over its health and stamina
	void SetFightPanel(std::optional<creature_fight_hud::Values> values) { _fightPanel = std::move(values); }
	/// The fight's panel and the creature's status panel are drawn in the renderer's end of the frame, as the original
	/// draws them: over the cinema bars and the dialogue, under the film and the screen fade
	/// (graphics::DrawSceneDesc's hooks). Neither while a dialog is open
	void DrawFightPanelLayer(glm::u16vec2 resolution);
	/// The scripts' clipping window and the words they write across the screen, the newest first, after the fight's
	/// panel; none while a video plays
	void DrawScriptTextLayer(glm::u16vec2 resolution);
	void DrawCreaturePanelLayer(glm::u16vec2 resolution);

private:
	/// The tattoo editor's dialog takes the mouse while it is open, and of the keyboard Enter and Escape
	bool ProcessTattooEditorEvent(const SDL_Event& event);
	/// The game's message box takes the mouse while it is shown; true when it took the event
	bool ProcessGameMessageEvent(const SDL_Event& event);
	/// The story history's buttons take the mouse only over a button; true when they took the event
	bool ProcessStoryHistoryEvent(const SDL_Event& event);
	/// The dialog opens with the tattoo editor and follows it every frame
	void UpdateTattooEditor(float deltaSeconds);
	/// The creature's status panel, at the left of the screen
	void DrawCreaturePanel(glm::u16vec2 resolution);
	/// The fight's panel, at the top left of the screen
	void DrawFightPanel(glm::u16vec2 resolution);
	/// A bar of the creature's panels in screen pixels: its framed box, its fill from its left (or its middle, with a line
	/// there) brightening towards where it ends, and shadows inside its frame
	void DrawStatBar(const creature_panel::Rect& bar, float fill, glm::u8vec3 colour, bool fromMiddle);
	/// The caption at the top of the screen
	void DrawCaption(glm::u16vec2 resolution);
	/// The front end's swirling backdrop over the screen's 4:3 box, the swirls turning by the frame's milliseconds
	void DrawMenuBackdrop(glm::u16vec2 resolution, float frameMilliseconds);

	TextDatabase _texts;
	GameFont _font;
	std::shared_ptr<graphics::Texture2D> _atlas; ///< from the texture cache, as _symbols and _mice
	std::unique_ptr<graphics::Texture2D> _fontTexture;
	std::shared_ptr<graphics::Texture2D> _symbols;
	std::shared_ptr<graphics::Texture2D> _mice;
	Canvas _canvas;
	Canvas _pointerCanvas {graphics::RenderPass::Cursor};
	/// The panels' canvas, in the renderer's end of frame view, in its order there
	Canvas _panelCanvas {graphics::RenderPass::ScreenOverlay, Canvas::ViewUse::Shared};
	DialogPainter _painter;
	DialogPainter _panelPainter;
	std::unique_ptr<GameMenu> _menu;
	std::unique_ptr<TattooEditorDialog> _tattooEditor;
	std::unique_ptr<StoryHistoryBox> _storyHistoryBox;
	std::unique_ptr<NewPlayerBox> _newPlayerBox;
	/// The front end's backdrop: its texture, repeating, its swirls and the two layers' texture coordinates
	std::shared_ptr<graphics::Texture2D> _backdrop;
	std::optional<menu_backdrop::Swirls> _backdropSwirls;
	std::array<menu_backdrop::Layer, menu_backdrop::k_Layers> _backdropLayers {};
	menu_backdrop::Pace _backdropPace;
	/// The clock when the backdrop was last drawn over the game, none while it isn't
	std::optional<uint32_t> _lastBackdropTicks;
	/// The black of the new player's box's screen over the game, from the Players page, while it is up or fading
	std::optional<menu_backdrop::Fade> _boxOverGameFade;
	/// Whether the left mouse button is down, while the tattoo editor's dialog has the mouse
	bool _leftButtonDown {false};
	GameMenu::Action _action {GameMenu::Action::None};
	std::optional<SkipBox> _skipBox;
	/// The game's own message box: its text and OK, fading in, and whether its OK was clicked
	struct GameMessage
	{
		std::unique_ptr<Dialog> box;
		Zoomer fade;
		bool answered {false};
	};
	std::optional<GameMessage> _gameMessage;
	std::optional<int32_t> _skipAnswer;
	bool _menuOpenedByKey {false};
	std::optional<Message> _message;
	std::optional<Caption> _caption;
	ToolTips _toolTips;
	std::optional<creature_panel::Values> _creaturePanel;
	std::optional<creature_fight_hud::Values> _fightPanel;
};

} // namespace openblack::gui
