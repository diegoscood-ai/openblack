/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameInterface.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/TempleChallengeRoom.h"
#include "Audio/Audio.h"
#include "Common/GameRandom.h"
#include "Common/HelpText.h"
#include "Controls.h"
#include "Creature/CreatureSkin.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "Gui/ButtonSound.h"
#include "Gui/ScreenFit.h"
#include "Gui/ScriptTexts.h"
#include "Help/Bubble.h"
#include "Locator.h"
#include "ModLoaderStatus.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "ScreenFit.h"
#include "Video/VideoPlayer.h"

using namespace openblack::gui;
using openblack::Locator;
using openblack::filesystem::Path;

namespace
{
/// The scripts the game's text is in, in the order the game reads them
constexpr std::array k_TextScripts = {"InfoScript2.txt", "InfoScriptPatch2.txt", "InfoScriptMultiplayer2.txt"};
/// The dialogs' font, the first of the game's fonts
constexpr std::string_view k_Font = "j0";
constexpr uint16_t k_AtlasSize = 256;
/// The time the game's message box takes to fade in, as the menu's questions
constexpr float k_MessageFadeSeconds = 0.5f;

std::vector<uint8_t> ReadIfExists(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Missing {} for the interface", path.generic_string());
		return {};
	}
	// the byte cache, by the resolved path as the help text reads the same scripts
	return openblack::resources::LoadBlob(Locator::resources::value().GetBlobs(), fileSystem.FindPath(path));
}
} // namespace

namespace
{
/// A texture of the game made of a colour file and an alpha file, 256 pixels square, from the texture cache. Null when
/// they are missing. The packing and wrapping are the atlas's unless given
std::shared_ptr<openblack::graphics::Texture2D>
LoadTexture(const std::string& name,
            openblack::resources::Texture2DLoader::ColourAlpha packing =
                openblack::resources::Texture2DLoader::ColourAlpha::Interleaved,
            openblack::graphics::Wrapping wrap = openblack::graphics::Wrapping::ClampEdge)
{
	using openblack::resources::Texture2DLoader;
	auto& fileSystem = Locator::filesystem::value();
	auto& textures = Locator::resources::value().GetTextures();
	const auto id = entt::hashed_string(("interface/" + name).c_str()).value();
	if (!textures.Contains(id))
	{
		try
		{
			textures.Load(id, Texture2DLoader::FromColourAlphaTag {}, name,
			              Texture2DLoader::ColourAlphaDesc {
			                  .colour = fileSystem.GetPath<Path::Textures>() / (name + ".raw"),
			                  .alpha = fileSystem.GetPath<Path::Textures>() / (name + "a.raw"),
			                  .size = k_AtlasSize,
			                  .packing = packing,
			                  .wrap = wrap,
			              });
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the texture {}: {}", name, e.what());
			return nullptr;
		}
	}
	return textures.Handle(id).handle();
}
} // namespace

std::unique_ptr<GameInterface> GameInterface::Create(std::u16string_view playerName, MenuSettings settings)
{
	auto& fileSystem = Locator::filesystem::value();

	TextDatabase texts;
	for (const auto* script : k_TextScripts)
	{
		const auto data = ReadIfExists(fileSystem.GetPath<Path::Scripts>() / script);
		// The first is the help texts' script, whose texts the game's scripts refer to by number
		if (script == k_TextScripts.front())
		{
			texts.AddHelpScript(data);
		}
		else
		{
			texts.AddScript(data);
		}
	}

	// the j0 font of the font cache, the one the renderer's help text draws with. (openblack guard, not the original's:
	// its fonts always exist) without it, as in the tests' data, there are no menus rather than a crash
	const auto font = GameFont::From(
	    openblack::resources::LoadGameFont(Locator::resources::value().GetFonts(), fileSystem.GetPath<Path::Data>() / k_Font));
	if (!font)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the font {}", k_Font);
		return nullptr;
	}

	// The front end atlas, and the pictures of the player's symbols and of the mice
	auto atlas = LoadTexture("Front_end_buttons");
	if (!atlas)
	{
		return nullptr;
	}
	auto symbols = LoadTexture("ChooseSymbol");
	auto mice = LoadTexture("mousehelp");

	std::unique_ptr<graphics::Texture2D> fontTexture; // the font owns its atlas
	auto created =
	    std::make_unique<GameInterface>(Key {}, std::move(texts), std::move(*font), std::move(atlas), std::move(fontTexture),
	                                    std::move(symbols), std::move(mice), playerName, std::move(settings));
	// The front end's swirling backdrop, its texture repeating across the screen, kept as the game kept it: 4 bits to a
	// colour and to its alpha (menu_backdrop::SixteenBitLevel)
	created->_backdrop = LoadTexture("pin", resources::Texture2DLoader::ColourAlpha::Argb4444, graphics::Wrapping::Repeat);
	return created;
}

