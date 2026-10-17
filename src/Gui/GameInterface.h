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

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <glm/vec2.hpp>

#include "Canvas.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureStatusPanel.h"
#include "DialogPainter.h"
#include "GameFont.h"
#include "GameMenu.h"
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

/// The game's own interface, drawn over the scene: for now the menu that Escape brings up.
///
/// It loads what Black & White's dialogs are made of: the texts of the info scripts, the font j0 and the front end
/// atlas, and takes the mouse and keyboard from the game while a dialog is open.
class GameInterface
{
public:
	/// Null when the game's files for it are missing
	static std::unique_ptr<GameInterface> Create(std::u16string_view playerName, MenuSettings settings);
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
	/// Whether a box with a background panel takes the input: the menu while it is open (not while it fades out) or
	/// the SkipBox. The fight and creature panels and the input prompt icons are not drawn under it
	[[nodiscard]] bool IsBoxActive() const noexcept { return _menu->IsOpen() || IsSkipBoxShown(); }
	/// The SkipBox's answer once given (its OK), then nothing
	std::optional<int32_t> TakeSkipAnswer() { return std::exchange(_skipAnswer, std::nullopt); }
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

	/// What the hand shows for what it is over
	[[nodiscard]] ToolTips& GetToolTips() noexcept { return _toolTips; }
	/// Where on the screen the hand is, in pixels, which the temple's tooltip is shown by, or nowhere to show none
	void SetHandOnScreen(std::optional<glm::vec2> position) { _handOnScreen = position; }
	[[nodiscard]] const std::optional<glm::vec2>& GetHandOnScreen() const noexcept { return _handOnScreen; }
	/// The creature's status panel shown this frame, or none. It shows its reward row when the values have a reward,
	/// and near the top of the screen without one, as while the camera follows a creature.
	void SetCreaturePanel(std::optional<creature_panel::Values> values) { _creaturePanel = values; }
	/// The fight's panel shown this frame, or none: each fighter's name over its health and stamina
	void SetFightPanel(std::optional<creature_fight_hud::Values> values) { _fightPanel = std::move(values); }
	/// The fight's panel and the creature's status panel are drawn in the renderer's end of the frame, as the original
	/// draws them: over the cinema bars and the dialogue, under the film and the screen fade
	/// (graphics::DrawSceneDesc's hooks). Neither while a dialog is open
	void DrawFightPanelLayer(glm::u16vec2 resolution);
	void DrawCreaturePanelLayer(glm::u16vec2 resolution);

private:
	GameInterface(TextDatabase texts, GameFont font, std::shared_ptr<graphics::Texture2D> atlas,
	              std::unique_ptr<graphics::Texture2D> fontTexture, std::shared_ptr<graphics::Texture2D> symbols,
	              std::shared_ptr<graphics::Texture2D> mice, std::u16string_view playerName, MenuSettings settings);

	/// The tattoo editor's dialog takes the mouse while it is open, and of the keyboard Enter and Escape
	bool ProcessTattooEditorEvent(const SDL_Event& event);
	/// The story history's buttons take the mouse only over a button; true when they took the event
	bool ProcessStoryHistoryEvent(const SDL_Event& event);
	/// The dialog opens with the tattoo editor and follows it every frame
	void UpdateTattooEditor(float deltaSeconds);
	/// The creature's status panel, at the left of the screen
	void DrawCreaturePanel(glm::u16vec2 resolution);
	/// The fight's panel, at the top left of the screen
	void DrawFightPanel(glm::u16vec2 resolution);

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
	/// Whether the left mouse button is down, while the tattoo editor's dialog has the mouse
	bool _leftButtonDown {false};
	GameMenu::Action _action {GameMenu::Action::None};
	std::optional<SkipBox> _skipBox;
	std::optional<int32_t> _skipAnswer;
	std::optional<Message> _message;
	ToolTips _toolTips;
	std::optional<glm::vec2> _handOnScreen;
	std::optional<creature_panel::Values> _creaturePanel;
	std::optional<creature_fight_hud::Values> _fightPanel;
};

} // namespace openblack::gui
