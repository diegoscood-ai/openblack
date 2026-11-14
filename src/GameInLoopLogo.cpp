/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Lionhead logo of the game loop's first pass: its sound, its own frames until a key, a click or its end, and what
// the game clears after it. The rules are in_loop_logo's, the drawing graphics::InLoopLogoDraw's. Wiki:
// docs/wiki/engine/game-clock.md#the-in-loop-logo.

#include <cstdint>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <system_error>
#include <vector>

#include <SDL.h>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Debug/FixedClock.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/InLoopLogoDraw.h"
#include "Graphics/RendererInterface.h"
#include "Gui/InLoopLogo.h"
#include "Gui/ScreenFade.h"
#include "Input/FixedMouse.h"
#include "Input/InputRecording.h"
#include "Input/InputReplay.h"
#include "Input/RealInput.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Video/FallingSpellVideo.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

namespace
{
/// The mouse and the keys as the logo reads them between two of its frames
struct LogoInput
{
	/// A key went down and has not been let go
	bool keyHeld {false};
	/// The left or the right button is held
	uint8_t buttons {0};
	glm::ivec2 move {0};
	glm::ivec2 mouse {0};
	/// 120 a notch
	int32_t wheel {0};
	bool quit {false};
};

/// The window's input events since the last frame (the mouse and the keys only), through the test runs' filters. Every
/// other event stays for the game, a quit and the window's close included: the logo stops for them and the game takes
/// them at its first frame, as during the other start-up screens
void TakeLogoInput(LogoInput& input)
{
	SDL_PumpEvents();
	std::array<SDL_Event, 32> events {};
	for (;;)
	{
		const int count =
		    SDL_PeepEvents(events.data(), static_cast<int>(events.size()), SDL_GETEVENT, SDL_KEYDOWN, SDL_MOUSEWHEEL);
		for (int i = 0; i < count; ++i)
		{
			const auto& event = events.at(static_cast<size_t>(i));
			if (input::DropRealEvent(event.type, input::FixedMouse().has_value(), input::IgnoreRealInput()))
			{
				continue;
			}
			switch (event.type)
			{
			case SDL_KEYDOWN:
				input.keyHeld = true;
				break;
			case SDL_KEYUP:
				input.keyHeld = false;
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)
				{
					const uint8_t bit = event.button.button == SDL_BUTTON_LEFT ? 1u : 2u;
					input.buttons = static_cast<uint8_t>(event.type == SDL_MOUSEBUTTONDOWN ? (input.buttons | bit)
					                                                                       : (input.buttons & ~bit));
				}
				input.mouse = {event.button.x, event.button.y};
				break;
			case SDL_MOUSEMOTION:
				input.move += glm::ivec2(event.motion.xrel, event.motion.yrel);
				input.mouse = {event.motion.x, event.motion.y};
				break;
			case SDL_MOUSEWHEEL:
				input.wheel += static_cast<int32_t>(event.wheel.preciseY * 120.0f);
				break;
			default:
				break;
			}
		}
		if (count <= 0)
		{
			break;
		}
	}
	// a quit, or the window closed, left in the queue
	input.quit = SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT) > 0 || input.quit;
	std::array<SDL_Event, 16> windowEvents {};
	const int windows = SDL_PeepEvents(windowEvents.data(), static_cast<int>(windowEvents.size()), SDL_PEEKEVENT,
	                                   SDL_WINDOWEVENT, SDL_WINDOWEVENT);
	for (int i = 0; i < windows; ++i)
	{
		if (windowEvents.at(static_cast<size_t>(i)).window.event == SDL_WINDOWEVENT_CLOSE)
		{
			input.quit = true;
		}
	}
}

/// Milliseconds as the logo's frames count them: the fixed step of a run of the checks, else the wall clock
uint32_t LogoMilliseconds(bool fixedStep)
{
	return fixedStep ? game_clock::TickCount() : SDL_GetTicks();
}

/// The picture this start shows, and its name: one start in four the user's file in the temporary folder, if it has a
/// name; else the game's own
struct ChosenPicture
{
	in_loop_logo::Picture picture {};
	std::u16string name {in_loop_logo::k_DefaultName};
	bool defaultPicture {true};
};
ChosenPicture ChoosePicture()
{
	auto& fileSystem = Locator::filesystem::value();
	ChosenPicture chosen;
	if (in_loop_logo::TriesUserPicture(machine_clock::Ticks()))
	{
		std::error_code error;
		const auto temporary = std::filesystem::temp_directory_path(error);
		if (!error)
		{
			if (const auto bytes = fileSystem.ReadUserFile(temporary / in_loop_logo::k_UserPictureFile); bytes.has_value())
			{
				if (auto user = in_loop_logo::ReadUserPicture(*bytes); user.has_value())
				{
					chosen.picture = user->picture;
					chosen.name = std::move(user->name);
					chosen.defaultPicture = false;
					return chosen;
				}
			}
		}
	}
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / in_loop_logo::k_DefaultPicture;
	if (fileSystem.Exists(path))
	{
		const auto& bytes = resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(), fileSystem.FindPath(path),
		                                                "the logo's picture");
		std::copy_n(bytes.begin(), std::min(bytes.size(), chosen.picture.size()), chosen.picture.begin());
	}
	return chosen;
}

