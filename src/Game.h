/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <glm/mat4x4.hpp>
#include <spdlog/common.h>

#include "Debug/TestbedOptions.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Input/MouseButtons.h"
#include "Windowing/WindowingInterface.h" // For DisplayMode

union SDL_Event;

namespace openblack
{
namespace gui
{
class GameInterface;
}
namespace engine
{
struct FrameSnapshot;
}
namespace graphics
{
struct ObjectListFrame;
}

enum class LoggingSubsystem : uint8_t
{
	game,
	input,
	graphics,
	scripting,
	audio,
	pathfinding,
	ai,

	_count
};

constexpr static std::array<std::string_view, static_cast<size_t>(LoggingSubsystem::_count)> k_LoggingSubsystemStrs {
    "game",        //
    "input",       //
    "graphics",    //
    "scripting",   //
    "audio",       //
    "pathfinding", //
    "ai",          //
};

struct Arguments
{
	std::string executablePath;
	int windowWidth;
	int windowHeight;
	bool vsync {true};
	uint8_t detailLevel {4};
	openblack::windowing::DisplayMode displayMode;
	GraphicsBackend graphicsBackend;
	std::string gamePath;
	float guiScale;
	uint32_t numFramesToSimulate;
	std::string logFile;
	std::array<spdlog::level::level_enum, k_LoggingSubsystemStrs.size()> logLevels;
	std::string startLevel;
	std::string creatureFile {k_DefaultProfileCreatureFile};
	/// Start on the flat testbed rather than startLevel
	bool startTestbed {false};
	/// A testbed scenario to run as the game starts, by its id, and how to measure its crowd if it has one; it starts
	/// the game on the testbed
	std::optional<ScenarioRequest> scenario;
	/// Log frame time statistics every so many frames, never when 0
	uint32_t frameStatsInterval {0};
	/// With the frame statistics, the GPU time of each render view
	bool frameStatsViews {false};
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> requestScreenshot;
};

class Camera;

class Game
{
public:
	/// The scheduler's turn (game_clock::k_SchedulerMsPerTurn), for the debug view
	static constexpr auto k_TurnDuration = std::chrono::milliseconds(game_clock::k_SchedulerMsPerTurn);
	static constexpr float k_TurnDurationMultiplierSlow = 2.0f;
	static constexpr float k_TurnDurationMultiplierNormal = 1.0f;
	static constexpr float k_TurnDurationMultiplierFast = 0.5f;

	explicit Game(Arguments&& args) noexcept;
	virtual ~Game() noexcept;

	bool ProcessEvents(const SDL_Event& event) noexcept;
	bool GameLogicLoop() noexcept;
	bool Update() noexcept;
	/// One frame of Run, in this order: the end of the frame's logic into `out` (the overlays, the
	/// SuperVillagers' frame, the camera copy, the clocks, the hand, the draw's flags and this frame's screenshot
	/// request; then PreDraw), the draw from `in` only, what runs between the draw and Frame() (the map-cycle hook; a
	/// later sync point), then Frame(), the stale request cleared and the frame counted (Locator::screenshotRequest)
	void LogicFrame(engine::FrameSnapshot& out, uint32_t timeMs);
	void DrawFrame(const engine::FrameSnapshot& in);
	void LateLogic();
	void FinishFrame();
	bool Initialize() noexcept;
	bool Run() noexcept;

	bool LoadMap(const std::filesystem::path& path) noexcept;
	/// The testbed: a flat land made in memory, with no map script, its player's influence over the middle
	void LoadTestbed() noexcept;
	/// The same with its plane at that altitude, for a hand demo recorded with the camera close to the ground
	void LoadTestbed(uint8_t planeAltitude) noexcept;
	/// LOAD_MAP: a script's change of land, by the map script's path in the game folder, at once (from inside the
	/// script's step, as the original changes land inside the call)
	bool ChangeLand(const std::filesystem::path& scriptPath) noexcept;
	/// LOAD_LANDSCAPE: it uses only the Locator's services, no Game state
	static void LoadLandscape(const std::filesystem::path& path);
	/// What a landscape's open does once its land is in place: the walkable mask, the player, the bookmarks and the
	/// island's rigid bodies
	static void SetUpLandscape();

	/// SET_GAME_TIME: the day / night clock and the sky (Locator), no Game state
	static void SetTime(float time) noexcept;
	/// The turn length multiplier (2 = slow, 0.5 = fast): the game speed is set to the speed-up factor 1 / it
	void SetGameSpeed(float multiplier) { game_clock::SetSpeed(1.0f / multiplier); }
	[[nodiscard]] float GetGameSpeed() const { return 1.0f / game_clock::Speed(); }