namespace
{
/// The Mods page's mods, only when the mod loader library is there and its mods folder is read (the checks' empty mods
/// folder is not); a switch is kept through the library
std::optional<MenuMods> MenuModsFromLoader()
{
	if (!openblack::Locator::modLoader::has_value() || !openblack::Locator::modLoader::value().folderRead)
	{
		return std::nullopt;
	}
	const auto& status = openblack::Locator::modLoader::value();
	MenuMods mods {.heading = ToUtf16("Mod loader " + std::to_string(status.version) + ": " +
	                                  std::to_string(status.mods.size()) + " mods; a switch counts from the next start")};
	for (const auto& mod : status.mods)
	{
		mods.mods.push_back(
		    {.label = MenuModLabel(ToUtf16(mod.name), ToUtf16(mod.version), ToUtf16(mod.author)), .enabled = mod.enabled});
	}
	mods.onSwitch = [](size_t index, bool on) {
		return openblack::Locator::modLoader::has_value() && openblack::Locator::modLoader::value().SetEnabled(index, on);
	};
	return mods;
}
} // namespace

GameInterface::GameInterface(Key /*key*/, TextDatabase texts, GameFont font, std::shared_ptr<graphics::Texture2D> atlas,
                             std::unique_ptr<graphics::Texture2D> fontTexture, std::shared_ptr<graphics::Texture2D> symbols,
                             std::shared_ptr<graphics::Texture2D> mice, std::u16string_view playerName, MenuSettings settings)
    : _texts(std::move(texts))
    , _font(std::move(font))
    , _atlas(std::move(atlas))
    , _fontTexture(std::move(fontTexture))
    , _symbols(std::move(symbols))
    , _mice(std::move(mice))
    , _painter(_canvas, _font, *_atlas, _font.GetTexture())
    , _panelPainter(_panelCanvas, _font, *_atlas, _font.GetTexture())
    , _menu(std::make_unique<GameMenu>(_texts, _font, playerName, std::move(settings), MenuModsFromLoader()))
    , _tattooEditor(std::make_unique<TattooEditorDialog>(_texts, _font))
    , _storyHistoryBox(std::make_unique<StoryHistoryBox>(_texts, _font))
    , _newPlayerBox(std::make_unique<NewPlayerBox>(_texts, _font))
{
	_painter.SetPictures(_symbols.get(), _mice.get());
}

GameInterface::~GameInterface() = default;

namespace
{
/// A dialog control's click: the menu button's sound, k_ButtonSound (the force feedback that follows is not ported)
void PlayButtonSound()
{
	constexpr auto& k_Sound = openblack::gui::k_ButtonSound;
	openblack::audio::PlaySoundEffect(openblack::audio::Owner::None(), k_Sound.sample, k_Sound.mode, k_Sound.loops,
	                                  k_Sound.extra3DFlag, k_Sound.is3D, k_Sound.bank);
}
} // namespace

void openblack::gui::HideAllDialogs(GameMenu& menu, TattooEditorDialog& tattooEditor, StoryHistoryBox& storyHistory)
{
	if (menu.IsOpen())
	{
		menu.Close();
	}
	// The editor itself stays open, so its box opens again on its next update, with neither OK nor Cancel
	if (tattooEditor.IsOpen())
	{
		tattooEditor.Close();
	}
	storyHistory.SetShown(false);
}

void GameInterface::ShowNewPlayerBox(bool canCancel, NewPlayerBox::CheckName checkName)
{
	_newPlayerBox->checkName = std::move(checkName);
	_newPlayerBox->Show(canCancel);
	// The control under the pointer lights up at once
	glm::ivec2 mouse;
	SDL_GetMouseState(&mouse.x, &mouse.y);
	_newPlayerBox->MouseMove(_painter.ToDialog(mouse));
}

void GameInterface::ShowNewPlayerBoxOverGame(bool canCancel, NewPlayerBox::CheckName checkName)
{
	_boxOverGameFade = menu_backdrop::Fade {};
	ShowNewPlayerBox(canCancel, std::move(checkName));
}

