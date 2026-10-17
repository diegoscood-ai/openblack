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

#include <array>
#include <optional>
#include <span>
#include <utility>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Creature/CreatureSkin.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

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
/// they are missing
std::shared_ptr<openblack::graphics::Texture2D> LoadTexture(const std::string& name)
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
	return std::unique_ptr<GameInterface>(new GameInterface(std::move(texts), std::move(*font), std::move(atlas),
	                                                        std::move(fontTexture), std::move(symbols), std::move(mice),
	                                                        playerName, std::move(settings)));
}

GameInterface::GameInterface(TextDatabase texts, GameFont font, std::shared_ptr<graphics::Texture2D> atlas,
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
    , _menu(std::make_unique<GameMenu>(_texts, _font, playerName, std::move(settings)))
    , _tattooEditor(std::make_unique<TattooEditorDialog>(_texts, _font))
    , _storyHistoryBox(std::make_unique<StoryHistoryBox>(_texts, _font))
{
	_painter.SetPictures(_symbols.get(), _mice.get());
}

GameInterface::~GameInterface() = default;

namespace
{
/// A dialog control's click: sound 159 G_MenuButton, mode 3, 2D, InGame bank (the force feedback that follows is not
/// ported)
void PlayButtonSound()
{
	openblack::audio::PlaySoundEffect(openblack::audio::Owner::None(), 159, 3, 0, false, false,
	                                  openblack::audio::SfxBank::InGame);
}
} // namespace

void GameInterface::ShowSkipBox()
{
	_skipBox.emplace(static_cast<float>(DialogPainter::MidTextSize()));
	_skipAnswer.reset();
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
	_menu->Update(deltaSeconds);
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
	if (_menu->IsVisible())
	{
		_menu->Draw(_painter);
		if (_menu->IsOpen())
		{
			_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
		}
	}
	// Over a debug window the game's pointer stands in for the system's cursor, which stays hidden
	if (overDebugWindow && !IsSkipBoxShown() && !(_menu->IsVisible() && _menu->IsOpen()))
	{
		_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
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
	const float textSize = static_cast<float>(resolution.y) / 32.0f;

	// The labels' column is as wide as the widest label
	std::array<std::u16string_view, creature_panel::k_RowCount> labels {};
	std::array<float, creature_panel::k_RowCount> widths {};
	for (size_t i = 0; i < labels.size(); ++i)
	{
		labels.at(i) = _texts.Get(creature_panel::k_LabelNames.at(i));
		widths.at(i) = _font.GetWidth(labels.at(i), textSize);
	}
	const auto layout =
	    creature_panel::Compute(resolution, withReward, creature_panel::LabelColumnWidth(widths, textSize * 2.0f));
	const creature_panel::RewardTexts rewardTexts {
	    .bad = _texts.Get(creature_panel::k_BadBoyName),
	    .good = _texts.Get(creature_panel::k_GoodBoyName),
	    .none = _texts.Get(creature_panel::k_NoRewardName),
	};

	// See-through black at the left, fading out to the right
	const glm::vec4 shade {0.0f, 0.0f, 0.0f, 95.0f / 255.0f};
	const glm::vec4 clear {0.0f, 0.0f, 0.0f, 0.0f};
	const auto& box = layout.box;
	_panelCanvas.DrawShape({box.min, glm::vec2(box.max.x, box.min.y), box.max, glm::vec2(box.min.x, box.max.y)},
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

		// The bar's dark box and white frame, its fill brightening towards where it ends, and shadows inside its frame
		const auto& bar = row.bar;
		_panelPainter.DrawBevelBox({_panelPainter.ToDialog(glm::ivec2(bar.min)), _panelPainter.ToDialog(glm::ivec2(bar.max))},
		                           1, DialogPainter::Edges::All, white);
		const float fill = creature_panel::Fill(row.row, values);
		const auto filled = creature_panel::FillOf(bar, fill, row.row == Row::Reward);
		if (!filled.Empty())
		{
			const auto bright = creature_panel::BarColour(row.row, fill);
			const auto dim = glm::vec4(glm::vec3(bright) * 0.5f, 1.0f);
			_panelCanvas.DrawShape({glm::vec2(filled.from, filled.top), glm::vec2(filled.to, filled.top),
			                        glm::vec2(filled.to, filled.bottom), glm::vec2(filled.from, filled.bottom)},
			                       {dim, bright, bright, dim});
		}
		if (filled.middle.has_value())
		{
			_panelCanvas.DrawLine(glm::ivec2(static_cast<int>(*filled.middle), static_cast<int>(filled.top)),
			                      glm::ivec2(static_cast<int>(*filled.middle), static_cast<int>(filled.bottom) - 1), black);
		}
		const float inset = creature_panel::k_BarInset;
		const glm::vec2 inner {bar.min.x + inset, bar.min.y + inset};
		const glm::vec2 innerMax {bar.max.x - inset, bar.max.y - inset};
		_panelCanvas.DrawShape({inner, glm::vec2(innerMax.x, inner.y), glm::vec2(innerMax.x, inner.y + inset),
		                        glm::vec2(inner.x, inner.y + inset)},
		                       {black, black, clear, clear});
		_panelCanvas.DrawShape({inner, glm::vec2(inner.x + inset, inner.y), glm::vec2(inner.x + inset, innerMax.y),
		                        glm::vec2(inner.x, innerMax.y)},
		                       {black, clear, clear, black});
	}
}

void GameInterface::DrawFightPanel(glm::u16vec2 resolution)
{
	if (!_fightPanel.has_value())
	{
		return;
	}
	const auto layout = creature_fight_hud::Compute(resolution);
	const glm::vec4 shade {0.0f, 0.0f, 0.0f, 95.0f / 255.0f};
	const glm::vec4 white {1.0f, 1.0f, 1.0f, 1.0f};
	const glm::vec4 black {0.0f, 0.0f, 0.0f, 1.0f};
	const auto quad = [this](const creature_fight_hud::Rect& rect, const glm::vec4& colour) {
		_panelCanvas.DrawShape({rect.min, glm::vec2(rect.max.x, rect.min.y), rect.max, glm::vec2(rect.min.x, rect.max.y)},
		                       {colour, colour, colour, colour});
	};
	quad(layout.box, shade);
	for (size_t i = 0; i < layout.rows.size(); ++i)
	{
		const auto& row = layout.rows.at(i);
		const auto& side = _fightPanel->sides.at(i);
		// The name in white over its black shadow
		_panelPainter.DrawString(row.name + 2.0f, side.name, layout.textSize, black);
		_panelPainter.DrawString(row.name, side.name, layout.textSize, white);
		for (const auto& [bar, value] : {std::pair(row.health, side.health), std::pair(row.stamina, side.stamina)})
		{
			quad(bar, black);
			quad(creature_fight_hud::Filled(bar, value), creature_fight_hud::BarColour(value));
		}
	}
}
