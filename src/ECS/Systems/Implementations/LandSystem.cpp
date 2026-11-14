/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LandSystem.h"

#include <chrono>
#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/NightLights.h"
#include "3D/PlayerSymbolSheet.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/SpookyVoices.h"
#include "Camera/Camera.h"
#include "Camera/CameraHelp.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameRandom.h"
#include "ECS/AnimalAI.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureLoop.h"
#include "ECS/DesignedScenery.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Events/DrawListEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/Fields.h"
#include "ECS/FireFlies.h"
#include "ECS/Footpaths.h"
#include "ECS/LeashHandTie.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SaveTriggers.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/Wonders.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerSoul.h"
#include "EngineConfig.h"
#include "Game/GameStats.h"
#include "GameClock.h"
#include "GameServices.h"
#include "Graphics/RendererInterface.h"
#include "Help/HelpProfile.h"
#include "Help/ToolTips.h"
#include "Input/GamePackets.h"
#include "LandBalance.h"
#include "LandLoad.h"
#include "LandLoadTemple.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicLoop.h"
#include "Video/VideoPlayer.h"
#include "Windowing/WindowingInterface.h"
#include "Worship/TempleLeash.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace std::chrono_literals;

void LandSystem::PrepareNewLand(bool playgroundStart, land_load::Resets resets)
{
	// the map's clear runs from here to the end of the registry's reset
	_clearingMap = resets.mapClear;
	// A new land keeps nothing of the last load's world: a load keeps its own again once its objects are made
	if (Locator::saveGame::has_value())
	{
		Locator::saveGame::value().ForgetLoadedWorld();
	}
	// A land loaded while the player is inside the temple takes them out of it first, rooms and all, but for a script's
	// change of land, which leaves them inside
	if (Locator::temple::has_value())
	{
		land_load::TempleBeforeLoad(Locator::temple::value(), resets);
	}
	// The players keep what is theirs rather than the land's before anything of the land goes
	Locator::playerSystem::value().KeepForNextLand();
	// and so do the computer players' minds: their trees, weights and attitudes last the whole game
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().KeepForNextLand();
	}
	// the map's clear: every player's game statistics back to their start, the histories kept
	game_stats::ClearAll();
	// and nothing of a skirmish's end is decided on the new land
	if (Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().Reset();
	}
	// and the camera features back to all but the self-tilting camera, with the self-tilt's start angle and height
	camera_help::Reset();
	// the map's clear: the help profile's clock starts again from 0, its counts kept; and the camera's fence and height
	// limits go, with the places to keep out of that go with the map
	if (resets.mapClear)
	{
		help_profile::OnClearMap();
		if (Locator::cameraZoneSystem::has_value())
		{
			Locator::cameraZoneSystem::value().ClearMap();
		}
	}
	Locator::particleSystem::value().Reset();
	TheMagicSystem().Reset();
	// a new game makes the player's interface again: its gestures' state and its last seed type go; a land change keeps
	// them
	if (resets.newGame)
	{
		magic::OnNewGame();
	}
	// the map's clear: the session's packets go; those the interface sent since the last flush wait for the next one,
	// and reach the new land (an object of the last land as none). Then the hand's part of the clear; a new game also
	// makes the interface status again (its synced hand and turn motion)
	game_packets::ClearSession();
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().ResetTurnState(resets.newGame);
		// then the map's clear runs the interface status's pass once, with the land's things deleted: a held object
		// leaves the hand with its out-of-hand calls before the registry goes
		if (resets.mapClear)
		{
			Locator::handSystem::value().ProcessStatusAtClearMap(resets.newGame);
		}
	}
	// every land balance value back to 1 before the script
	land_balance::Reset();
	// the map's clear: the object creation counter back to 0 (2 on the first land: two HelpSpirits)
	ecs::object_index::OnLoadMap();
	// and the game loop's passes, only with a clear: the campaign's first land, which the game's start loads with no
	// clear, keeps the passes counted since the clock started, its start-up menu's included, as the original's loop
	// counts them from its first pass
	if (resets.mapClear)
	{
		game_clock::SetFrameCount(0);
	}
	// the map's clear: the script highlights (after the bookmarks)
	ecs::script_highlight::OnClearMap();
	// the map's clear: the camera help icons by the hand faded out, the rotate arrow unturned and the red cross no longer
	// held back
	if (resets.mapClear)
	{
		View().RestartHandIcons();
	}
	// the map's clear: the tooltip and its timers back to their start values; the times each text was shown stay
	help::tooltips::Reset();
	// the game's start puts both random seeds at their start value once, before the first land of a new campaign (start
	// mode 1: the map script loaded, no map clear); the map's clear puts them at 0 for every later LOAD_MAP (a
	// playground game start) and for the skirmish playground (start mode 4).
	// (inferred) openblack has no start mode: "a script in Playgrounds" stands for mode 4
	if (!playgroundStart)
	{
		game_random::Init();
	}
	// a new game then makes the eight players' interfaces, each starting its guidance with draws of the local stream
	// (a playground's are made before the map's clear puts the seeds at 0, so its draws are gone with them)
	if (resets.newGame)
	{
		audio::guidance::StartAtNewGame();
	}
	if (playgroundStart)
	{
		game_random::Reset();
	}
	// the map's clear puts the turns back to 0 with the seeds, before the land's script: what the script makes is not
	// made in play. The turn of the last autosave is kept
	game_clock::SetTurn(land_load::TurnBeforeLandScript(resets, game_clock::Turn()));
	// a new game starts in no sequence mode; a land change keeps the one it is in (the citadel's for a player the script's
	// change of land left inside the temple)
	if (resets.newGame)
	{
		game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	}
	// the interact desire back to 0 before the land's feature script; the land number and the influence multipliers
	// keep the last land's values until its script sets them
	ecs::components::ResetForLandLoad(Locator::mapScriptSystem::value().Globals());
	// a new game: the day / night clock's default cycle at noon, running; the Land script may change it (SET_NIGHTTIME).
	// A land change keeps the clock as it was: its time, its cycle, whether it runs and a move of the time going on
	if (resets.newGame)
	{
		TheDayNightClock().Reset();
	}
	Locator::skySystem::value().SetTime(TheDayNightClock().GetScriptTime());
	ecs::ClearFireFlies();
	ecs::ClearForests();
	ecs::animal_ai::ClearReactions();
	ecs::DisappearSmoke::Clear();
	ecs::object_ghosts::Clear();               // the objects' ghosts go with the map
	Locator::explosionSystem::value().Reset(); // the marks on the ground, with the map's clear
	// the creatures' footprints read the date for the new land; the last land's prints stay until they fade
	ecs::creature_loop::OnLoadMap();
	night_lights::Clear();
	// the audio's reset with the map's SoundTags and street lanterns, before the registry reset (it destroys the
	// lanterns' emitters without freeing their sources)
	audio::ClearMap();
	ecs::wonders::RegisterStateHash();
	magic::players::RegisterStateHash();
	ecs::town_queries::RegisterStateHash();
	ecs::footpaths::RegisterStateHash();
	ecs::player_creature::RegisterStateHash();
	ecs::town_belief::RegisterStateHash();
	// the camera's modes go with the map (no script mode); the lens is kept, unless the camera is made again (a new
	// game: 70 degrees, its start value)
	if (resets.newGame)
	{
		script_camera::Reset();
	}
	else
	{
		script_camera::ClearModes();
	}
	// the map's clear and the script reboot: every SuperVillager released
	ecs::super_villager::ReleaseAll();
	// the map's clear (with a temple) and the scripts' restart: the challenge records let go of the things they hold,
	// and go
	ecs::challenge_records::Clear(Locator::entitiesRegistry::value(), {.remove = [](entt::entity thing) {
		                              if (static_cast<uint32_t>(thing) != 0)
		                              {
			                              TheScriptObjects().RemoveReference(thing);
		                              }
	                              }});
	// the map's clear and the scripts' restart: the bookmarks on
	if (Locator::cameraBookmarkSystem::has_value())
	{
		Locator::cameraBookmarkSystem::value().SetEnabled(true);
	}
	// a new game: no film goes on into it (the old game's end deletes the film; a land change and the scripts' restart
	// leave it)
	if (resets.newGame && video::Get().IsPlaying())
	{
		video::Get().Stop();
	}
	// a new game or a playground's start restarts the scripts; a land change keeps what they set
	if (resets.scripts)
	{
		View().ResetScripts();
	}
	// and with them their whole program starts again, last of the restart
	if (resets.program)
	{
		View().RestartScriptProgram();
	}
	ecs::designed_scenery::OnLoadMap();
}

