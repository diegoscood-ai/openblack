/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The screens the game shows as it starts, before it loads the rest: the logos, the pre-intro and the tips screen,
// which stays up while the game and each land load. Wiki:
// docs/wiki/video/playback.md#startup-and-loading-screen.

#include <array>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>
#include <thread>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/Engine/MusicBank.h"
#include "Audio/Engine/MusicEngine.h"
#include "Audio/Engine/MusicStream.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/RenderPass.h"
#include "Graphics/RendererInterface.h"
#include "Gui/LoadingScreen.h"
#include "Gui/LoadingScreenRules.h"
#include "Gui/StartupRules.h"
#include "Locator.h"
#include "Video/StillPicture.h"
#include "Video/VideoPlayer.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

namespace
{
/// The films, in the game's Data folder
constexpr std::string_view k_LogoFilm = "logo.bik";
constexpr std::string_view k_PreIntroFilm = "pre_intro.bik";
/// The pre-intro's music, played with it from its own bank
constexpr std::string_view k_TrailerMusic = "audio/music/intro/trailer.sad";
/// A pre-intro frame waits for its time 0.5 ms at a time, at most this many times, and is then shown anyway
constexpr auto k_FrameWaitPoll = std::chrono::microseconds(500);
constexpr int32_t k_FrameWaitPolls = 50000;
/// The colour the screen is cleared to between the game's own frames, and the sky's while the game runs
constexpr uint32_t k_StartupClearColour = 0x000000ff;
constexpr uint32_t k_GameClearColour = 0x274659ff;

/// What the player did since the last frame
struct StartupInput
{
	/// A key went down
	bool key {false};
	/// The left or right mouse button went down
	bool mouse {false};
	/// The window is being closed
	bool quit {false};
};

/// Takes the keys and mouse buttons the player pressed and let go, which the game takes no notice of while these screens
/// show. Every other event stays for the game, a close of the window included: the game then ends at its first frame
StartupInput TakeStartupInput()
{
	SDL_PumpEvents();
	StartupInput input;
	std::array<SDL_Event, 16> events {};
	for (;;)
	{
		const int keys = SDL_PeepEvents(events.data(), static_cast<int>(events.size()), SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYUP);
		for (int i = 0; i < keys; ++i)
		{
			input.key = input.key || events.at(static_cast<size_t>(i)).type == SDL_KEYDOWN;
		}
		const int buttons = SDL_PeepEvents(events.data(), static_cast<int>(events.size()), SDL_GETEVENT, SDL_MOUSEBUTTONDOWN,
		                                   SDL_MOUSEBUTTONUP);
		for (int i = 0; i < buttons; ++i)
		{
			const auto& event = events.at(static_cast<size_t>(i));
			input.mouse = input.mouse || (event.type == SDL_MOUSEBUTTONDOWN &&
			                              (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT));
		}
		if (keys <= 0 && buttons <= 0)
		{
			break;
		}
	}
	input.quit = SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT) > 0;
	return input;
}

/// A film of the game's Data folder, none when it isn't there
std::optional<std::filesystem::path> FindFilm(std::string_view name)
{
	const auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / name;
	if (!fileSystem.Exists(path))
	{
		return std::nullopt;
	}
	return path;
}

/// Milliseconds since the program started, as the game counts frames with
uint32_t Milliseconds()
{
	return SDL_GetTicks();
}
} // namespace

void Game::PresentStartupFrame(const std::function<void(glm::u16vec2 resolution)>& draw)
{
	auto& renderer = Locator::rendererInterface::value();
	const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
	// Each frame starts from black
	renderer.ConfigureView(graphics::RenderPass::Main, resolution, k_StartupClearColour);
	bgfx::touch(static_cast<bgfx::ViewId>(graphics::RenderPass::Main));
	draw(resolution);
	renderer.Frame();
}