bool GameInterface::ProcessNewPlayerBoxEvent(const SDL_Event& event, glm::u16vec2 resolution)
{
	_painter.Begin(resolution);
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_newPlayerBox->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_newPlayerBox->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		// Every control clicks as it is let go over, the texts too
		if (event.button.button == SDL_BUTTON_LEFT &&
		    _newPlayerBox->MouseUp(_painter.ToDialog({event.button.x, event.button.y})))
		{
			PlayButtonSound();
		}
		return true;
	case SDL_TEXTINPUT:
		_newPlayerBox->TextInput(ToUtf16(event.text.text));
		return true;
	case SDL_KEYDOWN:
		_newPlayerBox->KeyDown(event.key.keysym.sym);
		return true;
	default:
		return false;
	}
}

void GameInterface::DrawNewPlayerScreen(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds,
                                        float frameMilliseconds, uint8_t black)
{
	_canvas.Begin(resolution);
	_pointerCanvas.Begin(resolution);
	_painter.Begin(resolution);
	_newPlayerBox->Update(frameMilliseconds / 1000.0f);
	DrawMenuBackdrop(resolution, frameMilliseconds);
	_newPlayerBox->Draw(_painter);
	_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
	// The screen fades from and to black over it all, the pointer too
	if (black > 0)
	{
		_pointerCanvas.DrawQuad(glm::vec2(0.0f), glm::vec2(resolution), glm::vec2(0.0f), glm::vec2(1.0f),
		                        glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(black) / 255.0f), nullptr);
	}
	_canvas.End();
	_pointerCanvas.End();
}

void GameInterface::DrawMenuBackdrop(glm::u16vec2 resolution, float frameMilliseconds)
{
	// The swirls turn on at most once a screen refresh; they start at random the first time, from the C runtime's
	// numbers as the game's own front end draws them
	if (!_backdropSwirls.has_value())
	{
		_backdropSwirls = menu_backdrop::Start([](float a, float b) { return game_random::crt::Random(a, b); });
		_backdropPace.sinceStep = 0.0f;
		for (int layer = 0; layer < menu_backdrop::k_Layers; ++layer)
		{
			menu_backdrop::PlaceLayer(*_backdropSwirls, layer, 0.0f, _backdropLayers.at(static_cast<size_t>(layer)));
		}
	}
	if (const auto step = menu_backdrop::StepPace(_backdropPace, frameMilliseconds); step.has_value())
	{
		const auto turn = static_cast<float>(*step) * menu_backdrop::k_TurnPerMillisecond;
		for (int layer = 0; layer < menu_backdrop::k_Layers; ++layer)
		{
			menu_backdrop::PlaceLayer(*_backdropSwirls, layer, turn, _backdropLayers.at(static_cast<size_t>(layer)));
		}
	}
	// The backdrop covers the screen's 4:3 box, black round it
	if (_backdrop)
	{
		const auto box = FitInside(k_FourByThree, resolution.x, resolution.y);
		const auto size = glm::ivec2(box.width, box.height);
		const auto origin = glm::vec2(static_cast<float>(box.x), static_cast<float>(box.y));
		const auto point = [&size, &origin](int column, int row) {
			return origin + glm::vec2(menu_backdrop::PointOnScreen(column, row, size));
		};
		const auto white = glm::vec4(1.0f);
		for (const auto& layer : _backdropLayers)
		{
			const auto uv = [&layer](int column, int row) {
				return layer.at((static_cast<size_t>(column) * menu_backdrop::k_Rows) + static_cast<size_t>(row));
			};
			for (int column = 1; column < menu_backdrop::k_Columns; ++column)
			{
				for (int row = 1; row < menu_backdrop::k_Rows; ++row)
				{
					// Two triangles a square, split from its top left corner to its bottom right
					_canvas.DrawShape(
					    {point(column - 1, row - 1), point(column, row - 1), point(column, row), point(column - 1, row)},
					    {uv(column - 1, row - 1), uv(column, row - 1), uv(column, row), uv(column - 1, row)},
					    {white, white, white, white}, _backdrop.get());
				}
			}
		}
	}
}

void GameInterface::SetPlayerProfile(std::u16string_view name, std::u16string_view creatureName, int symbol,
                                     const player_profiles::Settings& kept)
{
	auto settings = _menu->GetSettings();
	settings.creatureName = creatureName;
	settings.symbol = symbol;
	TakeProfileSettings(settings, kept);
	const bool insideTemple = _menu->IsInsideTemple();
	_menu = std::make_unique<GameMenu>(_texts, _font, name, std::move(settings), MenuModsFromLoader());
	_menu->SetInsideTemple(insideTemple);
}

void GameInterface::ShowSkipBox()
{
	_skipBox.emplace(static_cast<float>(DialogPainter::MidTextSize()));
	_skipAnswer.reset();
}

