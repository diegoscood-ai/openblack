/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Game.h"

#include <sstream>
#include <string>

#include <LHVM.h>
#include <bgfx/bgfx.h>
#include <SDL.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/NightLights.h"
#include "PSys/PSysManager.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/ScreenFade.h"
#include "3D/SkyInterface.h"
#include "3D/SkyType.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AnimationSounds.h"
#include "Audio/AtmosBanks.h"
#include "Audio/Audio.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameMusic.h"
#include "Audio/LanternSounds.h"
#include "Audio/MusicStream.h"
#include "Audio/ScriptAudioState.h"
#include "Audio/Voices.h"
#include "Audio/SamplePlay.h"
#include "Audio/SoundMap.h"
#include "Audio/SoundTags.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Common/EventManager.h"
#include "Common/StringUtils.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fields.h"
#include "ECS/AnimalAI.h"
#include "ECS/SmokyStuff.h"
#include "ECS/ScriptHeld.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Animations.h"
#include "ECS/CarriedProps.h"
#include "ECS/DesignedScenery.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/FireFlies.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Trees.h"
#include "ECS/FishShoals.h"
#include "ECS/GroundMarks.h"
#include "ECS/PetitNavire.h"
#include "ECS/Physics/Buildings.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Rivers.h"
#include "ECS/WaterRings.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Sharks.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "LandBalance.h"
#include "Magic/MagicLoop.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"
#include "Mods/Lua/LuaHost.h"
#include "Mods/Native/NativeHost.h"
#include "Mods/Replacements.h"
#include "Mods/Switches.h"
#include "Parsers/InfoFile.h"
#include "Profiler.h"
#include "Resources/HdTweaks.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Serializer/FotFile.h"

#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

using namespace openblack;
using namespace openblack::lhscriptx;
using namespace std::chrono_literals;

const std::string k_WindowTitle = "openblack";

namespace
{
/// What GAudio's music reads from the game (Audio/GameQueries.h); the queries left unset are the systems openblack does
/// not have yet (videos, the wide screen bars moving, the camera's alignment, the towns' tribes, citadel, creature, worship)
audio::GameQueries MakeMusicQueries(Game& game)
{
	audio::GameQueries queries;
	queries.landNumber = [&game]() { return game.GetMapScriptGlobals().landNumber; };
	// (inferred) openblack's turn counter, back to 0 at each LoadMap, stands for g_game+0x205A40
	queries.turn = [&game]() { return game.GetTurn(); };
	queries.camera = []() -> std::optional<audio::CameraState> {
		if (!Locator::camera::has_value())
		{
			return std::nullopt;
		}
		audio::CameraState camera;
		camera.position = Locator::camera::value().GetOrigin();
		const float ground = Locator::terrainSystem::has_value() ?
		                         Locator::terrainSystem::value().GetHeightAt(glm::vec2(camera.position.x, camera.position.z)) :
		                         0.0f;
		camera.heightAboveGround = camera.position.y - ground;
		return camera;
	};
	// (approximated) a valid entity with a Transform stands for GameThing::IsAvailable, and its float position for the
	// thing's MapCoords (without their 16.16 rounding)
	queries.thingPosition = [](audio::ThingId thing) -> std::optional<glm::vec3> {
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = static_cast<entt::entity>(thing);
		if (!registry.Valid(entity))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<ecs::components::Transform>(entity);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		return transform->position;
	};
	// LH3DIsland::GetAltitude 0x803090 (SoundTag::Create(MapCoords&) 0x71EB71)
	queries.landAltitude = [](float x, float z) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
	};
	// HelpSystem +0x45E8 && +0x45EC (ProcessAlignmentMusic 0x4279E9..0x427A01)
	queries.scriptWideScreen = []() {
		const auto* helpSystem = help::Get();
		return helpSystem != nullptr && helpSystem->IsScriptWideScreen();
	};
	return queries;
}
} // namespace

Game* Game::sInstance = nullptr;

Game::Game(Arguments&& args) noexcept
    : _gamePath(args.gamePath)
    , _startMap(args.startLevel)
    , _requestScreenshot(args.requestScreenshot)
    , _screenFade(std::make_unique<ScreenFade>())
    , _dayNightClock(std::make_unique<DayNightClock>())
{
	Locator::camera::emplace(glm::zero<glm::vec3>());
	std::function<std::shared_ptr<spdlog::logger>(const std::string&)> createLogger;
#ifdef __ANDROID__
	if (!args.logFile.empty() && args.logFile == "logcat")
	{
		createLogger = [](const std::string& name) { return spdlog::android_logger_mt(name, "spdlog-android"); };
	}
	else
#endif // __ANDROID__
	{
		if (!args.logFile.empty() && args.logFile != "stdout")
		{
			createLogger = [&args](const std::string& name) { return spdlog::basic_logger_mt(name, args.logFile); };
		}
		else
		{
			createLogger = [](const std::string& name) { return spdlog::stdout_color_mt(name); };
		}
	}
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& subsystem : k_LoggingSubsystemStrs)
	{
		auto logger = createLogger(subsystem.data());
		logger->set_level(args.logLevels.at(i));
		// test hook: OPENBLACK_FLUSH_LOG=1 writes every line at once (the last lines before a crash are kept)
		if (std::getenv("OPENBLACK_FLUSH_LOG") != nullptr)
		{
			logger->flush_on(spdlog::level::trace);
		}
		++i;
	}
	sInstance = this;
	// the GGame ctor 0x54B58A (the game timer at speed 1); paused until a map is loaded (openblack)
	game_clock::Reset();
	game_clock::Start(true);

	auto& config = Locator::config::emplace();
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.resolution = {args.windowWidth, args.windowHeight};
	config.displayMode = args.displayMode;
	config.graphicsBackend = args.graphicsBackend;
	config.vsync = args.vsync;
	config.detailLevel = args.detailLevel;

	// Mods (docs/bw1-notes/mod-library.md): the engine switches they may set, then the mods of <executable>/Mods (and
	// the ones built into openblack), with the state saved in each Mods/<mod>/settings.cfg, then the command line for
	// this session. Applied now so the engine starts with them.
	{
		mods::switches::RegisterEngineSwitches();
		auto& mods = Locator::mods::emplace();
		std::filesystem::path baseDirectory;
		if (char* base = SDL_GetBasePath(); base != nullptr)
		{
			baseDirectory = base;
			SDL_free(base);
		}
		// everything about mods lives in <executable>/Mods, a folder per mod with its settings.cfg (and its files); the
		// old single mods.cfg (next to the executable, or in Mods) is split into them once
		mods.Discover(baseDirectory / "Mods");
		mods.ImportLegacySettings(baseDirectory / "mods.cfg");
		mods.ImportLegacySettings(baseDirectory / "Mods" / "mods.cfg");
		mods.LoadSettings();
		for (const auto& argument : args.modArguments)
		{
			if (const auto error = mods.ApplyArgument(argument); !error.empty())
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "--mod {}: {}", argument, error);
			}
		}
		mods.ApplyAll();
		// what the active mods replace (meshes, textures, info.dat objects), read before the game data loads
		mods::replace::Collect(mods);
	}
	config.guiScale = args.guiScale;
}

Game::~Game() noexcept
{
	// the mods first, while the engine they talk to (audio included) is still up
	mods::native::Stop();
	mods::lua::Stop();
	// GAudio::ToBeDeleted 0x426FE0: the sample channels, then GAudio's music before LHMusic, then the music thread and
	// its OpenAL sources before the audio context (LHMusicClose 0x1000E7A0)
	audio::Shutdown();
	audio::game_music::Shutdown();
	audio::music::Shutdown();
	help::Shutdown();
	ShutDownServices();
	SDL_Quit(); // todo: move to GameWindow
	spdlog::shutdown();
}