	/// The game turn (game_clock::Turn): it goes up at the start of the turn
	[[nodiscard]] uint32_t GetTurn() const { return game_clock::Turn(); }
	[[nodiscard]] bool IsPaused() const { return game_clock::IsPaused(); }
	/// The wall clock time between the last two turns (debug view only)
	[[nodiscard]] std::chrono::duration<float, std::milli> GetDeltaTime() const { return _turnDeltaTime; }
	/// How far the current game turn is, 0..0.99 (game_clock::TurnFraction): kept while paused
	[[nodiscard]] float GetTurnFraction() const { return game_clock::TurnFraction(); }
	/// What the object draw list left out of the last frame's draw (debug readout only); null outside Run
	[[nodiscard]] const graphics::ObjectListFrame* GetObjectListFrame() const { return _objectListFrame; }

	/// OPENBLACK_TEST_TEXT_SHOT (openblack only): when (SDL ticks) to take the screenshot, and where
	std::optional<uint32_t> _textShotAtMs;
	std::string _textShotPath;

	static Game* Instance() { return sInstance; }

private:
	static Game* sInstance;

	/// A land load in three steps, around the map script's read and the script itself: the resets of the last land,
	/// the registry's reset with the player's hand and the camera's projection, then the start of the new land's clock
	/// and game loop. playgroundStart: every land but the campaign's first, whose random seeds go to 0
	void PrepareNewLand(bool playgroundStart);
	void ResetRegistryForNewLand();
	void StartNewLand();

	/// The in-game menus' choices (Gui/GameInterface): the escape menu's actions, the SkipBox's answer; a box pauses
	/// the game while it is open (game_clock::Pause)
	void HandleInterfaceAction();
	/// The keys for the temple's rooms, every frame: outside they take the player into the temple at a room, inside
	/// they cut to it
	void ProcessTempleRoomKeys();
	/// The temple's own turn while the world is paused inside it: the temple's help scripts, the audio game turn, the
	/// room's tooltip and the help system's turn
	void ProcessTempleTurn();
	/// When the temple's turns are due, kept from one visit to the next
	game_clock::PausedTurnTimer _pausedTurn;
	std::unique_ptr<gui::GameInterface> _interface;
	bool _menuWasOpen {false};
	/// Whether the open menu paused the game, which closing it gives back: not inside the temple, which holds the pause
	bool _menuPaused {false};
	/// When the escape menu last closed (SDL_GetTicks), for Escape's 300 ms debounce
	uint32_t _menuClosedTicks {0xFFFFFFFFu}; ///< No debounce before the first close

	/// path to Lionhead Studios Ltd/Black & White folder
	const std::filesystem::path _gamePath;

	std::filesystem::path _startMap;
	/// Started on the testbed (--testbed or --scenario): no map script and no story
	bool _startTestbed {false};
	/// The scenario asked for on the command line, until the scenarios' window takes it as the game starts
	std::optional<ScenarioRequest> _scenarioRequest;
	/// (openblack) whether a land was loaded already: only the first one keeps the start-up seeds (game_random)
	bool _firstMapLoaded {false};

	std::chrono::steady_clock::time_point _lastGameLoopTime;
	std::chrono::steady_clock::duration _turnDeltaTime;
	/// The real mouse buttons and motion the hand reads (Input/MouseButtons)
	input::MouseButtonsState _mouseButtons;
	input::MouseMotionState _mouseMotion;
	/// Whether the window is minimised, which holds the game loop until it is restored (Windowing/WindowAway.h)
	bool _windowAway {false};
	/// The camera the draw reads: Locator::camera copied at the end of every frame's logic (Run)
	std::unique_ptr<Camera> _drawCamera;
	/// This frame's Update ran the object draw list
	bool _objectListRan {false};
	/// The snapshot's object draw list part, while Run's snapshot lives
	const graphics::ObjectListFrame* _objectListFrame {nullptr};

	/// What the testbed's scenarios reach of the game, handed to the debug windows as the game initializes
	class TestbedHostOfGame;
	std::unique_ptr<TestbedHostOfGame> _testbedHost;
	/// The time the last frame moved the game on by: the fixed step with OPENBLACK_FIXED_FRAME_MS, else the frame's own
	std::chrono::microseconds _frameDelta {0};
};
} // namespace openblack