void GameInterface::ShowGameMessage(std::u16string text)
{
	// The message box's text, at the big text size made smaller while the wrapped text is 125 or more high, and one
	// 150 by 32 OK under it, as the menu's notices
	auto box = std::make_unique<Dialog>();
	box->Add<StaticText>(DialogRect {.min = {200, 220}, .max = {600, 350}}, std::move(text), StaticText::Layout::Wrapped,
	                     DialogPainter::k_BigTextSize)
	    .messageBox = true;
	box->Add<Button>(DialogRect {.min = {325, 338}, .max = {475, 370}},
	                 std::u16string(_texts.Get("HELP_TEXT_REQUESTER_BOXES_04")), DialogPainter::MidTextSize())
	    .onClick = [this] {
		if (_gameMessage.has_value())
		{
			_gameMessage->answered = true;
		}
	};
	_gameMessage = GameMessage {.box = std::move(box), .fade = Zoomer()};
	_gameMessage->fade.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_MessageFadeSeconds);
}

bool GameInterface::ProcessGameMessageEvent(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_gameMessage->box->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_gameMessage->box->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_LEFT &&
		    _gameMessage->box->MouseUp(_painter.ToDialog({event.button.x, event.button.y})))
		{
			PlayButtonSound();
		}
		// the OK clicked: the box goes and the input is the game's again
		if (_gameMessage->answered)
		{
			_gameMessage.reset();
		}
		return true;
	case SDL_MOUSEWHEEL:
		return true;
	// Every key pressed goes to the box, as to the game's box that wants keys, and Escape does not close it while it
	// holds a message; the keys let go still reach the game, so that none stays held down
	case SDL_KEYDOWN:
		if (event.key.keysym.sym != SDLK_ESCAPE)
		{
			_gameMessage->box->KeyDown(event.key.keysym.sym);
		}
		return true;
	case SDL_TEXTINPUT:
		return true;
	default:
		return false;
	}
}

bool GameInterface::ProcessEvent(const SDL_Event& event, glm::u16vec2 resolution)
{
	_painter.Begin(resolution);
	// the SkipBox takes every event while shown; it cannot be escaped out of: Escape does nothing
	if (IsSkipBoxShown())
	{
		if (event.type == SDL_MOUSEMOTION)
		{
			_skipBox->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		}
		else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
		{
			PlayButtonSound(); // (inferred) the box's click on a control, as the menu's
			if (const auto answer = _skipBox->Click(_painter.ToDialog({event.button.x, event.button.y})); answer)
			{
				_skipAnswer = *answer;
			}
		}
		return event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
		       event.type == SDL_MOUSEWHEEL || event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT;
	}
	// The game's message box takes the mouse until its OK is clicked
	if (_gameMessage.has_value())
	{
		return ProcessGameMessageEvent(event);
	}
	// The new player's box, from the Players page, takes the input while it is up; nothing else does while its screen
	// fades
	if (_newPlayerBox->IsActive())
	{
		return ProcessNewPlayerBoxEvent(event, resolution);
	}
	if (_boxOverGameFade.has_value())
	{
		return true;
	}
	if (_tattooEditor->IsOpen() && Locator::tattooEditorSystem::has_value())
	{
		return ProcessTattooEditorEvent(event);
	}
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
	{
		if (_menu->IsOpen())
		{
			_action = _menu->Escape();
			if (_action == GameMenu::Action::Continue)
			{
				_menu->Close();
				SDL_StopTextInput();
			}
		}
		else
		{
			_menu->Open();
			_menuOpenedByKey = true;
			// For the names typed into the menu
			SDL_StartTextInput();
		}
		return true;
	}
	if (!_menu->IsOpen())
	{
		return _storyHistoryBox->IsActive() && ProcessStoryHistoryEvent(event);
	}

	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_menu->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_menu->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_action = _menu->MouseUp(_painter.ToDialog({event.button.x, event.button.y}));
			// Every control clicks as it acts
			if (_menu->TakeClicked())
			{
				PlayButtonSound();
			}
			if (_action == GameMenu::Action::Continue)
			{
				_menu->Close();
				SDL_StopTextInput();
			}
		}
		return true;
	case SDL_MOUSEWHEEL:
	{
		glm::ivec2 mouse;
		SDL_GetMouseState(&mouse.x, &mouse.y);
		_menu->Wheel(_painter.ToDialog(mouse), event.wheel.y);
		return true;
	}
	case SDL_TEXTINPUT:
		_menu->TextInput(ToUtf16(event.text.text));
		return true;
	// Keys let go of still reach the game, so that none stays held down
	case SDL_KEYDOWN:
		_menu->KeyDown(event.key.keysym.sym);
		return true;
	default:
		return false;
	}
}

