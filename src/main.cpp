/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

#include <SDL_messagebox.h>
#include <cxxopts.hpp>

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
// clang-format on
#endif

#include "Common/CrashHandler.h"
#include "Debug/TestbedOptions.h"
#include "EngineConfig.h"
#include "Game.h"
#include "GamePath.h"
#include "Graphics/DetailLevel.h"
#include "LaunchOptions.h"

bool parseOptions(int argc, char** argv, openblack::Arguments& args, int& returnCode)
{
	cxxopts::Options options("openblack", "Open source reimplementation of the game Black & White (2001).");

	const std::string defaultLogFile =
#if defined(OPENBLACK_DEBUG) || defined(__EMSCRIPTEN__)
	    "stdout";
#else
	    "openblack.log";
#endif

	std::string loggingSubsystems = "all";
	for (const auto& system : openblack::k_LoggingSubsystemStrs)
	{
		loggingSubsystems += std::string(", ") + system.data();
	}

	// clang-format off
	options.add_options()
		("h,help", "Display this help message.")
		("g,game-path", "Path to the Data/ and Scripts/ directories of the original Black & White game. (Required)", cxxopts::value<std::string>())
		("W,width", "Window resolution in the x axis (0: fit the desktop).", cxxopts::value<uint16_t>()->default_value("0"))
		("H,height", "Window resolution in the y axis (0: fit the desktop).", cxxopts::value<uint16_t>()->default_value("0"))
		("u,ui-scale", "Scaling of the GUI", cxxopts::value<float>()->default_value("1.0"))
		("s,start-level", "Level that is loaded at start-up", cxxopts::value<std::string>()->default_value(std::string(openblack::k_DefaultStartLevel)))
		("testbed", "Start on the flat testbed, a plane with a lake, instead of a level.")
		("creature-file", "The profile's creature: a mind file in Scripts/CreatureMind, loaded when a land asks for the player's creature. Empty for none.",
		    cxxopts::value<std::string>()->default_value(std::string(openblack::k_DefaultProfileCreatureFile)))
		("profile-folder", "Keep the player profiles in this folder, as a player's are, whatever the run (an empty folder is a first start). Without it a player's are kept in the working directory's Profiles folder, and a run of the checks keeps none.",
		    cxxopts::value<std::filesystem::path>())
		("replay-input", "(Test runs) Play the raw input recorded in this file (--record-input) from the first frame, in place of the real mouse and keys; run with OPENBLACK_IGNORE_REAL_INPUT=1 and a fixed frame time for the same run every time.", cxxopts::value<std::filesystem::path>())
		("record-input", "(Test runs) Record the raw input, the mouse and the keys frame by frame, from the first frame to this file, written as the game ends.", cxxopts::value<std::filesystem::path>())
		("no-vsync", "Draw frames as fast as possible instead of waiting for the screen's refresh, as the original does by default.")
		("detail-level", "Graphics detail level of the original, 0..6 (4 is the original's default, 5 custom, 6 top, where openblack starts).", cxxopts::value<uint16_t>()->default_value(std::to_string(openblack::graphics::detail_level::k_Start)))
		("m,window-mode", "Which mode to run window.", cxxopts::value<std::string>()->default_value("windowed"))
		("b,backend-type", "Which backend to use for rendering.", cxxopts::value<std::string>())
		("n,num-frames-to-simulate", "Number of frames to simulate before quitting.", cxxopts::value<uint32_t>()->default_value("0"))
		("l,log-file", "Output file for logs, 'stdout'/'logcat' for terminal output.", cxxopts::value<std::string>()->default_value(defaultLogFile))
		("L,log-level", "Level (trace, debug, info, warning, error, critical, off) of logging per subsystem (" + loggingSubsystems + ").",
		    cxxopts::value<std::vector<std::string>>()->default_value("all=debug"))
		("screenshot-frame", "Request a screenshot of the backbuffer at a certain frame number.", cxxopts::value<uint32_t>())
		("screenshot-path", "Path of the request a screenshot of the backbuffer.", cxxopts::value<std::filesystem::path>()->default_value("screenshot.png"))
		("crash-dialogs", "Show the system's and C runtime's crash dialogs (Abort/Retry/Ignore) instead of writing a crash report to crashes/ and exiting.")
		("seed", "Start every random number openblack's own generators draw from this seed and pin the date the game reads (1 January 2001), so that two runs of the same scenario with a fixed frame time and the same input go the same way.", cxxopts::value<uint32_t>())
		("skip-opening", "For developers: start the new game with the start-of-game question answered, the story then going as it does for that answer: creature (straight to choosing a creature, the opening skipped), story (all of the first land's creature tutorial skipped too), old (keep the old creature), normal; or ask, to be asked it whatever the player profiles.", cxxopts::value<std::string>())
		("frame-stats", "Log the average and 95th percentile frame time and the profiler stages every so many frames (0 for never).", cxxopts::value<uint32_t>()->default_value("0"))
		("frame-stats-views", "With --frame-stats, also profile and log the GPU time of each render view.")
		("pre-intro", "Play the pre-intro film at start-up, as the game does on a first run.")
		("skip-logos", "Leave out the logo pictures at start-up.")
		("skip-loading-screens", "Draw no loading screen (the tips screen and the please-wait banner): the lands load and the tip is picked as with them.")
		("scenario", "Start on the testbed and run the testbed scenario of this id, such as benchmark.creatures_100.", cxxopts::value<std::string>())
		("benchmark-warmup", "With --scenario, the frames a benchmark's crowd settles for once spawned, before it is measured.", cxxopts::value<uint32_t>()->default_value("120"))
		("benchmark-frames", "With --scenario, the frames of a benchmark measured, after which the game quits.", cxxopts::value<uint32_t>()->default_value("600"))
		("benchmark-out", "With --scenario, where a benchmark writes its results (with .json and .csv after it); nothing is written without it.", cxxopts::value<std::string>())
		("scenarios", "Run these testbed scenarios one after the other in one launch, each a new game as a new launch starts it: id[:frames],... (-n's frames by default). An entry land:<file>:<frames>[:key=value]... is a land instead, with its test options (demo=<name>, skip=<answer>/<frame>, menu=<frame>, citadel=<in>[/<out>]). Each takes its state hash, and a screenshot 20 frames before its end.", cxxopts::value<std::string>())
		("scenarios-out", "With --scenarios, where each entry's state hash and screenshot go: <this><name>_hash.txt and <this><name>.png (a land entry's name: its demo, else its land).", cxxopts::value<std::string>()->default_value(""))
	;
	// clang-format on

	try
	{
		auto result = options.parse(argc, argv);
		if (result["help"].as<bool>())
		{
			std::cout << options.help() << std::endl;
			returnCode = EXIT_SUCCESS;
			return false;
		}

		// pick a sane renderer based on the user os
		openblack::GraphicsBackend graphicsBackend = openblack::launch_options::DefaultBackend();

		// allow user to specify a renderer
		if (result.count("backend-type") != 0)
		{
			const auto named = openblack::launch_options::BackendNamed(result["backend-type"].as<std::string>());
			if (!named.has_value())
			{
				throw cxxopts::exceptions::no_such_option(result["backend-type"].as<std::string>());
			}
			graphicsBackend = *named;
		}

		const auto namedMode = openblack::launch_options::DisplayModeNamed(result["window-mode"].as<std::string>());
		if (!namedMode.has_value())
		{
			throw cxxopts::exceptions::no_such_option(result["window-mode"].as<std::string>());
		}
		const openblack::windowing::DisplayMode displayMode = *namedMode;

		std::array<spdlog::level::level_enum, openblack::k_LoggingSubsystemStrs.size()> logLevels;
		{
			std::map<std::string, spdlog::level::level_enum> logLevelMap;
			logLevelMap.insert_or_assign("all", spdlog::level::debug);
			for (const auto& levelStr : result["log-level"].as<std::vector<std::string>>())
			{
				const auto delim = levelStr.find_first_of('=');
				const auto key = delim == std::string::npos ? "all" : levelStr.substr(0, delim);
				const auto value = spdlog::level::from_str(delim == std::string::npos ? levelStr : levelStr.substr(delim + 1));
				logLevelMap.insert_or_assign(key, value);
			}

			const auto all = logLevelMap["all"];
			for (auto& level : logLevels)
			{
				level = all;
			}
			// TODO (#749) use std::views::enumerate
			for (size_t i = 0; const auto& str : openblack::k_LoggingSubsystemStrs)
			{
				const auto iter = logLevelMap.find(str.data());
				if (iter != logLevelMap.cend())
				{
					logLevels.at(i) = iter->second;
				}
				++i;
			}
		}

		args.executablePath = argv[0];
		const auto commandLine =
		    result.count("game-path") != 0 ? std::optional(result["game-path"].as<std::string>()) : std::nullopt;
		const openblack::game_path::RegistryReader registry = []() -> std::optional<std::string> {
#ifdef _WIN32
			// if we're on windows we can find the install path
			DWORD dataLen = 0;
			LSTATUS status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir",
			                             RRF_RT_REG_SZ, nullptr, nullptr, &dataLen);
			if (status == ERROR_SUCCESS)
			{
				// Not zero-filled, like the old new char[]
				auto path = std::make_unique_for_overwrite<char[]>(dataLen);
				status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir",
				                     RRF_RT_REG_SZ, nullptr, path.get(), &dataLen);

				return std::string(path.get());
			}
#endif
			return std::nullopt;
		};
		const auto gamePath = openblack::game_path::Choose(commandLine, registry);
		if (!gamePath.has_value())
		{
			throw cxxopts::exceptions::option_has_no_value("game-path");
		}
		args.gamePath = *gamePath;

		if (result.count("seed") != 0)
		{
			args.seed = result["seed"].as<uint32_t>();
		}
		if (result.count("skip-opening") != 0)
		{
			const auto text = result["skip-opening"].as<std::string>();
			args.newGameStart = openblack::new_game_choice::ParseNewGameStart(text);
			if (!args.newGameStart.has_value())
			{
				throw cxxopts::exceptions::incorrect_argument_type(text);
			}
		}

		if (result.count("screenshot-frame") != 0)
		{
			args.requestScreenshot = std::make_pair(result["screenshot-frame"].as<uint32_t>(),
			                                        result["screenshot-path"].as<std::filesystem::path>());
		}

		args.windowWidth = result["width"].as<uint16_t>();
		args.windowHeight = result["height"].as<uint16_t>();
		args.guiScale = result["ui-scale"].as<float>();
		args.vsync = !result["no-vsync"].as<bool>();
		args.detailLevel = static_cast<uint8_t>(std::min<uint16_t>(result["detail-level"].as<uint16_t>(), 6));
		args.displayMode = displayMode;
		args.graphicsBackend = graphicsBackend;
		args.numFramesToSimulate = result["num-frames-to-simulate"].as<uint32_t>();
		args.logFile = result["log-file"].as<std::string>();
		args.logLevels = logLevels;
		args.startLevel = result["start-level"].as<std::string>();
		args.startTestbed = result.count("testbed") != 0;
		args.creatureFile = result["creature-file"].as<std::string>();
		if (result.count("profile-folder") != 0)
		{
			args.profileFolder = result["profile-folder"].as<std::filesystem::path>();
		}
		if (result.count("replay-input") != 0)
		{
			args.replayInput = result["replay-input"].as<std::filesystem::path>();
		}
		if (result.count("record-input") != 0)
		{
			args.recordInput = result["record-input"].as<std::filesystem::path>();
		}
		args.frameStatsInterval = result["frame-stats"].as<uint32_t>();
		args.frameStatsViews = result.count("frame-stats-views") != 0;
		args.preIntro = result.count("pre-intro") != 0;
		args.skipLogos = result.count("skip-logos") != 0;
		args.skipLoadingScreens = result.count("skip-loading-screens") != 0;
		args.scenario = openblack::testbed_scenarios::MakeScenarioRequest(
		    result.count("scenario") != 0 ? std::optional(result["scenario"].as<std::string>()) : std::nullopt,
		    result["benchmark-warmup"].as<uint32_t>(), result["benchmark-frames"].as<uint32_t>(),
		    result.count("benchmark-out") != 0 ? std::optional(result["benchmark-out"].as<std::string>()) : std::nullopt);
		if (result.count("scenarios") != 0)
		{
			const auto& text = result["scenarios"].as<std::string>();
			// one scenario, or a list: the list's entries take their own frames and screenshots
			if (result.count("scenario") != 0 || result.count("screenshot-frame") != 0)
			{
				throw cxxopts::exceptions::incorrect_argument_type(
				    text + " (--scenarios goes without --scenario and --screenshot-frame)");
			}
			std::string error;
			auto list = openblack::testbed_scenarios::MakeScenarioList(
			    text, args.numFramesToSimulate, result["scenarios-out"].as<std::string>(),
			    result["benchmark-warmup"].as<uint32_t>(), result["benchmark-frames"].as<uint32_t>(),
			    result.count("benchmark-out") != 0 ? std::optional(result["benchmark-out"].as<std::string>()) : std::nullopt,
			    &error);
			if (!list.has_value())
			{
				throw cxxopts::exceptions::incorrect_argument_type(text + " (" + error + ")");
			}
			// the first entry starts as --scenario starts the game; the next ones start as new games (Game::Run)
			args.scenario = list->front().request;
			args.numFramesToSimulate = list->front().frames;
			args.requestScreenshot = list->front().screenshot;
			args.scenarioList = std::move(*list);
		}
	}
	catch (cxxopts::exceptions::parsing& err)
	{
		std::cerr << err.what() << std::endl;
		std::cerr << options.help() << std::endl;

		returnCode = EXIT_FAILURE;
		return false;
	}

	return true;
}