/// The logo's camera of a frame, as the game's 3D transform keeps it: its matrices and its near plane
struct LogoCamera
{
	affine::CameraMatrices matrices;
	screen_point::NearPlane plane;
	glm::vec3 eye;
	glm::ivec2 screen;
	float nearClip;
};
LogoCamera CameraAt(glm::vec3 eye, glm::vec3 focus, glm::ivec2 screen, float horizontalFov, float nearClip)
{
	const float aspect = static_cast<float>(screen.x) / static_cast<float>(screen.y);
	return {.matrices = affine::FrameMatrices(eye, focus, horizontalFov, aspect),
	        .plane = screen_point::NearPlaneOf(screen, horizontalFov, nearClip),
	        .eye = eye,
	        .screen = screen,
	        .nearClip = nearClip};
}

/// The point of a pixel at a depth through the logo's camera
in_loop_logo::PointFn PointAtOf(const LogoCamera& camera)
{
	return [camera](glm::ivec2 pixel, float depth) {
		return screen_point::PixelPoint(camera.matrices.worldToCamera, camera.eye, camera.plane, pixel, depth);
	};
}

/// A point's pixel and depth when it projects onto the screen: in front of the near clip, its pixel ((X / Z + 1) half
/// the width, (1 - Y / Z) half the height) cut towards zero inside the screen
in_loop_logo::ProjectFn ProjectOf(const LogoCamera& camera)
{
	return [camera](const glm::vec3& point) -> std::optional<in_loop_logo::OnScreen> {
		const auto clip = affine::ToClipForScreenTest(camera.matrices.worldToClipping, point);
		if (clip.z < camera.nearClip)
		{
			return std::nullopt;
		}
		const float inverse = 1.0f / clip.z;
		const float halfWidth = static_cast<float>(camera.screen.x) * 0.5f;
		const float halfHeight = static_cast<float>(camera.screen.y) * 0.5f;
		const float y = (1.0f - inverse * clip.y) * halfHeight;
		const float x = (inverse * clip.x + 1.0f) * halfWidth;
		const glm::ivec2 pixel {static_cast<int32_t>(x), static_cast<int32_t>(y)};
		if (pixel.x < 0 || pixel.y < 0 || pixel.x >= camera.screen.x || pixel.y >= camera.screen.y)
		{
			return std::nullopt;
		}
		return in_loop_logo::OnScreen {.pixel = pixel, .depth = clip.z};
	};
}

/// The game camera's far clip, from its projection
float FarClipOf(const Camera& camera)
{
	const auto& projection = camera.GetProjectionMatrix(Camera::Projection::Normal);
	const float denominator = projection[2][2] - 1.0f;
	return denominator != 0.0f ? -projection[3][2] / denominator : 10000.0f;
}
} // namespace

