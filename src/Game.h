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
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <glm/mat4x4.hpp>
#include <spdlog/common.h>

#include "EngineConfig.h"
#include "GameClock.h"
#include "Windowing/WindowingInterface.h" // For DisplayMode

union SDL_Event;

namespace openblack
{

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
	bool vsync;
	uint8_t detailLevel {4};
	/// --mod values (and the older switches that stand for mods), applied after the mods' settings.cfg files
	std::vector<std::string> modArguments;
	openblack::windowing::DisplayMode displayMode;
	GraphicsBackend graphicsBackend;
	std::string gamePath;
	float guiScale;
	uint32_t numFramesToSimulate;
	std::string logFile;
	std::array<spdlog::level::level_enum, k_LoggingSubsystemStrs.size()> logLevels;
	std::string startLevel;
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> requestScreenshot;
};

class DayNightClock;
class ScreenFade;

/// Values the map script sets that live in g_game or in statics of runblack.exe (only data so far)
struct MapScriptGlobals
{
	static constexpr size_t k_MagicCount = 42;

	/// VERSION (0x716FF9 -> 0xD9957C); CREATE_FLOCK reads its town from N5 from 2.1 on, else from N4
	float version {0.0f};
	/// SET_LAND_NUMBER (0x7177A4): g_game+0x205A08, 0 in the GGame ctor
	int32_t landNumber {0};
	/// SET_TOWN_INFLUENCE_MULTIPLIER / SET_PLAYER_INFLUENCE_MULTIPLIER: g_game+0x250078 / +0x25007C, back to 1 in
	/// GGame::Init before the map script runs (read by Town::Process and Citadel::GetInfluence)
	float townInfluenceMultiplier {1.0f};
	float playerInfluenceMultiplier {1.0f};
	/// FIRE_FLY_SPELL_REWARD_PROB (0x717998 -> 0x52B630): 0xCCFBAC, by magic type (GMagicInfo::GetInfoFromText 0x5FB3B0,
	/// the first magic effect whose name matches without case; an unknown name gives 42 and is dropped). Not reset
	/// between lands.
	std::array<float, k_MagicCount> fireFlySpellRewardProbability {};
	/// 0xCCFB04: the running sums of the table above, remade on every change
	std::array<float, k_MagicCount> fireFlySpellRewardCumulative {};
};

/// The tutorial-skip bits of the GGame flags word g_game+0x14, cleared at every new game by
/// GGame::DoYesNoSkipTutorialRequestersIfNecessary (0x54CBD0, called by GGame::OnNewGame 0x55395B) and set by the
/// answer to its SkipBox (callback 0x544480); read by the scripts through CAN_SKIP_TUTORIAL (bit 23, 0x6FFEF0),
/// CAN_SKIP_CREATURE_TRAINING (bit 24, 0x6FFF10) and IS_KEEPING_OLD_CREATURE (bit 25, 0x6FFF30)
struct TutorialSkipFlags
{
	bool canSkipTutorial {false};
	bool canSkipCreatureTraining {false};
	bool isKeepingOldCreature {false};
};

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
	bool Initialize() noexcept;
	bool Run() noexcept;

	bool LoadMap(const std::filesystem::path& path) noexcept;
	void LoadLandscape(const std::filesystem::path& path);

	void SetTime(float time) noexcept;
	/// The turn length multiplier (2 = slow, 0.5 = fast): GGame::SetSpeed 0x5537F0 with the speed-up factor 1 / it
	void SetGameSpeed(float multiplier) { game_clock::SetSpeed(1.0f / multiplier); }
	[[nodiscard]] float GetGameSpeed() const { return 1.0f / game_clock::Speed(); }

	/// g_game +0x205A40 (game_clock::Turn): it goes up at the start of the turn
	[[nodiscard]] uint32_t GetTurn() const { return game_clock::Turn(); }
	[[nodiscard]] bool IsPaused() const { return game_clock::IsPaused(); }
	/// The wall clock time between the last two turns (debug view only)
	[[nodiscard]] std::chrono::duration<float, std::milli> GetDeltaTime() const { return _turnDeltaTime; }
	/// How far the current game turn is, 0..0.99 (g_game +0x205D64, game_clock::TurnFraction): kept while paused
	[[nodiscard]] float GetTurnFraction() const { return game_clock::TurnFraction(); }
	[[nodiscard]] const glm::ivec2& GetMousePosition() const { return _mousePosition; }

	void RequestScreenshot(const std::filesystem::path& path) noexcept;
	/// OPENBLACK_TEST_TEXT_SHOT (openblack only): when (SDL ticks) to take the screenshot, and where
	std::optional<uint32_t> _textShotAtMs;
	std::string _textShotPath;

	/// Script fade and cinema bars (SET_FADE, SET_WIDESCREEN)
	[[nodiscard]] ScreenFade& GetScreenFade() { return *_screenFade; }
	/// The original's day/night clock (GLandAlignement::UpdateTime)
	[[nodiscard]] DayNightClock& GetDayNightClock() { return *_dayNightClock; }
	/// Globals set by the map script (VERSION, SET_LAND_NUMBER, influence multipliers, firefly rewards)
	[[nodiscard]] MapScriptGlobals& GetMapScriptGlobals() { return _mapScriptGlobals; }
	/// The tutorial-skip bits of g_game+0x14 (CAN_SKIP_TUTORIAL, CAN_SKIP_CREATURE_TRAINING, IS_KEEPING_OLD_CREATURE)
	[[nodiscard]] const TutorialSkipFlags& GetTutorialSkipFlags() const { return _tutorialSkipFlags; }

	static Game* Instance() { return sInstance; }

private:
	static Game* sInstance;

	/// path to Lionhead Studios Ltd/Black & White folder
	const std::filesystem::path _gamePath;

	std::filesystem::path _startMap;
	/// (openblack) whether a land was loaded already: only the first one keeps GGame::Init's seeds (game_random)
	bool _firstMapLoaded {false};

	std::chrono::steady_clock::time_point _lastGameLoopTime;
	std::chrono::steady_clock::duration _turnDeltaTime;
	uint32_t _frameCount {0};
	glm::ivec2 _mousePosition;
	bool _handAction {false};
	bool _handGripping;
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> _requestScreenshot;
	std::unique_ptr<ScreenFade> _screenFade;
	std::unique_ptr<DayNightClock> _dayNightClock;
	MapScriptGlobals _mapScriptGlobals;
	TutorialSkipFlags _tutorialSkipFlags;
};
} // namespace openblack