void Game::PlayStartupScreens()
{
	if (!Locator::windowing::has_value() || Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		return;
	}
	_loadingScreen = gui::LoadingScreen::Create();
	if (!_loadingScreen)
	{
		return;
	}
	if (!_skipLogos && !PlayLogoScreens())
	{
		_startupClosing = true;
	}
}

void Game::ContinueStartupScreens()
{
	if (!_loadingScreen || _startupClosing)
	{
		return;
	}
	if (startup::PlaysPreIntro(_preIntro, HasPlayerProfiles()) && !PlayPreIntro())
	{
		_startupClosing = true;
		return;
	}
	// A new player's first film can't be skipped
	if (!HasPlayerProfiles())
	{
		video::Get().SetNoSkip(true);
	}
	StartTipOfTheDay();
	// Setting the game up starts the loading time again
	_loadingScreen->GetClock().Reset();
}

bool Game::PlayLogoScreens()
{
	const auto film = FindFilm(k_LogoFilm);
	if (!film)
	{
		return true;
	}
	bool closing = TakeStartupInput().quit;
	for (int32_t still = 0; still < startup::k_LogoStills && !closing; ++still)
	{
		// The film is opened again for each still, at its frame
		const auto picture = video::ReadStill(*film, static_cast<uint32_t>(still));
		startup::LogoStill logo;
		auto last = Milliseconds();
		while (!logo.Ended() && !closing)
		{
			const auto now = Milliseconds();
			const auto input = TakeStartupInput();
			closing = input.quit;
			const auto alpha = logo.Frame(static_cast<int32_t>(now - last), input.key || input.mouse);
			last = now;
			PresentStartupFrame([this, &picture, alpha, still](glm::u16vec2 resolution) {
				if (picture)
				{
					// Over the whole screen from a pixel before its top left corner
					_loadingScreen->DrawPicture(resolution, picture->rgba, {picture->width, picture->height},
					                            static_cast<uint32_t>(still),
					                            {.x = -1, .y = -1, .width = resolution.x, .height = resolution.y}, alpha);
				}
			});
		}
	}
	// What was pressed during the logos is forgotten
	TakeStartupInput();
	return !closing;
}
bool Game::PlayPreIntro()
{
	const auto path = FindFilm(k_PreIntroFilm);
	if (!path)
	{
		return true;
	}
	auto film = video::VideoFile::Open(*path);
	if (!film)
	{
		return true;
	}
	// The trailer's music plays with it, at full volume, once, from its own bank
	std::unique_ptr<audio::MusicBank> trailer;
	if (auto* music = audio::music::Get(); music != nullptr)
	{
		try
		{
			trailer = audio::MusicBank::Register(
			    Locator::filesystem::value().FindPath(std::filesystem::path(std::string(k_TrailerMusic))));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "music: {}: {}", k_TrailerMusic, e.what());
		}
		if (trailer)
		{
			music->With([&trailer](audio::MusicEngine& engine) {
				engine.NoteBankRegistered(*trailer);
				audio::MusicPlayOptions options;
				options.bank = trailer.get();
				options.volume = audio::k_MusicMaxVolume;
				options.startChunk = 1;
				engine.Play(options);
			});
		}
	}

	const auto start = Milliseconds();
	const auto frameCount = static_cast<int32_t>(film->FrameCount());
	int32_t shown = 0;
	bool keyPressed = false;
	bool closing = false;
	do
	{
		// Each frame waits until it is due by the film's own pace, 0.5 ms at a time and at most 50000 times, and is
		// never skipped
		for (int32_t poll = 0; poll < k_FrameWaitPolls && !closing; ++poll)
		{
			const auto due = video::FramesDue(Milliseconds() - start, film->FpsNumerator(), film->FpsDenominator());
			if (static_cast<int32_t>(due) > shown)
			{
				break;
			}
			const auto input = TakeStartupInput();
			keyPressed = keyPressed || input.key;
			closing = input.quit;
			std::this_thread::sleep_for(k_FrameWaitPoll);
		}
		film->Decode(static_cast<uint32_t>(shown));
		++shown;
		PresentStartupFrame([this, &film](glm::u16vec2 resolution) {
			_loadingScreen->DrawPicture(resolution, film->GetRgba(), {film->Width(), film->Height()}, film->GetSerial(),
			                            {.x = 0, .y = 0, .width = resolution.x, .height = resolution.y}, 255);
		});
		const auto after = TakeStartupInput();
		keyPressed = keyPressed || after.key;
		closing = closing || after.quit;
	} while (startup::PreIntroGoesOn(shown, frameCount, keyPressed) && !closing);

	// The music is cut off with it
	audio::MusicStop(0);
	trailer.reset();
	TakeStartupInput();
	return !closing;
}