bool Game::ProcessEvents(const SDL_Event& event) noexcept
{
	static bool leftMouseButton = false;
	static bool middleMouseButton = false;
	static bool rightMouseButton = false;

	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT)
	{
		leftMouseButton = !leftMouseButton;
		// GInterface 0x5D11C0 -> HelpSystem::ProcessInterface (inferred: the click is the left button going down)
		if (auto* helpSystem = help::Get(); helpSystem != nullptr && event.type == SDL_MOUSEBUTTONDOWN)
		{
			helpSystem->ProcessInterface(true);
		}
	}
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_MIDDLE)
	{
		middleMouseButton = !middleMouseButton;
	}

	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_RIGHT)
	{
		rightMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}

	_handGripping = middleMouseButton || leftMouseButton;
	_handAction = rightMouseButton;

	auto& window = Locator::windowing::value();
	auto& camera = Locator::camera::value();

	switch (event.type)
	{
	case SDL_QUIT:
		return false;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == window.GetID())
		{
			return false;
		}
		else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED || event.window.event == SDL_WINDOWEVENT_RESTORED)
		{
			// LHScreen's activation callback 0x642470: LHGlobalSwitch(0 / 1). Not on a focus change: GameWindowProc sets
			// the state 0x8002 for wParam 1 (0x7DBFF6..0x7DC009, inferred WM_SIZE SIZE_MINIMIZED), which sub_7DE8D0
			// 0x7DE8DC turns into AltTabDeactivate 0x7DE6D0; AltTabReactivate 0x7DE6F0 comes from ProcessWindowMessages
			// 0x7DB9DB after the flag [0xE8C0FB] that 0xF120 sets (0x7DC23A..0x7DC245, inferred WM_SYSCOMMAND SC_RESTORE)
			audio::OnFocus(event.window.event == SDL_WINDOWEVENT_RESTORED);
		}
		else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
		{
			const auto resolution = glm::u16vec2(event.window.data1, event.window.data2);
			Locator::rendererInterface::value().Reset(resolution);
			Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, resolution, 0x274659ff);

			auto aspect = window.GetAspectRatio();
			const auto& config = Locator::config::value();
			camera.SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip, config.cameraFarClip);
		}
		break;
	case SDL_KEYDOWN:
		switch (event.key.keysym.sym)
		{
		case SDLK_ESCAPE:
			return false;
		case SDLK_f:
			window.SetDisplayMode(windowing::DisplayMode::Fullscreen);
			break;
		case SDLK_p:
			game_clock::Pause(!game_clock::IsPaused()); // PauseGame 0x54AE20
			break;
		case SDLK_F1:
			Locator::rendererInterface::value().SetDebug(!Locator::rendererInterface::value().GetDebug());
			break;
		case SDLK_1:
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
		case SDLK_5:
		case SDLK_6:
		case SDLK_7:
		case SDLK_8:
			if ((event.key.keysym.mod & KMOD_CTRL) != 0)
			{
				const auto index = static_cast<uint8_t>(event.key.keysym.sym - SDLK_1);
				const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
				if (positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)] ||
				    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)])
				{
					const auto handPosition =
					    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)].value_or(
					        positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)].value_or(
					            glm::zero<glm::vec3>()));
					Locator::cameraBookmarkSystem::value().SetBookmark(index, handPosition, camera.GetOrigin());
				}
			}
			else
			{
				const auto& entitiesRegistry = Locator::entitiesRegistry::value();
				const size_t index = event.key.keysym.sym - SDLK_1;
				const auto& bookmarkEntities = Locator::cameraBookmarkSystem::value().GetBookmarks();
				const auto entity = bookmarkEntities.at(index);
				const auto [transform, bookmark] =
				    entitiesRegistry.TryGet<ecs::components::Transform, ecs::components::CameraBookmark>(entity);
				if (transform != nullptr && bookmark != nullptr)
				{
					camera.GetModel().SetFlight(bookmark->savedOrigin, transform->position);
				}
			}
			break;
		}
		break;
	case SDL_MOUSEMOTION:
	{
		SDL_GetMouseState(&_mousePosition.x, &_mousePosition.y);
		break;
	}
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		switch (event.button.button)
		{
		case SDL_BUTTON_MIDDLE:
		{
			// Relative mode while held: the cursor stays put and only the motion drives the camera.
			static glm::ivec2 pressPosition {0, 0};
			const bool pressed = event.type == SDL_MOUSEBUTTONDOWN;
			if (pressed)
			{
				pressPosition = {event.button.x, event.button.y};
			}
			SDL_SetRelativeMouseMode(pressed ? SDL_TRUE : SDL_FALSE);
			if (!pressed)
			{
				SDL_WarpMouseInWindow(static_cast<SDL_Window*>(window.GetHandle()), pressPosition.x, pressPosition.y);
			}
		}
		break;
		}
		break;
	}

	return true;
}

bool Game::GameLogicLoop() noexcept
{
	using namespace ecs::components;
	using namespace ecs::systems;

	// ProcessNetworkPackets 0x54CD93 / GGame::StartTurn 0x54E507: the turn number goes up at the start of the turn
	game_clock::StartTurn();
	const auto currentTime = std::chrono::steady_clock::now();
	_turnDeltaTime = currentTime - _lastGameLoopTime;
	_lastGameLoopTime = currentTime;
	const uint32_t turn = game_clock::Turn();

	// Build Map Grid Acceleration Structure
	Locator::entitiesMap::value().Rebuild();
	// the reactions' clock (GGame +0x205A40) for the whole turn, and the ones whose initiator went (ECS/Effects/Reactions)
	ecs::effects::reactions::BeginTurn();

	// Living::ProcessLiving: where each villager and animal starts this turn's move (drawn between it and the end)
	ecs::BeginMobileTurn();
	// fn_00775140 (0x54E5C7): the sharks' turn (Whale::Process), then the WALK_PATH list (GlobalGameLists::Process)
	ecs::ProcessSharksTurn();
	// GlobalGameLists::Process 0x591449: the PuzzleGames (fn_006D7480), before the scripts
	ecs::ProcessPuzzleGamesTurn();

	auto& profiler = Locator::profiler::value();

	{
		auto pathfinding = profiler.BeginScoped(Profiler::Stage::PathfindingUpdate);
		Locator::pathfindingSystem::value().Update();
	}
	{
		auto actions = profiler.BeginScoped(Profiler::Stage::LivingActionUpdate);
		Locator::livingActionSystem::value().Update();
		// Living::ProcessLiving for the animals: Animal::ProcessState (ecs/AnimalAI.h)
		ecs::animal_ai::ProcessAnimalsTurn(_dayNightClock->GetVisualTime());
	}
	// The miracles' part of GGame::ProcessTurn (Magic/MagicLoop.cpp: fire, reactions, spells, the seed in the hand...)
	magic::ProcessTurn(turn);

	{
		auto scripts = profiler.BeginScoped(Profiler::Stage::ScriptsUpdate);
		auto& lhvm = Locator::vm::value();
		lhvm.LookIn(lhvm::ScriptType::All);
		// GScript::Process: fn_0070D480 (the things no script variable holds any more are released)
		ecs::script_held::Process();
		// GScript::Process: ProcessFade(false) once per turn
		_screenFade->ProcessTurn();
		// GGame::ProcessTurn: GLandAlignement::UpdateTime once per turn
		_dayNightClock->ProcessTurn();
		// OPENBLACK_TIME_OF_DAY=<script hour> pins the clock there every turn (screenshots), over the scripts' times
		if (const char* hour = std::getenv("OPENBLACK_TIME_OF_DAY"); hour != nullptr)
		{
			_dayNightClock->ForceScriptTime(std::clamp(static_cast<float>(std::atof(hour)), 0.0f, 24.0f));
		}
		Locator::skySystem::value().SetTime(_dayNightClock->GetScriptTime());
		ecs::ProcessFireFliesTurn(*_dayNightClock);
		if (turn % 50 == 0 && std::getenv("OPENBLACK_CLOCK_TRACE") != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Clock: turn {} visual {:.4f} script {:.4f} sky type {:.3f} frame {:.3f} dome {:.3f}", turn,
			                   _dayNightClock->GetVisualTime(), _dayNightClock->GetScriptTime(),
			                   _dayNightClock->GetSkyType(), sky_type::Frame(), sky_type::Dome().Built());
		}
		// OPENBLACK_TEST_TEXT_CLICK=1 (openblack only): the player's click on a text that waits for one (RUN_TEXT with
		// interaction 1), every turn while it waits, as the left button going down does (ProcessEvents);
		// ProcessInterface itself ignores the click until the text has been shown long enough
		if (static const bool textClick = std::getenv("OPENBLACK_TEST_TEXT_CLICK") != nullptr; textClick)
		{
			if (auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->IsWaitingForClick())
			{
				helpSystem->ProcessInterface(true);
				if (!helpSystem->IsWaitingForClick())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_TEXT_CLICK: click taken at turn {}", turn);
				}
			}
		}
		ecs::ProcessFishFarmsTurn(turn);
		ecs::ProcessFieldsTurn(turn);
		// GGame::ProcessTurn 0x54E763..0x54E771: Fragment::ProcessTimer 0x76EAF0 for each fragment, once a turn
		ecs::physics::Buildings::ProcessTurn();
		// PSysGlobal: the particle effects, one step per turn of the turn's length
		psys::manager::RunDebugHooks();
		magic::RunDebugHooks();
		psys::manager::ProcessTurn(game_clock::k_TurnSeconds);
		// GGame::EndTurn 0x54E960 (unpaused: this loop does not run in pause): GSoundMap::Update 0x71D6F0 (+ Dump),
		// SoundTag::ProcessSoundTags 0x71E5F0 (the street lanterns' too), then GAudio::ProcessAudioGameTurn 0x427080 after
		// turn 5 (its music, atmos, channels and listener), AtmosProcess(0) before
		audio::ProcessTurn(_dayNightClock->GetSkyType(), turn);
		audio::AnimationSounds::RunTestHooks(turn); // OPENBLACK_AUDIO_TEST_VIEW / _ANIM
	}
	// The end of the miracles' turn, after the particle step: the PSys sounds, the seed in the hand (Magic/MagicLoop.cpp)
	magic::ProcessTurnEnd();
	ecs::effects::reactions::EndTurn();

	// mods: their turn event, at the end of the game's turn (the turn of game_clock, as it began)
	mods::lua::OnTurn(turn);
	mods::native::OnTurn(turn);

	return false;
}