int main(int argc, char** argv) noexcept
{
	const bool crashDialogs = openblack::crash_handler::WantsCrashDialogs(std::span(argv, static_cast<size_t>(argc)));
	if (!crashDialogs)
	{
		openblack::crash_handler::Install();
	}

	// clang-format off
	std::cout <<
	    "==============================================================================\n"
	    "   openblack - A modern reimplementation of Lionhead's Black & White (2001)   \n"
	    "==============================================================================\n"
	    "\n";
	// clang-format on

	try
	{
		openblack::Arguments args;
		int returnCode = EXIT_FAILURE;
		if (!parseOptions(argc, argv, args, returnCode))
		{
			return returnCode;
		}
		openblack::crash_handler::SetLogFile(args.logFile);
		auto game = std::make_unique<openblack::Game>(std::move(args));
		if (!game->Initialize())
		{
			return EXIT_FAILURE;
		}
		if (!game->Run())
		{
			return EXIT_FAILURE;
		}
		game.reset();
	}
	catch (std::exception& e)
	{
		if (!crashDialogs)
		{
			openblack::crash_handler::ReportFatal(openblack::crash_report::CrashKind::UncaughtException, e.what(), {}, 0,
			                                      typeid(e).name());
		}
		std::cerr << e.what() << std::endl;
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", e.what(), nullptr);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#if defined(_WIN32) && !defined(_CONSOLE)
int WINAPI WinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                   [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nShowCmd)
{
	return main(__argc, __argv);
}
#endif