void Game::StartTipOfTheDay()
{
	if (!_loadingScreen)
	{
		return;
	}
	_loadingScreen->PrepareTip(Milliseconds(), HasPlayerProfiles());
	// with the loading screens off, the tip is picked but nothing is drawn
	if (_tipFadedIn || !Locator::config::value().drawLoadingScreens)
	{
		return;
	}
	// The first time, the screen fades in over about a second; this holds everything up
	_tipFadedIn = true;
	float fade = 0.0f;
	auto last = Milliseconds();
	while (fade < 1.0f && !_startupClosing)
	{
		PresentStartupFrame([this, fade](glm::u16vec2 resolution) { _loadingScreen->DrawTips(resolution, fade, 0.0f); });
		const auto now = Milliseconds();
		fade = startup::NextTipFade(fade, static_cast<int32_t>(now - last));
		last = now;
		_startupClosing = TakeStartupInput().quit;
	}
}

void Game::BeginLoadingScreen(loading::LoadingClock::Mode look)
{
	// A new game made after the last one went makes its screen again
	if (!_loadingScreen && Locator::windowing::has_value() && Locator::config::value().graphicsBackend != GraphicsBackend::Noop)
	{
		_loadingScreen = gui::LoadingScreen::Create();
	}
	if (!_loadingScreen)
	{
		return;
	}
	if (look == loading::LoadingClock::Mode::Tips)
	{
		// A load without a tip picks a new one
		if (!_loadingScreen->HasTipPicture())
		{
			StartTipOfTheDay();
		}
	}
	else
	{
		// A script's map counts its own time
		_loadingScreen->GetClock().Reset();
	}
	_loadingLook = look;
}

void Game::RenderLoadingFrame()
{
	if (!_loadingScreen || !_loadingLook || !_loadingScreen->TipShowing())
	{
		return;
	}
	auto& clock = _loadingScreen->GetClock();
	const auto look = *_loadingLook;
	if (!clock.Tick(Milliseconds(), look))
	{
		return;
	}
	// Nothing is drawn while the window is minimised, nor with the loading screens off, but the time still counts
	if (!Locator::windowing::value().IsMinimised() && Locator::config::value().drawLoadingScreens)
	{
		const float progress = clock.Progress();
		const auto time = clock.Time();
		PresentStartupFrame([this, look, progress, time](glm::u16vec2 resolution) {
			if (look == loading::LoadingClock::Mode::Tips)
			{
				_loadingScreen->DrawTips(resolution, 1.0f, progress);
			}
			else if (const auto alpha = loading::PleaseWaitAlpha(time); alpha > 0)
			{
				// Black until the load has taken five seconds, then the banner fades in
				_loadingScreen->DrawPleaseWait(resolution, alpha);
			}
		});
	}
	// The window stays responsive; its events are left for the game
	SDL_PumpEvents();
}

void Game::EndLoadingScreen()
{
	if (!_loadingScreen || !_loadingLook)
	{
		return;
	}
	const auto look = *_loadingLook;
	_loadingLook.reset();
	// After the tips screen its tip goes before the game's first frame; the last loading frame stays up until then
	if (look == loading::LoadingClock::Mode::Tips)
	{
		_loadingScreen->ClearTip();
	}
	Locator::rendererInterface::value().ConfigureView(
	    graphics::RenderPass::Main, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), k_GameClearColour);
}