bool GameInterface::ProcessTattooEditorEvent(const SDL_Event& event)
{
	auto& editor = Locator::tattooEditorSystem::value();
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_tattooEditor->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_leftButtonDown = true;
			_tattooEditor->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_leftButtonDown = false;
			// Every control clicks as it acts
			if (_tattooEditor->MouseUp(_painter.ToDialog({event.button.x, event.button.y}), editor))
			{
				PlayButtonSound();
			}
		}
		return true;
	case SDL_MOUSEWHEEL:
		return true;
	case SDL_KEYDOWN:
		// Only Enter and Escape are the dialog's; the movement keys go on to the camera, which closes the editor
		return _tattooEditor->KeyDown(event.key.keysym.sym, editor);
	default:
		return false;
	}
}

bool GameInterface::ProcessStoryHistoryEvent(const SDL_Event& event)
{
	switch (event.type)
	{
	// The game also sees the pointer move
	case SDL_MOUSEMOTION:
		_storyHistoryBox->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return false;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button != SDL_BUTTON_LEFT || !_storyHistoryBox->WantsMouse())
		{
			return false;
		}
		_storyHistoryBox->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		return true;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button != SDL_BUTTON_LEFT || !_storyHistoryBox->WantsMouse())
		{
			return false;
		}
		// The button clicks as it acts
		if (_storyHistoryBox->MouseUp(_painter.ToDialog({event.button.x, event.button.y})))
		{
			PlayButtonSound();
		}
		return true;
	default:
		return false;
	}
}

GameMenu::Action GameInterface::TakeAction()
{
	return std::exchange(_action, GameMenu::Action::None);
}

void GameInterface::Update(float deltaSeconds)
{
	if (_skipBox)
	{
		_skipBox->Update(deltaSeconds);
	}
	if (_gameMessage)
	{
		_gameMessage->fade.Update(deltaSeconds);
		_gameMessage->box->Update(deltaSeconds);
	}
	_menu->Update(deltaSeconds);
	_newPlayerBox->Update(deltaSeconds);
	_storyHistoryBox->Update(deltaSeconds);
	UpdateTattooEditor(deltaSeconds);
}

void GameInterface::UpdateTattooEditor(float deltaSeconds)
{
	if (!Locator::tattooEditorSystem::has_value())
	{
		return;
	}
	auto& editor = Locator::tattooEditorSystem::value();
	if (!editor.IsOpen() && !_tattooEditor->IsOpen())
	{
		return;
	}
	if (editor.IsOpen() && !_tattooEditor->IsOpen())
	{
		// In the temple the dialog stands over the cave without a frame
		_tattooEditor->Open(false);
	}
	std::span<const std::array<uint8_t, 3>> palette;
	auto& arts = Locator::resources::value().GetCreatureSkinArt();
	if (arts.Contains(creature_skin::k_ArtId))
	{
		palette = arts.Handle(creature_skin::k_ArtId)->palette;
	}
	if (!_tattooEditor->IsOpen())
	{
		_leftButtonDown = false;
	}
	_tattooEditor->Update(deltaSeconds, editor, palette, _leftButtonDown);
}

