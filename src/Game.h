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
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <spdlog/common.h>

#include "Common/LoadTimer.h"
#include "Debug/TestbedOptions.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Gui/LoadingScreenRules.h"
#include "Input/MouseButtons.h"
#include "Input/ShortcutKeys.h"
#include "Story/NewGameChoice.h"
#include "Windowing/WindowingInterface.h" // For DisplayMode

union SDL_Event;

namespace openblack
{
namespace gui
{
class GameInterface;
class LoadingScreen;
} // namespace gui
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
	/// Play the pre-intro film at start-up, as on a first run
	bool preIntro {false};
	/// Leave out the logo pictures at start-up
	bool skipLogos {false};
	/// Draw no loading screen
	bool skipLoadingScreens {false};
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> requestScreenshot;
	/// --scenarios: the scenarios run one after the other in this launch, each a new game; the first is `scenario`, with
	/// its frames and screenshot in numFramesToSimulate and requestScreenshot
	std::vector<ScenarioListEntry> scenarioList;
	/// The seed of every random number openblack's own generators draw, and the date pinned, for a deterministic run;
	/// none for a seed of the machine's and the wall clock's date
	std::optional<uint32_t> seed;
	/// --skip-opening: the start-of-game question answered at once, or asked; none to start as the game does
	std::optional<new_game_choice::NewGameStart> newGameStart;
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

	/// Loads a land from its map script. The tips screen shows while it loads, as for a new game; a script's own map
	/// command shows the "please wait" banner instead, after five seconds
	bool LoadMap(const std::filesystem::path& path,
	             loading::LoadingClock::Mode look = loading::LoadingClock::Mode::Tips) noexcept;
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
	/// The in-game interface (its menus and dialogs), none before it is made
	[[nodiscard]] gui::GameInterface* GetInterface() const { return _interface.get(); }

private:
	static Game* sInstance;

	/// What the game shows as it starts, before it loads the rest: the logo pictures
	void PlayStartupScreens();
	/// What follows once the audio has started: the pre-intro film, asked for or on a first run, and the tips screen
	/// fading in, which stays up while the game loads
	void ContinueStartupScreens();
	/// Picks the tip of the day and makes its picture; the first time, fades the tips screen in over a second
	void StartTipOfTheDay();
	/// The tips screen, or the banner, shows while a land loads, drawn again at the loading's redraw points
	void BeginLoadingScreen(loading::LoadingClock::Mode look);
	/// A redraw point inside the loading: the tips screen is drawn and shown if it is due
	void RenderLoadingFrame();
	/// The loading is over: its tip goes before the first frame of the game
	void EndLoadingScreen();
	/// The logo film's two pictures, each fading in and out, any key or mouse button hurrying them on. False when the
	/// window is being closed
	bool PlayLogoScreens();
	/// The pre-intro film in its own loop with the trailer's music, until its last frame or a key. False when the window
	/// is being closed
	bool PlayPreIntro();
	/// Whether the player has a profile: openblack keeps none yet, so it counts as having one
	[[nodiscard]] static bool HasPlayerProfiles() noexcept { return true; }
	/// Draws one frame outside the game's loop over a black screen and shows it
	void PresentStartupFrame(const std::function<void(glm::u16vec2 resolution)>& draw);

	/// The state a game starts with, before anything else: the script fade, the day / night clock, the map script's
	/// globals, the frame count with its screenshot request, and the camera with the draw's copy of it
	void StartGameState(const std::optional<std::pair<uint32_t, std::filesystem::path>>& screenshot);
	/// The game timer at speed 1, paused until a land is loaded; the fixed step's ticks from 0
	static void StartClock();
	/// What the game gives the engine's services once they are made: the testbed's host, and its handlers of the land
	/// changes and the window's events
	void ConnectToEngine();
	/// The in-game menus, greeting the player by OPENBLACK_PLAYER_NAME or the login name (openblack has no profiles),
	/// and the temple, the Creature Cave and the hand that write with them
	void CreateInterface();
	/// The audio's start: its banks, the voice table and the music player
	void StartAudio();
	/// With info.dat read: the game music, the guidance's lists, the spooky voices, the advisors and the help system
	void StartHelp();
	/// The start of Run: the first land (or the testbed, or a list's land run) with its rigid bodies, the challenge
	/// script, the test hooks, the map's acceleration structure, the views and the time of day. False when the land or
	/// the script cannot be read
	bool LoadStartLand() noexcept;
	/// Run's frames, from frame 0 until the frame limit or a quit
	void RunFrames() noexcept;
	/// A new game in the same process, as a new program would start it, in two halves. EndGame: the last game's
	/// services go, but for what a process makes once (the window, the renderer, the file system, the asset caches and
	/// info.dat's values stay)
	void EndGame() noexcept;
	/// StartNewGame: the services are made again in the program's order, with a new registry, the clock, the frame
	/// count and the seeds as at the program's start, the configuration as the command line made it, and the
	/// renderer's and the Game's own state as new. Then Run's land loads as at the program's start: the scenario, if
	/// any, is handed out at its first frame. False when a service cannot be made
	bool StartNewGame(std::optional<ScenarioRequest> scenario, uint32_t frames,
	                  const std::optional<std::pair<uint32_t, std::filesystem::path>>& screenshot) noexcept;
	/// Run with --scenarios: each entry's land and frames, the entries after the first each in a new game (EndGame,
	/// then StartNewGame). None of them may write into the game's folder: such a list is refused before it starts.
	/// The log marks each entry's start and end; each takes its own state hash. A quit ends the list
	bool RunScenarioList() noexcept;
	/// The shutdown of what the game started over the services, before they go: the audio, the interface, the advisors'
	/// spirits and the help system
	void ShutDownGame();