bool Game::Update() noexcept
{
	auto& profiler = Locator::profiler::value();

	profiler.Frame();

	auto& camera = Locator::camera::value();
	auto& config = Locator::config::value();

	auto previous = profiler.GetEntries().at(profiler.GetEntryIndex(-1)).frameStart;
	auto current = profiler.GetEntries().at(profiler.GetEntryIndex(0)).frameStart;
	// Prevent spike at first frame
	if (previous.time_since_epoch().count() == 0)
	{
		current = previous;
	}
	auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(current - previous);

	Locator::debugGui::value().SetScale(config.guiScale);
	// mod graphics.hd-tweaks changed in the Mods menu: its villager textures and meshes, before anything uses them
	resources::hd_tweaks::Update();
	// mods: their frame event
	mods::lua::OnFrame(static_cast<float>(deltaTime.count()) / 1e6f);
	mods::native::OnFrame(static_cast<float>(deltaTime.count()) / 1e6f);

	// Physics
	{
		auto physics = profiler.BeginScoped(Profiler::Stage::PhysicsUpdate);
		if (_frameCount > 0)
		{
			auto& dynamicsSystem = Locator::dynamicsSystem::value();
			dynamicsSystem.Update(deltaTime);
			dynamicsSystem.UpdatePhysicsTransforms();
		}
	}

	// Input events
	{
		auto sdlInput = profiler.BeginScoped(Profiler::Stage::SdlInput);
		if (!Locator::debugGui::value().StealsFocus())
		{
			Locator::gameActionSystem::value().Frame();
		}
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
		{
			Locator::events::value().Create<SDL_Event>(e);
		}
		camera.HandleActions(deltaTime);
	}

	if (!config.running)
	{
		return false;
	}

	// ImGui events + prepare
	{
		auto guiLoop = profiler.BeginScoped(Profiler::Stage::GuiLoop);
		if (Locator::debugGui::value().Loop())
		{
			return false; // Quit event
		}
	}

	{
		auto cameraSection = profiler.BeginScoped(Profiler::Stage::CameraUpdate);
		camera.Update(deltaTime);
		// The original's near plane follows the camera height above the ground: 0.3 + 0.16 h, clamped to 0.3..3.5
		if (Locator::terrainSystem::has_value() && Locator::windowing::has_value())
		{
			const auto origin = camera.GetOrigin();
			const float height = origin.y - Locator::terrainSystem::value().GetHeightAt(glm::vec2(origin.x, origin.z));
			const float nearClip = std::clamp(0.3f + 0.16f * height, 0.3f, 3.5f);
			auto& config = Locator::config::value();
			if (std::abs(nearClip - config.cameraNearClip) > 0.01f)
			{
				config.cameraNearClip = nearClip;
				camera.SetProjectionMatrixPerspective(config.cameraXFov, Locator::windowing::value().GetAspectRatio(),
				                                      config.cameraNearClip, config.cameraFarClip);
			}
		}
		Locator::cameraBookmarkSystem::value().Update(deltaTime);
	}

	// Update Game Logic in Registry: GGame::Loop 0x54D28A ProcessNetworkPackets, the turns before the frame clock and the draw
	{
		auto gameLogic = profiler.BeginScoped(Profiler::Stage::GameLogic);
		// 0x54CD45: while LocalTimerSaysDoATurn and fewer than 1 turn this frame (game_clock::TurnDue)
		while (game_clock::TurnDue())
		{
			if (GameLogicLoop())
			{
				return false; // Quit event
			}
		}
		if (game_clock::IsPaused())
		{
			// GGame::EndTurn while paused: GAudio::AtmosProcess(0)
			audio::Paused();
		}
	}
	// GGame::Loop 0x54D2A8..0x54D3A6: the remainder, the visual clock, g_game_time_inc and the fraction of the turn;
	// LH3DRender::StartFrame 0x82F14E: g_delta_time
	game_clock::UpdateFrameClock();
	game_clock::UpdateRealClock();

	// Fields: visibility and sinking with their food (Field::Draw)
	ecs::UpdateFields(std::chrono::duration<float>(deltaTime).count());
	// Tree::PreDraw / Tree::Draw: the trees' brightness this frame and the rustle of the tall ones by the camera
	ecs::UpdateTrees(std::chrono::duration<float>(deltaTime).count());

	// Fireflies (FireFly::Draw): orbit and fade, in game time
	ecs::UpdateFireFlies(game_clock::FrameGameSeconds(), camera.GetOrigin());

	// Water rings (fn_005E5100): g_game_time_inc, in milliseconds
	ecs::UpdateWaterRings(static_cast<float>(game_clock::FrameGameMs()));
	// DesignedWaterFall 0x5E3770: the scenery of Land 3 (waterfall) and Land 4 (ark, dinosaur), by land number
	ecs::designed_scenery::Update(static_cast<float>(game_clock::FrameGameMs()));
	// The marks on the ground (fn_00825350, from fn_005E5CD0 0x5E6197 just before the SmokyStuff): fade and go
	ecs::ground_marks::Update(static_cast<float>(game_clock::FrameGameMs()));
	// PetitNavire::PreDraw 0x5DFF20 / SmokyStuff fn_00824140 / PostDraw 0x5E03F0 (the missionaries' boat, ecs/PetitNavire.h).
	// fn_00824140 also moves the smoke an object leaves when it goes (ecs/SmokyStuff.h), in game time, boat or not.
	ecs::petit_navire::Update(static_cast<float>(game_clock::FrameGameMs()));

	// Villagers and animals drawn between turns, turning smoothly, on the slope (ecs/MobileDrawing.h)
	ecs::UpdateMobileDrawing(GetTurnFraction(), static_cast<float>(game_clock::FrameGameMs()));
	// Skeletal animation of villagers and animals (ecs/Animations.h), in milliseconds of game time
	ecs::UpdateVillagerAnimations();
	ecs::UpdateAnimalAnimations();
	ecs::UpdateAnimations(static_cast<float>(game_clock::FrameGameMs()));
	ecs::UpdateCarriedProps();
	// fn_00774E30: the sharks drawn between turns, heading, wake rings (ecs/Sharks.h)
	ecs::UpdateSharks(GetTurnFraction(), static_cast<float>(game_clock::FrameGameMs()));

	// FishFarm shoals (fn_00824DA0), moved with the frame's game time
	ecs::UpdateFishShoals(game_clock::FrameGameSeconds(), camera.GetOrigin());

	// fn_005C6BB0 (from HelpSystem::Draw3D): the cinema bars slide with the game time of this frame
	_screenFade->UpdateWideScreen(static_cast<float>(game_clock::FrameGameMs()));

	// Update Uniforms
	{
		auto profilerScopedUpdateUniforms = profiler.BeginScoped(Profiler::Stage::UpdateUniforms);

		// Update Hand and intersection point
		ecs::components::Transform intersectionTransform {};
		{
			const auto screenSize =
			    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
			const auto scale = glm::vec3(50.0f, 50.0f, 50.0f);
			if (screenSize.x > 0 && screenSize.y > 0)
			{
				// Test hook: fixed cursor at a fraction of the window ("0.5,0.6"), for screenshots without the real mouse
				if (const char* at = std::getenv("OPENBLACK_MOUSE_AT"); at != nullptr)
				{
					glm::vec2 fraction(0.5f);
					if (std::sscanf(at, "%f,%f", &fraction.x, &fraction.y) == 2)
					{
						_mousePosition = glm::ivec2(glm::vec2(screenSize) * fraction);
					}
				}
				auto rayCast = profiler.BeginScoped(Profiler::Stage::HandRayCast);
				glm::vec3 rayOrigin;
				glm::vec3 rayDirection;
				camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize),
				                              rayOrigin, rayDirection);
				auto& dynamicsSystem = Locator::dynamicsSystem::value();

				if (!glm::any(glm::isnan(rayOrigin) || glm::isnan(rayDirection)))
				{
					if (auto hit = dynamicsSystem.RayCastClosestHit(rayOrigin, rayDirection, 1e10f))
					{
						intersectionTransform = hit->first;
					}
					else // For the water
					{
						float intersectDistance = 0.0f;
						const auto planeOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
						const auto planeNormal = glm::vec3(0.0f, 1.0f, 0.0f);
						if (glm::intersectRayPlane(rayOrigin, rayDirection, planeOrigin, planeNormal, intersectDistance))
						{
							intersectionTransform.position = rayOrigin + rayDirection * intersectDistance;
							intersectionTransform.rotation = glm::mat3(1.0f);
						}
					}
					// ObtainRequiredHandPosition: the hand goes along the mouse ray to the surface under the cursor
					// (an object's mesh or the land), smoothed by the hand distance zoomer.
					{
						const bool land = intersectionTransform.position != glm::zero<glm::vec3>();
						const auto point = Locator::handSystem::value().ResolveCursorPoint(
						    rayOrigin, rayDirection, land ? std::optional(intersectionTransform.position) : std::nullopt,
						    _handGripping, deltaTime);
						intersectionTransform.position = point.value_or(glm::zero<glm::vec3>());
					}
				}
				intersectionTransform.scale = scale;
			}

			// Hand animation (hh.HBN): Cwiggle / Cgrip + L*_lr / L*_fb layers driven by the cursor motion.
			{
				auto handUpdate = profiler.BeginScoped(Profiler::Stage::HandUpdate);
				static glm::ivec2 previousMousePosition = _mousePosition;
				const auto mouseDelta = glm::vec2(_mousePosition - previousMousePosition);
				previousMousePosition = _mousePosition;
				Locator::handSystem::value().Update(deltaTime, mouseDelta, _handGripping, _handAction);
			}
			// The miracles' per-frame part (the one-shot orbs' texture), in game time
			magic::Update(game_clock::FrameGameSeconds());

			// Palm towards the ground, index fingertip on the point under the cursor, fingertips dug in while gripping.
			const bool overLand = intersectionTransform.position != glm::zero<glm::vec3>();
			auto handPlace = profiler.BeginScoped(Profiler::Stage::HandPlace);
			Locator::handSystem::value().Place(overLand ? std::optional(intersectionTransform.position) : std::nullopt,
			                                   camera.GetForward(), _handGripping, deltaTime);
		}

		// Update Entities
		{
			auto updateEntities = profiler.BeginScoped(Profiler::Stage::UpdateEntities);
			if (config.drawEntities)
			{
				Locator::rendereringSystem::value().PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths,
				                                                config.drawStreams);
			}
		}
	} // Update Uniforms

	// Update Audio
	{
		auto updateAudio = profiler.BeginScoped(Profiler::Stage::UpdateAudio);
		Locator::audio::value().Update();
		// the sample master of the configuration, live (the options dialog's slider 0x5145A3)
		audio::UpdateFrame();
		audio::music::Update();
		// HelpDudeControl's loop 0x5C3B05..0x5C3CBC, once a frame: the advisors' delayed sentences (UpdateSaySentence
		// 0x5BB610, GetTickCount) and their lip-sync (ApplyLipSync 0x5BCD00 with the frame's seconds)
		audio::advisor::Update(std::chrono::duration<float>(deltaTime).count());
	} // Update Audio

	return config.numFramesToSimulate == 0 || _frameCount < config.numFramesToSimulate;
}