void GameInterface::Draw(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds, bool overDebugWindow)
{
	_canvas.Begin(resolution);
	_pointerCanvas.Begin(resolution);
	_painter.Begin(resolution);
	// The words the temple's future room shows, under the dialogs
	if (_message.has_value())
	{
		_painter.DrawTextWrapped(DialogRect {{0, 0}, DialogPainter::k_Size}, true, _message->text, 60,
		                         glm::vec4(1.0f, 1.0f, 1.0f, _message->alpha));
	}
	DrawCaption(resolution);
	if (IsSkipBoxShown())
	{
		_skipBox->Draw(_painter);
		_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
	}
	if (_tattooEditor->IsVisible())
	{
		// The marker on the place under the pointer while the mouse button is down
		std::optional<glm::ivec2> marker;
		if (Locator::tattooEditorSystem::has_value() && _leftButtonDown)
		{
			if (const auto point = Locator::tattooEditorSystem::value().GetHoveredPoint(); point.has_value())
			{
				marker = _painter.ToDialog(*point);
			}
		}
		_tattooEditor->Draw(_painter, marker, milliseconds);
		if (_tattooEditor->IsOpen())
		{
			_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
		}
	}
	_storyHistoryBox->Draw(_painter);
	// The new player's box from the Players page takes the screen as the first start's does: the front end's swirling
	// backdrop on black, coming up from black and going back to black, the box and the pointer under the black too.
	// The menu comes back once it is over.
	const auto backdropMilliseconds = menu_backdrop::FrameMilliseconds(_lastBackdropTicks, milliseconds);
	_lastBackdropTicks = _boxOverGameFade.has_value() ? std::optional(milliseconds) : std::nullopt;
	if (_boxOverGameFade.has_value())
	{
		menu_backdrop::StepFade(*_boxOverGameFade, !_newPlayerBox->IsActive(), static_cast<int>(backdropMilliseconds));
		_canvas.DrawQuad(glm::vec2(0.0f), glm::vec2(resolution), glm::vec2(0.0f), glm::vec2(1.0f),
		                 glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), nullptr);
		DrawMenuBackdrop(resolution, backdropMilliseconds);
		_newPlayerBox->Draw(_painter);
		_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
		if (const auto black = _boxOverGameFade->alpha; black > 0)
		{
			_pointerCanvas.DrawQuad(glm::vec2(0.0f), glm::vec2(resolution), glm::vec2(0.0f), glm::vec2(1.0f),
			                        glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(black) / 255.0f), nullptr);
		}
		if (menu_backdrop::FadeOver(*_boxOverGameFade))
		{
			_boxOverGameFade.reset();
		}
	}
	else
	{
		if (_menu->IsVisible())
		{
			_menu->Draw(_painter);
			if (_menu->IsOpen())
			{
				_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
			}
		}
		// The game's message box over the game and the menu, fading in by itself, and the pointer over it
		if (_gameMessage.has_value())
		{
			_painter.SetAlpha(std::clamp(_gameMessage->fade.value, 0.0f, 1.0f));
			_painter.DrawBackground(GameMenu::GetQuestionRect(), glm::vec3(1.0f), true, DialogPainter::Edges::All);
			_gameMessage->box->Draw(_painter, true);
			_painter.SetAlpha(1.0f);
			_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
		}
		// Over a debug window the game's pointer stands in for the system's cursor, which stays hidden
		else if (overDebugWindow && !IsSkipBoxShown() && !(_menu->IsVisible() && _menu->IsOpen()))
		{
			_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
		}
	}
	_canvas.End();
	_pointerCanvas.End();
}

void GameInterface::DrawFightPanelLayer(glm::u16vec2 resolution)
{
	// The game leaves the panels out under a dialog
	if (!_fightPanel.has_value() || IsBoxActive() || _tattooEditor->IsOpen())
	{
		return;
	}
	_panelCanvas.Begin(resolution);
	_panelPainter.Begin(resolution);
	DrawFightPanel(resolution);
	_panelCanvas.End();
}

void GameInterface::DrawScriptTextLayer(glm::u16vec2 resolution)
{
	if (!Locator::cinematicDirectorSystem::has_value() || openblack::video::IsPlaying())
	{
		return;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	const auto texts = director.GetTexts();
	const auto black = script_text::ClipAlphaByte(director.GetClipWindow());
	if (texts.empty() && !black.has_value())
	{
		return;
	}
	_panelCanvas.Begin(resolution);
	_panelPainter.Begin(resolution);
	// the clipping window's black first, the four boxes about it
	if (black.has_value())
	{
		const auto colour = glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(*black) / 255.0f);
		for (const auto& box : script_text::ClipBoxes(director.GetClipWindow(), glm::ivec2(resolution)))
		{
			_panelCanvas.DrawBoxQuad(glm::vec2(box.min), glm::vec2(box.max), glm::vec2(0.0f), glm::vec2(1.0f), colour, nullptr);
		}
	}
	const auto screen = glm::vec2(resolution);
	for (const auto& text : texts)
	{
		const auto box = script_text::Place(text, screen);
		// the font's own line breaking at the line's height, each line centred across the box
		const auto widthOf = [this, &box](std::u16string_view words) { return _font.GetWidth(words, box.size); };
		const auto wrapped = openblack::help::WrapGatheringText(text.words, box.max.x - box.min.x, widthOf);
		const std::vector<std::u16string_view> lines(wrapped.begin(), wrapped.end());
		const auto laid = script_text::Lay(box, lines, widthOf);
		// In the scripts' colour at its fade's alpha. The game also draws it a pixel down and right first, as a shadow,
		// but with no alpha at all, so that nothing of it shows
		const auto colour = glm::vec3(text.colour) / 255.0f;
		const auto alpha = static_cast<float>(script_text::AlphaByte(text.alpha)) / 255.0f;
		for (const auto& line : laid)
		{
			_panelPainter.DrawString(line.at, line.words, box.size, glm::vec4(colour, alpha * line.topShare),
			                         glm::vec4(colour, alpha * line.bottomShare));
		}
	}
	_panelCanvas.End();
}