	/// A land load in three steps, around the map script's read and the script itself: the resets of the last land,
	/// the registry's reset with the player's hand and the camera's projection, then the start of the new land's clock
	/// and game loop. playgroundStart: every land but the campaign's first, whose random seeds go to 0
	void PrepareNewLand(bool playgroundStart);
	void ResetRegistryForNewLand();
	void StartNewLand();

	/// When the game was launched
	const LoadTimer::Clock::time_point _launchTime {LoadTimer::Clock::now()};
	/// Times the start of the game, from its launch to the first frame drawn on the first land
	std::optional<LoadTimer> _startupTimer;

	/// The in-game menus' choices (Gui/GameInterface): the escape menu's actions, the SkipBox's answer; a box pauses
	/// the game while it is open (game_clock::Pause)
	void HandleInterfaceAction();
	/// The interface's panels this frame, after the hand's update: the fight's health and stamina while a fight is on,
	/// and the status of the creature the hand holds or Creature Mode follows; neither in the temple
	void UpdateHandInterface();
	/// The keys for the temple's rooms, every frame: outside they take the player into the temple at a room, inside
	/// they cut to it
	void ProcessTempleRoomKeys();
	/// The temple's own turn while the world is paused inside it: the temple's help scripts, the audio game turn, the
	/// room's tooltip and the help system's turn
	void ProcessTempleTurn();
	/// When the temple's turns are due, kept from one visit to the next
	game_clock::PausedTurnTimer _pausedTurn;
	/// The options screen's one-press actions: the temple and realm keys, the villagers' names
	input::ShortcutKeys _shortcutKeys;
	std::unique_ptr<gui::GameInterface> _interface;
	/// The screens drawn as the game starts, none without a window
	std::unique_ptr<gui::LoadingScreen> _loadingScreen;
	/// The window was closed during the start-up screens: the rest of them are left out
	bool _startupClosing {false};
	/// What is drawn at the loading's redraw points, nothing while not loading
	std::optional<loading::LoadingClock::Mode> _loadingLook;
	/// The tips screen has faded in once: later loads show it at once
	bool _tipFadedIn {false};
	bool _menuWasOpen {false};
	/// Whether the open menu paused the game, which closing it gives back: not inside the temple, which holds the pause
	bool _menuPaused {false};
	/// When the escape menu last closed (SDL_GetTicks), for Escape's 300 ms debounce
	uint32_t _menuClosedTicks {0xFFFFFFFFu}; ///< No debounce before the first close

	/// path to Lionhead Studios Ltd/Black & White folder
	const std::filesystem::path _gamePath;

	std::filesystem::path _startMap;
	/// --pre-intro: the pre-intro film plays at start-up, as on a first run
	bool _preIntro {false};
	/// --skip-logos: no logo pictures at start-up
	bool _skipLogos {false};
	/// Started on the testbed (--testbed or --scenario): no map script and no story
	bool _startTestbed {false};
	/// The scenario asked for on the command line, until the scenarios' window takes it as the game starts
	std::optional<ScenarioRequest> _scenarioRequest;
	/// --seed: the run's seed, and the date pinned
	std::optional<uint32_t> _seed;
	/// --skip-opening: the start-of-game question answered at once (no SkipBox, no pause), or asked as always
	std::optional<new_game_choice::NewGameStart> _newGameStart;
	/// The land run of a --scenarios list this game plays, taken from its request as its land loads (LoadStartLand);
	/// none in any other game
	std::optional<LandRun> _landRun;
	/// OPENBLACK_TEST_MENU_AT's frame, read once as the program makes the Game (nothing changes the variable while it
	/// runs): every game opens the escape menu at that frame of its own run, unless its land run gives its own menu=
	const std::optional<uint32_t> _testMenuAt;
	/// --scenarios: the scenarios this launch runs one after the other, the first included
	std::vector<ScenarioListEntry> _scenarioList;
	/// (openblack) whether a land was loaded already: only the first one keeps the start-up seeds (game_random)
	bool _firstMapLoaded {false};

	std::chrono::steady_clock::time_point _lastGameLoopTime;
	std::chrono::steady_clock::duration _turnDeltaTime;
	/// The real mouse buttons and motion the hand reads (Input/MouseButtons)
	input::MouseButtonsState _mouseButtons;
	/// The dialogue's click (help::dialogue_click) since the help system's last interface pass
	bool _dialogueClick {false};
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
	/// The configuration as the command line made it, which a new game starts from
	EngineConfig _startConfig;
	/// With --scenarios: the debug windows' settings file as the launch found it, which every new game's debug GUI
	/// starts from, as a new launch would
	std::optional<std::string> _debugGuiStartSettings;
	/// The C runtime's seed right before the program's first land, once it is kept: a new game's start-up draws the
	/// same, so it is back at this seed before its own first land
	std::optional<uint32_t> _startCrtSeed;
};
} // namespace openblack