void LandSystem::ResetRegistryForNewLand()
{
	// Reset everything. Deletes all entities and their components. The field / fish farm deletion listeners first:
	// clearing the registry must not send their workers to 163 (fields::DisconnectDeletionListeners). The temple leash
	// posts' one too: the clear takes the posts with everything else
	ecs::fields::DisconnectDeletionListeners();
	worship::temple_leash::DisconnectDeletionListener();
	// the artefacts' one as well, and those whose objects went before their towns' turn are forgotten with the land
	if (Locator::artefactSystem::has_value())
	{
		Locator::artefactSystem::value().ClearMap();
	}
	// what the player last clicked outlives the land: the new hand takes it on
	std::optional<ecs::components::HandClicked> clicked;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const ecs::components::HandClicked>([&clicked](const ecs::components::HandClicked& kept) { clicked = kept; });
	// the map's clear asks for one draw-list rebuild before the objects go, and again once the list is emptied
	ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
	// the tattoo editor closes as it is, before its creature goes with everything else
	if (Locator::tattooEditorSystem::has_value())
	{
		Locator::tattooEditorSystem::value().Close();
	}
	// The sky goes on as it was: it is kept while every entity goes, and made again first, as it was first made
	TheSkySystem().KeepForNextLand();
	// what waits on the dead list goes first, as the original's map clear drains it
	ecs::ProcessDeadList(true);
	// the creatures' regions of the land outlive it: the game's first land made them, and every later land keeps them
	auto explorationRegions = std::move(Locator::entitiesRegistry::value().Context().explorationRegions);
	// the football matches' last turn too: the game keeps it from land to land
	const auto footballTurn = Locator::entitiesRegistry::value().Context().footballTurn;
	Locator::entitiesRegistry::value().Reset();
	Locator::entitiesRegistry::value().Context().explorationRegions = std::move(explorationRegions);
	Locator::entitiesRegistry::value().Context().footballTurn = footballTurn;
	TheSkySystem().Initialize();
	if (Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().OnClearMap();
	}
	ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
	// the dust puffs are plain data now (not entities): they go with the map (the physics' dust)
	Locator::dynamicsSystem::value().ClearDust();
	// the map's clear: the scripts' hit record forgets the last land's things, all gone with the registry
	Locator::dynamicsSystem::value().ForgetUnavailableHits();
	// the souls' list goes with the land's entities, whose ids the new registry gives out again
	ecs::villager_soul::Clear();
	// TODO(#661): split entities that are permanent from map entities and move hand and camera to init
	// We need a hand for the player
	Locator::handSystem::value().Initialize();
	// and the bands it kept from the last land are drawn on it again
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().OnNewLand();
	}
	if (clicked.has_value())
	{
		const auto hand =
		    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
		Locator::entitiesRegistry::value().Get<ecs::components::HandClicked>(hand) = *clicked;
	}
	// the leash picked at a temple's posts reaches the next turn as a packet, handled with the temple's leash
	worship::temple_leash::RegisterPacketHandler();
	// the hand's leash tie and the taps with the leash in the hand, handled with the creature's leash
	ecs::leash_tie::RegisterPacketHandlers();
	// the quick save, sent by its key and applied at the next turn
	ecs::save_triggers::RegisterPacketHandlers();

	// create our camera
	auto& config = Locator::config::value();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	Locator::camera::value().SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip,
	                                                        config.cameraFarClip);
	_clearingMap = false;
}