void GameInterface::DrawCreaturePanelLayer(glm::u16vec2 resolution)
{
	if (!_creaturePanel.has_value() || IsBoxActive() || _tattooEditor->IsOpen())
	{
		return;
	}
	_panelCanvas.Begin(resolution);
	_panelPainter.Begin(resolution);
	DrawCreaturePanel(resolution);
	_panelCanvas.End();
}

void GameInterface::DrawCreaturePanel(glm::u16vec2 resolution)
{
	if (!_creaturePanel.has_value())
	{
		return;
	}
	using creature_panel::Row;
	const auto& values = *_creaturePanel;
	const bool withReward = values.reward.has_value();
	const int rowPitch = creature_panel::RowPitch(resolution);
	const auto textSize = static_cast<float>(rowPitch / 2);

	// The labels' column is as wide as the widest label
	std::array<std::u16string_view, creature_panel::k_RowCount> labels {};
	std::array<float, creature_panel::k_RowCount> widths {};
	for (size_t i = 0; i < labels.size(); ++i)
	{
		labels.at(i) = _texts.Get(creature_panel::k_LabelNames.at(i));
		widths.at(i) = _font.GetWidth(labels.at(i), textSize);
	}
	const auto layout =
	    creature_panel::Compute(resolution, withReward, creature_panel::LabelColumnWidth(widths, static_cast<float>(rowPitch)));
	const creature_panel::RewardTexts rewardTexts {
	    .bad = _texts.Get(creature_panel::k_BadBoyName),
	    .good = _texts.Get(creature_panel::k_GoodBoyName),
	    .none = _texts.Get(creature_panel::k_NoRewardName),
	};

	// See-through black at the left, fading out to the right
	const glm::vec4 shade {0.0f, 0.0f, 0.0f, 95.0f / 255.0f};
	const glm::vec4 clear {0.0f, 0.0f, 0.0f, 0.0f};
	const auto& box = layout.box;
	_panelCanvas.DrawBoxShape({box.min, glm::vec2(box.max.x, box.min.y), box.max, glm::vec2(box.min.x, box.max.y)},
	                          {shade, clear, clear, shade});

	const glm::vec4 white {1.0f, 1.0f, 1.0f, 1.0f};
	const glm::vec4 black {0.0f, 0.0f, 0.0f, 1.0f};
	// Each text in white over its black shadow, two pixels down and right
	const auto drawText = [&](glm::vec2 at, std::u16string_view text) {
		_panelPainter.DrawString(at + 2.0f, text, layout.textSize, black);
		_panelPainter.DrawString(at, text, layout.textSize, white);
	};
	for (const auto& row : layout.Rows())
	{
		const auto label = labels.at(static_cast<size_t>(row.row));
		drawText({row.labelRight.x - std::floor(_font.GetWidth(label, layout.textSize)), row.labelRight.y}, label);
		drawText(row.valueLeft, creature_panel::FormatValue(row.row, values, rewardTexts));

		const float fill = creature_panel::Fill(row.row, values);
		const auto bright = creature_panel::BarColour(row.row, fill);
		DrawStatBar(row.bar, fill, glm::u8vec3(glm::vec3(bright) * 255.0f), row.row == Row::Reward);
	}
}