bool Game::Initialize() noexcept
{
	auto& config = Locator::config::value();

	if (config.graphicsBackend != GraphicsBackend::Noop)
	{
		uint32_t extraFlags = 0;
		if (config.graphicsBackend == GraphicsBackend::Metal)
		{
			extraFlags |= SDL_WINDOW_METAL;
		}
		openblack::InitializeWindow(k_WindowTitle, config.resolution.x, config.resolution.y, config.displayMode, extraFlags);
	}

	using filesystem::Path;
	if (!InitializeEngine(config.graphicsBackend, config.vsync))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize engine services.");
		return false;
	}
	auto& fileSystem = Locator::filesystem::value();
	auto& events = Locator::events::value();

	events.AddHandler(std::function([this, &config](const SDL_Event& event) {
		// If gui captures this input, do not propagate
		if (!Locator::debugGui::value().ProcessEvents(event))
		{
			config.running = this->ProcessEvents(event);
			Locator::gameActionSystem::value().ProcessEvent(event);
		}
	}));

	if (!fileSystem.IsPathValid(_gamePath))
	{
		// no key, don't guess, let the user know to set the command param
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Path missing",
		                         "Game path was not supplied, use the -g "
		                         "command parameter to set it.",
		                         nullptr);
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to find the GameDir.");
		return false;
	}

	fileSystem.SetGamePath(_gamePath);
	Locator::mods::value().MountDataMods(fileSystem);

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The GamePath is \"{}\".", fileSystem.GetGamePath().generic_string());

	if (std::filesystem::path(_startMap).is_absolute())
	{
		if (std::find(_startMap.begin(), _startMap.end(), "Scripts") != _startMap.end())
		{
			auto p = _startMap;
			while (p.filename() != "Scripts" && p != p.parent_path())
			{
				p = p.parent_path();
			}
			fileSystem.AddAdditionalPath(p.parent_path());
		}
		else
		{
			fileSystem.AddAdditionalPath(_startMap.parent_path());
		}
	}
	else
	{
		_startMap = fileSystem.GetPath<Path::Scripts>() / _startMap;
	}

	if (!InitializeGame())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize game services.");
		return false;
	}

	auto& resources = Locator::resources::value();
	auto& meshManager = resources.GetMeshes();
	auto& textureManager = resources.GetTextures();
	auto& animationManager = resources.GetAnimations();
	auto& levelManager = resources.GetLevels();
	auto& soundManager = resources.GetSounds();
	auto& glowManager = resources.GetGlows();

	fileSystem.Iterate(
	    fileSystem.GetPath<Path::Citadel>() / "OutsideMeshes", false, [&meshManager](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading temple mesh: {}", f.stem().string());
			    try
			    {
				    meshManager.Load(fmt::format("temple/{}", f.stem().string()), resources::L3DLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });

	fileSystem.Iterate( //
	    fileSystem.GetPath<filesystem::Path::Citadel>() / "engine", false,
	    [&meshManager, &glowManager](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    if (f.stem().string().ends_with("lo_l3d"))
			    {
				    SPDLOG_LOGGER_WARN(
				        spdlog::get("game"),
				        "Skipping lo duplicate lo meshes. See https://github.com/openblack/openblack/issues/727");
				    return;
			    }
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple mesh: {}", f.stem().string());
			    try
			    {
				    meshManager.Load(fmt::format("temple/interior/{}", f.stem().string()), resources::L3DLoader::FromDiskTag {},
				                     f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
		    else if (f.extension() == ".glw")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple glows: {}", f.stem().string());
			    try
			    {
				    glowManager.Load(fmt::format("temple/interior/glow/{}", f.stem().string()),
				                     resources::LightLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });

	pack::PackFile pack;

	auto packResult = pack.ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllMeshes.g3d"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllMeshes.g3d: {}", pack::ResultToStr(packResult));
		return false;
	}

	// mod graphics.hd-tweaks: the villagers' textures come from the HD images in its folder, and their meshes (the ones
	// with those textures) can be smoothed
	const auto hdTextures = resources::hd_tweaks::Begin();
	const auto& meshes = pack.GetMeshes();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& mesh : meshes)
	{
		const auto meshId = static_cast<MeshId>(i);
		// a modded pack may have more meshes than openblack has names for
		const auto name = i < k_MeshNames.size() ? k_MeshNames[i] : fmt::format("Mesh{}", i);
		// a mod's mesh (mod.json "replace": {"meshes": ...}, Mods/Replacements.h) instead of the pack's
		if (const auto file = mods::replace::Mesh(i))
		{
			try
			{
				meshManager.Load(meshId, resources::L3DLoader::FromDiskTag {}, *file);
				++i;
				continue;
			}
			catch (const std::exception& error)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Mods: mesh {} from {}: {}", name, file->generic_string(),
				                    error.what());
			}
		}
		meshManager.Load(meshId, resources::L3DLoader::FromBufferTag {}, name, mesh);
		++i;
	}

	const auto& textures = pack.GetTextures();
	for (auto const& [name, g3dTexture] : textures)
	{
		// a mod's image (mod.json "replace": {"textures": {"pack:<id>": ...}}) instead of the pack's texture
		if (const auto image = mods::replace::PackTexture(g3dTexture.header.id))
		{
			try
			{
				textureManager.Load(g3dTexture.header.id, resources::Texture2DLoader::FromImageTag {}, name, *image);
				continue;
			}
			catch (const std::exception& error)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Mods: texture {} from {}: {}", name, image->generic_string(),
				                    error.what());
			}
		}
		resources::hd_tweaks::LoadTexture(hdTextures, name, g3dTexture);
	}

	pack::PackFile animationPack;
	packResult = animationPack.ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllAnims.anm"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllAnims.anm: {}", pack::ResultToStr(packResult));
		return false;
	}

	const auto& animations = animationPack.GetAnimations();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; i < animations.size(); i++)
	{
		animationManager.Load(i, resources::L3DAnimLoader::FromBufferTag {}, animations[i]);
	}

	fileSystem.Iterate(fileSystem.GetPath<Path::CreatureMesh>(), false, [&meshManager](const std::filesystem::path& f) {
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading creature mesh: {}", fileName);
		try
		{
			if (string_utils::BeginsWith(fileName, "Hand"))
			{
				return;
			}

			const auto meshId = creature::GetIdFromMeshName(fileName);
			meshManager.Load(meshId, resources::L3DLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	// Load loose one-off assets
	{
		using AFromDiskTag = resources::L3DAnimLoader::FromDiskTag;
		animationManager.Load("coffre", AFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.anm");

		using LFromDiskTag = resources::L3DLoader::FromDiskTag;
		meshManager.Load("hand", LFromDiskTag {}, fileSystem.GetPath<Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
		meshManager.Load("coffre", LFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.l3d");
		meshManager.Load("cone", LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "cone.l3d");
		meshManager.Load("marker", LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "marker.l3d");
		meshManager.Load("river", LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river.l3d");
		meshManager.Load("river2", LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river2.l3d");
		meshManager.Load("metre_sphere", LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "metre_sphere.l3d");
		// OneOffSpellSeed::CallVirtualFunctionsForCreation 0x72A450: .\data\spells\meshes\O_Bibble_up.l3d (not in the
		// test data)
		try
		{
			meshManager.Load("O_Bibble_up", LFromDiskTag {},
			                 fileSystem.GetPath<Path::Data>() / "Spells" / "Meshes" / "O_bibble_up.l3d");
			// GetSharedMesh 0x72A490 with MaterialProperties {1, 1, 0, 1, 1} (0x72A474..0x72A485): +3 = 1 makes
			// PGetSharedMesh 0x57DF18 rewrite every primitive (GJUtils::SetMaterialProperties 0x57E120): the cap's
			// AlphaTextured (4) -> 6 -> additive 13 -> with Z write 12 (SRCALPHA / ONE, alpha = texture x diffuse), and the
			// double-sided bit cleared. The object alpha's mode table 0xC387C8 keeps mode 12, so the bubble ADDS its
			// texture x 0x95 to what is behind it: the bright, pearly bubble of the original
			meshManager.Handle(entt::hashed_string("O_Bibble_up"))
			    ->SetMaterialProperties({.additive = true, .zWrite = true, .doubleSided = false, .change = true, .alpha = true});
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	// TODO(raffclar): #400: Parse level files within the resource loader
	// TODO(raffclar): #405: Determine campaign levels from the challenge script file
	// Load the campaign levels
	fileSystem.Iterate(fileSystem.GetPath<Path::Scripts>(), false, [&levelManager](const std::filesystem::path& f) {
		const auto& name = f.stem().string();
		if (f.extension() != ".txt" || name.rfind("InfoScript", 0) != std::string::npos)
		{
			return;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading campaign level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("campaign/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Campaign);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});
	// Load Playgrounds
	// Attempt to load additional levels as playgrounds
	fileSystem.Iterate(fileSystem.GetPath<Path::Playgrounds>(), false, [&levelManager](const std::filesystem::path& f) {
		if (f.extension() != ".txt")
		{
			return;
		}
		const auto& name = f.stem().string();
		if (levelManager.Contains(fmt::format("playgrounds/{}", name)))
		{
			// Already added
			return;
		}

		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading custom level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("playgrounds/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Skirmish);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	// The GAudio ctor 0x426D40: the sample master and the channels' queries, before its banks (fn_00429CB0)
	audio::Init(MakeMusicQueries(*this));

	// Load all sound packs in the Audio directory
	auto& audioManager = Locator::audio::value();
	fileSystem.Iterate(
	    fileSystem.GetPath<Path::Audio>(), true, [&audioManager, &soundManager, &fileSystem](const std::filesystem::path& f) {
		    if (f.extension() != ".sad")
		    {
			    return;
		    }

		    pack::PackFile soundPack;
		    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Opening sound pack {}", f.filename().string());
		    // The dialogue banks of 0x9CB3F8 (types 6..10, Audio\Dialogue) are registered as LHBankRegister(path, 0)
		    // 0x10002240 does: only the headers are read, and each wave is read from the file at its first play
		    // (0x10011420 -> fn_100032D0; Sound::waveFile). The other banks keep their bytes in memory (approximated: the
		    // original reads every bank that way, 0x426EEE).
		    bool onDemand = false;
		    for (const auto bank : {audio::SfxBank::HelpSprites, audio::SfxBank::Villagers, audio::SfxBank::VillagersBanter,
		                            audio::SfxBank::SpellDialogue, audio::SfxBank::Guidance})
		    {
			    const auto path = string_utils::LowerCase(f.generic_string());
			    const auto wanted = string_utils::LowerCase(std::string(audio::SfxBankPath(bank)));
			    onDemand = onDemand || (path.size() >= wanted.size() &&
			                            path.compare(path.size() - wanted.size(), wanted.size(), wanted) == 0);
		    }
		    const auto result =
		        onDemand ? soundPack.ReadAudioHeaders(*fileSystem.GetData(f)) : soundPack.ReadFile(*fileSystem.GetData(f));
		    if (result != pack::PackResult::Success)
		    {
			    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to load sound pack {}: {}", f.filename().string(),
			                        pack::ResultToStr(result));
			    return;
		    }
		    const auto& audioHeaders = soundPack.GetAudioSampleHeaders();
		    const auto& audioData = soundPack.GetAudioSamplesData();
		    if (audioHeaders.empty())
		    {
			    SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound pack found for {}. Skipping", f.filename().string());
			    return;
		    }
		    auto soundName = std::filesystem::path(audioHeaders[0].name.data());

		    auto groupName = f.filename().string();

		    // The wave names of the dialogue banks of the voice table 0x915D40 (k_SfxBankPaths 6, 7, 10)
		    for (const auto bank : {audio::SfxBank::Villagers, audio::SfxBank::HelpSprites, audio::SfxBank::Guidance})
		    {
			    const auto path = string_utils::LowerCase(f.generic_string());
			    const auto wanted = string_utils::LowerCase(std::string(audio::SfxBankPath(bank)));
			    if (path.size() >= wanted.size() && path.compare(path.size() - wanted.size(), wanted.size(), wanted) == 0)
			    {
				    std::vector<std::string> names;
				    names.reserve(audioHeaders.size());
				    for (const auto& header : audioHeaders)
				    {
					    names.emplace_back(header.name.begin(), std::find(header.name.begin(), header.name.end(), '\0'));
				    }
				    audio::voices::SetBankSampleNames(bank, std::move(names));
			    }
		    }

		    // A music bank (its waves are ".mpg"): LHMusic registers it by MUSIC_TYPE (audio::music, k_MusicBanks 0x9C9748)
		    if (soundName.extension() != ".mpg")
		    {
			    audioManager.CreateSoundGroup(groupName);
			    // LHBankRegister 0x10002240: the bank of its samples (the 11 types of 0x9CB3F8 by path, any case)
			    const auto bankId = audio::RegisterBank(f, groupName);
			    audio::SetBankSampleCount(bankId, static_cast<int>(audioHeaders.size()));
			    // 0x10002778..0x100029AB: its anim effect tables, read once here (audio::anim_effects)
			    audio::anim_effects::RegisterTables(bankId, soundPack);
			    for (size_t i = 0; i < audioHeaders.size(); i++)
			    {
				    soundName = std::filesystem::path(audioHeaders[i].name.data());
				    if (onDemand ? audioHeaders[i].size == 0 : audioData[i].empty())
				    {
					    SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound buffer found for {}. Skipping",
					                       soundName.string());
					    continue; // the next ones still load (spells.sad has an empty entry 31 before 32..88)
				    }

				    const auto stringId = fmt::format("{}/{}", groupName, audioHeaders[i].id);
				    const entt::id_type id = entt::hashed_string(stringId.c_str());
				    const std::vector<std::vector<uint8_t>> buffer =
				        onDemand ? std::vector<std::vector<uint8_t>> {} : std::vector<std::vector<uint8_t>> {audioData[i]};
				    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Loading sound {}: {}", stringId, audioHeaders[i].name.data());
				    soundManager.Load(id, resources::SoundLoader::FromBufferTag {}, audioHeaders[i], buffer);
				    soundManager.Handle(id)->bank = bankId;
				    if (onDemand)
				    {
					    soundManager.Handle(id)->waveFile = f;
					    soundManager.Handle(id)->waveOffset = soundPack.GetAudioWaveDataOffset() + audioHeaders[i].offset;
					    soundManager.Handle(id)->waveSize = audioHeaders[i].size;
				    }
				    audioManager.AddToSoundGroup(groupName, id);
			    }
		    }
	    });

	// The voices of the help texts (0x915D40), rebuilt from the wave names of villagers, HelpSprites and Guidance
	audio::voices::BuildTable();

	// LHMusic on the OpenAL context of the audio manager (LH_AudioSystem init 0x1000DD50, from the GAudio constructor)
	audio::music::Start();

	{
		InfoFile infoFile;
		auto result = infoFile.LoadFromFile(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "info.dat");
		if (!result)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to load game info data.");
			return false;
		}
		if (mods::replace::HasObjectPatches())
		{
			// mods change objects of info.dat (mod.json "replace": {"objects": ...}) before it is published as const
			auto patched = std::make_unique<InfoConstants>(*result);
			mods::replace::PatchObjects(*patched);
			Locator::infoConstants::reset(patched.release());
		}
		else
		{
			Locator::infoConstants::reset(result.release());
		}
	}

	// GAudio's music (the banks of 0x9C9748, fn_00426D40), with the town distances of info.dat (0xD9A934 / 0xD9A938)
	{
		const auto& sound = Locator::infoConstants::value().sound;
		audio::game_music::Start(MakeMusicQueries(*this), {sound.townTriggerDistance, sound.townTriggerOffDistance});
	}

	// HelpSystem::CallVirtualFunctionsForCreation 0x5C5860: HelpDudeControl::Init with the HelpSprites bank for both
	// advisors (fn_005C3660 -> fn_005BB060); the models MarkGood.Hd / MarkEvil.Hd are not ported
	audio::advisor::Init(audio::Bank(audio::SfxBank::HelpSprites));

	// HelpSystem (texts A11, voices B7) with HelpSystemInfo of info.dat (0xD16178 / 0xD1617C)
	{
		const auto& helpInfo = Locator::infoConstants::value().helpSystem;
		help::HelpSystem::Queries queries;
		// g_game +0x205A40
		queries.turn = []() { return game_clock::Turn(); };
		// LH3DTech::g_timer's ms (0xEA1C78..0xEA1C80, 0x5C6250)
		queries.nowMs = []() { return game_clock::EngineMs(); };
		// ScriptDLL::GetScriptType 0x6F6C50 (fn_005C6800 0x5C681B)
		queries.taskScriptType = [](uint32_t task) -> uint32_t {
			return Locator::vm::has_value() ? static_cast<uint32_t>(Locator::vm::value().GetTaskScriptType(task)) : 1;
		};
		// fn_005C62F0 0x5C631E: GAudio+0x3A8 + 4 * bank != 0
		queries.voiceBankLoaded = [](audio::SfxBank bank) { return audio::voices::BankRegistered(bank); };
		// HelpSystem+0x10, HelpDudeControl (0x5C6372..0x5C63A0)
		queries.advisorsTalking = []() { return audio::advisor::AnyTalking(); };
		// fn_0042A280(owner, sample, bank) (0x5C63CA)
		queries.isPlaying = [](audio::SfxBank bank, audio::VoiceOwner owner, uint32_t sample) {
			return audio::IsPlaying(audio::Owner::Key(static_cast<uint32_t>(owner)), static_cast<int>(sample), bank);
		};
		help::HelpSystem::Hooks hooks;
		// fn_005C5F90's voice (0x5C6025..0x5C60DB)
		hooks.sayVoice = [](uint32_t textId, help::VoiceRoute /*route*/, audio::TextVoice voice) {
			audio::voices::RunTextVoice(helptext::GetEntry(textId).narrator, voice);
		};
		// ProcessInterface 0x5C6AAD: the villagers' narration cut with the 20 ms ramp
		hooks.stopVoicesOnClick = []() { audio::voices::CutByClick(); };
		// fn_005C6720(spirit, arg) -> fn_005C4C20 -> HelpDudeControl fn_005C3780(dude, arg): dude = spirit+0x54 != 1
		// (fn_005C5250) (inferred: the good spirit, HelpSystem+0xC, has type 1 and dude 0)
		hooks.spiritStop = [](int32_t spirit, int32_t arg) {
			audio::advisor::Interrupt(spirit == 1 ? audio::advisor::k_GoodSpirit : audio::advisor::k_EvilSpirit, arg);
		};
		// HelpSystem::SetWideScreen 0x5C6AD0: the bars slide in HelpSystemInfo.wideScreenTime (0xD16174) seconds from
		// where they are (0x5C6B3F..0x5C6B4E); DialogBoxBase::HideAll and GInterface::SetActive are not ported
		// and +0x45EC (the owning task while on) is what GAudio::PlaySoundEffect reads to skip the user-param-1 samples
		hooks.wideScreen = [this, time = helpInfo.wideScreenTime](bool on) {
			GetScreenFade().SetWideScreen(on, time);
			const auto* helpSystem = help::Get();
			audio::SetScriptWideScreen(helpSystem != nullptr && helpSystem->IsScriptWideScreen());
		};
		help::Start({helpInfo.readDefaultAdjustGTTime, helpInfo.readDefaultWordGTTime}, std::move(queries),
		            std::move(hooks));
	}

	// a mod's image or .raw for Data/Textures/<stem>.raw (mod.json "replace": {"textures": {"raw:<stem>": ...}}),
	// loaded under the game's own name; the ones the game does not have are added after
	const auto loadRaw = [&textureManager](const std::string& stem, const std::filesystem::path& file) {
		const auto key = fmt::format("raw/{}", stem);
		if (string_utils::LowerCase(file.extension().string()) == ".png")
		{
			textureManager.Load(key, resources::Texture2DLoader::FromImageTag {}, key, file);
		}
		else
		{
			textureManager.Load(key, resources::Texture2DLoader::FromDiskTag {}, file);
		}
	};
	std::vector<std::string> rawLoaded;
	fileSystem.Iterate(fileSystem.GetPath<Path::Textures>(), false,
	                   [&textureManager, &loadRaw, &rawLoaded](const std::filesystem::path& f) {
		if (string_utils::LowerCase(f.extension().string()) == ".raw")
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading raw texture: {}", f.stem().string());
			try
			{
				const auto stem = f.stem().string();
				rawLoaded.push_back(string_utils::LowerCase(stem));
				if (const auto replacement = mods::replace::RawTexture(stem))
				{
					loadRaw(stem, *replacement);
					return;
				}
				textureManager.Load(fmt::format("raw/{}", stem), resources::Texture2DLoader::FromDiskTag {}, f);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			}
		}
	});
	for (const auto& stem : mods::replace::RawTextureNames())
	{
		if (std::ranges::find(rawLoaded, string_utils::LowerCase(stem)) == rawLoaded.end())
		{
			try
			{
				loadRaw(stem, *mods::replace::RawTexture(stem));
			}
			catch (const std::exception& error)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Mods: raw texture {}: {}", stem, error.what());
			}
		}
	}

	return true;
}

bool Game::Run() noexcept
{
	auto& config = Locator::config::value();

	// mods: the Lua scripts of the active mods run once the engine is up, before the first land (so they see its
	// land_loaded)
	mods::lua::Start(Locator::mods::value());
	mods::native::Start(Locator::mods::value());

	if (!LoadMap(_startMap))
	{
		return false;
	}

	Locator::dynamicsSystem::value().RegisterRigidBodies();

	auto& fileSystem = Locator::filesystem::value();

	auto challengePath = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
	if (fileSystem.Exists(challengePath))
	{
		auto& chlapi = Locator::chlapi::value();
		auto& lhvm = Locator::vm::value();
		// the VM's object references: the original ScriptLibraryR.dll calls the ADD_REFERENCE / REMOVE_REFERENCE natives
		// (GScript::AddReference 0x6FA450 -> IncrementScriptReference 0x70CF90, RemoveReference 0x6FA470 ->
		// DecrementScriptReference 0x70CFD0) for a popped object and the variable's old one (POP 0x10008BC0) and for a
		// stopped task's object locals (0x10006604); object 0 is the scripts' null (0x10008A64)
		lhvm.Initialise(
		    &chlapi.GetFunctionsTable(), nullptr, nullptr,
		    // the task-stop callback 0x6EC6D0 (fn_006EB1D0 gives it to ScriptDLL, 0x6EB1F1): the dialogue, the wide
		    // screen and the camera of the task go back (Help/ScriptControl.cpp)
		    [](uint32_t taskNumber) {
			    help::script_control::OnTaskStopped(taskNumber, help::Get(), help::script_control::GetCameraControl(),
			                                        audio::GetScriptAudioState());
		    },
		    nullptr,
		    [](uint32_t objId) {
			    if (objId != 0)
			    {
				    ecs::script_held::IncrementReference(static_cast<entt::entity>(objId));
			    }
		    },
		    [](uint32_t objId) {
			    if (objId != 0)
			    {
				    ecs::script_held::DecrementReference(static_cast<entt::entity>(objId));
			    }
		    });
		try
		{
			lhvm.LoadBinary(fileSystem.ReadAll(challengePath));
			lhvm.StartScript("LandControlAll", lhvm::ScriptType::All);
			// GGame::OnNewGame (0x55395B) right after starting LandControlAll: DoYesNoSkipTutorialRequestersIfNecessary
			// (0x54CBD0) clears bits 23, 24 and 25 of g_game+0x14, pauses the game and shows the SkipBox; its callback
			// (0x544480, jump table 0x5445A0) sets them for the chosen answer: 0 none, 1 bit 23, 2 bits 23+24,
			// 3 bits 23+24+25. openblack draws no SkipBox: the answer is the one of mod game.skip-intro (0 when off,
			// the box's default). SetupLand1 reads them later through CAN_SKIP_TUTORIAL and the others.
			const int skipChoice = config.skipTutorialChoice;
			_tutorialSkipFlags.canSkipTutorial = skipChoice >= 1;
			_tutorialSkipFlags.canSkipCreatureTraining = skipChoice >= 2;
			_tutorialSkipFlags.isKeepingOldCreature = skipChoice >= 3;
		}
		catch (const std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to read challenge file at {}: {}",
			                    (fileSystem.GetGamePath() / challengePath).generic_string(), err.what());
		}
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Challenge file not found at {}",
		                    (fileSystem.GetGamePath() / challengePath).generic_string());
		return false;
	}

	// Test hooks: OPENBLACK_TEST_FADE="r,g,b,seconds" runs SET_FADE, OPENBLACK_TEST_WIDESCREEN=1 SET_WIDESCREEN(1)
	if (const char* fade = std::getenv("OPENBLACK_TEST_FADE"); fade != nullptr)
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float seconds = 0.0f;
		if (std::sscanf(fade, "%f,%f,%f,%f", &r, &g, &b, &seconds) == 4)
		{
			_screenFade->FadeTo(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), seconds);
		}
	}
	if (std::getenv("OPENBLACK_TEST_WIDESCREEN") != nullptr)
	{
		_screenFade->SetWideScreen(true, Locator::infoConstants::value().helpSystem.wideScreenTime);
		// as SET_WIDESCREEN: the HelpSystem's owning task (+0x45EC) is set too (the user-param-1 samples are skipped)
		audio::SetScriptWideScreen(true);
	}
	// OPENBLACK_TEST_MOVE_TIME="hour,seconds" runs MOVE_GAME_TIME; OPENBLACK_CLOCK_TRACE=1 logs the clock every 50 turns
	if (const char* move = std::getenv("OPENBLACK_TEST_MOVE_TIME"); move != nullptr)
	{
		float hour = 0.0f;
		float seconds = 0.0f;
		if (std::sscanf(move, "%f,%f", &hour, &seconds) == 2)
		{
			_dayNightClock->MoveScriptTime(hour, seconds);
		}
	}

	// Initialize the Acceleration Structure
	Locator::entitiesMap::value().Rebuild();

	if (Locator::windowing::has_value())
	{
		const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, size, 0x274659ff);
	}

	{
		uint16_t width;
		uint16_t height;
		Locator::oceanSystem::value().GetReflectionFramebuffer().GetSize(width, height);
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Reflection, {width, height}, 0x274659ff);
	}

	if (config.drawIsland)
	{
		uint16_t width;
		uint16_t height;
		Locator::terrainSystem::value().GetFootprintFramebuffer().GetSize(width, height);
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Footprint, {width, height}, 0x00000000);
	}

	Game::SetTime(config.timeOfDay);

	_frameCount = 0;
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto& profiler = Locator::profiler::value();
	while (Update())
	{
		auto duration = std::chrono::high_resolution_clock::now() - lastTime;
		auto milliseconds = std::chrono::duration_cast<std::chrono::duration<uint32_t, std::milli>>(duration);
		{
			auto section = profiler.BeginScoped(Profiler::Stage::SceneDraw);

			const graphics::RendererInterface::DrawSceneDesc drawDesc {
			    .camera = &Locator::camera::value(),
			    .frameBuffer = nullptr,
			    .entities = Locator::entitiesRegistry::value(),
			    .time = milliseconds.count(), // TODO(#481): get actual time
			    .timeOfDay = Locator::skySystem::value().GetTime(),
			    .bumpMapStrength = config.bumpMapStrength,
			    .smallBumpMapStrength = config.smallBumpMapStrength,
			    .viewId = graphics::RenderPass::Main,
			    .drawSky = config.drawSky,
			    .drawWater = config.drawWater,
			    .drawIsland = config.drawIsland,
			    .drawEntities = config.drawEntities,
			    .drawSprites = config.drawSprites,
			    .drawBoundingBoxes = config.drawBoundingBoxes,
			    .cullBack = false,
			    .wireframe = config.wireframe,
			};
			Locator::rendererInterface::value().DrawScene(drawDesc);
		}

		{
			auto section = profiler.BeginScoped(Profiler::Stage::GuiDraw);
			const bool screenshotThisFrame = _requestScreenshot.has_value() && _requestScreenshot->first == _frameCount;
			// Skip drawing Debug UI for screenshots
			if (screenshotThisFrame)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Requesting a screenshot at frame {}...", _frameCount);
				Locator::rendererInterface::value().RequestScreenshot(_requestScreenshot->second);
				// test hook OPENBLACK_SCREENSHOT_GUI=1: the debug UI (menus, the Mods window) in the screenshot too
				if (std::getenv("OPENBLACK_SCREENSHOT_GUI") != nullptr)
				{
					Locator::debugGui::value().Draw();
				}
			}
			else
			{
				Locator::debugGui::value().Draw();
			}
		}

		if (std::getenv("OPENBLACK_DRAW_STATS") != nullptr && (_frameCount % 30 == 0 || _frameCount < 8))
		{
			const auto* stats = bgfx::getStats();
			SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "DRAWSTATS frame {} draws {}", _frameCount, stats->numDraw);
		}

		// Test hook: "<frames>:<script>,<script>..." loads the next script every <frames> frames, at the point where the
		// debug menu's "Load Island" does (a check that changing maps doesn't crash)
		if (static const char* cycle = std::getenv("OPENBLACK_TEST_MAP_CYCLE"); cycle != nullptr)
		{
			static const auto parsed = [](const std::string& text) {
				std::vector<std::string> scripts;
				const auto colon = text.find(':');
				const int frames = colon == std::string::npos ? 300 : std::max(1, std::atoi(text.substr(0, colon).c_str()));
				std::stringstream list(colon == std::string::npos ? text : text.substr(colon + 1));
				for (std::string script; std::getline(list, script, ',');)
				{
					scripts.push_back(script);
				}
				return std::make_pair(static_cast<uint32_t>(frames), scripts);
			}(cycle);
			const auto& [frames, scripts] = parsed;
			if (_frameCount > 0 && _frameCount % frames == 0 && _frameCount / frames <= scripts.size())
			{
				const auto& script = scripts[_frameCount / frames - 1];
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Map cycle: loading {}", script);
				LoadMap(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / script);
			}
		}

		{
			auto section = profiler.BeginScoped(Profiler::Stage::RendererFrame);
			Locator::rendererInterface::value().Frame();
		}

		// Clear the stale screenshot request
		if (_requestScreenshot.has_value())
		{
			if (_requestScreenshot->first <= _frameCount)
			{
				_requestScreenshot = std::nullopt;
			}
		}

		_frameCount++;
	}

	return true;
}

bool Game::LoadMap(const std::filesystem::path& path) noexcept
{
	auto& fileSystem = Locator::filesystem::value();

	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find script {}", path.generic_string());
		return false;
	}

	psys::manager::Clear();
	magic::OnLoadMap();
	// GSetup::LoadMapFeatures -> GLandBalance::Init: every land balance value back to 1 before the script
	land_balance::Reset();
	// ClearMap -> GData::Reset: the object creation counter back to 0 (2 on the first land: two HelpSpirits)
	ecs::object_index::OnLoadMap();
	// GGame::Init 0x54F66F: both influence multipliers back to 1 before the map script
	_mapScriptGlobals.townInfluenceMultiplier = 1.0f;
	_mapScriptGlobals.playerInfluenceMultiplier = 1.0f;
	// GLandAlignement::Open: default cycle at noon; the Land script may change it (SET_NIGHTTIME)
	_dayNightClock->Reset();
	Locator::skySystem::value().SetTime(_dayNightClock->GetScriptTime());
	ecs::ClearFireFlies();
	ecs::ClearForests();
	ecs::animal_ai::ClearReactions();
	ecs::SmokyStuff::Clear();
	ecs::ground_marks::Clear(); // ClearAllStuff 0x82AED0 (GGame::ClearMap 0x552F22)
	night_lights::Clear();
	// GGame::Init: GAudio::Reset 0x426CA0 (call 0x54F474) with the map's SoundTags and street lanterns, before the
	// registry reset (it destroys the lanterns' emitters without freeing their sources); then GScript::Reset 0x6EB2D0
	// (its audio switches, call 0x54F53A)
	audio::ClearMap();
	audio::GetScriptAudioState().Reset();
	// GScript::Reset 0x6EB2FA..0x6EB303: the camera switches (+0x80, +0x78, +0x7C)
	help::script_control::GetCameraControl().Reset();
	// GScript::Reset 0x6EB2D0 also calls HelpSystem::Reset (0x6EB340): the text part
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->Reset();
	}
	ecs::designed_scenery::OnLoadMap();

	const auto data = fileSystem.ReadAll(path);
	const auto source = std::string(reinterpret_cast<const char*>(data.data()), data.size());

	// Reset everything. Deletes all entities and their components
	Locator::entitiesRegistry::value().Reset();
	// TODO(#661): split entities that are permanent from map entities and move hand and camera to init
	// We need a hand for the player
	Locator::handSystem::value().Initialize();

	// create our camera
	auto& config = Locator::config::value();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	Locator::camera::value().SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip,
	                                                        config.cameraFarClip);

	Script script;
	try
	{
		script.Load(source);
	}
	catch (const std::exception& e)
	{
		// LoadMap is noexcept: a script it cannot read must not end the program
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Error in the map script {}: {}", path.generic_string(), e.what());
	}

	// GStream::CreateAll 0x733FF0: the rivers' landscape footprints, once the script has placed their points
	ecs::CreateRiverFootprints();

	// Each released map comes with an optional .fot file which contains the footpath information for the map
	const auto stem = string_utils::LowerCase(path.stem().generic_string());
	const auto fotPath = fileSystem.GetPath<filesystem::Path::Landscape>() / fmt::format("{}.fot", stem);

	if (fileSystem.Exists(fotPath))
	{
		FotFile fotFile(*this);
		fotFile.Load(fotPath);
	}
	else
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The map at {} does not come with a footpath file. Expected {}",
		                   path.generic_string(), fotPath.generic_string());
	}

	_lastGameLoopTime = std::chrono::steady_clock::now();
	_turnDeltaTime = 0ns;
	SetGameSpeed(Game::k_TurnDurationMultiplierNormal);
	// a new game from turn 0 (inferido: openblack has no saved games, so this stands for GGame::ResolveLoad 0x555080
	// too), then the start of GGame::Loop: the timer from 0 and ResetLocalGameTimer
	game_clock::SetTurn(0);
	game_clock::OnLoad();
	// The original runs from the first frame; OPENBLACK_START_PAUSED=1 keeps openblack's old paused start (test hook)
	game_clock::Start(std::getenv("OPENBLACK_START_PAUSED") != nullptr);

	// mods: the land is ready (their land_loaded event)
	mods::lua::OnLandLoaded(path.stem().string());
	mods::native::OnLandLoaded(path.stem().string());

	return true;
}

void Game::LoadLandscape(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();

	auto fixedName = fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path));

	if (!fileSystem.Exists(fixedName))
	{
		throw std::runtime_error("Could not find landscape " + path.generic_string());
	}
	InitializeLevel(fixedName);
	// GLandscape::Open 0x5E5541: the creature's walkable mask of the new landscape
	land_avoid::Validate(Locator::terrainSystem::value());
	land_avoid::DumpIfRequested();

	// There is always a player active
	Locator::playerSystem::value().AddPlayer(ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE));

	// There is always at least one player active.
	ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE);

	Locator::cameraBookmarkSystem::value().Initialize();
	Locator::dynamicsSystem::value().RegisterIslandRigidBodies(Locator::terrainSystem::value());
	Locator::playerSystem::value().RegisterPlayers();
}

void Game::SetTime(float time) noexcept
{
	// SET_GAME_TIME: the clock keeps running from this script time
	_dayNightClock->ForceScriptTime(time);
	Locator::skySystem::value().SetTime(_dayNightClock->GetScriptTime());
}

void Game::RequestScreenshot(const std::filesystem::path& path) noexcept
{
	_requestScreenshot = std::make_pair(_frameCount, path);
}