void LandSystem::StartNewLand(std::string_view land, bool startsAfresh, bool newGame)
{
	// With the land and its towns ready, the game's first land gives the creatures' map its regions, which every later
	// land keeps (a land that starts afresh makes them again)
	ecs::creature_exploration_land::MakeMap(land, startsAfresh);
	// a new game gives each god the computer plays now its image in the players' symbol sheet, which its sparkles show
	if (newGame)
	{
		player_symbol_sheet::MakeComputerSymbols();
	}
	View().RestartTurnClock();
	// the game speed is kept: a land change leaves it as it was, and the clock's start below keeps its speed
	// a new game from turn 0 (inferred: openblack has no saved games, so this stands for a loaded game too), then the
	// start of the game loop: the timer from 0 and the local game timer reset
	game_clock::SetTurn(0);
	game_clock::OnLoad();
	// a new game's guidance was started with its interfaces (PrepareNewLand); a land change keeps it, and what it
	// remembers of the last land's things matches nothing. The spooky voices take the player's name
	audio::guidance::StartLand(newGame);
	audio::spooky::UpdatePlayerName();
	// The original runs from the first frame; OPENBLACK_START_PAUSED=1 keeps openblack's old paused start (test hook)
	game_clock::Start(std::getenv("OPENBLACK_START_PAUSED") != nullptr);

	// (openblack engine) the draw's lazy loads now, not in the middle of a frame, and the upload of what the land made
	if (Locator::rendererInterface::has_value())
	{
		Locator::rendererInterface::value().PreloadForLand();
	}
}