void GameInterface::DrawStatBar(const creature_panel::Rect& bar, float fill, glm::u8vec3 colour, bool fromMiddle)
{
	// The frame: a three-quarters black box with a white line round it two pixels in
	const glm::vec4 frame {0.0f, 0.0f, 0.0f, 191.0f / 255.0f};
	const glm::vec4 line {254.0f / 255.0f, 254.0f / 255.0f, 254.0f / 255.0f, 1.0f};
	const glm::vec4 black {0.0f, 0.0f, 0.0f, 1.0f};
	const glm::vec4 clear {0.0f, 0.0f, 0.0f, 0.0f};
	const auto quad = [this](glm::vec2 min, glm::vec2 max, const std::array<glm::vec4, 4>& colours) {
		_panelCanvas.DrawBoxShape({min, glm::vec2(max.x, min.y), max, glm::vec2(min.x, max.y)}, colours);
	};
	quad(bar.min, bar.max, {frame, frame, frame, frame});
	const auto inner = glm::ivec2(bar.min) + 2;
	const auto innerMax = glm::ivec2(bar.max) - 2;
	_panelCanvas.DrawBoxLine(inner, {innerMax.x, inner.y}, line);
	_panelCanvas.DrawBoxLine({inner.x, innerMax.y}, innerMax, line);
	_panelCanvas.DrawBoxLine(inner, {inner.x, innerMax.y}, line);
	_panelCanvas.DrawBoxLine({innerMax.x, inner.y}, innerMax, line);

	// The fill, from half the colour where it starts to the whole colour where it ends
	const auto filled = creature_panel::FillOf(bar, fill, fromMiddle);
	if (!filled.Empty())
	{
		const auto bright = glm::vec4(glm::vec3(colour) / 255.0f, 1.0f);
		const auto dim = glm::vec4(glm::vec3(colour / glm::u8vec3(2)) / 255.0f, 1.0f);
		quad({filled.from, filled.top}, {filled.to, filled.bottom}, {dim, bright, bright, dim});
	}
	if (filled.middle.has_value())
	{
		_panelCanvas.DrawBoxLine(glm::ivec2(static_cast<int>(*filled.middle), static_cast<int>(filled.top)),
		                         glm::ivec2(static_cast<int>(*filled.middle), static_cast<int>(filled.bottom) - 1), black);
	}

	// Shadows inside the frame along its top and its left, along the whole bar
	const float inset = creature_panel::k_BarInset;
	const glm::vec2 top {bar.min.x + inset, bar.min.y + inset};
	const glm::vec2 bottomRight {bar.max.x - inset, bar.max.y - inset};
	quad(top, {bottomRight.x, top.y + inset}, {black, black, clear, clear});
	quad(top, {top.x + inset, bottomRight.y}, {black, clear, clear, black});
}

void GameInterface::DrawFightPanel(glm::u16vec2 resolution)
{
	if (!_fightPanel.has_value())
	{
		return;
	}
	const auto layout = creature_fight_hud::Compute(resolution);
	const glm::vec4 shade {0.0f, 0.0f, 0.0f, 95.0f / 255.0f};
	const glm::vec4 clear {0.0f, 0.0f, 0.0f, 0.0f};
	const glm::vec4 white {1.0f, 1.0f, 1.0f, 1.0f};
	const glm::vec4 black {0.0f, 0.0f, 0.0f, 1.0f};
	// The shade is darkest in the screen's corner and clear at the box's other three corners, split from its top right
	// to its bottom left, so the lower right half of the box is clear
	const auto& box = layout.box;
	_panelCanvas.DrawBoxShape({glm::vec2(box.max.x, box.min.y), box.max, glm::vec2(box.min.x, box.max.y), box.min},
	                          {clear, clear, clear, shade});
	const auto widthOf = [this, &layout](std::u16string_view text) { return _font.GetWidth(text, layout.textSize); };
	const auto toPanel = [](const creature_fight_hud::Rect& rect) {
		return creature_panel::Rect {.min = rect.min, .max = rect.max};
	};
	for (size_t i = 0; i < layout.rows.size(); ++i)
	{
		const auto& row = layout.rows.at(i);
		const auto& side = _fightPanel->sides.at(i);
		// The stamina bar first, then the health bar over its top
		DrawStatBar(toPanel(row.stamina), side.stamina, creature_fight_hud::StaminaColour(side.stamina), false);
		DrawStatBar(toPanel(row.health), side.health, creature_fight_hud::HealthColour(side.health), false);
		// The name to the bars' right in white over its black shadow, two pixels down and right
		const auto name = creature_fight_hud::FitName(side.name, layout.nameWidth, widthOf);
		_panelPainter.DrawString(row.name + 2.0f, name, layout.textSize, black);
		_panelPainter.DrawString(row.name, name, layout.textSize, white);
	}
}

void GameInterface::DrawCaption(glm::u16vec2 resolution)
{
	if (!_caption.has_value())
	{
		return;
	}
	namespace room = openblack::temple_challenge_room;
	// The sizes are pixels of the default screen; each line is centred on the screen, the second a line's height below
	// the first
	const float scale = ReferenceScale(resolution.x, resolution.y);
	const float size = room::TextSize(helptext::NeedsBiggerText()) * scale;
	const float shadow = room::k_ShadowOffset * scale;
	const float alpha = static_cast<float>(_caption->alpha) / 255.0f;
	float top = room::k_TextTop * scale;
	for (const auto& line : _caption->lines)
	{
		const auto width = static_cast<int32_t>(_font.GetWidth(line, size));
		const auto x = room::LineLeft(resolution.x, width);
		_painter.DrawString({x, top}, line, size, glm::vec4(0.0f, 0.0f, 0.0f, alpha));
		_painter.DrawString({x + shadow, top + shadow}, line, size, glm::vec4(1.0f, 1.0f, 0.0f, alpha));
		top += size;
	}
}