std::optional<uint32_t> Game::PlayInLoopLogo()
{
	if (!Locator::windowing::has_value() || Locator::config::value().graphicsBackend == GraphicsBackend::Noop ||
	    !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	auto draw = graphics::InLoopLogoDraw::Create();
	if (!draw)
	{
		return std::nullopt;
	}
	// Its sound plays from before it to after it
	audio::PlayOptions sound;
	sound.sample = {audio::Bank(audio::SfxBank::InGame), in_loop_logo::k_Sample};
	sound.is3D = false;
	sound.track = false;
	audio::PlaySoundEffect(sound);

	const auto& gameCamera = Locator::camera::value();
	const float fov = gameCamera.GetHorizontalFieldOfView();
	const float farClip = FarClipOf(gameCamera);
	auto window = Locator::windowing::value().GetSize();
	auto chosen = ChoosePicture();
	// The stars are placed through the start camera, at the near clip the game left
	const auto start = CameraAt(in_loop_logo::k_StartEye, glm::vec3(0.0f), window, fov,
	                            graphics::billboard::CameraFrame::From(gameCamera).nearZ);
	const auto random = [](float a, float b) { return game_random::crt::Random(a, b); };
	auto state =
	    in_loop_logo::Start(chosen.picture, chosen.defaultPicture, std::move(chosen.name), window, PointAtOf(start), random);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The logo plays ({} picture)", state.defaultPicture ? "the game's" : "the user's");

	// What was moved and the key pressed before it are forgotten; a button still held is not
	LogoInput input;
	TakeLogoInput(input);
	input = {.mouse = input.mouse, .quit = input.quit};
	if (!input::FixedMouse().has_value() && !input::IgnoreRealInput())
	{
		const uint32_t held = SDL_GetMouseState(&input.mouse.x, &input.mouse.y);
		input.buttons =
		    static_cast<uint8_t>(((held & SDL_BUTTON_LMASK) != 0 ? 1u : 0u) | ((held & SDL_BUTTON_RMASK) != 0 ? 2u : 0u));
	}
	const bool fixedStep = fixed_clock::Enabled();
	auto last = LogoMilliseconds(fixedStep);
	uint32_t updates = 0;
	auto& renderer = Locator::rendererInterface::value();
	while (in_loop_logo::GoesOn(input.keyHeld, input.buttons != 0, state.finished) && !input.quit)
	{
		// a minimised window ends it
		if (Locator::windowing::value().IsMinimised())
		{
			break;
		}
		if (fixedStep)
		{
			fixed_clock::AdvanceFrame();
		}
		const auto now = LogoMilliseconds(fixedStep);
		// the frame's whole milliseconds, at least 1
		const auto deltaMs = static_cast<int32_t>(std::max<uint32_t>(now - last, 1u));
		last = now;
		window = Locator::windowing::value().GetSize();
		const auto frame = in_loop_logo::Advance(state, deltaMs);
		const auto camera = CameraAt(frame.eye, frame.focus, window, fov, in_loop_logo::k_NearClip);
		in_loop_logo::Step(
		    state, frame,
		    {.move = input.move, .mouse = input.mouse, .wheel = input.wheel, .deltaMs = deltaMs, .screenWidth = window.x},
		    ProjectOf(camera), PointAtOf(camera), random);
		++updates;
		input.move = glm::ivec2(0);
		input.wheel = 0;
		draw->Draw({.resolution = static_cast<glm::u16vec2>(window),
		            .eye = frame.eye,
		            .focus = frame.focus,
		            .horizontalFov = fov,
		            .farClip = farClip,
		            .skyArgb = frame.skyArgb,
		            .stars = state.stars,
		            .name = state.name,
		            .nameFade = frame.nameFade,
		            .trademark = state.defaultPicture && static_cast<int32_t>(frame.nameFade * 255.0f) > 0,
		            .trademarkAlpha = frame.starAlpha,
		            .fade = frame.fade});
		renderer.Frame();
		TakeLogoInput(input);
	}
	graphics::InLoopLogoDraw::ReleaseViews();
	draw.reset();
	// Black frames while a button is still held
	while (input.buttons != 0 && !input.quit)
	{
		graphics::InLoopLogoDraw::DrawBlack(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()));
		renderer.Frame();
		TakeLogoInput(input);
	}
	if (input.quit)
	{
		// the window is being closed: the event stays in the queue, and the game ends at its first frame
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The logo stopped: the window is being closed");
	}
	// The input is cleared, and the temple's fade put at rest, black
	TakeLogoInput(input);
	auto& fade = video::GetFallingSpell().GetScreenFade();
	fade.current = 0.0f;
	fade.target = 0.0f;
	fade.rgb = 0;
	fade.done = 0;
	audio::StopSoundEffect(in_loop_logo::k_Sample, audio::Owner {}, audio::SfxBank::InGame);
	// the game timer starts again from the turn: the logo's time does not count
	game_clock::ResetLocalTimer();
	renderer.ConfigureView(graphics::RenderPass::Main, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()),
	                       graphics::k_BackgroundColour);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The logo ended after {} frames (time {:.3f})", updates, state.time);
	return updates;
}

void Game::SpendRecordedLogo()
{
	// a recording made while the logo played: the same draws of the C runtime's stream, without the logo
	for (const auto& event : input::replay::PlaybackEvents())
	{
		if (event.kind != input::recording::Kind::Logo)
		{
			continue;
		}
		const auto updates = static_cast<uint32_t>(event.values[0]);
		const auto draws = in_loop_logo::RandomDraws(updates);
		for (uint64_t i = 0; i < draws; ++i)
		{
			static_cast<void>(game_random::crt::Rand());
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input replay: the recorded logo's {} frames ({} draws)", updates, draws);
		return;
	}
}

std::optional<input::recording::Event> Game::StartInLoopLogo(bool replaying)
{
	// once, on the first pass of the game loop
	if (std::exchange(_inLoopLogoDone, true))
	{
		return std::nullopt;
	}
	const auto& config = Locator::config::value();
	if (replaying || !config.startupLogos || !config.inLoopLogo || _startupClosing)
	{
		return std::nullopt;
	}
	const auto updates = PlayInLoopLogo();
	if (!updates.has_value())
	{
		return std::nullopt;
	}
	return input::recording::Event {
	    .frame = 0, .kind = input::recording::Kind::Logo, .values = {static_cast<int32_t>(*updates), 0, 0, 0, 0, 0}};
}
