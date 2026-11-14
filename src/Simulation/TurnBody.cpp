/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnBody.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/MapCoords.h"
#include "3D/SkyType.h"
#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameRandom.h"
#include "Debug/TraceLog.h"
#include "ECS/AudioQueries.h"
#include "ECS/CreatureLoop.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/FireFlies.h"
#include "ECS/FishShoals.h"
#include "ECS/Footpaths.h"
#include "ECS/LivingTurn.h"
#include "ECS/Map.h"
#include "ECS/MistFades.h"
#include "ECS/OtherPlayerSparkles.h"
#include "ECS/PuzzleGames.h"
#include "ECS/SaveTriggers.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptTimer.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Weather/WeatherLoop.h"
#include "EngineConfig.h"
#include "Game/GameStats.h"
#include "GameClock.h"
#include "GameServices.h"
#include "Help/DialogueText.h"
#include "Help/HelpSystem.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Particles/ParticleManager.h"
#include "Profiler.h"

namespace openblack::simulation
{
namespace
{

/// Where the camera is, for the view's part of a step: the origin when there is a camera
glm::vec3 CameraPosition()
{
	return Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3 {};
}

} // namespace

void PlayTurnStep(game_turn::Step step, TurnViewInterface& view, uint32_t& turn) noexcept
{
	using namespace ecs::components;
	using namespace ecs::systems;
	using game_turn::Step;

	switch (step)
	{
	case Step::PlayerCommands:
	{
		// a skirmish's first turn: the view brings the player's creature from the profile first
		view.PlayViewStep(step, CameraPosition());
		game_packets::DispatchQueuedPackets();
		break;
	}
	case Step::HandInterface:
	{
		if (Locator::handSystem::has_value())
		{
			Locator::handSystem::value().ProcessTurn();
		}
		break;
	}
	case Step::PowerUpInputs:
	{
		TheMagicSystem().ProcessGameInputs();
		break;
	}
	case Step::Clock:
	{
		// the turn number goes up at the start of the turn
		game_clock::StartTurn();
		// the view measures the time since the last turn
		view.PlayViewStep(step, CameraPosition());
		break;
	}
	case Step::MapCells:
	{
		auto& profiler = Locator::profiler::value();
		turn = game_clock::Turn();

		// Build Map Grid Acceleration Structure
		{
			auto mapRebuild = profiler.BeginScoped(Profiler::Stage::TurnMapRebuild);
			Locator::entitiesMap::value().Sync();
		}
		break;
	}
	case Step::ReactionClock:
	{
		// the reactions' clock (the game turn) for the whole turn (ECS/Effects/Reactions)
		ecs::effects::reactions::BeginTurn();
		break;
	}
	case Step::Sharks:
	{

		// the game's turn, call by call in the original's order (docs/wiki/engine/loop.md#the-turn-call-by-call)
		// the sharks' turn
		Locator::sharkSystem::value().ProcessTurn();
		break;
	}
	case Step::TurnStart:
	{
		// the atmosphere, the influence rings, the players (their towns' turn, ECS/Town) and the dances
		// (Magic/MagicLoop.cpp)
		TheMagicSystem().ProcessTurnStart(turn);
		break;
	}
	case Step::Fields:
	{
		// the global game lists: the fields, the fish farms, the walk along the path of the WALK_PATH list, the scripts'
		// mist fades, (not ported) the food piles, the puzzle games, the footpaths being planned
		Locator::fieldSystem::value().ProcessTurn(turn);
		break;
	}
	case Step::FishFarms:
	{
		ecs::ProcessFishFarmsTurn(turn);
		break;
	}
	case Step::WalkPaths:
	{
		Locator::walkPathSystem::value().ProcessTurn();
		break;
	}
	case Step::MistFades:
	{
		ecs::mist_fade::ProcessTurn();
		break;
	}
	case Step::PuzzleGames:
	{
		ecs::ProcessPuzzleGamesTurn();
		break;
	}
	case Step::Footpaths:
	{
		ecs::footpaths::ProcessFootpathFindersTurn();
		break;
	}
	case Step::ForestGrowth:
	{
		// the forests, before the living
		TheMagicSystem().ProcessForests(turn);
		break;
	}
	case Step::Living:
	{
		auto& profiler = Locator::profiler::value();

		// the living: the villagers and the animals in the one living list, each one's turn start (its position kept as the
		// last one), reaction and state, the walk inside the states that walk (ECS/LivingTurn.h)
		{
			auto actions = profiler.BeginScoped(Profiler::Stage::LivingActionUpdate);
			ecs::living_turn::ProcessLiving(TheDayNightClock().GetVisualTime());
			// (approximate) the creatures' turn, after the living list rather than inside it (ECS/CreatureLoop.h)
			ecs::creature_loop::ProcessTurn(profiler);
			// (approximate) the good advisor tells of the player's creature: the messages waiting their turn, and what the
			// help watches of it, after the creatures and before the souls
			if (Locator::creatureHelpSystem::has_value())
			{
				Locator::creatureHelpSystem::value().ProcessTurn();
			}
			// what follows the list on openblack's side: the souls of the dead (LivingActionSystem::Update)
			Locator::livingActionSystem::value().Update();
		}
		break;
	}
	case Step::Miracles:
	{
		auto& profiler = Locator::profiler::value();
		// the fire, the balls, the reactions, the spells (Magic/MagicLoop.cpp)
		{
			auto magicTurn = profiler.BeginScoped(Profiler::Stage::TurnMagic);
			TheMagicSystem().ProcessTurn();
		}
		break;
	}
	case Step::DebugHooks:
	{
		// the particle effects, one step per turn of the turn's length (the test hooks first)
		particles::manager::RunDebugHooks();
		TheMagicSystem().RunDebugHooks();
		break;
	}
	case Step::Particles:
	{
		auto& profiler = Locator::profiler::value();
		{
			auto particlesTurn = profiler.BeginScoped(Profiler::Stage::TurnParticles);
			Locator::particleSystem::value().ProcessTurn();
		}
		break;
	}
	case Step::Fireflies:
	{
		// the fireflies
		ecs::ProcessFireFliesTurn(TheDayNightClock());
		break;
	}
	case Step::Physics:
	{
		auto& profiler = Locator::profiler::value();
		// the physics objects
		{
			auto physicsTurn = profiler.BeginScoped(Profiler::Stage::TurnPhysicsObjects);
			Locator::dynamicsSystem::value().ProcessTurn();
		}
		break;
	}
	case Step::ParticleEnd:
	{
		// the particle system's end of loop: the exploded meshes' queue and the PSys sounds (Magic/MagicLoop.cpp)
		TheMagicSystem().ProcessSpellParticlesEndOfLoop();
		break;
	}
	case Step::Scripts:
	{
		auto& profiler = Locator::profiler::value();

		{
			auto scripts = profiler.BeginScoped(Profiler::Stage::ScriptsUpdate);
			// the scripts
			auto& lhvm = Locator::vm::value();
			// the scripts' turn starts by forgetting a thing of the hit record that is no longer available
			Locator::dynamicsSystem::value().ForgetUnavailableHits();
			// the countdown timer's turn, before the scripts
			ecs::script_countdown::ProcessTurn();
			lhvm.LookIn(lhvm::ScriptType::All);
			// (debug) a pause given here, as a script gives one in its turn
			if (Locator::config::has_value() && std::exchange(Locator::config::value().pauseInNextTurn, false))
			{
				game_clock::Pause(true);
			}
			// the things no script variable holds any more are released
			TheScriptObjects().ReleaseUnreferenced();
			// the script fade, once per turn
			CinematicDirector().ProcessTurn();
			// the help system: the hand state, the input prompt icons, the spirits' turn and the tooltips
			help::Process(chlapi::ScriptVm(), game_clock::Turn());
			// the help profile, after the scripts and the help system: the events of this turn are counted
			// (GET_TOTAL_EVENTS), the next turn may count them again
			TheHelpProfileSystem().ProcessTurn();
			// the day / night clock, a turn of the sky's
			TheSkySystem().ProcessTurn();
			// OPENBLACK_TIME_OF_DAY=<script hour> pins the clock there every turn (screenshots), over the scripts' times
			if (const char* hour = std::getenv("OPENBLACK_TIME_OF_DAY"); hour != nullptr)
			{
				TheDayNightClock().SetScriptTime(std::clamp(static_cast<float>(std::atof(hour)), 0.0f, 24.0f));
			}
			Locator::skySystem::value().SetTime(TheDayNightClock().GetScriptTime());
			if (turn % 50 == 0 && debug_env::TraceLogs(std::getenv("OPENBLACK_CLOCK_TRACE") != nullptr))
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Clock: turn {} visual {:.4f} script {:.4f} sky type {:.3f} frame {:.3f} dome {:.3f}", turn,
				                   TheDayNightClock().GetVisualTime(), TheDayNightClock().GetScriptTime(),
				                   TheDayNightClock().SkyType(), sky_type::Frame(), sky_type::Dome().Built());
			}
			// OPENBLACK_TEST_TEXT_CLICK=1 (openblack only): the player's click on a text that waits for one (RUN_TEXT with
			// interaction 1), every turn while it waits, as the left button let go does (the interface's frame pass);
			// the dialogue's ProcessClick itself ignores the click until the text has been shown long enough
			if (static const bool textClick = std::getenv("OPENBLACK_TEST_TEXT_CLICK") != nullptr; textClick)
			{
				if (auto* dialogue = TheDialogue(); dialogue != nullptr && dialogue->IsWaitingForClick())
				{
					dialogue->ProcessClick(true);
					if (!dialogue->IsWaitingForClick())
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_TEXT_CLICK: click taken at turn {}", turn);
					}
				}
			}
		}
		break;
	}
	case Step::WeatherThings:
	{
		// the weather things
		weather::ProcessWeatherThings();
		break;
	}
	case Step::ScriptHighlights:
	{
		// the bookmarks on a thing follow it, and the camera flies to a followed one while the player's camera has the
		// view; a turn whose camera input counted for the camera help stops the follow
		if (Locator::cameraBookmarkSystem::has_value() && Locator::camera::has_value())
		{
			auto& camera = Locator::camera::value();
			const auto* playersCamera = dynamic_cast<const DefaultWorldCameraModel*>(&camera.GetModel());
			const auto flight = Locator::cameraBookmarkSystem::value().ProcessTurn(
			    playersCamera != nullptr, playersCamera != nullptr && playersCamera->InputCountsForHelp(),
			    JumpFromCamera(camera));
			if (flight.has_value())
			{
				FlyToBookmark(camera, *flight);
			}
		}
		// the script highlights
		ecs::script_highlight::ProcessHighlights();
		break;
	}
	case Step::Climates:
	{
		// the climate (+ the weather's test hooks)
		weather::ProcessClimate();
		break;
	}
	case Step::TownBelief:
	{
		ecs::town_belief::ProcessOncePerTurn(); // the belief
		break;
	}
	case Step::HeldThing:
	{
		// the hand's turn (Magic/MagicLoop.cpp)
		TheMagicSystem().ProcessHandTurn();
		break;
	}
	case Step::DeadList:
	{
		// the other gods' sparkles (ECS/OtherPlayerSparkles.h); the mobile objects' checksum: (not ported)
		ecs::other_player_sparkles::AddForTurn();
		// the dead list's pass (ECS/ToBeDeleted.h)
		ecs::ProcessDeadList(false);
		break;
	}
	case Step::Rewards:
	{
		// the rewards: a chest that landed goes into the map's cells
		Locator::rewardSystem::value().ProcessTurn();
		break;
	}
	case Step::Guidance:
	{
		// the spooky voices, the help sprites' moon phase check, the town desires' sounds (and the interface's heart beat),
		// the confirmations (Audio/Services/Guidance.h)
		audio::guidance::ProcessGameTurn();
		break;
	}
	case Step::InfluenceBorders:
	{
		// the influence circles rebuilt every 10 turns when a radius moved
		// (ECS/Influence/InfluenceCircles.cpp)
		Locator::influenceSystem::value().UpdateBorders();
		break;
	}
	case Step::CameraChecks:
	{
		// the camera's stacked modes are checked: the script camera's things that have gone are dropped, and the placed
		// paths let go of are taken off, the player's camera starting again when the last goes
		script_camera::Validate();
		if (Locator::cameraPathSystem::has_value())
		{
			Locator::cameraPathSystem::value().CheckPaths();
		}
		// a follow whose thing has gone comes off too
		if (Locator::creatureModeSystem::has_value())
		{
			Locator::creatureModeSystem::value().CheckFollow();
		}
		break;
	}
	case Step::BuildingPieces:
	{
		// each fragment's timer, once a turn
		Locator::buildingDamageSystem::value().ProcessTurn();
		break;
	}
	case Step::Sounds:
	{
		// the music mood: (not verified) in audio::ProcessTurn
		// the end of the turn: the sound map, the sound tags (the street lanterns' too), then the audio's turn after turn 5
		// (its music, atmos, channels and listener), the atmos before. No turn runs while paused, so a turn ends paused only
		// when its own inputs or scripts paused the game: its end then silences the ambience instead of the audio's turn
		audio::ProcessTurn();
		break;
	}
	case Step::AudioTestHooks:
	{
		ecs::audio_queries::RunTestHooks(turn); // OPENBLACK_AUDIO_TEST_VIEW / _ANIM / _LANTERN / _CITADEL
		break;
	}
	case Step::AutoSave:
	{
		// the end of the turn's autosave, when the options' autosave is on and the game is not over (ECS/SaveTriggers.h);
		// a run of the checks keeps no profile, so it never writes one
		ecs::save_triggers::EndTurn();
		break;
	}
	case Step::GameOver:
	{
		// then, still at the end of the turn, the game's end for the local player whose temple is being destroyed
		Locator::templeDestructionSystem::value().EndTurn();
		break;
	}
	case Step::ReactionsEnd:
	{
		ecs::effects::reactions::EndTurn();
		break;
	}
	case Step::GlobalStatistics:
	{
		// last of the turn's end, every 10 turns (every 5 when the turn ends paused): the statistics' invented lines of code
		// take a step, drawing a local random number while the game's first land's towns are counted, and the frame rate
		// goes into the most or fewest
		if (turn % game_stats::UpdateInterval(game_clock::IsPaused()) == 0)
		{
			game_stats::AddToTotalLinesOfCodeExecuted(&game_random::LocalRand);
			game_stats::TrackFrameRate(view.FrameRate());
		}
		break;
	}
	case Step::_Count:
		break;
	}
}

} // namespace openblack::simulation
