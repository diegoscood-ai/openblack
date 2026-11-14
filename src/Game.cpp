/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Game.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <LHVM.h>
#include <SDL.h>
#include <SaveSlots.h>
#include <WorldReader.h>
#include <bgfx/bgfx.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/FlatLand.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandBlock.h"
#include "3D/LandData.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/NightLights.h"
#include "3D/OceanInterface.h"
#include "3D/PlayerOwnSymbols.h"
#include "3D/PlayerSymbolSheet.h"
#include "3D/SkyType.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Engine/MusicStream.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"
#include "Audio/MainVolumesSetup.h"
#include "Audio/Services/AtmosBanks.h"
#include "Audio/Services/Confirmation.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/LanternSounds.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Audio/Services/SoundTags.h"
#include "Audio/Services/SpookyVoices.h"
#include "Audio/Services/Voices.h"
#include "Audio/SoundMap.h"
#include "CHLApi.h"
#include "Camera/BookmarkSteps.h"
#include "Camera/Camera.h"
#include "Camera/CameraHelp.h"
#include "Camera/CameraPathControl.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScreenPoint.h"
#include "Camera/ScriptCamera.h"
#include "Camera/StartCamera.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/HelpText.h"
#include "Common/LoadTimer.h"
#include "Common/MachineClock.h"
#include "Common/PlayerProfiles.h"
#include "Common/RandomNumberManager.h"
#include "Common/StringUtils.h"
#include "Creature/CreaturePanelChoice.h"
#include "Creature/CreatureSkin.h"
#include "Creature/CreatureStatusPanel.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FixedClock.h"
#include "Debug/FrameCounter.h"
#include "Debug/FrameStatsLog.h"
#include "Debug/GameHost.h"
#include "Debug/ResourceTrace.h"
#include "Debug/StateHash.h"
#include "Debug/TestbedScenarioRegistry.h"
#include "Debug/TraceLog.h"
#include "ECS/AbodeKnock.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Animations.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/AudioQueries.h"
#include "ECS/CarriedProps.h"
#include "ECS/ChallengePictures.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/FireFlyObject.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureLoop.h"
#include "ECS/CreaturePose.h"
#include "ECS/DesignedScenery.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/DiscipleMarks.h"
#include "ECS/DrawList/DrawGate.h"
#include "ECS/DrawList/FrameInputs.h"
#include "ECS/DrawList/ObjectDraw.h"
#include "ECS/DrawList/RegistryObjects.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/CameraEvents.h"
#include "ECS/Events/DrawListEvents.h"
#include "ECS/Events/LandEvents.h"
#include "ECS/Events/PlaygroundEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/SaveEvents.h"
#include "ECS/Events/ScriptEvents.h"
#include "ECS/Fields.h"
#include "ECS/FireFlies.h"
#include "ECS/FishShoals.h"
#include "ECS/FloatingNumber.h"
#include "ECS/FootballRules.h"
#include "ECS/Footpaths.h"
#include "ECS/GroundMarks.h"
#include "ECS/Influence/Influence.h"
#include "ECS/LeashHandTie.h"
#include "ECS/LivingTurn.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/MistFades.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/OtherPlayerSparkles.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlayerColours.h"
#include "ECS/PlayerCreature.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Rivers.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/SaveTriggers.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedPlayers.h"
#include "ECS/SavedScriptObjects.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/LoadCamera.h"
#include "ECS/SavedWorld/LoadLeashStatus.h"
#include "ECS/SavedWorld/LoadPlayerInterface.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptTimer.h"
#include "ECS/ShowNeeds.h"
#include "ECS/SkirmishGame.h"
#include "ECS/SnowLevels.h"
#include "ECS/Statistics.h"
#include "ECS/SunDraw.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AbodeSoundSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DrawUpdate.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MapCellsSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SavePictureSystemInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/ThingFunctional.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownFeatures.h"
#include "ECS/Town/TownProcess.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/Wonders.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerSoul.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerLabelSource.h"
#include "ECS/VortexSave.h"
#include "ECS/Weather/WeatherLoop.h"
#include "Engine/FrameSnapshot.h"
#include "Engine/GpuCommands.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game/GameStats.h"
#include "Game/SettingsFile.h"
#include "Game/VersionFile.h"
#include "GameClock.h"
#include "GameServices.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GestureIconEffect.h"
#include "Graphics/ObjectOrder.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/SuperVillagerFrame.h"
#include "Gui/BoundControls.h"
#include "Gui/CinemaBars.h"
#include "Gui/ControlBindings.h"
#include "Gui/ControlRebind.h"
#include "Gui/GameInterface.h"
#include "Gui/GameMenu.h"
#include "Gui/LoadingScreen.h"
#include "Gui/SkirmishBox.h"
#include "Gui/StartupRules.h"
#include "Gui/TextDatabase.h"
#include "Hand/HandDemo.h"
#include "Help/AdvisorVoices.h"
#include "Help/Bubble.h"
#include "Help/DialogueClick.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/HelpProfileFile.h"
#include "Help/HelpSystem.h"
#include "Help/InputPromptIcon.h"
#include "Help/InterfaceInteraction.h"
#include "Help/PlayerChoices.h"
#include "Help/ScriptControl.h"
#include "Help/SpeedIndicator.h"
#include "Help/SpiritsRuntime.h"
#include "Help/ToolTips.h"
#include "Input/BookmarkKeys.h"
#include "Input/DoubleClickBox.h"
#include "Input/FixedMouse.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GameCursor.h"
#include "Input/GamePackets.h"
#include "Input/InputReplay.h"
#include "Input/InterfaceActive.h"
#include "Input/MiracleKeys.h"
#include "Input/PlaygroundEscape.h"
#include "Input/PlaygroundKey.h"
#include "Input/RealInput.h"
#include "Input/SpeedKeys.h"
#include "Input/SystemKeyName.h"
#include "Input/TempleRoomKeys.h"
#include "LHScriptX/MapScript.h"
#include "LHScriptX/Script.h"
#include "LandBalance.h"
#include "LandLoad.h"
#include "LandLoadTemple.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Gestures/GestureIconModels.h"
#include "Magic/Gestures/GestureIconSeeds.h"
#include "Magic/Gestures/GestureIcons.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicLoop.h"
#include "ModLoaderStatus.h"
#include "ModSdk/ModLoader.h"
#include "ModSdkHandshake.h"
#include "Parsers/InfoFile.h"
#include "Particles/ParticleManager.h"
#include "Particles/Rules/Forest.h"
#include "Particles/TownBelief.h"
#include "Profiler.h"
#include "Resources/BlobStream.h"
#include "Resources/CreatureFileWriter.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"
#include "Serializer/FotFile.h"
#include "Simulation/FrameBody.h"
#include "Simulation/TurnBody.h"
#include "Sound/GameMusic.h"
#include "Temple/TempleKeys.h"
#include "Temple/TempleTurn.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Visual/DetailLevel.h"
#include "Windowing/WindowAway.h"
#include "Worship/Citadel.h"
#include "Worship/TempleLeash.h"
#include "Worship/WorshipSite.h"
#include "generated/ModSdk/ModSdkVersion.h"

#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

using namespace openblack;
using namespace openblack::lhscriptx;
using namespace std::chrono_literals;

constexpr std::string_view k_WindowTitle = "openblack";

namespace
{
/// The trip to the Gods' Playground once the player says yes to F2's question, through the game's services
class PlaygroundTrip final: public input::playground_key::TripSteps
{
public:
	explicit PlaygroundTrip(Game& game)
	    : _game(game)
	{
	}
	void Unpause() override { game_clock::Pause(false); }
	void LeaveTemple() override
	{
		if (Locator::temple::has_value() && Locator::temple::value().Active())
		{
			Locator::temple::value().Deactivate();
		}
	}
	void Save() override { ecs::save_triggers::PlaygroundTrip(); }
	void StopAllScripts() override
	{
		if (Locator::vm::has_value())
		{
			Locator::vm::value().StopAllTasks();
		}
	}
	void ChangeLand(std::string_view landScript) override
	{
		// the island's land script under the game's Scripts folder, as a map script's land change gives one
		_game.ChangeLand(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / std::filesystem::path(landScript));
	}
	void LessonStarted() override { ecs::events::Publish(ecs::events::PlaygroundLessonStarted {}); }
	void StartScript(std::string_view script) override
	{
		if (Locator::vm::has_value() && Locator::vm::value().StartScript(std::string(script), lhvm::ScriptType::All) == 0)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The Gods' Playground: no script {}", script);
		}
	}

private:
	Game& _game;
};

/// Where the camera starts on the testbed: above and behind the middle of the map
constexpr float k_TestbedCameraHeight = 60.0f;
constexpr float k_TestbedCameraBack = 120.0f;
/// How far the testbed's player's influence reaches from its middle
constexpr float k_TestbedInfluenceRadius = 400.0f;

/// The land a new game starts on: the first LOAD_FEATURE_SCRIPT of the map script (Scripts/Map.txt), as the game
/// reads it before its first land, in the Scripts folder under the name its file has there. None, with the reason
/// logged, when the map script is missing, names no land or names one outside the Scripts folder
std::optional<std::filesystem::path> MapScriptLand(const filesystem::FileSystemInterface& fileSystem)
{
	namespace map_script = lhscriptx::map_script;
	const auto scripts = fileSystem.GetPath<filesystem::Path::Scripts>();
	// the files of the Scripts folder, to find each name whatever the case it is written in
	std::vector<std::string> files;
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(fileSystem.GetPath<filesystem::Path::Scripts>(true), error))
	{
		files.push_back(entry.path().filename().string());
	}
	const auto mapName = map_script::MatchingName(map_script::k_FileName, files);
	if (!mapName.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find the map script {}",
		                    (scripts / map_script::k_FileName).generic_string());
		return std::nullopt;
	}
	const auto mapScript = scripts / *mapName;
	std::vector<std::string> lands;
	try
	{
		const auto& data = resources::LoadBlob(Locator::resources::value().GetBlobs(), mapScript);
		// the game loads each land the map script names in turn; the shipped map script names one
		lands = map_script::FeatureScripts(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not read the map script {}: {}", mapScript.generic_string(), e.what());
		return std::nullopt;
	}
	if (lands.empty())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The map script {} names no land script", mapScript.generic_string());
		return std::nullopt;
	}
	const auto land = map_script::InScriptsFolder(lands.front());
	if (!land.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The map script's land script {} is not in the Scripts folder", lands.front());
		return std::nullopt;
	}
	return scripts / map_script::MatchingName(*land, files).value_or(*land);
}

/// openblack's test hook OPENBLACK_TEST_MENU_AT=<frame>: the frame the escape menu opens at, as strtoul reads it (a
/// value that is not a number is frame 0); none when the variable is not set
std::optional<uint32_t> TestMenuAtFromEnvironment()
{
	if (const char* at = std::getenv("OPENBLACK_TEST_MENU_AT"); at != nullptr)
	{
		return static_cast<uint32_t>(std::strtoul(at, nullptr, 10));
	}
	return std::nullopt;
}

/// The name the game greets the player by without a profile of theirs, as it always has: OPENBLACK_PLAYER_NAME, else the
/// login name, else "Player"
std::u16string GreetingName()
{
	const char* user = std::getenv("OPENBLACK_PLAYER_NAME");
	user = user != nullptr ? user : std::getenv("USERNAME");
	return gui::ToUtf16(user != nullptr ? user : "Player");
}

/// The program's working directory as it starts, none when it cannot be read
std::filesystem::path StartingWorkingFolder() noexcept
{
	std::error_code error;
	auto folder = std::filesystem::current_path(error);
	return error ? std::filesystem::path {} : folder;
}

/// What the audio's music reads from the game (Audio/GameQueries.h); the queries left unset are the systems openblack
/// does not have yet (videos, the wide screen bars moving, the towns' desires, creature, worship)
audio::GameQueries MakeMusicQueries(Game& game)
{
	audio::GameQueries queries;
	queries.landNumber = []() { return Locator::mapScriptSystem::value().Globals().landNumber; };
	// (inferred) openblack's turn counter, back to 0 at each LoadMap, stands for the game's turn
	queries.turn = [&game]() { return game.GetTurn(); };
	queries.camera = []() -> std::optional<audio::CameraState> {
		if (!Locator::camera::has_value())
		{
			return std::nullopt;
		}
		audio::CameraState camera;
		camera.position = Locator::camera::value().GetOrigin();
		// the camera's map position is updated with the camera: x, z in map units (x 6553.6, truncated); the height
		// above ground = y - the altitude byte of the cell (x >> 16, z >> 16) x 0.67, not interpolated; y alone off the
		// map (a cell > 0x1FF) or where no block is
		float ground = 0.0f;
		if (Locator::terrainSystem::has_value())
		{
			const auto& island = Locator::terrainSystem::value();
			const glm::u16vec2 cell {map_coords::CellOf(map_coords::ToFixed(camera.position.x)),
			                         map_coords::CellOf(map_coords::ToFixed(camera.position.z))};
			if (cell.x <= 0x1FF && cell.y <= 0x1FF && island.HasBlockAt(cell))
			{
				ground = static_cast<float>(island.GetCellAltitude(island.GetCell(cell))) * LandIslandInterface::k_HeightUnit;
			}
		}
		camera.heightAboveGround = camera.position.y - ground;
		return camera;
	};
	// an available thing (ecs::IsAvailable) with a Transform; (approximate) its float position for the thing's
	// MapCoords (without their 16.16 rounding)
	queries.thingPosition = [](audio::ThingId thing) -> std::optional<glm::vec3> {
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = static_cast<entt::entity>(thing);
		if (!ecs::IsAvailable(entity))
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
	// a thing works by the rule of its kind
	queries.thingFunctional = [](audio::ThingId thing) {
		return ecs::thing_functional::OfThing(static_cast<entt::entity>(thing));
	};
	// the land's altitude, as a sound tag made at a map position takes it
	queries.landAltitude = [](float x, float z) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
	};
	// a script holds the wide screen (read by the alignment music)
	queries.scriptWideScreen = []() {
		const auto* helpSystem = help::Get();
		return helpSystem != nullptr && helpSystem->IsScriptWideScreen();
	};
	// the guidance level of the help system (0 when it is off); 3 without a help system (its defaults)
	queries.helpLevel = []() {
		const auto* helpSystem = help::Get();
		return helpSystem != nullptr ? helpSystem->GetGuidanceLevel() : 3;
	};
	// game_clock::IsInsideCitadel
	queries.insideCitadel = []() { return game_clock::IsInsideCitadel(); };
	// a skirmish, which the guidance and the fight music ask about
	queries.playgroundGame = []() { return ecs::IsSkirmishGame(); };
	// the local interface's player number: openblack's local player is PLAYER_ONE
	queries.localPlayerNumber = []() { return static_cast<uint32_t>(PlayerNames::PLAYER_ONE); };
	// whether it is night to the eye (read by the moon phase check of the help sprites)
	queries.visualNight = []() { return TheDayNightClock().IsVisualNight(); };
	// the interface's hand position (inferred: the hand's MapCoords, as the sound map takes it): the player's first
	// hand
	queries.handPosition = []() -> std::optional<glm::vec3> {
		if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(hand))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<const ecs::components::Transform>(hand);
		return transform != nullptr ? std::optional<glm::vec3>(transform->position) : std::nullopt;
	};
	// a help spirit's message: the help system's RunMessage and TriggerCategory
	queries.helpRunMessage = [&game](uint32_t first, uint32_t last, std::string_view script) {
		auto* helpSystem = help::Get();
		if (!Locator::dialogueControlSystem::has_value())
		{
			std::fputs("game: a help spirit's message: no dialogue control in the locator (Locator::dialogueControlSystem)\n",
			           stderr);
			std::abort();
		}
		return helpSystem != nullptr && Locator::vm::has_value() &&
		       help::script_control::RunMessage(*helpSystem, Locator::dialogueControlSystem::value(), first, last, script,
		                                        chlapi::ScriptVm(), game.GetTurn());
	};
	queries.helpTriggerCategory = [](int category) {
		if (auto* helpSystem = help::Get(); helpSystem != nullptr)
		{
			helpSystem->TriggerCategory(category);
		}
	};
	// What the audio reads of the ECS things (the clips' villagers and animals, the lanterns, the surface, the weather)
	ecs::audio_queries::Fill(queries);
	return queries;
}

/// The mod loader library next to the executable, when it is there: used only when it carries the game's own SDK
/// version (mod_sdk::Check), then asked to read the mods folder, with one log line either way. The game is not linked
/// against it and starts the same, unmodded, without it or with one built from other SDK headers
void LoadModLoader()
{
	std::filesystem::path baseDirectory;
	if (char* base = SDL_GetBasePath(); base != nullptr)
	{
		baseDirectory = base;
		SDL_free(base);
	}
#if defined(_WIN32)
	constexpr const char* k_LibraryName = "ModLoader.dll";
#elif defined(__APPLE__)
	constexpr const char* k_LibraryName = "ModLoader.dylib";
#else
	constexpr const char* k_LibraryName = "ModLoader.so";
#endif
	auto logger = spdlog::get("game");
	// a missing library leaves SDL's error message as it was
	const std::string sdlError = SDL_GetError();
	void* library = SDL_LoadObject((baseDirectory / k_LibraryName).string().c_str());
	if (library == nullptr)
	{
		if (sdlError.empty())
		{
			SDL_ClearError();
		}
		else
		{
			SDL_SetError("%s", sdlError.c_str());
		}
		SPDLOG_LOGGER_INFO(logger, "{}", mod_sdk::WhyUnmodded(mod_sdk::Handshake::NoLibrary));
		return;
	}
	const auto sdkVersion = reinterpret_cast<ModLoaderSdkVersionFn>(SDL_LoadFunction(library, k_ModLoaderSdkVersionSymbol));
	const auto version = reinterpret_cast<ModLoaderVersionFn>(SDL_LoadFunction(library, k_ModLoaderVersionSymbol));
	const auto loadMods = reinterpret_cast<ModLoaderLoadModsFn>(SDL_LoadFunction(library, k_ModLoaderLoadModsSymbol));
	const auto listMods = reinterpret_cast<ModLoaderListModsFn>(SDL_LoadFunction(library, k_ModLoaderListModsSymbol));
	const auto setModEnabled =
	    reinterpret_cast<ModLoaderSetModEnabledFn>(SDL_LoadFunction(library, k_ModLoaderSetModEnabledSymbol));
	const auto enabledFolders =
	    reinterpret_cast<ModLoaderEnabledModFoldersFn>(SDL_LoadFunction(library, k_ModLoaderEnabledModFoldersSymbol));
	const bool functionsFound = sdkVersion != nullptr && version != nullptr && loadMods != nullptr && listMods != nullptr &&
	                            setModEnabled != nullptr && enabledFolders != nullptr;
	const char* librarySdk = functionsFound ? sdkVersion() : nullptr;
	const auto handshake = mod_sdk::Check(true, functionsFound, librarySdk != nullptr ? librarySdk : "", k_ModSdkVersion);
	if (handshake != mod_sdk::Handshake::Active)
	{
		SPDLOG_LOGGER_INFO(logger, "{}", mod_sdk::WhyUnmodded(handshake));
		SDL_UnloadObject(library);
		return;
	}
	auto& status = Locator::modLoader::emplace();
	status.version = version();
	const auto keepLine = [](const char* line, void* context) { static_cast<ModLoaderStatus*>(context)->result = line; };
	const auto modsFolder = (baseDirectory / "Mods").string();
	status.folderRead = loadMods(modsFolder.c_str(), keepLine, &status) == static_cast<int>(ModLoaderResult::Matched);
	const auto keepMod = [](const ModLoaderModInfo* mod, void* context) {
		static_cast<std::vector<ModLoaderMod>*>(context)->push_back(ModLoaderMod {.folder = mod->folder,
		                                                                          .name = mod->name,
		                                                                          .version = mod->version,
		                                                                          .author = mod->author,
		                                                                          .enabled = mod->enabled != 0});
	};
	if (status.folderRead)
	{
		listMods(modsFolder.c_str(), keepMod, &status.mods);
		const auto keepRoot = [](const char* folder, void* context) {
			auto& kept = *static_cast<std::pair<std::filesystem::path, std::vector<std::filesystem::path>>*>(context);
			kept.second.push_back(kept.first / folder);
		};
		std::pair<std::filesystem::path, std::vector<std::filesystem::path>> roots {baseDirectory / "Mods", {}};
		enabledFolders(modsFolder.c_str(), keepRoot, &roots);
		status.modRoots = std::move(roots.second);
	}
	// The library stays loaded while the service lives, for the menu's switches
	const std::shared_ptr<void> kept(library, SDL_UnloadObject);
	status.setEnabled = [kept, setModEnabled, modsFolder](const std::string& folder, bool on) {
		return setModEnabled(modsFolder.c_str(), folder.c_str(), on ? 1 : 0) == static_cast<int>(ModLoaderSetResult::Kept);
	};
	SPDLOG_LOGGER_INFO(logger, "ModLoader {}: {}; {} mods", status.version, status.result, status.mods.size());
}

/// An SDL mouse button event as the hand's buttons read it; none for any other event
std::optional<input::MouseButtonEvent> ToMouseButtonEvent(const SDL_Event& event)
{
	if (event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEBUTTONUP)
	{
		return std::nullopt;
	}
	return input::MouseButtonEvent {
	    .button = event.button.button == SDL_BUTTON_LEFT     ? input::MouseButton::Left
	              : event.button.button == SDL_BUTTON_MIDDLE ? input::MouseButton::Middle
	              : event.button.button == SDL_BUTTON_RIGHT  ? input::MouseButton::Right
	                                                         : input::MouseButton::Other,
	    .down = event.type == SDL_MOUSEBUTTONDOWN,
	    .position = {event.button.x, event.button.y},
	    .doubleClick = input::double_click::IsDoubleClick(event.button.clicks),
	};
}

/// The object draw list's near clip for this frame, from the camera drawn the frame before (before this frame's update
/// moves it) over the land read at the hand's quantised point; 0.1 while a script asks for close clipping
float DrawListNearClip(const glm::vec3& previousEye)
{
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	const float ground =
	    Locator::terrainSystem::has_value()
	        ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(lookup(previousEye.x), lookup(previousEye.z)))
	        : 0.0f;
	return ecs::draw_list::NearClipFor(previousEye.y - ground, CinematicDirector().IsCloseClipping());
}

/// The cursor's pixel on the near plane, and the hand's land under the cursor: the island's land under the pixel
/// (PickingSystemInterface::LandFrom2DPoint), the line moved forward along itself by the near clip and one more. The
/// eye, the screen and the near clip are the camera's, the window's and the last camera update's, as LandFrom2DPoint
/// reads them. No land when the line finds neither
struct HandCursorLine
{
	glm::vec3 nearPoint;
	std::optional<glm::vec3> land;
};
HandCursorLine HandLandUnderCursor(const Camera& camera, const glm::vec3& eye, glm::ivec2 screenSize, glm::ivec2 cursor,
                                   float nearClip, const ecs::systems::PickingSystemInterface& picking)
{
	return {.nearPoint = camera.PixelNearPoint(eye, screenSize, cursor, nearClip),
	        .land = picking.LandFrom2DPoint(cursor, true, nearClip + 1.0f)};
}

/// A script holds the cinema bars
bool ScriptHoldsWideScreen()
{
	return help::Get() != nullptr && help::Get()->IsScriptWideScreen();
}

/// A key bound to an action that acts on its key going down goes no further, to the miracle, pause, bookmark and jump
/// keys; a key bound to an action read while held, or any key under a script's cinema bars, goes on
bool KeyTakenByBinding(const SDL_Event& event)
{
	const bool taken = input::KeyTakenByBinding(event.key.keysym.scancode, event.key.keysym.mod,
	                                            Locator::gameActionSystem::value().GetKeyBindings(), ScriptHoldsWideScreen());
	// (openblack, a readout at debug) a key the action bound to it takes
	if (const auto log = spdlog::get("game"); taken && log != nullptr)
	{
		log->debug("Key {} (scan code {}, modifier bits {}): taken by the action bound to it, it goes no further",
		           SDL_GetScancodeName(event.key.keysym.scancode), static_cast<int>(event.key.keysym.scancode),
		           static_cast<int>(event.key.keysym.mod));
	}
	return taken;
}

/// Tab alone: the local player's next town (bookmark_steps::NextTown), its storage pit or else its town centre, becomes
/// the town bookmark, and the camera flies to view it from bookmark_steps' offset while the player's camera has the view.
/// The bookmark is taken even when the camera does not fly
std::optional<ecs::systems::BookmarkFlight> FlightToNextTown(const Camera& camera)
{
	// inside the temple the keys are the temple's
	if (game_clock::IsInsideCitadel())
	{
		return std::nullopt;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto towns = ecs::map_cells::TownsOf(player);
	auto& registry = Locator::entitiesRegistry::value();
	// the place Tab alone flies to: the town's storage pit, else its town centre
	const auto placeOf = [&registry](entt::entity town) -> entt::entity {
		if (const auto pit = ecs::town_queries::GetStoragePit(town); pit != entt::null)
		{
			return pit;
		}
		const auto centre = registry.Get<const ecs::components::Town>(town).centre;
		return centre != entt::null && ecs::IsAvailable(centre) ? centre : entt::null;
	};
	std::vector<bookmark_steps::Town> walked;
	walked.reserve(towns.size());
	for (const auto town : towns)
	{
		walked.push_back({.id = ecs::town_queries::ScriptIdOf(town), .hasPlace = placeOf(town) != entt::null});
	}
	auto& bookmarks = Locator::cameraBookmarkSystem::value();
	const auto chosen = bookmark_steps::NextTown(walked, bookmarks.GetLastTown());
	if (!chosen.has_value())
	{
		return std::nullopt;
	}
	bookmarks.SetTownBookmark(walked.at(*chosen).id);
	if (dynamic_cast<const DefaultWorldCameraModel*>(&camera.GetModel()) == nullptr)
	{
		return std::nullopt;
	}
	const auto place = placeOf(towns.at(*chosen));
	const auto xz = ecs::town_queries::PosOf(place);
	const auto focus = map_coords::ToWorld(map_coords::MapCoords {xz.x, xz.y, 0.0f});
	const auto flight = ecs::systems::BookmarkFlight {
	    .origin = focus + glm::vec3(bookmark_steps::k_TownViewX, bookmark_steps::k_TownViewY, bookmark_steps::k_TownViewZ),
	    .focus = focus};
	// One runtime line a flight, so a release build's real-input runs show where Tab took the camera
	if (const auto log = spdlog::get("game"); log != nullptr)
	{
		log->info("Tab: the next town is town {} of {} (script id {}), the camera flies to its {} at ({:.1f}, {:.1f}, {:.1f}) "
		          "from ({:.1f}, {:.1f}, {:.1f})",
		          *chosen, towns.size(), walked.at(*chosen).id,
		          ecs::town_queries::GetStoragePit(towns.at(*chosen)) == place ? "storage pit" : "town centre", flight.focus.x,
		          flight.focus.y, flight.focus.z, flight.origin.x, flight.origin.y, flight.origin.z);
	}
	return flight;
}

/// The landscape's object draw list, once a drawn frame with the camera as it is drawn: the visible blocks and their land
/// clip, a rebuild when one is due, then the Draw of each object the pass draws, in the list's order. The script
/// highlights have a Draw of their own; every other object's Draw is its mesh's on-screen test (draw_list::DrawOnScreen),
/// after an animated static has played its clip on. Whether the list ran
bool UpdateObjectDrawList(const Camera& camera, float nearClip)
{
	if (!Locator::objectDrawListSystem::has_value())
	{
		std::fputs("game: no object draw list in the locator (Locator::objectDrawListSystem)\n", stderr);
		std::abort();
	}
	if (!Locator::mapCellsSystem::has_value())
	{
		std::fputs("game: no map cells in the locator (Locator::mapCellsSystem)\n", stderr);
		std::abort();
	}
	if (!Locator::animatedStaticSystem::has_value())
	{
		std::fputs("game: no animated statics in the locator (Locator::animatedStaticSystem)\n", stderr);
		std::abort();
	}
	if (!Locator::abodeSoundSystem::has_value())
	{
		std::fputs("game: no building sounds in the locator (Locator::abodeSoundSystem)\n", stderr);
		std::abort();
	}
	if (!Locator::townDesireSystem::has_value())
	{
		std::fputs("game: no town desires in the locator (Locator::townDesireSystem)\n", stderr);
		std::abort();
	}
	if (!Locator::fieldSystem::has_value())
	{
		std::fputs("game: no fields in the locator (Locator::fieldSystem)\n", stderr);
		std::abort();
	}
	if (!Locator::terrainSystem::has_value() || !Locator::windowing::has_value())
	{
		return false;
	}
	const auto screen = Locator::windowing::value().GetSize();
	if (screen.x <= 0 || screen.y <= 0)
	{
		return false;
	}
	const ecs::draw_list::FrameCamera frameCamera {
	    .eye = camera.GetOrigin(),
	    .focus = camera.GetFocus(),
	    .horizontalFov = camera.GetHorizontalFieldOfView(),
	    .aspect = camera.GetAspect(),
	    .screen = screen,
	    .nearClip = nearClip,
	    .landReflection = graphics::detail_level::Get(Locator::config::value().detailLevel).landReflection,
	};
	const auto drawCamera = ecs::draw_list::MakeDrawCamera(frameCamera);
	const auto view = ecs::draw_list::MakeOnScreenView(frameCamera, drawCamera);
	std::vector<ecs::systems::DrawListBlock> blocks;
	ecs::draw_list::CollectBlocks(Locator::terrainSystem::value().GetBlocks(), blocks);
	auto& registry = Locator::entitiesRegistry::value();
	ecs::draw_list::RegistryObjects objects(registry);
	const uint32_t frameMs = game_clock::FrameGameMs();
	const uint32_t realMs = game_clock::FrameRealMs();
	auto& animatedStatics = Locator::animatedStaticSystem::value();
	auto& abodeSounds = Locator::abodeSoundSystem::value();
	auto& townDesires = Locator::townDesireSystem::value();
	auto& fields = Locator::fieldSystem::value();
	const float frameSeconds = game_clock::FrameGameSeconds();
	const glm::vec3 eye = camera.GetOrigin();
	const ecs::draw_list::Consumer consumer = [&view, &registry, &animatedStatics, &abodeSounds, &townDesires, &fields, &eye,
	                                           frameMs, realMs, frameSeconds](entt::entity object) -> std::optional<bool> {
		if (ecs::script_highlight::IsHighlight(object))
		{
			return ecs::script_highlight::Draw(object, view, frameMs);
		}
		// A field's Draw sinks its crop with its food in the matrix it is drawn with; a hidden crop is not drawn, and so
		// not tested on the screen
		if (const auto onScreen = ecs::draw_list::DrawField(registry, fields, object, frameMs, view); onScreen.has_value())
		{
			return *onScreen;
		}
		// A firefly's Draw steps it and draws its sprite; it tests nothing on the screen, so it stays on the screen
		if (std::as_const(registry).AllOf<ecs::components::FireFlyObject>(object))
		{
			ecs::DrawFireFly(object, frameSeconds, eye);
			return true;
		}
		// A town desire flag's Draw steps it; only a flag that shows is drawn, and so tested on the screen
		if (std::as_const(registry).AllOf<ecs::components::TownDesireFlag>(object))
		{
			if (!townDesires.DrawFlag(object, frameSeconds, view.eye))
			{
				return true;
			}
			// a flag that shows takes its step of the snow where it stands
			ecs::snow_levels::Drawn(object, frameMs);
			return ecs::draw_list::DrawOnScreen(ecs::draw_list::OnScreenInputsOf(registry, object), view);
		}
		// A puzzle totem's Draw stands it at the altitude its share gives, the share read as the draw reads it (a totem
		// puzzle game's move blends in over these reads); one the hand holds is drawn where the hand holds it instead
		if (const auto* totem = std::as_const(registry).TryGet<const ecs::components::PuzzleTotem>(object);
		    totem != nullptr && !totem->held)
		{
			ecs::archetypes::PuzzleTotemArchetype::Place(object);
		}
		// A workshop's and a worship site's Draw draw their need signs first (the frame's game ms)
		ecs::show_needs::DrawFor(object, frameMs, eye);
		// An animated static's Draw plays its clip on first
		animatedStatics.Draw(object, frameMs);
		// A creature's Draw places the glows on its body, on the screen or not
		if (std::as_const(registry).AllOf<ecs::components::Creature>(object) && Locator::creatureAnimationSystem::has_value())
		{
			Locator::creatureAnimationSystem::value().PlaceHandGlows(object, realMs, frameMs);
		}
		// A villager's or an animal's Draw leaves the matrix it is drawn with on it first: its screen test takes it, and
		// the game reads it until its next draw (ECS/ObjectPlacement.h)
		if (std::as_const(registry).AnyOf<ecs::components::Villager, ecs::components::Animal>(object) &&
		    !std::as_const(registry).AllOf<ecs::components::Creature>(object))
		{
			// its drawn yaw turns only as it is drawn
			ecs::DrawMobileTurns(object, game_clock::TurnFraction(), static_cast<float>(frameMs));
			ecs::object_placement::KeepDrawn(registry, object);
		}
		// Its snow takes a step towards the snow lying where it is drawn, as every draw of a thing in the map does (the
		// creature's body has a draw of its own, which takes none)
		if (!std::as_const(registry).AllOf<ecs::components::Creature>(object))
		{
			ecs::snow_levels::Drawn(object, frameMs);
		}
		const auto onScreen = ecs::draw_list::DrawOnScreen(ecs::draw_list::OnScreenInputsOf(registry, object), view);
		// A building rolls for its ambience after its own draw, on screen or not
		abodeSounds.Draw(object, frameMs);
		return onScreen;
	};
	Locator::objectDrawListSystem::value().Update(
	    {.camera = drawCamera},
	    {.blocks = blocks,
	     .blockArrays = Locator::mapCellsSystem::value().Blocks(),
	     .turn = game_clock::Turn(),
	     .objects = &objects,
	     .detailIndex = ecs::draw_list::DetailIndexOf(Locator::config::value().detailLevel)},
	    consumer);
	// The land's block textures: this frame's work of their queue, then the blocks drawn this frame with their own
	// texture, nearest first, whose texture is out of date are asked for, as the land's draw asks for them (a block
	// beyond the texture detail line shows its tile only and is not asked for)
	auto& island = Locator::terrainSystem::value();
	island.UpdateBlockTextures(eye, ecs::draw_list::DetailIndexOf(Locator::config::value().detailLevel));
	std::vector<glm::ivec2> drawnBlocks;
	for (const auto index : Locator::objectDrawListSystem::value().TextureRequestBlocks())
	{
		drawnBlocks.push_back(blocks.at(index).coords);
	}
	island.RequestBlockTextures(drawnBlocks);
	return true;
}

/// The hand's buttons from the interface's
void UpdateHandButtons(const input::MouseButtonsState& buttons)
{
	// A hand demo plays the recorded buttons (the mouse buttons are not read while it plays)
	if (!hand_demo::IsPlaying())
	{
		auto& hand = input::HandButtonsRef();
		hand.gripping = input::HandGripping(buttons);
		hand.action = input::HandAction(buttons);
		hand.grab = buttons.left;
	}
}

/// The middle button let go: out of relative mode, the cursor back where it went down
void LetGoOfMiddleButton(const glm::ivec2& pressPosition)
{
	// only the real pointer: a test run's or a playback's cursor is brought back by the recorded motion
	if (!input::UsesRealInput())
	{
		return;
	}
	SDL_SetRelativeMouseMode(SDL_FALSE);
	if (Locator::windowing::has_value())
	{
		SDL_WarpMouseInWindow(static_cast<SDL_Window*>(Locator::windowing::value().GetHandle()), pressPosition.x,
		                      pressPosition.y);
	}
}

/// The places of the scripts' object table a loaded world file held, each with the entity its object became
std::vector<ecs::saved_script_objects::LoadedPlace> LoadedScriptPlaces(const gamesave::WorldRead& read,
                                                                       const ecs::saved_world_write::LoadedObjects& loaded)
{
	std::vector<ecs::saved_script_objects::LoadedPlace> places;
	if (read.objects.empty() || !read.objects.front().has_value())
	{
		return places;
	}
	const auto* game = std::get_if<gamesave::GameHead>(&*read.objects.front());
	if (game == nullptr)
	{
		return places;
	}
	places.reserve(game->tail.scriptObjects.size());
	for (const auto& entry : game->tail.scriptObjects)
	{
		const auto found = loaded.find(entry.object.number);
		places.push_back({.object = entry.object.number != 0 && found != loaded.end() ? found->second.entity : entt::null,
		                  .createdByScript = entry.word != 0,
		                  .references = entry.byte});
	}
	return places;
}

/// The scripts' machine takes back a saved state, each object it names put back on the loaded object of its place
void RestoreScripts(lhvm::LHVM& vm, const std::filesystem::path& file, std::span<const uint32_t> places)
{
	if (vm.RestoreState(file) != EXIT_SUCCESS)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The scripts' state {} could not be read", file.generic_string());
		return;
	}
	vm.MapObjects([places](uint32_t place) { return ecs::saved_script_objects::ValueOf(places, place); });
}

} // namespace

/// What the frames of the game loop keep from one frame to the next
struct Game::FrameLoop
{
	/// one frame's logic -> draw hand-over (Engine/FrameSnapshot.h): one, refilled every frame (its vectors keep their
	/// capacity)
	engine::FrameSnapshot snapshot;
	/// The game clock's ticks as the run started
	uint32_t runStartTicks {0};
	/// --frame-stats: the frame times and profiler stages logged every so many frames
	std::optional<debug::FrameStatsLog> frameStats;
	std::chrono::steady_clock::time_point frameStart;
	/// OPENBLACK_RESOURCE_TRACE: the resources held logged every so many seconds of real time; off by default
	std::optional<std::chrono::seconds> resourceTraceInterval;
	std::chrono::steady_clock::time_point resourceTraceNext;
};

/// The game as the debug windows and the testbed's scenarios see it: they reach it only through this
class Game::GameHostOfGame final: public debug::gui::GameHost
{
public:
	explicit GameHostOfGame(Game& game)
	    : _game(game)
	{
	}

	void LoadTestbed(uint8_t planeAltitude) override { _game.LoadTestbed(planeAltitude); }
	[[nodiscard]] float GetGameSpeed() const override { return _game.GetGameSpeed(); }
	void SetGameSpeed(float multiplier) override { _game.SetGameSpeed(multiplier); }
	[[nodiscard]] bool IsPaused() const override { return _game.IsPaused(); }
	[[nodiscard]] float FrameSeconds() const override { return std::chrono::duration<float>(_game._frameDelta).count(); }
	[[nodiscard]] std::optional<ScenarioRequest> TakeScenarioRequest() override
	{
		return std::exchange(_game._scenarioRequest, std::nullopt);
	}
	void RequestQuit() override
	{
		Locator::config::value().running = false;
		_game._quitNow = true;
		// A later event of the next frame sets the flag again as it is handled, so the quit goes in after them as well
		SDL_Event quit {};
		quit.type = SDL_QUIT;
		SDL_PushEvent(&quit);
	}
	bool ChangeLand(std::string_view landScript) override
	{
		// The script's path under the game's Scripts folder, as a map script's land change gives it
		return _game.ChangeLand(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() /
		                        std::filesystem::path(landScript));
	}
	bool StartSkirmish(std::string_view landScript) override
	{
		// The land under the game's Scripts folder, as the skirmish box gives one from its Playgrounds folder
		return _game.StartSkirmish(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() /
		                           std::filesystem::path(landScript));
	}
	[[nodiscard]] std::optional<std::vector<std::string>> TakeLandTimeline() override
	{
		if (!_game._landRun.has_value() || _game._landRun->timeline.empty())
		{
			return std::nullopt;
		}
		return std::exchange(_game._landRun->timeline, {});
	}
	[[nodiscard]] std::optional<std::vector<testbed_land_timeline::QuestScript>> TakeLandQuest() override
	{
		if (!_game._landRun.has_value() || _game._landRun->quest.empty())
		{
			return std::nullopt;
		}
		return std::exchange(_game._landRun->quest, {});
	}
	[[nodiscard]] gui::GameInterface* GetInterface() const override { return _game.GetInterface(); }
	bool LoadMap(const std::filesystem::path& scriptPath) override { return _game.LoadMap(scriptPath); }
	bool ChangeLand(const std::filesystem::path& scriptPath) override { return _game.ChangeLand(scriptPath); }
	void AskForAnotherPlayer(bool canCancel) override { _game.AskForAnotherPlayer(canCancel); }
	bool LoadSavedGame(uint32_t slot) override { return _game.LoadSavedGame(slot); }
	[[nodiscard]] const ecs::saved_world_load::Census& GetSavedGameCensus() const override
	{
		return _game.GetSavedGameCensus();
	}
	[[nodiscard]] std::string_view GetQuickLoadReadout() const override { return _game.GetQuickLoadReadout(); }
	[[nodiscard]] std::string_view GetHandLoadReadout() const override { return _game.GetHandLoadReadout(); }
	[[nodiscard]] std::chrono::duration<float, std::milli> GetDeltaTime() const override { return _game.GetDeltaTime(); }
	[[nodiscard]] const hand_tricons::Fades& GetHandTriconFades() const override { return _game.GetHandTriconFades(); }
	[[nodiscard]] const graphics::ObjectListFrame* GetObjectListFrame() const override { return _game.GetObjectListFrame(); }
	bool WriteVersionFile() override { return _game.WriteVersionFile(); }
	void AskForInLoopLogo() override { _game.AskForInLoopLogo(); }
	void AskForSavingBanner() override { _game.AskForSavingBanner(); }

private:
	Game& _game;
};

Game::Game(Arguments&& args) noexcept
    : _gamePath(args.gamePath)
    , _startMap(args.startLevel)
    , _preIntro(args.preIntro)
    , _skipLogos(args.skipLogos)
    , _startTestbed(args.startTestbed || args.scenario.has_value())
    , _scenarioRequest(args.scenario)
    , _seed(args.seed)
    , _newGameStart(args.newGameStart)
    , _profileFolder(std::move(args.profileFolder))
    , _replayInput(std::move(args.replayInput))
    , _recordInput(std::move(args.recordInput))
    , _resumeInput(std::move(args.resumeInput))
    , _workingFolder(StartingWorkingFolder())
    , _writeVersionFile(args.writeVersionFile)
    , _programFile(std::move(args.programFile))
    , _noLoadMusic(args.noLoadMusic)
    , _settingsFile(std::move(args.settingsFile))
    , _questFolder(settings_file::k_DefaultQuestFolder)
    , _testMenuAt(TestMenuAtFromEnvironment())
{
	// with the members, before anything else: the script fade, the day / night clock, the map script's globals, the
	// frame count with its screenshot request (--screenshot-frame's, if given) and the camera
	StartGameState(args.requestScreenshot);
	// The launch's SKIRMISH switch: the first start shows the skirmish box, unless NEWGAME was given too
	_skirmishBoxDue = args.startMode == launch_options::StartMode::Skirmish;
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
			// One file sink shared by every subsystem's logger: with a basic_logger_mt each, every logger opened the same
			// file on its own and their writes overwrote each other (lines went missing from openblack.log)
			auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(args.logFile);
			createLogger = [fileSink](const std::string& name) {
				auto logger = std::make_shared<spdlog::logger>(name, fileSink);
				spdlog::register_logger(logger);
				return logger;
			};
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
	StartClock();
	// the keys are named as the system names them, in the language of its keyboard layout, as the game names them; a
	// run that reads no real input names them as an English (United States) layout does, on every machine
	input::system_key_name::InstallSystemNames(&input::UsesRealInput);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Key names from the {}: Ctrl is \"{}\", Space \"{}\"",
	                   input::UsesRealInput() ? "system" : "English (United States) layout, a run with no real input",
	                   gui::ToUtf8(gui::control_bindings::KeyName(0x1D)), gui::ToUtf8(gui::control_bindings::KeyName(0x39)));

	auto& config = Locator::config::emplace();
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.resolution = {args.windowWidth, args.windowHeight};
	config.displayMode = args.displayMode;
	config.graphicsBackend = args.graphicsBackend;
	config.vsync = args.vsync;
	config.detailLevel = args.detailLevel;
	config.profileCreatureFile = args.creatureFile;
	config.frameStatsInterval = args.frameStatsInterval;
	config.frameStatsViews = args.frameStatsViews;
	config.drawLoadingScreens = !args.skipLoadingScreens;
	config.startupLogos = !args.skipLogos;
	config.inLoopLogo = !args.skipInLoopLogo;
	// the corner frame counter stays off in the checks' runs, so that their screenshots do not change
	config.checkRun = debug::frame_counter::IsCheckRun(input::IgnoreRealInput(), _startTestbed, args.numFramesToSimulate);
	// football, read once as the game starts, as the original reads the Soccer Add-On's switch: the add-on's setting,
	// and never in a network game, which openblack does not play
	config.footballEnabled = ecs::football_rules::EnabledAtGameStart(config.footballAddOn, false);
	// Mods: the mod loader library checks the mods folder's version and loads no mod yet. A mod SDK is planned on it
	// once the whole game is rebuilt: it should give the mods only the Locator's interfaces, send them the game's
	// events instead of calls from the game loop, and let them replace resources only through the resource loaders
	LoadModLoader();
	config.guiScale = args.guiScale;
	// what a new game in the same process starts from (StartNewGame)
	_startConfig = config;
	_scenarioList = std::move(args.scenarioList);
}

void Game::StartGameState(const std::optional<std::pair<uint32_t, std::filesystem::path>>& screenshot)
{
	InitializeGameState();
	if (screenshot.has_value())
	{
		Locator::screenshotRequest::value().RequestAt(screenshot->first, screenshot->second);
	}
	Locator::camera::emplace(glm::zero<glm::vec3>());
	// what the draw reads instead: a copy of it made every frame (Run)
	_drawCamera = std::make_unique<Camera>();
}

void Game::StartClock()
{
	// the game timer at speed 1, as the game starts; paused until a map is loaded (openblack)
	// OPENBLACK_FIXED_FRAME_MS: the same ms every frame, for runs that must repeat (Debug/FixedClock.h)
	InitializeClock();
	fixed_clock::InstallFromEnvironment();
	game_clock::Reset();
	game_clock::Start(true);
}

Game::~Game() noexcept
{
	ShutDownGame();
	// What the scripts asked of openblack that it can't do yet, for the natives to write next
	if (Locator::chlapi::has_value())
	{
		Locator::chlapi::value().LogStubCalls();
	}
	ShutDownServices();
	SDL_Quit(); // todo: move to GameWindow
	spdlog::shutdown();
}

void Game::ShutDownGame()
{
	// The game's close first clears its map, which saves the local player's creature to the profile the game was played
	// as, as a land change does: so leaving a skirmish or the game keeps the creature as it was then
	ecs::player_creature::SaveMyCreatureAsTheGameCloses();
	// The profile keeps the time played as the game closes, and its help record
	if (Locator::playerProfileSystem::has_value())
	{
		Locator::playerProfileSystem::value().StopPlaying(machine_clock::WallTime());
		help::profile::KeepInProfile();
	}
	// the audio's shutdown order: the sample channels, then the game music before the music player, then the music
	// thread and its OpenAL sources before the audio context
	// the guidance closes with the interface status; then the spooky voices shut down. A start that failed before the
	// audio's state was made started none of them
	if (Locator::audioState::has_value())
	{
		// the main volumes are kept for the next start as the audio closes
		audio::main_volumes::Keep();
		audio::guidance::Close();
		audio::spooky::Shutdown();
		audio::Shutdown();
		audio::game_music::Shutdown();
		audio::music::Shutdown();
	}
	// the temple's scrolls are written with the interface's text, and their textures go before the renderer too
	if (Locator::temple::has_value())
	{
		Locator::temple::value().SetInterface(nullptr);
	}
	// and the Creature Cave's screen is written with it
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetInterface(nullptr);
	}
	// and the tribes' names round the hand
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SetInterface(nullptr);
	}
	// and the help texts are read from its text database
	if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().SetTexts(nullptr);
	}
	_interface.reset();        // its textures before the renderer
	_loadingScreen.reset();    // and the start-up screens'
	help::spirits::Shutdown(); // its meshes before the renderer goes
	help::Shutdown();
	// what waits on the dead list goes before the services, as the original's close drains it. A start that failed
	// before the services were made has none
	if (Locator::toBeDeletedSystem::has_value())
	{
		ecs::ProcessDeadList(true);
	}
}

void Game::EndGame() noexcept
{
	// The scenarios' window lets go of its scenario first, while the services the scenario changed are still there
	if (Locator::debugGui::has_value())
	{
		Locator::debugGui::value().SetGameHost(nullptr);
	}
	// The last game goes as at the program's end, but for what a process makes once
	ShutDownGame();
	ShutDownServices(ShutDownScope::Game);
}

bool Game::StartNewGame(std::optional<ScenarioRequest> scenario, uint32_t frames,
                        const std::optional<std::pair<uint32_t, std::filesystem::path>>& screenshot) noexcept
{
	Locator::rendererInterface::value().ResetForNewGame();
	// The hand starts neutral, with its mesh as the file has it: the last game may have left its good or evil shape in it
	simulation::LoadHandMesh();
	// The configuration as the command line made it, with the new game's frames; the game goes on running
	auto& config = Locator::config::value();
	config = _startConfig;
	config.running = true;
	config.numFramesToSimulate = frames;
	// None of the last game's events reaches the new one
	SDL_PumpEvents();
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	// The Game's own state as it is made
	_pausedTurn = {};
	_menuWasOpen = false;
	_menuPaused = false;
	_menuClosedTicks = 0xFFFFFFFFu;
	_scenarioRequest = std::move(scenario);
	_firstMapLoaded = false;
	_mouseButtons = {};
	_mouseMotion = {};
	_windowAway = false;
	_frameDelta = std::chrono::microseconds {0};
	_objectListRan = false;
	_objectListFrame = nullptr;
	_textShotAtMs.reset();
	_textShotPath.clear();

	// Then the new game starts as the program starts one, in the same order
	StartGameState(screenshot);
	StartClock();
	InitializeSimulationCore(StartKind::NewGame);
	// the font cache keeps the permutation of the first start-up; this one's draws are the same
	std::array<uint8_t, 256> fontHashPermutation {};
	if (!InitializeEngine(StartKind::NewGame, config.graphicsBackend, config.vsync, fontHashPermutation))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize the new game's engine services.");
		return false;
	}
	// the debug windows where the launch's started, not where the last game left them in the settings file
	if (_debugGuiStartSettings.has_value())
	{
		Locator::debugGui::value().StartFromSettings(*_debugGuiStartSettings);
	}
	ConnectToEngine();
	if (!InitializeGame(StartKind::NewGame, fontHashPermutation))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize the new game's services.");
		return false;
	}
	CreateInterface();
	StartAudio();
	StartHelp();

	// The C runtime's stream as the program's start left it before its first land. The start-up above draws the same
	// as the program's own, so this changes nothing unless a draw was missed, which the log then says
	if (_startCrtSeed.has_value())
	{
		if (const auto seed = game_random::crt::Seed(); seed != *_startCrtSeed)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "New game: the C runtime's seed is {:#x}, a new program's is {:#x}", seed,
			                   *_startCrtSeed);
		}
		game_random::crt::Srand(*_startCrtSeed);
	}
	return true;
}

bool Game::ProcessEvents(const SDL_Event& event) noexcept
{
	// whether the hand held the middle button before this event: only its release brings the cursor back
	const bool middleWasHeld = _mouseButtons.middle;
	if (const auto button = ToMouseButtonEvent(event); button)
	{
		// a down or an up that sends no press or release (its binding's state, or a box took the input) does nothing
		if (!input::ApplyMouseButton(_mouseButtons, *button))
		{
			return true;
		}
		// the help system's click (help::dialogue_click: the left button let go), given at its next interface pass
		if (help::dialogue_click::IsClick(button->button, button->down))
		{
			_dialogueClick = true;
		}
	}

	UpdateHandButtons(_mouseButtons);

	auto& window = Locator::windowing::value();
	auto& camera = Locator::camera::value();

	switch (event.type)
	{
	case SDL_QUIT:
		// the program's own quit ends the frames at once; the system's is taken at the stop check
		if (_quitNow)
		{
			return false;
		}
		_systemQuit = true;
		break;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == window.GetID())
		{
			_systemQuit = true;
		}
		else if (const auto activation = windowing::ActivationFor(event.window.event);
		         activation != windowing::Activation::Unchanged)
		{
			// the screen's activation switches the audio off / on and holds the game loop while the window is minimised
			// (Update). Not on a focus change: only minimising deactivates and only restoring reactivates
			audio::OnFocus(activation == windowing::Activation::Reactivate);
			_windowAway = windowing::AwayAfter(_windowAway, activation);
		}
		else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
		{
			const auto resolution = glm::u16vec2(event.window.data1, event.window.data2);
			Locator::rendererInterface::value().Reset(resolution);
			Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, resolution,
			                                                  graphics::k_BackgroundColour);

			auto aspect = window.GetAspectRatio();
			const auto& config = Locator::config::value();
			camera.SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip, config.cameraFarClip);
		}
		break;
	case SDL_KEYDOWN:
		// Alt+1 and Alt+2 set the game speed, unless they are bound to an action; they then do nothing else (no
		// bookmark). Inside the temple the keys do not reach them
		if (const auto speed = input::SpeedForKey(event.key.keysym.scancode, event.key.keysym.mod,
		                                          Locator::gameActionSystem::value().GetKeyBindings(), game_clock::Speed()))
		{
			if (!(Locator::temple::has_value() && Locator::temple::value().Active()))
			{
				game_clock::SetSpeed(*speed);
			}
			break;
		}
		// R repeats the last miracle, unless R with the held modifier is bound to an action, which takes it. Inside the
		// temple the key does not reach it
		if (input::RepeatMiracleKey(event.key.keysym.scancode, event.key.keysym.mod,
		                            Locator::gameActionSystem::value().GetKeyBindings(), ScriptHoldsWideScreen()))
		{
			if (!(Locator::temple::has_value() && Locator::temple::value().Active()) && Locator::handSystem::has_value())
			{
				magic::gestures::RepeatKey(Locator::handSystem::value().IsHandReadyForObject());
			}
			break;
		}
		// M opens the player's miracle selection, unless M with the held modifier is bound to an action, which takes it.
		// Inside the temple the key does not reach it
		if (input::SelectMiracleKey(event.key.keysym.scancode, event.key.keysym.mod,
		                            Locator::gameActionSystem::value().GetKeyBindings(), ScriptHoldsWideScreen()))
		{
			if (!(Locator::temple::has_value() && Locator::temple::value().Active()) && Locator::handSystem::has_value())
			{
				magic::gestures::SelectKey(Locator::handSystem::value().IsHandReadyForObject());
			}
			break;
		}
		// P alone pauses and unpauses the game, unless P alone is bound to an action, which takes it. Inside the temple
		// the key does not reach it
		if (input::PauseKey(event.key.keysym.scancode, event.key.keysym.mod,
		                    Locator::gameActionSystem::value().GetKeyBindings(), ScriptHoldsWideScreen()))
		{
			if (!(Locator::temple::has_value() && Locator::temple::value().Active()))
			{
				game_clock::Pause(!game_clock::IsPaused());
			}
			break;
		}
		// The bookmark keys are read by their place on the keyboard, the top row's 1 to 7 whatever the layout gives them.
		// They are skipped while a script holds the wide screen or a SET_INTERFACE_INTERACTION level has a ControlMap
		// switch off, while the bookmarks are disabled (PLAY_JC_SPECIAL 14 / 15) and inside the temple, which takes every
		// key itself
		if (const auto bookmark = input::BookmarkOfScancode(event.key.keysym.scancode); bookmark.has_value())
		{
			if (!KeyTakenByBinding(event) &&
			    input::BookmarkKeyActs({
			        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			        .keyShortcutsEnabled = help::interface_interaction::KeyShortcutsEnabled(),
			        .bookmarksEnabled = Locator::cameraBookmarkSystem::value().IsEnabled(),
			        .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
			    }))
			{
				const auto mod = event.key.keysym.mod;
				const auto index = *bookmark;
				auto& bookmarks = Locator::cameraBookmarkSystem::value();
				const auto from = JumpFromCamera(camera);
				std::optional<ecs::systems::BookmarkFlight> flight;
				switch (input::ActionOfBookmarkKey((mod & KMOD_SHIFT) != 0, (mod & KMOD_CTRL) != 0, (mod & KMOD_ALT) != 0))
				{
				case input::BookmarkKeyAction::Drop:
				{
					// where the interface's hand was synced at the last turn, or on the interface's object when it has one
					const auto& hand = Locator::handSystem::value();
					bookmarks.SetBookmark(index, hand.GetSyncedPlace(), hand.GetActionObject(), camera.GetOrigin());
					// (openblack, a readout at debug) where the bookmark was dropped, and the thing it follows
					if (const auto log = spdlog::get("game"); log != nullptr)
					{
						const auto place = hand.GetSyncedPlace();
						const auto thing = hand.GetActionObject();
						const glm::vec3 origin = camera.GetOrigin();
						log->debug("Bookmark key {}: dropped at map ({}, {}), {}, the camera at ({:.1f}, {:.1f}, {:.1f}); "
						           "set {}",
						           index + 1, place.x, place.z,
						           thing ? fmt::format("following thing {}", entt::to_integral(*thing))
						                 : std::string("on no thing"),
						           origin.x, origin.y, origin.z, bookmarks.IsSet(index) ? "yes" : "no");
					}
					break;
				}
				case input::BookmarkKeyAction::Clear:
					if (bookmarks.IsSet(index))
					{
						bookmarks.ClearBookmark(index);
					}
					break;
				case input::BookmarkKeyAction::Follow:
					// a set bookmark on a thing is followed (or no longer), one on no thing jumped to
					flight = bookmarks.ToggleFollow(index, from);
					break;
				case input::BookmarkKeyAction::FlyTo:
					// to the land point of its map position (as it is now, for a bookmark on a thing), the camera at the
					// offset it had from it when it was dropped; not inside the citadel
					flight = bookmarks.JumpTo(index, from);
					break;
				case input::BookmarkKeyAction::None:
					break;
				}
				if (flight.has_value())
				{
					FlyToBookmark(camera, *flight);
				}
			}
			break;
		}
		switch (event.key.keysym.sym)
		{
		case SDLK_ESCAPE:
			// with a full screen film ESC skips it (Video/VideoPlayer.h); without one openblack quits. (inferred) one
			// skip a press: SDL's key repeats are not keys of the original. Once the game is over, or in the
			// playground, the key is taken before it reaches the film
			if (video::IsPlaying())
			{
				const auto* helpSystem = help::Get();
				const video::EscapeWay way {
				    .playground = Locator::mapScriptSystem::has_value() && influence::LandNumber() == video::k_PlaygroundLand,
				    .gameOver = Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Context().gameOver,
				    .scriptWideScreen = helpSystem != nullptr && helpSystem->IsScriptWideScreen(),
				};
				if (video::EscapeSkipsTheFilm(event.key.repeat != 0, way))
				{
					video::Get().EscapeKey((event.key.keysym.mod & KMOD_SHIFT) != 0, (event.key.keysym.mod & KMOD_CTRL) != 0);
				}
				break;
			}
			// nothing while a script holds the wide screen (a tutorial cinema or
			// hand demo; a demo whose bars come only from StartPlayBack, owner 0, is not protected: inferred)
			if (const auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->IsScriptWideScreen())
			{
				break;
			}
			// with the in-game menus, an Escape they did not take (Shift or Ctrl held) does nothing, as in the
			// original; openblack quits only without them
			if (_interface)
			{
				break;
			}
			return false;
		case SDLK_F2:
			// F2 asks whether to go to the Gods' Playground: only in a single-player story game, outside the temple, with
			// no other box up, no script's wide screen and not already there. The question pauses the game until it is
			// answered (HandleInterfaceAction). (inferred) one question a press: SDL's key repeats are not keys of the
			// original
			if (event.key.repeat == 0 && _interface &&
			    input::playground_key::KeyAsks({
			        .skirmish = ecs::IsSkirmishGame(),
			        .networkGame = false,
			        .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
			        .boxUp = _interface->IsBoxActive() && !_interface->GetMenu().IsOpen(),
			        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			        .land = Locator::mapScriptSystem::has_value() ? influence::LandNumber() : 0,
			    }))
			{
				const auto& question = _interface->GetTexts().GetHelpText(input::playground_key::k_QuestionText);
				_interface->ShowGameQuestion(question.text);
				game_clock::Pause(true);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "F2: the Gods' Playground question");
			}
			break;
		case SDLK_f:
			window.SetDisplayMode(windowing::DisplayMode::Fullscreen);
			break;
		case SDLK_F11: // F1 is the game's Help key
			Locator::rendererInterface::value().SetDebug(!Locator::rendererInterface::value().GetDebug());
			break;
		case SDLK_BACKSPACE:
			// Backspace goes back to the view before the last jump, while no script holds the wide screen and the key
			// shortcuts are on (the bookmarks' switch does not gate it)
			if (KeyTakenByBinding(event) ||
			    !input::BeforeJumpKeyActs({
			        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			        .keyShortcutsEnabled = help::interface_interaction::KeyShortcutsEnabled(),
			        .bookmarksEnabled = true,
			        .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
			    }))
			{
				break;
			}
			if (const auto flight =
			        Locator::cameraBookmarkSystem::value().JumpTo(ecs::systems::k_BeforeJumpBookmark, JumpFromCamera(camera));
			    flight.has_value())
			{
				FlyToBookmark(camera, *flight);
			}
			break;
		case SDLK_TAB:
			// Tab: Shift and Tab jumps to the set bookmark before the current one, Ctrl and Tab to the next one, and Tab
			// alone flies to the player's next town, under the same gates as Backspace
			if (KeyTakenByBinding(event) ||
			    !input::BeforeJumpKeyActs({
			        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			        .keyShortcutsEnabled = help::interface_interaction::KeyShortcutsEnabled(),
			        .bookmarksEnabled = true,
			        .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
			    }))
			{
				break;
			}
			{
				const auto mod = event.key.keysym.mod;
				auto& bookmarks = Locator::cameraBookmarkSystem::value();
				std::optional<ecs::systems::BookmarkFlight> flight;
				if ((mod & KMOD_SHIFT) != 0)
				{
					flight = bookmarks.JumpToPrevious(JumpFromCamera(camera));
				}
				else if ((mod & KMOD_CTRL) != 0)
				{
					flight = bookmarks.JumpToNext(JumpFromCamera(camera));
				}
				else
				{
					flight = FlightToNextTown(camera);
				}
				if (flight.has_value())
				{
					FlyToBookmark(camera, *flight);
				}
			}
			break;
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		switch (event.button.button)
		{
		case SDL_BUTTON_MIDDLE:
		{
			// Relative mode while held: the cursor stays put and only the motion drives the camera.
			// (the press position is kept by ApplyMouseButton above)
			// Only the real pointer is held and put back: in a test run or an input recording's playback the recorded
			// motion brings the cursor back
			const bool pressed = event.type == SDL_MOUSEBUTTONDOWN;
			if (input::UsesRealInput())
			{
				SDL_SetRelativeMouseMode(pressed ? SDL_TRUE : SDL_FALSE);
			}
			// (a box that let go of it brought the cursor back already)
			if (!pressed && middleWasHeld && input::UsesRealInput())
			{
				const auto& pressPosition = _mouseButtons.middlePressPosition;
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

	// one game turn: the game inputs before the game code, so before the turn number goes up: the packets the last
	// flush passed on (Input/GamePackets), the hand's part of the interface's process, then the interface's pump with
	// the power-up system
	// the steps of the turn in their order (GameTurnOrder.h); the turn number is the one the clock's step gives
	uint32_t turn = 0;
	// once a skirmish is decided, the world stops: only the player's inputs of the turn run
	const bool decided = Locator::gameEndSystem::has_value() && Locator::gameEndSystem::value().IsDecided();
	game_turn::PlayTurn([this, &turn](game_turn::Step step) { simulation::PlayTurnStep(step, *this, turn); }, decided);
	// the turn's start, which counts it for the frame, is the game's code a decided skirmish leaves out; the loop counts
	// every turn it plays all the same, so it plays one a frame and the frame goes on
	if (decided)
	{
		game_clock::CountTurnWithoutGameCode();
	}
	// the turn's last step: Escape stopped a lesson on the Gods' Playground, so its scripts restart on its free play
	if (std::exchange(_playgroundRestartRequested, false))
	{
		RestartPlaygroundScripts();
	}
	// after the game code the original also runs the wall huggers' lookahead and the repair of missing mothers

	// OPENBLACK_STATE_HASH: the state of this turn, for the replay test (Debug/StateHash.h)
	state_hash::OnTurnEnd(turn);

	return false;
}

void Game::PlayViewStep(game_turn::Step step, glm::vec3 /*cameraPosition*/)
{
	using game_turn::Step;

	switch (step)
	{
	case Step::PlayerCommands:
	{
		// a skirmish's first turn: the packet the start sent first brings the player's creature from the profile
		if (std::exchange(_skirmishCreatureDue, false))
		{
			ecs::player_creature::BringIntoSkirmish();
		}
		break;
	}
	case Step::Clock:
	{
		const auto currentTime = std::chrono::steady_clock::now();
		_turnDeltaTime = currentTime - _lastGameLoopTime;
		_lastGameLoopTime = currentTime;
		break;
	}
	default:
		// the world does the other steps (Simulation/TurnBody.h)
		break;
	}
}

bool Game::Update() noexcept
{
	// the steps of the frame in their order (GameFrameOrder.h); a step that returns false ends the frame
	FrameState frameState;
	if (!game_frame::PlayFrame([this, &frameState](game_frame::Step step) { return PlayFrameStep(step, frameState); }))
	{
		return false;
	}
	auto& config = Locator::config::value();
	return config.numFramesToSimulate == 0 || Locator::screenshotRequest::value().Frame() < config.numFramesToSimulate;
}

bool Game::PlayFrameStep(game_frame::Step step, FrameState& frameState) noexcept
{
	using game_frame::Step;

	switch (step)
	{
	case Step::FrameClock:
	{
		auto& profiler = Locator::profiler::value();
		auto& config = Locator::config::value();
		auto& deltaTime = frameState.deltaTime;
		// set again only if this frame runs the object draw list
		_objectListRan = false;

		profiler.Frame();

		auto previous = profiler.GetEntries().at(profiler.GetEntryIndex(-1)).frameStart;
		auto current = profiler.GetEntries().at(profiler.GetEntryIndex(0)).frameStart;
		// Prevent spike at first frame
		if (previous.time_since_epoch().count() == 0)
		{
			current = previous;
		}
		deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(current - previous);
		if (fixed_clock::Enabled())
		{
			deltaTime = fixed_clock::AdvanceFrame();
		}
		_frameDelta = deltaTime;

		Locator::debugGui::value().SetScale(config.guiScale);
		break;
	}
	case Step::Input:
	{
		// the quick load a turn's packet asked for in the last frame: that frame went on to its end with the world it
		// started with, and the quick save's slot loads now, before any of this frame's input or services are taken
		if (std::exchange(_quickLoadDue, false))
		{
			RunQuickLoad();
		}
		// and the save game room's load, which leaves the temple as the land is cleared
		if (const auto roomLoad = std::exchange(_roomLoadDue, std::nullopt); roomLoad.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Save game room: loading slot {}", *roomLoad);
			if (!LoadSavedGame(*roomLoad))
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Save game room: slot {} could not be loaded; the game goes on",
				                   *roomLoad);
			}
		}
		auto& profiler = Locator::profiler::value();
		auto& camera = Locator::camera::value();
		auto& config = Locator::config::value();
		auto& deltaTime = frameState.deltaTime;

		// Input events
		{
			auto sdlInput = profiler.BeginScoped(Profiler::Stage::SdlInput);
			// The player's world camera turned with the middle button or both buttons holds the cursor still meanwhile, as
			// far as the camera lets it
			Locator::gameActionSystem::value().AllowCursorFreeze(
			    !(Locator::temple::has_value() && Locator::temple::value().Active()) && !script_camera::Drives() &&
			    dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) != nullptr &&
			    camera_help::IsFeatureEnabled(camera_help::Feature::JustZoom));
			// The land's scripts may take the camera's keys away, as the tutorials do
			Locator::gameActionSystem::value().SetBlockedActions(help::interface_interaction::BlockedActions());
			if (!Locator::debugGui::value().StealsFocus())
			{
				Locator::gameActionSystem::value().Frame();
			}
			const uint32_t inputFrame =
			    Locator::screenshotRequest::has_value() ? Locator::screenshotRequest::value().Frame() : 0;
			const auto dispatch = [inputFrame](const SDL_Event& event) {
				// OPENBLACK_FIXED_MOUSE and OPENBLACK_IGNORE_REAL_INPUT (test runs): the real mouse is not dispatched; while
				// an input recording plays, neither are the real mouse and keys; a recording being made keeps them
				if (!input::DropRealEvent(event.type, input::FixedMouse().has_value(), input::IgnoreRealInput()) &&
				    input::replay::TakeRealEvent(event, inputFrame))
				{
					Locator::events::value().Create<SDL_Event>(event);
				}
			};
			SDL_Event e;
			while (SDL_PollEvent(&e) != 0)
			{
				dispatch(e);
			}
			// an input recording playing (test runs): its events of this frame, after the window's own
			for (const auto& recorded : input::replay::DueEvents(inputFrame))
			{
				Locator::events::value().Create<SDL_Event>(recorded);
			}
			// the playback ends once its last events are dispatched, so that they are still taken as the recording's
			input::replay::EndIfPlayedOut(inputFrame);
			// a recording being made goes into its file every so many frames, so a crash keeps what came before
			input::replay::FlushIfDue(inputFrame);
			// --resume-input: the player takes over with the pointer where the game's cursor is, so the real mouse's first
			// reads agree with the recording's last place (the move this makes is recorded as the player's)
			if (input::replay::TakeHandOver().has_value() && Locator::windowing::has_value())
			{
				const auto cursor = input::GameCursor();
				SDL_WarpMouseInWindow(static_cast<SDL_Window*>(Locator::windowing::value().GetHandle()), cursor.x, cursor.y);
			}
			// While the window is minimised the game goes no further than its window's events: no turn, no frame and no
			// draw until it is restored or the game quits. The queue is emptied before the check, so what came with the
			// restore is handled before the frame goes on. The game's timer keeps counting meanwhile: on the way back the
			// turn scheduler drops more than 2 s of lag and catches up less, one turn a frame
			while (windowing::WaitsForWindow(_windowAway, config.running))
			{
				if (SDL_WaitEvent(&e) == 0)
				{
					break; // an error from SDL: the frame goes on rather than hang
				}
				dispatch(e);
				while (SDL_PollEvent(&e) != 0)
				{
					dispatch(e);
				}
			}
			// A miracle's camera path keeps the camera until a key moving it left, right, forwards or backwards would move it
			// this frame, or the hand grips the land (the interface's hand state 20, Grip Landscape), which give it straight
			// back; meanwhile the player's other camera controls do nothing
			bool pathHoldsCamera = false;
			if (Locator::cameraPathSystem::has_value())
			{
				constexpr int32_t k_GripLandscapeHandState = 20;
				const auto& actions = Locator::gameActionSystem::value();
				auto& cameraPaths = Locator::cameraPathSystem::value();
				cameraPaths.HandlePlayerControl(
				    {.movementKey =
				         actions.GetAny(input::BindableActionMap::MOVE_LEFT, input::BindableActionMap::MOVE_RIGHT,
				                        input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS),
				     .frameMilliseconds = game_clock::CameraFrameMs(),
				     .grippingLand = Locator::handSystem::has_value() &&
				                     Locator::handSystem::value().GetInterfaceHandState() == k_GripLandscapeHandState,
				     .scriptCameraOver = script_camera::HasMode()});
				pathHoldsCamera = cameraPaths.HoldsCamera();
			}
			// the script camera mode has no keys; nor does a hand demo, which sets the camera from its records. Then the
			// camera is told its controls were not read
			if (camera_path::PlayerCameraReadsControls({
			        .scriptCameraDrives = script_camera::Drives(),
			        .handDemoPlaying = hand_demo::IsPlaying(),
			        .pathHoldsCamera = pathHoldsCamera,
			    }))
			{
				camera.HandleActions(deltaTime);
			}
			else
			{
				camera.SkipActions();
			}
			// The room keys are read as raffclar's temple port reads them, during script cameras and hand demos too
			ProcessTempleRoomKeys();
			_shortcutKeys.Update();
			// the leash keys, not while the debug windows have the keyboard (the actions are not framed then, so a key would
			// read as pressed again every frame), nor in the citadel, where the world stands still (ECS/CreatureLoop.h)
			if (!Locator::debugGui::value().StealsFocus() &&
			    !ecs::creature_loop::FrozenInCitadel(game_clock::IsPaused(), game_clock::IsInsideCitadel()))
			{
				ecs::creature_loop::ProcessLeashKeys(Locator::gameActionSystem::value());
			}
			// the quick save and quick load keys, not while the debug windows have the keyboard
			if (!Locator::debugGui::value().StealsFocus())
			{
				const auto& actions = Locator::gameActionSystem::value();
				if (actions.Get(input::BindableActionMap::QUICK_SAVE) &&
				    actions.GetChanged(input::BindableActionMap::QUICK_SAVE))
				{
					ecs::save_triggers::QuickSaveKey();
				}
				// the quick load key, as the quick save's: the load is asked for by a packet, which the next turn applies
				if (actions.Get(input::BindableActionMap::QUICK_LOAD) &&
				    actions.GetChanged(input::BindableActionMap::QUICK_LOAD))
				{
					QuickLoadKeyPressed();
				}
			}
		}
		break;
	}
	case Step::StopCheck:
	{
		auto& config = Locator::config::value();

		// the system's quit (the window closed) waits while a script holds the wide screen, so a cinematic plays on to
		// its end, as the original's loop takes it only then
		if (_systemQuit)
		{
			const auto* helpSystem = help::Get();
			if (game_frame::SystemQuitEndsLoop(true, helpSystem != nullptr && helpSystem->IsScriptWideScreen()))
			{
				config.running = false;
			}
			else if (!std::exchange(_systemQuitHeldLogged, true))
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Quit: the window's close waits for the cinematic to end");
			}
		}

		// a restart asked for in the menu ends this game's frames too (Run starts the next). Leaving the game saves it
		// into the autosave's slot first, as the original's play loop does as it ends (a run of the checks keeps no
		// profile, so nothing is written)
		if (!config.running || _restartRequested)
		{
			// into the profile played as the Players page was last opened, if it was, which is then played for the save
			// (its controls, its push scrolling and its creature's name taken up), then the one played again
			ecs::save_triggers::Exiting(_profileAtPlayersPage, _restartForProfile, [this] {
				TakeProfileControls();
				if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
				{
					simulation::TakePushScrolling(profile->settings);
				}
				NameCreatureAfterProfile();
			});
			_profileAtPlayersPage.clear();
			_restartForProfile = false;
			return false;
		}
		break;
	}
	case Step::DebugGui:
	{
		auto& profiler = Locator::profiler::value();
		auto& config = Locator::config::value();

		// ImGui events + prepare
		{
			auto guiLoop = profiler.BeginScoped(Profiler::Stage::GuiLoop);
			// The debug menu bar comes up with the game's menu, or always without it
			Locator::debugGui::value().SetMenuBarVisible(
			    debug::gui::MenuBarShown(_interface != nullptr, _interface && _interface->GetMenu().IsOpen()));
			if (Locator::debugGui::value().Loop())
			{
				// the debug menu's Exit: the program ends, a list of scenarios included
				config.running = false;
				return false; // Quit event
			}
		}
		break;
	}
	case Step::Editor:
	{
		auto& profiler = Locator::profiler::value();
		auto& deltaTime = frameState.deltaTime;
		// The in-game editor keeps its camera on what it has picked
		if (Locator::editorSystem::has_value())
		{
			auto editor = profiler.BeginScoped(Profiler::Stage::EditorUpdate);
			Locator::editorSystem::value().Update(deltaTime);
		}
		break;
	}
	case Step::Turns:
	{
		auto& profiler = Locator::profiler::value();

		// Update Game Logic in Registry: the game loop's turns before the frame clock and the draw
		{
			auto gameLogic = profiler.BeginScoped(Profiler::Stage::GameLogic);
			// one more pass of the game's loop, before its turns and whether or not the game is paused (the saved games
			// carry the count)
			game_clock::CountFrame();
			// Inside the temple the world is paused (TempleInterior::Activate) and its turns wait, while the temple has a
			// turn of its own every 100 ms of the clock's ticks
			if (game_clock::IsPaused() && game_clock::IsInsideCitadel() && _pausedTurn.Due(game_clock::TickCount()))
			{
				ProcessTempleTurn();
			}
			// while the local timer says a turn is due and fewer than 1 turn this frame (game_clock::TurnDue)
			while (game_clock::TurnDue())
			{
				if (GameLogicLoop())
				{
					return false; // Quit event
				}
			}
		}
		break;
	}
	case Step::SendCommands:
	{
		// the packets sent so far go to the session; the next turn applies them
		game_packets::Flush();
		break;
	}
	case Step::FrameTime:
	{
		// the remainder, the visual clock, the frame's game ms and the fraction of the turn; then the frame's real delta
		// time, as the render's frame start takes it
		game_clock::UpdateFrameClock();
		game_clock::UpdateRealClock();
		break;
	}
	case Step::Camera:
	{
		auto& profiler = Locator::profiler::value();
		auto& camera = Locator::camera::value();
		auto& config = Locator::config::value();
		auto& deltaTime = frameState.deltaTime;
		auto& drawListNearClip = frameState.drawListNearClip;
		{
			auto cameraSection = profiler.BeginScoped(Profiler::Stage::CameraUpdate);
			// the camera's update (with the graphics, after the turns and the frame clock): the script camera mode moves it
			// while it lives, else the player's model, with this frame's game ms
			const auto lastDrawn = camera.GetOrigin(); // the camera drawn the frame before, shake included
			drawListNearClip = DrawListNearClip(lastDrawn);
			camera.SetPixelNearClip(drawListNearClip);
			const auto cameraMs = game_clock::CameraFrameMs();
			// the frame's whole camera ms, which a saved game keeps as the camera's frame time
			script_camera::Get().frameMs = cameraMs;
			if (!script_camera::UpdateCamera(camera, static_cast<float>(cameraMs) * 0.001f, game_clock::FrameGameMs(),
			                                 game_clock::FrameGameSeconds()))
			{
				// Creature Mode keeps the camera on its creature, and takes it or gives it back before it moves
				if (Locator::creatureModeSystem::has_value())
				{
					auto creatureMode = profiler.BeginScoped(Profiler::Stage::CreatureModeUpdate);
					Locator::creatureModeSystem::value().Update(deltaTime,
					                                            {.handGripping = input::HandGripping(_mouseButtons)});
				}
				// a miracle's camera path that has the camera moves it in the player's camera's place
				if (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera())
				{
					Locator::cameraPathSystem::value().Update(deltaTime);
				}
				else
				{
					camera.Update(deltaTime);
				}
			}
			else if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive() &&
			         script_camera::Active())
			{
				// A script's camera mode ends Creature Mode; a dual camera leaves the follow under it
				Locator::creatureModeSystem::value().Leave();
			}
			script_camera::ApplyShake(camera, lastDrawn); // SHAKE_CAMERA on whichever camera is drawn
			// Outside a camera with a lens of its own, as the temple's, the near plane is the frame's one near clip, as the
			// draw list's: from the height of the camera drawn the frame before over the land, but for a script's close
			// shots. Set again whenever it moves, as the game sets it every frame
			if (!camera.GetModel().GetLens().has_value() && Locator::windowing::has_value())
			{
				const float nearClip = drawListNearClip;
				if (nearClip != config.cameraNearClip)
				{
					config.cameraNearClip = nearClip;
					camera.SetProjectionMatrixPerspective(config.cameraXFov, Locator::windowing::value().GetAspectRatio(),
					                                      config.cameraNearClip, config.cameraFarClip);
				}
			}
			// The temple's frame: what its rooms do by themselves, and leaving it when its camera or a key asked to
			if (Locator::temple::has_value())
			{
				Locator::temple::value().Update(deltaTime);
				// Leaving the save game room the game over kept the player in ends the game, as the menu's Quit does
				if (Locator::temple::value().TakeGameEnded())
				{
					config.running = false;
				}
			}
			// The Creature Cave follows the temple into and out of its creature room
			if (Locator::creatureCaveSystem::has_value())
			{
				auto creatureCave = profiler.BeginScoped(Profiler::Stage::CreatureCaveUpdate);
				Locator::creatureCaveSystem::value().Update();
			}
			// The tattoo editor's view turns round the creature and slows
			if (Locator::tattooEditorSystem::has_value())
			{
				Locator::tattooEditorSystem::value().Update(std::chrono::duration<float, std::milli>(deltaTime).count());
			}
		}
		break;
	}
	case Step::Interface:
	{
		auto& deltaTime = frameState.deltaTime;
		if (_interface) // the in-game menus
		{
			// openblack's test hooks (not the original's; off unless the variable is set): OPENBLACK_TEST_MENU_AT=<frame>
			// opens the escape menu, OPENBLACK_TEST_SKIP_ANSWER=<answer>,<frame> answers the SkipBox (screenshots without a
			// keyboard or mouse)
			const uint32_t frame = Locator::screenshotRequest::value().Frame();
			// a list's land run gives its menu= in place of the variable, at the frame of its own run
			const auto menuAt = _landRun.has_value() && _landRun->menuAt.has_value() ? _landRun->menuAt : _testMenuAt;
			if (menuAt.has_value() && frame == *menuAt)
			{
				_interface->GetMenu().Open();
			}
			// a list's land run gives its skip= in place of the variable, at the frame of its own run
			if (_landRun.has_value() && _landRun->skipAnswer.has_value())
			{
				if (frame == _landRun->skipAnswer->frame)
				{
					_interface->AnswerSkipBox(_landRun->skipAnswer->answer);
				}
			}
			else if (const char* skip = std::getenv("OPENBLACK_TEST_SKIP_ANSWER"); skip != nullptr)
			{
				const std::string spec(skip);
				const auto comma = spec.find(',');
				if (comma != std::string::npos &&
				    frame == static_cast<uint32_t>(std::strtoul(spec.substr(comma + 1).c_str(), nullptr, 10)))
				{
					_interface->AnswerSkipBox(std::atoi(spec.substr(0, comma).c_str()));
				}
			}
			_interface->Update(std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
			HandleInterfaceAction();
		}
		break;
	}
	case Step::Video:
	{
		// the full screen film's frame (Video/VideoPlayer.h), paced by the wall
		// clock (the game is paused while it plays)
		video::Get().Process(game_clock::FrameRealMs());
		// the falling spell's film (its update, its end at state 4 or without a film) and the temple's fade, with the real
		// delta time (Video/FallingSpellVideo.h)
		video::GetFallingSpell().ProcessFrame(game_clock::FrameRealMs());
		break;
	}
	case Step::FrameEffects:
	{
		// what the frame moves on in the world, by the frame's game time (Simulation/FrameBody.h)
		simulation::PlayFrameEffects(frameState.deltaTime);
		break;
	}
	case Step::TextShot:
	{
		// OPENBLACK_TEST_TEXT_SHOT (openblack only): the screenshot after the text
		if (_textShotAtMs.has_value() && static_cast<uint32_t>(SDL_GetTicks()) >= *_textShotAtMs)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_TEXT_SHOT: {}", _textShotPath);
			Locator::screenshotRequest::value().Request(_textShotPath);
			_textShotAtMs = UINT32_MAX; // once
		}
		break;
	}
	case Step::HandDemo:
	{
		auto& camera = Locator::camera::value();
		// A hand demo (Input/HandDemo, played by the interface): the recorded mouse, on the picture between the cinema bars
		// (the Hand menu's debug switch spreads it over the whole screen, as before), buttons and camera.
		// SetPositionAndFocus sets the camera's zoomers (here the script camera's) and the drawn camera; the camera's own
		// update is skipped while it plays, so the record wins. The original runs it in the turn (with the game inputs),
		// before the frame's graphics (and the help system's Draw3D): its camera is set before the spirits are laid out and
		// drawn with it. Both passes; (approximate) here once a frame before Draw3D, where the original's frame pass (the
		// frame updates) reaches the screen a frame later
		{
			const auto screenSize =
			    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
			if (screenSize.x > 0 && screenSize.y > 0)
			{
				if (const auto demo = hand_demo::Update(game_clock::VisualMs()); demo)
				{
					const bool wholeScreen = Locator::config::has_value() && Locator::config::value().handDemoWholeScreenCursor;
					input::GameCursorRef() = wholeScreen ? glm::ivec2(glm::vec2(screenSize) * demo->mouse + 0.5f)
					                                     : hand_demo::CursorPixel(demo->mouse, screenSize,
					                                                              CinematicDirector().GetWideScreenFraction());
					input::HandButtonsRef() = {.gripping = demo->grip, .action = demo->action, .grab = demo->grip};
					// each recorded mouse move is a sample of the gestures at its own pixel, as the playback sends it through
					// the interface's mouse messages; the real mouse gives none while a demo plays
					for (const auto& move : demo->moves)
					{
						magic::gestures::FeedSample(
						    wholeScreen
						        ? glm::ivec2(glm::vec2(screenSize) * move + 0.5f)
						        : hand_demo::CursorPixel(move, screenSize, CinematicDirector().GetWideScreenFraction()));
					}
					if (demo->eye && demo->focus)
					{
						script_camera::SetPositionAndFocus(*demo->eye, *demo->focus);
						camera.SetOrigin(*demo->eye);
						camera.SetFocus(*demo->focus);
					}
				}
			}
		}
		break;
	}
	case Step::ObjectDrawList:
	{
		auto& camera = Locator::camera::value();
		auto& drawListNearClip = frameState.drawListNearClip;
		// The landscape's object draw list, now that the camera is as it is drawn. Not inside the temple, under the falling
		// spell's film or behind a full screen film, which draw no landscape; it runs while the game is paused. Then the
		// script highlights' effects that it did not step are not drawn (the sparkles it queued step in the PreDraw)
		const bool landDrawn = !(Locator::temple::has_value() && Locator::temple::value().Active()) &&
		                       !video::Get().CoversScreen() && !video::GetFallingSpell().HidesWorld();
		if (landDrawn)
		{
			// the landscape's draw runs the knocked town's read-out down by the frame's game time first, with this frame's
			// camera's right: row 0 of the inverse of its world to camera matrix, as the camera's update inverts it
			if (Locator::abodeKnockSystem::has_value())
			{
				const auto cameraToWorld = affine::Inverse(affine::WorldToCamera(camera.GetOrigin(), camera.GetFocus()));
				const glm::vec3 right {cameraToWorld.m[0], cameraToWorld.m[1], cameraToWorld.m[2]};
				Locator::abodeKnockSystem::value().Update(std::chrono::milliseconds(game_clock::FrameGameMs()), right);
			}
			_objectListRan = UpdateObjectDrawList(camera, drawListNearClip);
			// then, at the end of the objects' draw, the apple barrel goes to the local player's storage pit nearest their
			// temple and its worm wriggles on by the frame's game ms
			if (Locator::appleBarrelSystem::has_value())
			{
				Locator::appleBarrelSystem::value().Draw(game_clock::FrameGameMs());
			}
			// then the bookmarks' shared pulse moves on by the frame's game ms and sizes the markers, which a script's wide
			// screen hides
			Locator::cameraBookmarkSystem::value().Update(game_clock::FrameGameMs(),
			                                              help::Get() != nullptr && help::Get()->IsScriptWideScreen());
		}
		// the worship sites' rings of light, stood and moved on by the altars the list drew on the screen, by the frame's
		// real time; with no land drawn they are all hidden
		worship::site::DrawAltarRings(landDrawn && _objectListRan ? Locator::objectDrawListSystem::value().OnScreenThisFrame()
		                                                          : std::span<const entt::entity> {},
		                              static_cast<float>(game_clock::FrameRealMs()) * 0.001f);
		// the villagers' names, placed as the list draws the villagers, then the names display's step
		PlaceVillagerNames(landDrawn && _objectListRan, landDrawn);
		break;
	}
	case Step::DrawnFrameEnd:
	{
		ecs::script_highlight::EndDrawnFrame();
		// the need signs that no building's draw drew this frame are not drawn
		ecs::show_needs::EndDrawnFrame();
		// and the plinths' stones that no Draw placed this frame are not drawn
		if (Locator::animatedStaticSystem::has_value())
		{
			Locator::animatedStaticSystem::value().EndDrawnFrame();
		}
		break;
	}
	case Step::Draw3D:
	{
		// the help system's Draw3D: the texts' slide-in and the click cue's fade, with the frame's game ms, or the real
		// delta time in the citadel
		if (auto* dialogue = TheDialogue(); dialogue != nullptr)
		{
			const bool citadel = game_clock::IsInsideCitadel();
			const auto resolution = Locator::rendererInterface::value().GetResolution();
			const int bars =
			    gui::CinemaBars::BarHeight(resolution.x, resolution.y, CinematicDirector().GetWideScreenFraction());
			dialogue->Draw3D(static_cast<float>(citadel ? game_clock::FrameRealMs() : game_clock::FrameGameMs()),
			                 help::ComputeTextRegion(resolution.x, resolution.y, bars), resolution.x);
		}
		break;
	}
	case Step::Advisors:
	{
		// in Draw3D: the advisors' control (the real delta time x 0.001, 0.4), their flight, anims and pose; drawn by
		// Renderer::DrawSpirits. Each advisor's delayed sentence starts in its update, and the sound part of its lip sync
		// runs there too, in the same frame as its mouth and tags (ours reads its frame itself, so the slot's Frame is left
		// empty)
		if (auto& advisors = TheAdvisorSystem(); advisors.IsLoaded())
		{
			advisors.Update({});
		}
		break;
	}
	case Step::InputPrompts:
	{
		// the key / mouse icons' update twice a frame with the same real delta time x 0.001:
		// #1 at the end of Draw3D (after the advisors' control and their draw),
		// #2 at the end of the help text's end-of-frame draw. Each icon keeps its alpha per pass (a1, a2) for
		// Renderer::DrawInputPrompts
		{
			const float inputPromptSeconds = static_cast<float>(game_clock::FrameRealMs()) * 0.001f;
			help::input_prompt::Frame(inputPromptSeconds, 0);
			help::input_prompt::Frame(inputPromptSeconds, 1);
		}
		break;
	}
	case Step::Hand:
	{
		auto& profiler = Locator::profiler::value();
		auto& camera = Locator::camera::value();
		auto& config = Locator::config::value();
		auto& deltaTime = frameState.deltaTime;
		auto& drawListNearClip = frameState.drawListNearClip;

		// Update Uniforms
		{
			auto profilerScopedUpdateUniforms = profiler.BeginScoped(Profiler::Stage::UpdateUniforms);

			// Update Hand and intersection point
			ecs::components::Transform intersectionTransform {};
			// The hand's state change comes first in its draw preparation: the hand's point under the cursor and the
			// interface's frame pass (which follows the draw) see this frame's state
			Locator::handSystem::value().PrepareState();
			// The interface's frame pass: while a box takes the input (the escape menu, the SkipBox, a debug window that took
			// a click), the buttons are cleared and the hand's action ends, every frame (Input/MouseButtons)
			{
				const bool middleWasHeld = _mouseButtons.middle;
				const bool menuUp = _interface && _interface->IsBoxActive();
				bool boxHasMouse = false;
				const bool boxTookInput = input::BoxTakesInput(_mouseButtons, menuUp);
				if (boxTookInput)
				{
					if (middleWasHeld)
					{
						LetGoOfMiddleButton(_mouseButtons.middlePressPosition);
					}
					UpdateHandButtons(_mouseButtons);
					// (openblack's own guard) not while a demo the test hook started plays: it plays from the land's
					// start, under the SkipBox. A demo a script plays gets the rule
					if (hand_demo::BoxRuleReachesHand(hand_demo::IsPlaying(), hand_demo::IsTestHookPlaying()))
					{
						Locator::handSystem::value().BoxTakesInput();
						boxHasMouse = true;
					}
				}
				// what the interface's passes read until the next frame's (the gestures wipe their path while it holds)
				interface_active::SetBoxHasMouse(boxHasMouse);
				// then the help system's interface pass, only when no box took the input (a box's frame ends the pass
				// before it, and its click is lost): the click since the last one, and its key, held, which cuts the text
				// with no click
				auto* dialogue = TheDialogue();
				if (!boxTookInput && dialogue != nullptr)
				{
					// a click or key that cut the text is taken from the hand: its action ends for this pass
					if (help::dialogue_click::TakesClickFromHand(
					        dialogue->ProcessClick(std::exchange(_dialogueClick, false))) &&
					    Locator::handSystem::has_value())
					{
						Locator::handSystem::value().TextTakesClick();
					}
				}
				else
				{
					_dialogueClick = false;
				}
			}
			{
				const auto screenSize =
				    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
				const auto scale = glm::vec3(50.0f, 50.0f, 50.0f);
				if (screenSize.x > 0 && screenSize.y > 0)
				{
					// (the hand demo's mouse, buttons and camera are set above, before the help system's Draw3D)
					// OPENBLACK_FIXED_MOUSE (test runs): the hand's ray from the fixed position (no SDL_MOUSEMOTION reaches
					// ProcessEvents to set it)
					// Test hook: fixed cursor at a fraction of the window ("0.5,0.6"), for screenshots without the real mouse.
					// Neither fixed cursor holds while a hand demo plays: the playback is the mouse then, as it set it above
					auto& cursor = input::GameCursorRef();
					cursor = input::FrameCursor(cursor, input::FixedMouse(), input::MouseAtPixel(input::MouseAt(), screenSize),
					                            hand_demo::IsPlaying());
					auto rayCast = profiler.BeginScoped(Profiler::Stage::HandRayCast);
					// the mouse ray starts at the eye, as the original's, towards the cursor's pixel on the near plane
					auto [rayOrigin, rayDirection] =
					    camera.RayFromEye(static_cast<glm::vec2>(cursor) / static_cast<glm::vec2>(screenSize));
					rayDirection = screen_point::RayDirection(
					    rayOrigin, camera.PixelNearPoint(rayOrigin, screenSize, cursor, drawListNearClip));
					// A hand a scenario drives (the testbed's casts through the hand) looks along its own line, at its own
					// point
					const auto driven = TheMagicSystem().GetDrivenHand();
					if (driven.has_value())
					{
						rayOrigin = driven->rayOrigin;
						rayDirection = driven->rayDirection;
					}
					auto& pickingSystem = Locator::pickingSystem::value();

					if (!glm::any(glm::isnan(rayOrigin) || glm::isnan(rayDirection)))
					{
						// ObtainRequiredHandPosition: the hand goes along the mouse ray to the surface under the cursor
						// (an object's mesh or the land), smoothed by the hand distance zoomer.
						{
							// The hand's own land: the land or near sea along the line from the eye through the cursor's
							// pixel on the near plane, moved forward along itself by the near clip and one more
							const auto handLand =
							    driven.has_value()
							        ? HandCursorLine {.nearPoint = driven->point.value_or(rayOrigin), .land = driven->point}
							        : HandLandUnderCursor(camera, rayOrigin, screenSize, cursor, drawListNearClip,
							                              pickingSystem);
							// The interface's land, against which it weighs the objects under the cursor: the same line
							// unmoved, the sea wherever it goes down, kept within reach of the map's middle
							const auto interfaceLand = driven.has_value()
							                               ? driven->point
							                               : pickingSystem.LandUnderPixel(rayOrigin, handLand.nearPoint, true);
							const auto point = Locator::handSystem::value().ResolveCursorPoint(
							    rayOrigin, rayDirection,
							    {.hand = handLand.land, .interface = interfaceLand, .nearPoint = handLand.nearPoint},
							    input::HandButtonsRef().gripping, deltaTime);
							intersectionTransform.position = point.value_or(glm::zero<glm::vec3>());
						}
					}
					intersectionTransform.scale = scale;
				}

				// Hand animation (hh.HBN): Cwiggle / Cgrip + L*_lr / L*_fb layers driven by the cursor motion.
				{
					auto handUpdate = profiler.BeginScoped(Profiler::Stage::HandUpdate);
					const auto mouseDelta = glm::vec2(input::MouseMotion(_mouseMotion, input::GameCursorRef()));
					// the Action button's double click, taken once a frame: kept from the hand while the temple's interior is
					// up, and while a hand demo plays its own buttons
					const bool templeUp = Locator::temple::has_value() && Locator::temple::value().Active();
					if (input::TakeActionDoubleClick(_mouseButtons, templeUp) && !hand_demo::IsPlaying())
					{
						Locator::handSystem::value().ActionDoubleClick();
					}
					// a press of the Action button while the action is still held, taken once a frame
					if (input::TakeActionPressWhileHeld(_mouseButtons) && !hand_demo::IsPlaying())
					{
						Locator::handSystem::value().ActionPressedWhileHeld();
					}
					Locator::handSystem::value().SetGrabHeld(input::HandButtonsRef().grab);
					Locator::handSystem::value().Update(deltaTime, mouseDelta, input::HandButtonsRef().gripping,
					                                    input::HandButtonsRef().action);
				}
				// The miracles' per-frame part (the one-shot orbs' texture), in game time
				{
					auto magicFrame = profiler.BeginScoped(Profiler::Stage::MagicFrame);
					TheMagicSystem().Update(game_clock::FrameGameSeconds());
				}

				// Palm towards the ground, index fingertip on the point under the cursor, fingertips dug in while gripping.
				const bool overLand = intersectionTransform.position != glm::zero<glm::vec3>();
				auto handPlace = profiler.BeginScoped(Profiler::Stage::HandPlace);
				Locator::handSystem::value().Place(overLand ? std::optional(intersectionTransform.position) : std::nullopt,
				                                   camera.GetForward(), input::HandButtonsRef().gripping, deltaTime);

				// after the landscape and the hand are placed, the hand's point (written by the landscape's draw) is tested
				// against the influence circles unless the game is paused: crossing a player's influence circle plays the
				// crossing sound. Then every ripple the crossings made is updated (life -= the frame's game ms, which is 0
				// while paused), outside the pause test.
				Locator::influenceSystem::value().Update(std::chrono::duration<float, std::milli>(game_clock::FrameGameMs()));
				// the hand's draw, past its mesh: what the hand keeps of its influence past the border shows with the
				// placed hand, its hum and its mana path's sparks
				{
					const auto& hand = Locator::handSystem::value();
					Locator::influenceSystem::value().DrawVirtualInfluence(glm::vec3(hand.GetHandMatrix()[3]),
					                                                       game_clock::FrameGameMs(), hand.DrawsHandEffects());
				}
			}
			// the leashes' ropes swing from where the hand now is, in the citadel too (only their drawing stops there)
			ecs::creature_loop::UpdateLeash(game_clock::FrameGameSeconds(), profiler);
			// the trees' bends, as the landscape's draw makes them after the hand is prepared for drawing: the object in the
			// hand bends them from where this frame's update and placement put it
			ecs::UpdateTreeBends();
			UpdateHandInterface();

			// Update Entities
			{
				auto updateEntities = profiler.BeginScoped(Profiler::Stage::UpdateEntities);
				// Not while a full screen film hides the world: the scene is not drawn (the same test as Renderer::PreDraw
				// and DrawScene, and the film only starts or ends before this point of the frame), and only the draw reads
				// what PrepareDraw writes. Its dirty flags stay set, so the first frame after the film writes the instances
				// from the world as it is then
				const bool worldHidden = video::Get().CoversScreen() || video::GetFallingSpell().HidesWorld();
				if (ecs::systems::ShouldPrepareDraw(config.drawEntities, worldHidden))
				{
					Locator::rendereringSystem::value().PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths,
					                                                config.drawStreams);
				}
			}
		} // Update Uniforms
		break;
	}
	case Step::Audio:
	{
		auto& profiler = Locator::profiler::value();

		// Update Audio
		{
			auto updateAudio = profiler.BeginScoped(Profiler::Stage::UpdateAudio);
			// the sample main volume of the configuration, live (the options dialog's slider)
			audio::UpdateFrame();
			audio::music::Update();
		} // Update Audio
		break;
	}
	case Step::_Count:
		break;
	}
	return true;
}

bool Game::WriteVersionFile() const noexcept
{
	if (!Locator::filesystem::has_value())
	{
		return false;
	}
	auto& fileSystem = Locator::filesystem::value();
	// the sum of the program file's bytes, read whole, else the value that says it could not be read
	auto checksum = version_file::k_ChecksumError;
	try
	{
		checksum = version_file::Checksum(fileSystem.ReadAll(_programFile));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The program's file {} could not be read: {}", _programFile.generic_string(),
		                   e.what());
	}
	const auto text = version_file::Text(checksum);
	const auto path = _workingFolder / version_file::k_FileName;
	const bool written =
	    resources::WriteFile(fileSystem, path, std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "VERSION: {} {} {}", path.generic_string(), written ? "holds" : "could not take",
	                   text);
	return written;
}

bool Game::Initialize() noexcept
{
	auto& config = Locator::config::value();
	_startupTimer.emplace("startup", _launchTime);
	_startupTimer->Step("launch");

	if (config.graphicsBackend != GraphicsBackend::Noop)
	{
		uint32_t extraFlags = 0;
		if (config.graphicsBackend == GraphicsBackend::Metal)
		{
			extraFlags |= SDL_WINDOW_METAL;
		}
		// Dev tooling, not original: OPENBLACK_WINDOW_TAG names the window so several running copies can be told apart
		// ("openblack [tag]").
		std::string windowTitle {k_WindowTitle};
		if (const char* tag = std::getenv("OPENBLACK_WINDOW_TAG"); tag != nullptr && *tag != '\0')
		{
			windowTitle += std::string(" [") + tag + "]";
		}
		openblack::InitializeWindow(windowTitle, config.resolution.x, config.resolution.y, config.displayMode, extraFlags);
	}

	using filesystem::Path;
	// The services the simulation needs first of all come first, then the services that show the world
	InitializeSimulationCore(StartKind::Launch);
	// the game's VERSION switch writes its file as the game starts, and the game carries on
	if (_writeVersionFile)
	{
		WriteVersionFile();
	}
	// drawn by the engine's start-up, kept with the game's resources
	std::array<uint8_t, 256> fontHashPermutation {};
	if (!InitializeEngine(StartKind::Launch, config.graphicsBackend, config.vsync, fontHashPermutation))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize engine services.");
		return false;
	}
	_startupTimer->Step("window, renderer and audio device");
	// a list of scenarios keeps the debug windows' settings as the launch finds them, before its first frame reads them
	if (!_scenarioList.empty())
	{
		_debugGuiStartSettings = Locator::debugGui::value().ReadStartSettings();
	}
	ConnectToEngine();
	auto& fileSystem = Locator::filesystem::value();

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
	// the game's SETTINGS: switch: its file, named from the working folder, is read once as the game starts; a file
	// that is not there leaves the game's own quest folder
	if (!_settingsFile.empty())
	{
		const auto text = fileSystem.ReadUserFile(_workingFolder / _settingsFile);
		if (text.has_value())
		{
			_questFolder =
			    settings_file::QuestFolder(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "SETTINGS: {} {}, the quest folder {}", _settingsFile,
		                   text.has_value() ? "read" : "not found", _questFolder);
	}

	fileSystem.SetGamePath(_gamePath);
	// the switched-on data mods' folders, looked in first for a file that is read (none without the mod loader)
	if (Locator::modLoader::has_value() && !Locator::modLoader::value().modRoots.empty())
	{
		for (const auto& root : Locator::modLoader::value().modRoots)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Data mod folder: {}", root.generic_string());
		}
		fileSystem.SetModRoots(Locator::modLoader::value().modRoots);
	}

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The GamePath is \"{}\".", fileSystem.GetGamePath().generic_string());

	// The player profiles, before the services that read them: a person's are kept in the Profiles folder of the
	// working directory, never in the game's folder; a run of the checks plays one profile made from the command line
	// and reads and writes nothing
	InitializePlayerProfiles(player_profiles::StoreFor({
	    .profileFolder = _profileFolder,
	    .workingFolder = _workingFolder,
	    .checkRun = config.checkRun,
	    .runProfile = player_profiles::RunProfile(GreetingName(), config.profileCreatureFile, config.rightHandedHand),
	}));
	// The main volumes kept for this computer, next to the profiles, as the audio starts (none in a run of the checks)
	audio::main_volumes::Read();

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
		// No land asked for: each game starts on the land the map script names, as a new game reads it
		_startOnMapScriptLand = _startMap == std::filesystem::path(k_DefaultStartLevel);
		_startMap = fileSystem.GetPath<Path::Scripts>() / _startMap;
	}

	if (!InitializeGame(StartKind::Launch, fontHashPermutation))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize game services.");
		return false;
	}
	// What the help's turn reads of the game: a box holding the mouse stops it, and the player's own camera tells it
	// whether the middle of the screen is on the land, how far that is and how far down the camera looks
	help_profile::SetQueries({
	    .processBlocked = [] { return interface_active::BoxHasMouse(); },
	    .playerCamera = []() -> std::optional<help_profile::CameraView> {
		    if (!Locator::camera::has_value() || script_camera::Drives() ||
		        (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera()))
		    {
			    return std::nullopt;
		    }
		    const auto& camera = Locator::camera::value();
		    const auto centre = camera.GetModel().GetScreenCentre();
		    if (!centre.has_value())
		    {
			    return std::nullopt;
		    }
		    float heading = 0.0f;
		    float pitch = 0.0f;
		    script_camera::HeadingAndPitchFromPoints(camera.GetOrigin(), camera.GetFocus(), heading, pitch);
		    return help_profile::CameraView {
		        .screenCentreOnLand = centre->onLand, .headingDistance = centre->headingDistance, .pitch = pitch};
	    },
	});
	// A deterministic run: every random number of openblack's own generators from the seed, the date pinned
	if (_seed.has_value())
	{
		Locator::rng::value().SetRunSeed(*_seed);
		Locator::time::value().RestartClock(ecs::systems::TimeSystemInterface::k_DeterministicDate);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Seeded run: seed {}, date pinned", *_seed);
	}
	_startupTimer->Step("game services and shaders");

	// The logos, before the rest loads
	PlayStartupScreens();

	// Every resource of the game's data. The game is the view of the start.
	return simulation::RegisterWorldData(*this);
}

void Game::CreateInterface()
{
	// the options box's controls: the sliders at the main volumes times the float 1/127; the detail at the running
	// level; the left-handed box as the hand is drawn
	const auto& config = Locator::config::value();
	gui::MenuSettings settings {
	    .sfxVolume = gui::SliderOfVolume(config.audioSampleMainVolume),
	    .musicVolume = gui::SliderOfVolume(config.audioMusicMainVolume),
	    .detail = graphics::detail_level::MenuEntryOf(config.detailLevel),
	    .leftHandedHand = !config.rightHandedHand,
	};
	// without a profile value, the help system's read speed
	if (const auto* dialogue = TheDialogue(); dialogue != nullptr)
	{
		settings.helpTextSpeed = dialogue->GetReadSpeed();
	}
	// The menu greets the player by their profile's name, with their creature's name, symbol and options; a run of the
	// checks plays its run profile, whose values are the game's own. The hand stays as it is drawn
	auto playerName = GreetingName();
	std::optional<player_profiles::Settings> kept;
	if (Locator::playerProfileSystem::has_value())
	{
		if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
		{
			playerName = gui::ToUtf16(profile->name);
			settings.creatureName = gui::ToUtf16(profile->creatureName);
			settings.symbol = profile->symbol;
			const bool leftHanded = settings.leftHandedHand;
			gui::TakeProfileSettings(settings, profile->settings);
			settings.leftHandedHand = leftHanded;
			kept = profile->settings;
		}
	}
	if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().SetTexts(nullptr);
	}
	_interface = gui::GameInterface::Create(playerName, settings);
	if (!_interface)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The in-game menus are not available, Escape quits");
	}
	if (kept.has_value())
	{
		ApplyPlayerSettings(*kept);
	}
	RefreshPlayersPage();
	// the help texts are read from its text database, which read the same script as the help text cache; without the
	// interface they are still read from the cache
	if (_interface && Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().SetTexts(&_interface->GetTexts());
	}
	// the temple writes its scrolls and signs with the interface's text and font, or none without it
	if (Locator::temple::has_value())
	{
		Locator::temple::value().SetInterface(_interface.get());
	}
	// the Game Statistics box takes the players' figures as it opens
	if (_interface)
	{
		_interface->GetMenu().SetStatisticsSource([] {
			const auto local =
			    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
			return std::pair(ecs::player_statistics::Box(), local);
		});
		// and so does the end box's Statistics tab
		_interface->GetEndGameBox().SetStatisticsSource([] {
			const auto local =
			    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
			return std::pair(ecs::player_statistics::Box(), local);
		});
		// and the volume sliders are set from the main volumes each time an options page shows: the kept volumes are
		// read first, after the box just left (a tab between its pages) has kept its own, as the original's box shown
		// reads the computer's setup before it sets its controls
		_interface->GetMenu().SetVolumeSource([](bool boxLeft) {
			if (boxLeft)
			{
				audio::main_volumes::Keep();
			}
			audio::main_volumes::Read();
			const auto& volumes = Locator::config::value();
			return std::pair(volumes.audioSampleMainVolume, volumes.audioMusicMainVolume);
		});
		// and the options' choices are taken again from the profile played each time an options page shows
		_interface->GetMenu().SetProfileSource([]() -> std::optional<player_profiles::Settings> {
			if (!Locator::playerProfileSystem::has_value())
			{
				return std::nullopt;
			}
			const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
			return profile.has_value() ? std::optional(profile->settings) : std::nullopt;
		});
	}
	// and the Creature Cave's screen with its text
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetInterface(_interface.get());
	}
	// and the hand writes the tribes' names with its font
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SetInterface(_interface.get());
	}
	// and the Controls page and the action map take the profile played's bindings, as the original's profile is set
	TakeProfileControls();
}

void Game::StartAudio()
{
	// the audio's start: the sample main volume and the channels' queries, then its banks (every .sad of the Audio
	// directory, audio::banks)
	audio::Init(MakeMusicQueries(*this));
	// the game's NOLOADMUSIC switch: none of the music is registered
	if (_noLoadMusic)
	{
		audio::banks::SkipMusicBanks();
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "NOLOADMUSIC: no music is loaded");
	}

	// The voices of the help texts, rebuilt from the wave names of villagers, HelpSprites and Guidance
	audio::voices::BuildTable();

	// the music player on the audio device (started with the audio)
	audio::music::Start();
}

void Game::StartHelp()
{
	// the game music (its banks), with the town distances of info.dat
	{
		const auto& sound = Locator::infoConstants::value().sound;
		audio::game_music::Start(MakeMusicQueries(*this), {sound.townTriggerDistance, sound.townTriggerOffDistance});
	}

	// the guidance's HELP_SPRITES_GUIDANCE lists (info.dat GHelpSpritesGuidance)
	{
		std::array<audio::guidance::SpriteList, audio::guidance::k_SpriteLists> lists {};
		const auto& guidance = Locator::infoConstants::value().helpSpritesGuidance;
		for (size_t k = 0; k < lists.size() && k < guidance.size(); ++k)
		{
			lists.at(k) = guidance.at(k).texts;
		}
		audio::guidance::SetSpriteLists(lists);
	}
	// the spooky voices, at the game's one-time start (the help texts and the voice table are ready)
	audio::spooky::Init();

	// when the help system is made: the advisors' control with the HelpSprites bank for both advisors; the models
	// MarkGood.Hd / MarkEvil.Hd are the advisors' Load below
	TheHelpTextSystem().InitVoices(audio::Bank(audio::SfxBank::HelpSprites));

	// the help system with HelpSystemInfo of info.dat
	{
		const auto& helpInfo = Locator::infoConstants::value().helpSystem;
		help::HelpSystem::Queries queries;
		// the game turn
		queries.turn = []() { return game_clock::Turn(); };
		// the engine timer's ms
		queries.nowMs = []() { return game_clock::EngineMs(); };
		// inside the citadel and paused
		queries.citadelClock = []() { return game_clock::IsInsideCitadel() && game_clock::IsPaused(); };
		// the task's script type
		queries.taskScriptType = [](uint32_t task) -> uint32_t {
			return Locator::vm::has_value() ? static_cast<uint32_t>(Locator::vm::value().GetTaskScriptType(task)) : 1;
		};
		// the voice bank is loaded: registered, and the sound device installed (without one no bank is registered)
		queries.voiceBankLoaded = [](audio::SfxBank bank) {
			return help::VoiceBankLoaded(audio::voices::BankRegistered(bank), audio::SoundExists());
		};
		// an advisor is talking
		queries.advisorsTalking = []() { return TheHelpTextSystem().GetVoices().AnyTalking(); };
		// the owner's sample of the bank is playing
		queries.isPlaying = [](audio::SfxBank bank, audio::VoiceOwner owner, uint32_t sample) {
			return audio::IsPlaying(audio::Owner::Key(static_cast<uint32_t>(owner)), static_cast<int>(sample), bank);
		};
		// the screen height the help text is made with (its line height is H / 30)
		queries.screenHeight = []() { return Locator::windowing::has_value() ? Locator::windowing::value().GetSize().y : 480; };
		// the interface plays a hand demo (no click on a text during a hand demo)
		queries.playBack = []() { return hand_demo::IsPlaying(0); };
		// the dialogue's key, the keypad's Enter, held down: it takes the text as read with no click, at once
		queries.skipKey = []() {
			return Locator::gameActionSystem::has_value() &&
			       Locator::gameActionSystem::value().IsKeyHeld(SDL_SCANCODE_KP_ENTER);
		};
		// a full screen film is open (the dialogue is not drawn and does not slide in)
		queries.filmPlaying = []() { return video::IsPlaying(); };
		help::HelpSystem::Hooks hooks;
		// the text's voice
		hooks.sayVoice = [](uint32_t /*textId*/, help::VoiceRoute route, audio::TextVoice voice) {
			audio::voices::RunTextVoice(route, voice);
		};
		// OPENBLACK_TEST_TEXT_SHOT="textId,path[,ms]" (openblack only): a screenshot `ms` ms (1000 by default, wall clock)
		// after that text is shown (the game loop takes it, see _textShotAtMs)
		if (const char* shot = std::getenv("OPENBLACK_TEST_TEXT_SHOT"); shot != nullptr)
		{
			const std::string spec(shot);
			const auto comma = spec.find(',');
			const auto comma2 = comma == std::string::npos ? std::string::npos : spec.find(',', comma + 1);
			if (comma != std::string::npos)
			{
				const auto id = static_cast<uint32_t>(std::strtoul(spec.substr(0, comma).c_str(), nullptr, 10));
				_textShotPath = spec.substr(comma + 1, comma2 == std::string::npos ? std::string::npos : comma2 - comma - 1);
				const uint32_t delay = comma2 == std::string::npos
				                           ? 1000u
				                           : static_cast<uint32_t>(std::strtoul(spec.c_str() + comma2 + 1, nullptr, 10));
				hooks.textStarted = [this, id, delay](uint32_t textId) {
					if (textId == id && !_textShotAtMs.has_value())
					{
						_textShotAtMs = static_cast<uint32_t>(SDL_GetTicks()) + delay;
					}
				};
			}
		}
		// a click cuts the villagers' narration with the 20 ms ramp
		hooks.stopVoicesOnClick = []() { audio::voices::CutByClick(); };
		// a spirit's stop goes to the advisors' control with dude = (the spirit's type != 1) (inferred: the good spirit
		// has type 1 and dude 0)
		// the spirit's way home (Help/Spirits.h SpiritHome)
		hooks.spiritHome = [](int32_t spirit, int32_t arg) {
			if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
			{
				Locator::advisorSystem::value().GetController().SpiritHome(spirit, arg != 0);
			}
		};
		hooks.spiritOut = [](int32_t spirit) {
			return Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded() &&
			       !Locator::advisorSystem::value().GetController().IsHome(
			           help::spirits::AdvisorSpiritController::DudeOf(spirit));
		};
		hooks.dialogueControlled = []() {
			return Locator::dialogueControlSystem::has_value() && Locator::dialogueControlSystem::value().GetOwner() != 0;
		};
		hooks.interruptAdvisors = []() { TheHelpTextSystem().InterruptAdvisors(); };
		// the help system's wide screen: as the bars come in, whoever holds them, every dialog is put away first; the
		// bars slide in HelpSystemInfo.wideScreenTime seconds from where they are; the interface is active unless on
		// with an owner (the owner kept is on ? owner : 0), and the owning task while on is what the sound effects
		// read to skip the user-param-1 samples
		hooks.wideScreen = [this, time = helpInfo.wideScreenTime](bool on) {
			if (on && _interface)
			{
				_interface->HideAllDialogs();
			}
			CinematicDirector().SetWideScreen(on, time);
			const auto* helpSystem = help::Get();
			interface_active::SetActive(
			    interface_active::ActiveWithWideScreen(on, helpSystem != nullptr ? helpSystem->GetWideScreenOwner() : 0));
			audio::SetScriptWideScreen(helpSystem != nullptr && helpSystem->IsScriptWideScreen());
		};
		// the help system's reset puts the bars where they are going at once
		hooks.snapWideScreen = []() { CinematicDirector().SnapWideScreen(); };
		help::Start(TheHelpTextSystem(), {helpInfo.readDefaultAdjustGTTime, helpInfo.readDefaultWordGTTime}, std::move(queries),
		            std::move(hooks));
	}
	// what the help system and its texts do as the dialogue changes hands (the dialogue control holds its owner); nothing
	// without a help system
	if (Locator::dialogueControlSystem::has_value())
	{
		ecs::systems::DialogueControlSystemInterface::Hooks dialogueHooks;
		dialogueHooks.sendSpiritsHome = [](bool helpScript) {
			if (auto* helpSystem = help::Get(); helpSystem != nullptr)
			{
				helpSystem->SpiritHome(1, helpScript ? 1 : 0);
				helpSystem->SpiritHome(2, helpScript ? 1 : 0);
			}
		};
		dialogueHooks.taken = []() { TheHelpTextSystem().ClearAllText(); };
		dialogueHooks.released = [](bool helpScript) {
			// the advisors put away, then the texts cleared
			if (auto* helpSystem = help::Get(); helpSystem != nullptr)
			{
				helpSystem->OnDialogueReleased(helpScript);
			}
			TheHelpTextSystem().ClearAllText();
		};
		Locator::dialogueControlSystem::value().SetHooks(std::move(dialogueHooks));
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "No dialogue control: the help system is not told who has the dialogue");
	}
	// the advisors' control and its two .hd models (Locator::advisorSystem)
	TheAdvisorSystem().Load();
	// The help system just made, the texts and the tooltips as the profile played keeps them
	if (Locator::playerProfileSystem::has_value())
	{
		if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
		{
			ApplyPlayerSettings(profile->settings);
		}
	}
}

void Game::ConnectToEngine()
{
	auto& config = Locator::config::value();
	// The debug windows, the testbed's scenarios and its menu item reach the game only through its host
	if (!_gameHost)
	{
		_gameHost = std::make_unique<GameHostOfGame>(*this);
	}
	if (Locator::debugGui::has_value())
	{
		Locator::debugGui::value().SetGameHost(_gameHost.get());
	}
	auto& events = Locator::events::value();

	// LOAD_MAP: the land changes while the script's call is handled
	events.AddHandler(std::function([this](const ecs::events::LandChangeRequested& request) { ChangeLand(request.path); }));
	// The end of a script's camera control makes the player's camera again: its help icons by the hand start again too
	events.AddHandler(std::function([this](const ecs::events::PlayerCameraRestarted& /*restarted*/) {
		hand_tricons::Restart(_handTriconFades, _handTriconAngle);
		if (Locator::camera::has_value())
		{
			Locator::camera::value().SetShownHandIcons(0);
		}
	}));
	// The debug window's script restart: the scripts' state back to its start, then their whole program again
	events.AddHandler(std::function([this](const ecs::events::ScriptsRestartRequested& /*requested*/) {
		ResetScripts();
		RestartScriptProgram();
	}));
	// One of the Gods' Playground's lessons has started: Escape on the island stops it
	events.AddHandler(std::function([this](const ecs::events::PlaygroundLessonStarted& /*started*/) {
		_playgroundLessonRunning = true;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The Gods' Playground: a lesson runs, Escape stops it");
	}));
	// A land's script shows the land information box and fills its list. A number other than 0 names a help text that
	// the box shows in place of the script line's text
	events.AddHandler(std::function([this](const ecs::events::LandMessageGiven& given) {
		if (!_interface)
		{
			return;
		}
		const auto helpText = land_message::HelpTextOf(given.message.number);
		auto text =
		    helpText.has_value() ? std::u16string(_interface->GetTexts().Get(*helpText)) : gui::ToUtf16(given.message.text);
		const bool start = given.message.kind == land_message::Message::Kind::Start;
		if (start)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land information box shows: {}", gui::ToUtf8(text));
		}
		_interface->ApplyLandMessage(std::move(text), start);
	}));
	// The quick load's packet: the quick save's slot loads at the next frame's start
	events.AddHandler(std::function([this](const ecs::events::QuickLoadAsked& /*asked*/) { _quickLoadDue = true; }));
	// The save game room's load: its slot loads at the next frame's start
	events.AddHandler(std::function([this](const ecs::events::RoomLoadAsked& asked) { _roomLoadDue = asked.slot; }));
	// A save into a slot shows its banner as it starts writing
	events.AddHandler(std::function([this](const ecs::events::GameSaving& saving) { PresentSavingBanner(saving.slot); }));
	// A save that could not be written is told by the game's message box
	events.AddHandler(std::function([this](const ecs::events::SaveProblem& /*problem*/) {
		if (_interface)
		{
			_interface->ShowSaveProblem();
			// (openblack, a readout at debug) the message box put up
			if (const auto log = spdlog::get("game"); log != nullptr)
			{
				log->debug("Save problem: the game's message box tells the player (HELP_TEXT_DIALOG_ERRORHASOCCURED)");
			}
		}
	}));
	events.AddHandler(std::function([this, &config](const SDL_Event& event) {
		// The keys and buttons the window holds, whatever takes the event below
		Locator::gameActionSystem::value().SeeDispatchedEvent(event);
		// Every motion moves the game's cursor, live as in an input recording's playback, whatever takes the event below:
		// the escape menu's and the boxes' pointers, and the debug windows', are drawn at it too. A hand demo playing is
		// the mouse, and the real one moves nothing meanwhile
		if (event.type == SDL_MOUSEMOTION)
		{
			auto& cursor = input::GameCursorRef();
			cursor = input::CursorAfterMotion(cursor, {event.motion.x, event.motion.y}, hand_demo::IsPlaying());
		}
		// If gui captures this input, do not propagate
		if (!Locator::debugGui::value().ProcessEvents(event))
		{
			// The game's message box, and the tattoo editor's dialog, take the input first: the box the mouse and every
			// key, the editor the mouse and of the keys only Enter and Escape
			if (_interface && (_interface->IsGameMessageShown() || _interface->GetTattooEditor().IsOpen()) &&
			    Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				return;
			}
			// On the Gods' Playground the island takes Escape while the menu is closed, before the temple's and the
			// menu's uses of it (PlaygroundEscape). (inferred) one a press: SDL's key repeats are not keys of the original
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    !(_interface && _interface->GetMenu().IsOpen()) && PlaygroundEscape())
			{
				return;
			}
			// Inside the temple, Escape goes back to its main room and out, as the temple's keys do, when no Shift or Ctrl
			// is held
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    temple::TakesEscape((event.key.keysym.mod & (KMOD_SHIFT | KMOD_CTRL)) != 0) && Locator::temple::has_value() &&
			    Locator::temple::value().Active())
			{
				Locator::temple::value().Escape();
				return;
			}
			// The Creature Cave shown on its own, without a temple, closes on Escape
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().Escape())
			{
				return;
			}
			// Escape on its way to the menu first stops a help script that holds the dialogue, and cuts both advisors
			// short, the good one first, even when the menu then stays shut within 300 ms of closing
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::dialogueControlSystem::has_value())
			{
				const auto vm = chlapi::ScriptVm();
				const auto owner = Locator::dialogueControlSystem::value().GetOwner();
				if (gui::EscapeStopsHelp({
				        .helpScriptHasDialogue =
				            owner != 0 && vm.taskType && vm.taskType(owner) == help::script_control::k_ScriptTypeHelp,
				        .shiftOrCtrl = (event.key.keysym.mod & (KMOD_SHIFT | KMOD_CTRL)) != 0,
				        .filmPlaying = video::IsPlaying(),
				        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
				        .boxUp = _interface && _interface->IsBoxActive(),
				        .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
				        .gameOver =
				            Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Context().gameOver,
				        .playground =
				            Locator::mapScriptSystem::has_value() && influence::LandNumber() == video::k_PlaygroundLand,
				    }))
				{
					help::script_control::StopHelp(vm);
					if (Locator::helpTextSystem::has_value())
					{
						Locator::helpTextSystem::value().InterruptAdvisorsByEscape();
					}
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Escape: the help script holding the dialogue (task {}) stopped",
					                   owner);
				}
			}
			// the escape menu takes Escape, and the keyboard and mouse while a box is open. Escape opens no menu with
			// a modifier held (Shift or Ctrl), over a full screen film (it skips the film instead), while a script
			// holds the wide screen or within 300 ms of the menu closing.
			// Alt is not a modifier here: Alt + Escape opens the menu
			// gui::EscapeBlocked: the original's Escape gate (Gui/GameMenu.h)
			const bool escapeBlocked =
			    event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE &&
			    gui::EscapeBlocked({.shiftOrCtrl = (event.key.keysym.mod & (KMOD_SHIFT | KMOD_CTRL)) != 0,
			                        .filmPlaying = video::IsPlaying(),
			                        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			                        .menuOpen = _interface && _interface->GetMenu().IsOpen(),
			                        .msSinceMenuClosed = static_cast<int32_t>(SDL_GetTicks() - _menuClosedTicks)});
			if (_interface && !escapeBlocked && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				HandleInterfaceAction();
				return;
			}
			// The game takes the frame's key downs one at a time, but not a key down with the same key and modifiers as
			// the last it took this frame (two repeats of a held key in one frame act once)
			if (event.type == SDL_KEYDOWN &&
			    !Locator::gameActionSystem::value().TakeKeyDown(event.key.keysym.scancode, event.key.keysym.mod))
			{
				return;
			}
			config.running = this->ProcessEvents(event);
			Locator::gameActionSystem::value().ProcessEvent(event);
		}
		else if (const auto button = ToMouseButtonEvent(event); button)
		{
			// a debug window that takes a click takes the input from the hand, as a box does
			input::DebugWindowTookButton(_mouseButtons, *button);
		}
	}));
}

bool Game::Run() noexcept
{
	if (!_scenarioList.empty())
	{
		return RunScenarioList();
	}
	if (!PlayRun())
	{
		return false;
	}
	// The Players page's Restart and Start a New Game: the game starts again, as the profile now played
	while (std::exchange(_restartRequested, false))
	{
		if (!RestartForProfile() || !PlayRun())
		{
			return false;
		}
	}
	return true;
}

bool Game::PlayRun() noexcept
{
	if (StartRun() == RunStart::Failed)
	{
		return false;
	}
	while (RunFrame())
	{
	}
	EndRun();
	return true;
}

bool Game::RestartForProfile() noexcept
{
	if (!StartAgain(_startConfig.numFramesToSimulate, std::nullopt))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game could not start again");
		return false;
	}
	return true;
}

bool Game::StartAgain(uint32_t frames, const std::optional<std::pair<uint32_t, std::filesystem::path>>& screenshot) noexcept
{
	// (test runs) A recording playing plays on through the restart, and one being made records on: their frames count
	// on from the frame after the last game's last, and they are carried over into the next game's input state
	input::replay::CountAgainFrom(Locator::screenshotRequest::value().Frame() + 1);
	auto playback = input::replay::TakePlayback();
	auto recorder = input::replay::TakeRecording();
	EndGame();
	if (!StartNewGame(std::nullopt, frames, screenshot))
	{
		return false;
	}
	if (playback.has_value())
	{
		input::replay::ResumePlayback(std::move(*playback));
	}
	if (recorder.has_value())
	{
		input::replay::ResumeRecording(std::move(*recorder));
	}
	// the next run's start leaves the playback as it is
	_startedAgain = true;
	return true;
}

bool Game::RunScenarioList() noexcept
{
	// A scenario that writes into the game's folder changes what the next entries read from it, which a single run
	// restores before it starts: such a list is not run
	for (const auto& entry : _scenarioList)
	{
		if (const auto* scenario = testbed_scenarios::Find(entry.request.id);
		    scenario != nullptr && scenario->fixtures.writesGameData)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"),
			                    "Scenario list: {} writes into the game's folder, so it is not run in a list; nothing run",
			                    entry.request.id);
			return false;
		}
	}
	const auto count = _scenarioList.size();
	for (size_t i = 0; i < count; ++i)
	{
		const auto& entry = _scenarioList[i];
		// the log's marks, by which the log is cut into one log per entry: a game's end is in its own part, the next
		// game's start-up in the next one's, as in their single runs' logs
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Scenario list: entry {} of {}, {} begins", i + 1, count,
		                   entry.out.filename().string());
		if (i == 0)
		{
			// the game the program started: the C runtime's seed before its first land, which every next game's start
			// comes back to
			_startCrtSeed = game_random::crt::Seed();
		}
		else if (!StartNewGame(entry.request, entry.frames, entry.screenshot))
		{
			return false;
		}
		// each entry's state hash from its first turn, in its own file (the store is the game's, made new with it), and
		// with OPENBLACK_STATE_HASH_ENTITIES its per-entity trace next to it, as a single run's
		state_hash::Enable(entry.stateHash.string());
		if (const char* parts = std::getenv("OPENBLACK_STATE_HASH_ENTITIES"); parts != nullptr && *parts != '\0')
		{
			state_hash::EnableEntities(parts, state_hash::EntitiesFileOf(entry.stateHash.string()));
		}
		if (!PlayRun())
		{
			return false;
		}
		// A restart asked for in the game (a skirmish's start or leave, the Players page's Restart) starts the next game
		// inside the entry, as a run with no scenario starts it: the skirmish box when it is due, else the start land.
		// The entry's frames and its shot count on through it; its state hash stays the first game's
		testbed_scenarios::EntryRest rest {.frames = entry.frames, .screenshot = entry.screenshot};
		while (Locator::config::value().running && std::exchange(_restartRequested, false))
		{
			rest = testbed_scenarios::RestOfEntry(rest, Locator::screenshotRequest::value().Frame() + 1);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Scenario list: entry {} of {} starts again, {} frames left", i + 1, count,
			                   rest.frames);
			_restartInEntry = true;
			const bool played = StartAgain(rest.frames, rest.screenshot) && PlayRun();
			_restartInEntry = false;
			if (!played)
			{
				return false;
			}
		}
		_restartRequested = false;
		// a quit (the window closed, an unknown scenario, a benchmark measured) ends the list, and the program ends the
		// game; the last game is ended by the program's end too
		const bool quit = !Locator::config::value().running;
		if (!quit && i + 1 < count)
		{
			EndGame();
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Scenario list: entry {} of {}, {} ends", i + 1, count,
		                   entry.out.filename().string());
		if (quit)
		{
			break;
		}
	}
	return true;
}

bool Game::LoadStartLand() noexcept
{
	auto& config = Locator::config::value();
	auto& fileSystem = Locator::filesystem::value();

	// A land run of a --scenarios list: its land loads as a launch's start level does, in place of the testbed, with its
	// test options; the scenarios' window never gets its request
	_landRun.reset();
	if (_scenarioRequest.has_value() && _scenarioRequest->land.has_value())
	{
		_landRun = std::exchange(_scenarioRequest, std::nullopt)->land;
		if (_landRun->handDemo.has_value())
		{
			hand_demo::SetListedTestDemo(*_landRun->handDemo);
		}
		if (_landRun->citadel.has_value())
		{
			ecs::audio_queries::SetCitadelTestTurns(*_landRun->citadel);
		}
	}
	const bool startTestbed = _startTestbed && !_landRun.has_value() && !_restartInEntry;
	auto startMap = _landRun.has_value() ? fileSystem.GetPath<filesystem::Path::Scripts>() / _landRun->land : _startMap;
	if (!_landRun.has_value() && _startOnMapScriptLand)
	{
		startMap = MapScriptLand(fileSystem).value_or(_startMap);
	}

	// Each game started: its first film can be skipped, unless no player has a profile yet (the new player's first game)
	video::Get().SetNoSkip(startup::FilmsUnskippable(HasPlayerProfiles()));
	// the start loaded the profile's automatic save
	bool loadedAtStart = false;
	if (startTestbed)
	{
		LoadTestbed();
	}
	// While no player profile exists the new player is asked their name and the story starts, the skirmish box not shown
	else if (const auto skirmish =
	             std::exchange(_skirmishBoxDue, false) && HasPlayerProfiles() ? AskForSkirmishLand() : std::nullopt;
	         skirmish.has_value())
	{
		// The skirmish box's Start Game: its land, as a skirmish. The player has a profile already
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The skirmish starts on {}", skirmish->generic_string());
		if (!StartSkirmish(*skirmish))
		{
			return false;
		}
	}
	else
	{
		// The profile's automatic save when the start asks for it and it can be read (a player with no profile yet starts
		// a new game); else, or when its load fails, a new game, a first start's new player asked their name before the
		// first land
		const bool hasProfiles = HasPlayerProfiles();
		const bool autoSaveValid = Locator::saveGame::has_value() && Locator::saveGame::value().IsAutoSaveValid();
		const auto start = startup::StartOf(std::exchange(_autoSaveAsked, true), !_startOnMapScriptLand || _landRun.has_value(),
		                                    hasProfiles, autoSaveValid);
		_startLoading = true;
		const bool loaded = start == startup::GameStart::AutoSave && LoadSavedGame(gamesave::k_ExitSlot);
		loadedAtStart = loaded;
		_mainMenuAtStart = startup::OpensOnMainMenu(loaded ? startup::GameStart::AutoSave : startup::GameStart::NewGame);
		_startLoading = false;
		if (!loaded)
		{
			_startLoadScripts.reset();
			AskForNewPlayer();
			if (!LoadMap(startMap))
			{
				return false;
			}
		}
	}
	// A new game started on a land whose script leaves the start camera at the origin of the map starts with the camera
	// at the player's temple (openblack plays no network game); a loaded game keeps the camera its save holds, as the
	// original makes the start camera only for a new, skirmish or multiplayer start
	if (!startTestbed && !loadedAtStart &&
	    start_camera::StartsAtTemple({.scriptPlace = Locator::entitiesRegistry::value().Context().startCameraPlace}))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game starts with the camera at the player's temple");
		_shortcutKeys.StartAtTemple();
	}
	if (_startupTimer.has_value())
	{
		_startupTimer->Step("first land");
	}
	// The time played that the save game room shows counts from here, once the first land is loaded; the help record is
	// the profile's, as it was kept
	if (Locator::playerProfileSystem::has_value())
	{
		Locator::playerProfileSystem::value().StartPlaying(machine_clock::WallTime());
		// as the game's loop starts: the profile is read, then written back
		help::profile::TakeFromProfile();
		help::profile::KeepInProfile();
	}

	// The challenge program, in the quest folder the SETTINGS: switch's file gave, or the game's own. Every game start
	// reads it and starts its auto-start list, a skirmish's too; only the story starts its control script
	auto challengePath =
	    _questFolder == settings_file::k_DefaultQuestFolder
	        ? fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl"
	        : filesystem::FileSystemInterface::FixPath(_questFolder + std::string(settings_file::k_ChallengeFile));
	if (fileSystem.Exists(challengePath))
	{
		auto& chlapi = Locator::chlapi::value();
		auto& lhvm = Locator::vm::value();
		// the VM's object references: the original's script library calls the ADD_REFERENCE / REMOVE_REFERENCE
		// natives (which raise and lower the object's script reference count) for a popped object and the variable's
		// old one (POP) and for a stopped task's object locals; object 0 is the scripts' null. Before each native, the
		// scripts' objects learn whether it takes control of the things it is given
		lhvm.Initialise(
		    &chlapi.GetFunctionsTable(),
		    [](uint32_t func) {
			    TheScriptObjects().EnterNative(func);
			    Locator::chlapi::value().EnterNative(func);
		    },
		    [](uint32_t func) { Locator::chlapi::value().ExitNative(func); },
		    // the task-stop callback: the dialogue, the wide screen and the camera of the task go back
		    // (Help/ScriptControl.cpp; the script control gives the camera itself back, Camera/ScriptCamera.h)
		    [](uint32_t taskNumber) {
			    auto& cameraControl = help::script_control::GetCameraControl();
			    auto* dialogue =
			        Locator::dialogueControlSystem::has_value() ? &Locator::dialogueControlSystem::value() : nullptr;
			    auto* control = Locator::scriptControlSystem::has_value() ? &Locator::scriptControlSystem::value() : nullptr;
			    auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
			    help::script_control::OnTaskStopped(taskNumber, help::Get(), dialogue, control, camera, cameraControl,
			                                        audio::GetScriptAudioState());
		    },
		    [](lhvm::ErrorCode code, const std::string v0, uint32_t v1) {
			    SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "VM error {} ({}, {})", static_cast<int>(code), v0, v1);
		    },
		    [](uint32_t objId) {
			    if (objId != 0)
			    {
				    TheScriptObjects().AddReference(static_cast<entt::entity>(objId));
			    }
		    },
		    [](uint32_t objId) {
			    if (objId != 0)
			    {
				    TheScriptObjects().RemoveReference(static_cast<entt::entity>(objId));
			    }
		    });
		try
		{
			// the scripts' restart empties their record of the last hit before the program is read (no turn has run
			// yet, so it holds nothing here)
			Locator::dynamicsSystem::value().ForgetHits();
			_scriptProgramPath = challengePath;
			// a start from the automatic save: the machine made ready takes back the state the load read, and the story
			// goes on from it, with no question and no new start
			if (const auto restored = std::exchange(_startLoadScripts, std::nullopt); restored.has_value())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The start from the automatic save takes back its scripts' state");
				RestoreScripts(lhvm, restored->file, restored->places);
				FindSnapshotScripts();
			}
			else
			{
				const bool loaded =
				    lhvm.LoadBinary(resources::LoadBlob(Locator::resources::value().GetBlobs(), challengePath)) == EXIT_SUCCESS;
				FindSnapshotScripts();
				// the testbed has no story: no LandControlAll, and no skip-tutorial question
				// a quest run (a land entry's quest=) starts its own scripts instead, from the testbed's land timeline
				const bool questRun = _landRun.has_value() && !_landRun->quest.empty();
				if (questRun)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Quest run: the land's story script is not started");
				}
				// a skirmish runs no story either: no LandControlAll, no skip-tutorial question and no black opening
				if (new_game_choice::RunsTheStory(startTestbed, questRun, ecs::IsSkirmishGame()))
				{
					// LandControlAll starts only with the program read and on the first land of the story
					const int32_t landNumber = Locator::mapScriptSystem::value().Globals().landNumber;
					const bool started = new_game_choice::StartsTheStory(loaded, landNumber) &&
					                     lhvm.StartScript("LandControlAll", lhvm::ScriptType::All) != 0;
					// a new game, right after starting LandControlAll: the skip-tutorial question clears bits 23, 24 and 25 of
					// the game's flags, pauses the game and shows the SkipBox; its answer sets them (0 none, 1 bit 23, 2 bits
					// 23+24, 3 bits 23+24+25) and unpauses (Game::HandleInterfaceAction). SetupLand1 reads them later through
					// CAN_SKIP_TUTORIAL and the others (openblack guard: without a window nobody answers the box, and the pause
					// would stay)
					// --skip-opening with an answer: no box and no pause, the flags set at once as that answer sets them
					const bool answered = _newGameStart.has_value() && _newGameStart->answer.has_value();
					if (!answered && _interface && Locator::windowing::has_value())
					{
						_interface->ShowSkipBox();
						game_clock::Pause(true);
					}
					TheTutorialSkipSystem().Set({});
					if (answered)
					{
						TheTutorialSkipSystem().Set(new_game_choice::SkipFor(*_newGameStart->answer));
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "New game started with the start-of-game question answered {}",
						                   static_cast<int>(*_newGameStart->answer));
					}
					// The story opens on a black screen only on the first land of the story, and only when the question has
					// not held the game: asking pauses it, so a player who answers it sees the land as it is, and the scripts
					// fade the picture themselves. The answer given with --skip-opening stands for the question answered while
					// the game waited. With the black, the temple's fade is put at rest too
					if (new_game_choice::OpensOnBlack(started, landNumber, answered || game_clock::IsPaused()))
					{
						CinematicDirector().StartStory();
						auto& templeFade = video::GetFallingSpell().GetScreenFade();
						templeFade.current = 0.0f;
						templeFade.target = 0.0f;
					}
				}
			}
		}
		catch (const std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to read challenge file at {}: {}",
			                    (fileSystem.GetGamePath() / challengePath).generic_string(), err.what());
		}
	}
	else if (!ecs::IsSkirmishGame())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Challenge file not found at {}",
		                    (fileSystem.GetGamePath() / challengePath).generic_string());
		return false;
	}

	if (_startupTimer.has_value())
	{
		_startupTimer->Step("story scripts");
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
			CinematicDirector().FadeTo(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), seconds);
		}
	}
	if (std::getenv("OPENBLACK_TEST_WIDESCREEN") != nullptr)
	{
		CinematicDirector().SetWideScreen(true, Locator::infoConstants::value().helpSystem.wideScreenTime);
		// as SET_WIDESCREEN: the help system's owning task is set too (the user-param-1 samples are skipped)
		audio::SetScriptWideScreen(true);
	}
	// OPENBLACK_TEST_VIDEO=<intro|fall|path> plays a full screen film (docs/wiki/video/bink.md#test-hooks): intro as the intro
	// sequence (data\intro.bik, 60 s), fall as the falling spell's kick-off without its creature test (mode 2,
	// data\Spells\fall\fall.bik with alpha 0x50 and no world drawn, Video/FallingSpellVideo.h), else the given .bik
	if (const char* film = std::getenv("OPENBLACK_TEST_VIDEO"); film != nullptr)
	{
		const std::string name = film;
		const auto& data = fileSystem.GetPath<filesystem::Path::Data>();
		const std::filesystem::path path = name == "intro"  ? data / "intro.bik"
		                                   : name == "fall" ? data / "Spells" / "fall" / "fall.bik"
		                                                    : std::filesystem::path(name);
		std::filesystem::path found = path;
		try
		{
			found = fileSystem.FindPath(path);
		}
		catch (const std::exception&)
		{
		}
		if (name == "fall")
		{
			video::GetFallingSpell().Start();
		}
		else
		{
			video::Get().Play(found);
			if (name == "intro")
			{
				video::Get().ScheduleIntro();
			}
		}
	}
	// OPENBLACK_TEST_MOVE_TIME="hour,seconds" runs MOVE_GAME_TIME; OPENBLACK_CLOCK_TRACE=1 logs the clock every 50 turns
	if (const char* move = std::getenv("OPENBLACK_TEST_MOVE_TIME"); move != nullptr)
	{
		float hour = 0.0f;
		float seconds = 0.0f;
		if (std::sscanf(move, "%f,%f", &hour, &seconds) == 2)
		{
			TheDayNightClock().MoveScriptTime(hour, seconds);
		}
	}

	// Initialize the Acceleration Structure
	Locator::entitiesMap::value().Sync();

	if (Locator::windowing::has_value())
	{
		const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, size, graphics::k_BackgroundColour);
	}

	{
		uint16_t width;
		uint16_t height;
		Locator::oceanSystem::value().GetReflectionFramebuffer().GetSize(width, height);
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Reflection, {width, height}, 0x274659ff);
	}

	Game::SetTime(config.timeOfDay);
	return true;
}

Game::RunStart Game::StartRun() noexcept
{
	if (!LoadStartLand())
	{
		return RunStart::Failed;
	}
	const auto& config = Locator::config::value();
	Locator::screenshotRequest::value().ResetFrame();
	// --replay-input / --record-input (test runs): from the run's first frame; a game started again plays on the
	// recording the last one played and records on into the last one's (StartAgain)
	const bool startedAgain = std::exchange(_startedAgain, false);
	// The game loop's first pass plays the logo, but where a recording is replayed: a replay takes the recorded logo's
	// draws instead (docs/wiki/engine/game-clock.md#the-in-loop-logo)
	const bool replaying = !startedAgain && (_resumeInput.has_value() || _replayInput.has_value());
	const auto logo = StartInLoopLogo(replaying);
	// A game started from the automatic save opens on the main menu, once the logo has played
	if (std::exchange(_mainMenuAtStart, false) && _interface)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game started from the automatic save opens on the main menu");
		_interface->GetMenu().Open();
	}
	// --resume-input (developer tool): the recording plays, then the player takes over; a new recording starts with
	// its events, so that it can be resumed again
	bool resuming = false;
	if (_resumeInput.has_value() && !startedAgain)
	{
		resuming = input::replay::StartResume(*_resumeInput, 0);
	}
	else if (_replayInput.has_value() && !startedAgain)
	{
		input::replay::StartPlayback(*_replayInput, 0);
	}
	if (replaying)
	{
		SpendRecordedLogo();
	}
	if (_recordInput.has_value() && !startedAgain)
	{
		std::error_code sameError;
		if (_resumeInput.has_value() && std::filesystem::equivalent(*_resumeInput, *_recordInput, sameError))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Input recording: {} is the file resumed from; nothing is recorded",
			                    _recordInput->generic_string());
		}
		else
		{
			const auto window = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
			// a resumed recording's events, or the logo that played before the first frame
			const auto first = resuming           ? input::replay::PlaybackEvents()
			                   : logo.has_value() ? std::span<const input::recording::Event>(&*logo, 1)
			                                      : std::span<const input::recording::Event> {};
			input::replay::StartRecording(*_recordInput, 0, window.x, window.y, first);
		}
	}
	_frameLoop = std::make_unique<FrameLoop>();
	// the ms since Run started come from the game's clock, so that a run with a fixed frame time draws the same
	_frameLoop->runStartTicks = game_clock::TickCount();
	// --frame-stats: the frame times and profiler stages logged every so many frames; off by default
	if (config.frameStatsInterval > 0)
	{
		_frameLoop->frameStats.emplace(config.frameStatsInterval, config.frameStatsViews);
		if (config.frameStatsViews)
		{
			Locator::rendererInterface::value().SetProfile(true);
		}
	}
	_frameLoop->frameStart = std::chrono::steady_clock::now();
	_frameLoop->resourceTraceInterval = debug::resource_trace::IntervalFrom(std::getenv("OPENBLACK_RESOURCE_TRACE"),
	                                                                        std::getenv("OPENBLACK_RESOURCE_TRACE_SECONDS"));
	_frameLoop->resourceTraceNext = _frameLoop->frameStart;
	return RunStart::Play;
}

bool Game::RunFrame() noexcept
{
	// (debug) the logo again, as the debug bar asked, between two frames
	if (std::exchange(_inLoopLogoAsked, false))
	{
		static_cast<void>(PlayInLoopLogo());
	}
	// (debug) the saving banner, as a save into a player's slot shows it
	if (std::exchange(_savingBannerAsked, false))
	{
		PresentSavingBanner(0);
	}
	if (!Update())
	{
		return false;
	}
	auto& loop = *_frameLoop;
	LogicFrame(loop.snapshot, game_clock::TickCount() - loop.runStartTicks);
	DrawFrame(loop.snapshot);
	LateLogic();
	FinishFrame();
	// the statistics' frame rate, on the machine's clock
	++_statisticsFrames;
	_statisticsFrameRate.Frame(machine_clock::Ticks(), _statisticsFrames);
	// The game is playable from its first frame drawn
	if (_startupTimer.has_value())
	{
		_startupTimer->Step("first frame");
		_startupTimer.reset();
	}

	if (loop.frameStats.has_value())
	{
		const auto frameEnd = std::chrono::steady_clock::now();
		loop.frameStats->Frame(std::chrono::duration<float, std::milli>(frameEnd - loop.frameStart).count(),
		                       Locator::profiler::value());
		loop.frameStart = frameEnd;
	}
	if (loop.resourceTraceInterval.has_value())
	{
		if (const auto now = std::chrono::steady_clock::now(); now >= loop.resourceTraceNext)
		{
			loop.resourceTraceNext = now + *loop.resourceTraceInterval;
			if (auto log = spdlog::get("game"); log != nullptr)
			{
				log->info(debug::resource_trace::Format(debug::resource_trace::Take(GetTurn(), true)));
			}
		}
	}
	return true;
}

void Game::EndRun() noexcept
{
	// a recording being made is in its file up to here; it records on if the game starts again (StartAgain)
	input::replay::FlushRecording();
	_objectListFrame = nullptr;
	_frameLoop.reset();
}

void Game::LateLogic()
{
	// Test hook: "<frames>:<script>,<script>..." loads the next script every <frames> frames, at the point where the debug
	// menu's "Load Island" does (a check that changing maps doesn't crash). LoadMap is a sync point: the draw
	// thread parked
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
		const uint32_t frame = Locator::screenshotRequest::value().Frame();
		if (frame > 0 && frame % frames == 0 && frame / frames <= scripts.size())
		{
			const auto& script = scripts[frame / frames - 1];
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Map cycle: loading {}", script);
			LoadMap(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / script);
		}
	}
}

void Game::FinishFrame()
{
	auto& profiler = Locator::profiler::value();
	{
		auto section = profiler.BeginScoped(Profiler::Stage::RendererFrame);
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		Locator::rendererInterface::value().Frame();
	}
	// The challenges' pictures whose screens are back
	auto& renderer = Locator::rendererInterface::value();
	ecs::challenge_pictures::Collect(
	    Locator::entitiesRegistry::value(),
	    {.take = [&renderer](const std::string& name) { return renderer.TakeScreenPicture(name); },
	     .failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); },
	     .taken =
	         [](entt::entity record, const std::string& name) {
		         spdlog::get("game")->debug("Challenge pictures: record {} took its picture, {}", entt::to_integral(record),
		                                    graphics::ShownFrames::IsName(name) ? "of the frame on the screen as its script ran"
		                                                                        : "of the next frame drawn");
	         }});

	// and the saved games' pictures, with the cells of the slots saved by themselves that wait for them
	if (Locator::savePictures::has_value())
	{
		Locator::savePictures::value().Collect(
		    {.take = [&renderer](const std::string& name) { return renderer.TakeScreenPicture(name); },
		     .failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); }});
	}

	// Clear the stale screenshot request, then count the frame
	Locator::screenshotRequest::value().FinishFrame();
}

void Game::FindSnapshotScripts() noexcept
{
	_snapshotScripts.clear();
	if (!Locator::vm::has_value())
	{
		return;
	}
	const auto& vm = Locator::vm::value();
	const auto* natives = vm.GetFunctions();
	_snapshotScripts = ecs::challenge_pictures::ScriptsThatSnapshot(
	    vm.GetScripts(), vm.GetInstructions(),
	    natives != nullptr ? std::span<const lhvm::NativeFunction>(*natives) : std::span<const lhvm::NativeFunction> {});
	spdlog::get("game")->debug("Challenge pictures: {} of the {} scripts can take one",
	                           std::ranges::count(_snapshotScripts, true), vm.GetScripts().size());
}

bool Game::StartSkirmish(const std::filesystem::path& land) noexcept
{
	if (!LoadMap(land, loading::LoadingClock::Mode::Tips, false, true))
	{
		return false;
	}
	// The start's packet that brings the player's creature, which the first turn handles
	_skirmishCreatureDue = true;
	return true;
}

bool Game::ChangeLand(const std::filesystem::path& scriptPath) noexcept
{
	const auto path = Locator::filesystem::value().GetGamePath() / scriptPath;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "LOAD_MAP: changing land to {}", path.generic_string());
	// the map's clear: first the local player's creature is saved to the profile's mind file, which the next land's
	// LOAD_MY_CREATURE reads back (the creature goes with the land); a playground's start restarts the scripts before
	// it, which takes the creature out of a lesson script before it is saved
	ecs::player_creature::SaveMyCreatureAsTheMapClears(
	    land_load::ResetsFor(!_firstMapLoaded, path.parent_path().filename() == "Playgrounds").scripts);
	// then the things no script variable holds any more are released, as after each turn's scripts, while
	// the old land is still there (before the seeds go back). The things still held go with the land; the variables
	// themselves are kept
	TheScriptObjects().ReleaseUnreferenced();
	// a skirmish stays one across a script's change of land, as only a game's start clears it
	return LoadMap(path, loading::LookOf(loading::LoadKind::ScriptMap), true, ecs::IsSkirmishGame());
}

bool Game::LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look, bool scriptLandChange,
                   bool skirmish) noexcept
{
	const engine::gpu::ScopedPhase loadPhase(engine::gpu::Phase::Load); // a sync point with the draw thread
	auto& fileSystem = Locator::filesystem::value();

	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find script {}", path.generic_string());
		return false;
	}

	LoadTimer timer(fmt::format("land {}", path.stem().string()));
	BeginLoadingScreen(look);

	// a playground game start (every land but the campaign's first): its second town feature assignment below
	const bool inPlaygrounds = path.parent_path().filename() == "Playgrounds";
	const bool playgroundStart = _firstMapLoaded || inPlaygrounds;
	// the game's first land: its statistics are started once its map script has made the towns
	const bool firstLand = !_firstMapLoaded;
	auto resets = land_load::ResetsFor(firstLand, inPlaygrounds);
	resets.keepsTemple = scriptLandChange;
	Land().PrepareNewLand(playgroundStart, resets);
	_firstMapLoaded = true;

	// the map script from the byte cache, read afresh on every map load so that a script edited while the game runs is
	// the one loaded (the debug menu's map list)
	auto& blobs = Locator::resources::value().GetBlobs();
	blobs.Erase(resources::BlobId(path));
	const auto& data = resources::LoadBlob(blobs, path);
	const auto source = std::string(reinterpret_cast<const char*>(data.data()), data.size());

	Land().ResetRegistryForNewLand();
	// a skirmish is one from before its land's script, which its computer gods' start reads
	Locator::entitiesRegistry::value().Context().skirmish = skirmish;
	timer.Step("reset");
	RenderLoadingFrame();

	// The folder the vortices' crossing file is kept in, the current player's saved games, before the map script can open
	// one. A run of the checks keeps its profile nowhere, so there no crossing file is read or written
	const auto* profiles = Locator::playerProfileSystem::has_value() ? &Locator::playerProfileSystem::value() : nullptr;
	ecs::vortex_save::SetSaveFolder(ecs::vortex_save::SaveFolderOf(profiles));
	ecs::vortex_save::SetWritable(!Locator::config::value().checkRun);

	// the script's map state (the offset, the overrides, the last created), before the map features are loaded
	Script::BeginMapFeatures();
	Script script;
	try
	{
		// The tips screen is drawn again as the map's commands are carried out, when it is due
		script.Load(source, [this]() { RenderLoadingFrame(); });
	}
	catch (const std::exception& e)
	{
		// LoadMap is noexcept: a script it cannot read must not end the program
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Error in the map script {}: {}", path.generic_string(), e.what());
	}

	timer.Step("map script");
	// AssignTownFeatures once the whole script has made the towns, trees and stores: every town's scenic forest, then
	// every town's forest list
	try
	{
		ecs::town_features::AssignTownFeatures();
		// a playground game start: AssignTownFeatures again, after the map features
		if (playgroundStart)
		{
			ecs::town_features::AssignTownFeatures();
		}
	}
	catch (const std::exception& e)
	{
		// LoadMap is noexcept (as the script above): the stores it can make must not end the program
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Error assigning the town features of {}: {}", path.generic_string(),
		                    e.what());
	}

	timer.Step("town features");
	// as the land's script is done, the players' post-load clean-up: every player with a citadel adds its towns without
	// a worship site to it and works out their influence power, which the land's first turn reads
	worship::citadel::PostLoadCleanup();
	// the players' statistics start on the game's first land, after its map script, as the game's own start counts the
	// towns its script made
	if (firstLand)
	{
		ecs::player_statistics::Start();
	}
	// the rivers' landscape footprints, once the script has placed their points
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

	timer.Step("river footprints and footpaths");
	RenderLoadingFrame();
	Land().StartNewLand(path.stem().string(), false, resets.newGame);
	// a player kept inside the temple through a script's change of land is still inside it, the world paused
	if (Locator::temple::has_value())
	{
		land_load::TempleAfterLoad(Locator::temple::value(), resets);
	}
	EndLoadingScreen();
	timer.Step("start");

	return true;
}

bool Game::LoadSavedGame(uint32_t slot) noexcept
{
	if (!Locator::saveGame::has_value())
	{
		return false;
	}
	auto& saves = Locator::saveGame::value();
	auto& fileSystem = Locator::filesystem::value();
	const auto land = static_cast<uint32_t>(std::max(0, Locator::mapScriptSystem::value().Globals().landNumber));
	const auto world = saves.WorldFile(slot, land);
	// a world file two loads did not end is refused; else its counter goes up before anything is read
	if (!world.has_value() || !saves.BeginLoad(*world))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The saved game of slot {} cannot be loaded", slot);
		return false;
	}
	const auto bytes = fileSystem.ReadUserFile(*world);
	const auto read =
	    std::make_shared<const gamesave::WorldRead>(bytes.has_value() ? gamesave::ReadWorld(*bytes) : gamesave::WorldRead {});
	const auto state = ecs::saved_game_load::StateOf(*read);
	if (!state.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The world file {} could not be read: {}", world->generic_string(), read->stop);
		return false;
	}
	const engine::gpu::ScopedPhase loadPhase(engine::gpu::Phase::Load); // a sync point with the draw thread
	LoadTimer timer(fmt::format("saved game {}", slot));
	// As the original's: the game starts again from the saved file, under the tips screen a game's start shows, drawn
	// again at each step of the load (no sooner than its redraw time)
	BeginLoadingScreen(loading::LookOf(loading::LoadKind::SavedGame));
	RenderLoadingFrame();
	// As the original's load: the scripts restart with their program, the map is cleared and the saved land's file
	// opens. The map's clear first saves the local player's creature to the profile's mind file, as at a land change,
	// out of its lesson script as the scripts restart
	constexpr land_load::Resets k_LoadResets {.scripts = true, .program = true, .newGame = false, .mapClear = true};
	ecs::player_creature::SaveMyCreatureAsTheMapClears(k_LoadResets.scripts);
	Land().PrepareNewLand(true, k_LoadResets);
	RenderLoadingFrame();
	_firstMapLoaded = true;
	Land().ResetRegistryForNewLand();
	try
	{
		LoadLandscape(state->landscape);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The saved game's land could not be opened: {}", e.what());
		EndLoadingScreen();
		return false;
	}
	timer.Step("landscape");
	RenderLoadingFrame();
	// the world's objects, made in the order the original puts their links back once all are read; the classes made so
	// far are listed on the saved-games page
	ecs::saved_world_write::LoadedObjects loadedObjects;
	std::vector<entt::entity> resolves;
	// each crossing file put back from its copy beside the world file, as its save does as the game's load reads it,
	// before the vortices that open it are made; a run of the checks writes no crossing file
	if (ecs::vortex_save::Writable())
	{
		ecs::vortex_save::PutBackCrossingFiles(*read, fileSystem);
	}
	// from here to the load's end the world is loading: the obstacles it puts back send no footpath round them
	saves.SetLoading(true);
	_savedGameCensus = ecs::saved_world_load::Apply(ecs::saved_world_load::PlanOf(*read), ecs::saved_world_load::GameMakers(),
	                                                &loadedObjects, &resolves);
	// the players' values the game models, each player listed for the next save's players' writer
	ecs::saved_players::Load(*read, &loadedObjects);
	// with every object and player back, each totem wears the icon of its town's player's creature, as at its creation
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().WorldLoaded();
	}
	// and the loaded objects no entity stands for that the writers write from what openblack keeps
	ecs::saved_world_write::KeepLoadedSources(*read, loadedObjects);
	// (debug readout) the census, with what those steps kept, in the log as well as in the debug menu's Last Load: each
	// class the load left records of unmade
	ecs::saved_world_load::CountKept(_savedGameCensus, loadedObjects);
	for (const auto& line : _savedGameCensus)
	{
		if (line.made < line.saved)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load census: {} {} of {} made", gamesave::TypeName(line.type), line.made,
			                   line.saved);
		}
	}
	// the scripts' object table: each place's loaded object in a script again, held as its place says
	const auto scriptPlaces = ecs::saved_script_objects::PutBack(LoadedScriptPlaces(*read, loadedObjects));
	// the camera's zoomers, and the player's camera started again from them
	ecs::saved_camera::Load(*read);
	// the world read and what was made of it, kept for the next save, which writes from them what openblack does not
	// model yet
	saves.KeepLoadedWorld(read, std::move(loadedObjects));
	// the rivers' landscape footprints, once their points are back
	ecs::CreateRiverFootprints();
	timer.Step("objects");
	RenderLoadingFrame();
	// the help record, from the profile's file, read just before the scripts' machine
	help::profile::TakeFromProfile();
	// the scripts' machine, from its file beside the world file
	if (Locator::vm::has_value())
	{
		const auto script = world->parent_path() / gamesave::VanillaScriptFileName(slot);
		const auto path = script.is_absolute() ? script : fileSystem.GetGamePath() / script;
		// at the program's start the machine is not ready yet: it takes the state back once it is (LoadStartLand)
		if (_startLoading)
		{
			_startLoadScripts = LoadedScripts {.file = path, .places = scriptPlaces};
		}
		else
		{
			RestoreScripts(Locator::vm::value(), path, scriptPlaces);
			FindSnapshotScripts();
		}
	}
	timer.Step("scripts");
	RenderLoadingFrame();
	Land().StartNewLand(std::filesystem::path(filesystem::FileSystemInterface::FixPath(state->landscape)).stem().string(),
	                    false, false);
	// the game object's values, and the clock started again at the saved turn's time
	ecs::saved_game_load::Apply(*state);
	// with the saved turn and the seeds back, each player's interface status starts its guidance again as the game's
	// load does as it resolves them, in the order their reads ended
	audio::guidance::StartAtLoad(ecs::saved_game_load::InterfaceStatusPlayers(*read));
	// the loaded objects' resolves that draw random numbers, which the game makes once the whole file is read, the
	// seeds with it
	{
		const auto folder = world->parent_path();
		std::optional<uint8_t> leashes;
		if (!read->objects.empty() && read->objects.front().has_value())
		{
			if (const auto* head = std::get_if<gamesave::GameHead>(&*read->objects.front()); head != nullptr)
			{
				leashes = head->bytes[1];
			}
		}
		ecs::saved_world_load::Resolve(
		    resolves, {.slotFolder = folder.is_absolute() ? folder : fileSystem.GetGamePath() / folder, .leashes = leashes});
	}
	// each player's leash, once the creatures know their leashes again
	if (Locator::leashSystem::has_value())
	{
		ecs::saved_leash::Load(Locator::entitiesRegistry::value(), Locator::leashSystem::value(), *read,
		                       saves.LoadedWorldObjects());
	}
	// the hand's player's interface status: the hand, its object and the last seed type
	if (Locator::handSystem::has_value())
	{
		_handLoadReadout = ecs::saved_hand::ReadoutOf(ecs::saved_hand::Load(
		    Locator::entitiesRegistry::value(), Locator::handSystem::value(), *read, saves.LoadedWorldObjects()));
	}
	// then, as the load's end does for each player with a creature, the leash switched on or off again as its status
	// says
	if (Locator::leashSystem::has_value())
	{
		ecs::saved_leash::SwitchAgain(Locator::leashSystem::value(), *read, saves.LoadedWorldObjects());
	}
	// the camera's saved follow of a thing, once the things are back: Creature Mode's on its creature again
	ecs::saved_camera::LoadFollow(*read, saves.LoadedWorldObjects());
	// the scripts' record of the last thing hit and of what hit it, as the script's part of the game object names them
	if (Locator::dynamicsSystem::has_value())
	{
		const auto hits =
		    ecs::saved_world_write::LoadedScriptHits(Locator::entitiesRegistry::value(), *read, saves.LoadedWorldObjects());
		Locator::dynamicsSystem::value().RestoreHits(hits.at(0), hits.at(1));
	}
	saves.SetLoading(false);
	saves.EndLoad(*world);
	// the quick load key counts its turns from the turn this load was at
	saves.SetLastLoadTurn(game_clock::Turn());
	RenderLoadingFrame();
	EndLoadingScreen();
	timer.Step("start");
	return true;
}

void Game::QuickLoadKeyPressed()
{
	if (const auto refused = ecs::save_triggers::QuickLoadKey(); refused.has_value())
	{
		_quickLoadReadout =
		    fmt::format("refused at turn {}: {}", game_clock::Turn(), ecs::save_triggers::QuickLoadRefusalText(*refused));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Quick load: {}", _quickLoadReadout);
		return;
	}
	_quickLoadReadout = fmt::format("asked for at turn {}", game_clock::Turn());
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Quick load: {}", _quickLoadReadout);
}

void Game::RunQuickLoad()
{
	const auto slot = ecs::save_triggers::k_QuickSaveSlot;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Quick load: loading slot {} (quick save)", slot);
	// with no loadable save in the slot nothing loads and the game goes on, as the original's does when the slot's file
	// is not there
	const bool loaded = LoadSavedGame(slot);
	_quickLoadReadout = loaded ? fmt::format("loaded slot {} at turn {}", slot, game_clock::Turn())
	                           : fmt::format("slot {} could not be loaded; the game goes on", slot);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Quick load: {}", _quickLoadReadout);
}

ecs::systems::LandSystemInterface& Game::Land()
{
	auto& land = Locator::landSystem::value();
	land.SetView(this);
	return land;
}

void Game::RestartHandIcons()
{
	hand_tricons::Restart(_handTriconFades, _handTriconAngle);
	_handCrossHeldMs = 0;
	if (Locator::camera::has_value())
	{
		Locator::camera::value().SetShownHandIcons(0);
	}
}

void Game::ResetScripts()
{
	// the audio switches: the creatures' sounds, the dialogue-only sound, the alignment music, the music line and beats
	audio::GetScriptAudioState().Reset();
	// the camera control's switches: the leashes and the highlights drawn. The script control's owners of the camera
	// and the game speed are kept, as the dialogue control's is
	help::script_control::GetCameraControl().Reset();
	// the camera zone a script set goes, every place to keep out of with it
	if (Locator::cameraZoneSystem::has_value())
	{
		Locator::cameraZoneSystem::value().ClearForScriptsRestart();
	}
	// SET_INTERFACE_CITADEL's value back to 1 (the citadel entrance may be tapped)
	worship::citadel::ResetInterfaceCitadel();
	// the hand demo, the intro's things, the interface interaction, the sun and the confirmation
	CleanScriptsForRestart();
	// the help system: its texts, the message sets' counts, the cinema bars, the icons and the history; then both
	// advisors go home
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->ResetForScriptsRestart();
	}
	// the script fade stops, with nothing drawn (SET_FADE's colour and its alpha back to 0), the close clipping goes off
	// and the scripts' words on the screen go at once
	CinematicDirector().OnScriptsRestart();
	// the countdown timer off and hidden (its turns left stay), and a hand demo's pending trigger forgotten
	ecs::script_countdown::OnScriptsRestart();
	static_cast<void>(hand_demo::ConsumeTrigger());
}

void Game::RestartScriptProgram()
{
	// Only once the game has read its program: its first land reads it after its map script, and a skirmish reads none
	if (!_scriptProgramPath.has_value() || !Locator::vm::has_value())
	{
		return;
	}
	auto& vm = Locator::vm::value();
	// every task stops and the machine empties, then every thing a script made goes and every other one it held is let
	// go back into the game; the record of the last hit is emptied too
	vm.Reboot();
	TheScriptObjects().DeleteAllScriptThings();
	TheScriptObjects().Reset();
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().ForgetHits();
	}
	// the program read again, and the scripts of its auto-start list started; one that cannot be read leaves none
	bool loaded = false;
	try
	{
		const auto& program = resources::LoadBlob(Locator::resources::value().GetBlobs(), *_scriptProgramPath);
		loaded = vm.Restart(program) == EXIT_SUCCESS;
	}
	catch (const std::exception& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "The script program {} could not be read again: {}",
		                    _scriptProgramPath->generic_string(), err.what());
	}
	FindSnapshotScripts();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The script program starts again: {} {}, {} tasks running",
	                   _scriptProgramPath->filename().string(), loaded ? "read again" : "not read", vm.GetTasks().size());
}

void Game::CleanScriptsForRestart()
{
	// a hand demo still playing ends
	hand_demo::End();
	// the intro's things released (the intro light and hand; the debug camera off)
	TheIntroSystem().ReleaseAll();
	// the interface's state cleared, the hand reach 1800, the camera features 0x1BF, the ControlMap switches 1 / 1,
	// then the interface interaction back to normal, which writes all of them again. (pending) one more interface reset
	// is not ported
	help::interface_interaction::Set(0);
	// a sun a script hid (SET_SUN_DRAW) shows again
	ecs::sun_draw::ShowAgain(Locator::entitiesRegistry::value(), TheSkySystem().GetSun());
	// the confirmation stops: a START_ANGLE_SOUND 285 / 348 still on goes off
	audio::confirmation::Stop(audio::confirmation::Get());
}

bool Game::PlaygroundEscape()
{
	namespace playground_escape = input::playground_escape;
	const auto* helpSystem = help::Get();
	const bool director = Locator::cinematicDirectorSystem::has_value();
	const auto outcome = playground_escape::OnEscape(
	    {
	        .gameOver = Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Context().gameOver,
	        .networkGame = false,
	        .land = Locator::mapScriptSystem::has_value() ? influence::LandNumber() : 0,
	        .scriptWideScreen = helpSystem != nullptr && helpSystem->IsScriptWideScreen(),
	        .fadeMoving = director && !CinematicDirector().IsFadeFinished(),
	        .fadeDrawn = director && (CinematicDirector().GetFadeColour() >> 24U) != 0,
	    },
	    _playgroundLessonRunning);
	switch (outcome)
	{
	case playground_escape::Outcome::NotTaken:
		return false;
	case playground_escape::Outcome::Nothing:
		return true;
	case playground_escape::Outcome::StopLesson:
		// the fade put back to none at once, and the restart at the end of the turn
		if (director)
		{
			CinematicDirector().FadeBackToNormal(0.0f);
		}
		_playgroundRestartRequested = true;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Escape: the Gods' Playground's lesson stops");
		return true;
	case playground_escape::Outcome::AskToLeave:
		// the fade put back to none at once. (pending) the question whether to leave the Gods' Playground and the way
		// back: the escape menu opens instead
		if (director)
		{
			CinematicDirector().FadeBackToNormal(0.0f);
		}
		return false;
	}
	return false;
}

void Game::RestartPlaygroundScripts()
{
	/// The restart through the game's services
	class Restart final: public input::playground_escape::RestartSteps
	{
	public:
		explicit Restart(Game& game)
		    : _game(game)
		{
		}
		void DeleteScriptThings() override { TheScriptObjects().DeleteAllScriptThings(); }
		void StopAllScripts() override
		{
			if (Locator::vm::has_value())
			{
				Locator::vm::value().StopAllTasks();
			}
		}
		void CleanForRestart() override
		{
			// the close clipping off and the camera zone a script set gone, with every place to keep out of with it;
			// the dialogue-only sound off and the alignment music on; the bookmarks on; then what the scripts' restart
			// cleans too
			if (Locator::cinematicDirectorSystem::has_value())
			{
				CinematicDirector().SetCloseClipping(false);
			}
			if (Locator::cameraZoneSystem::has_value())
			{
				Locator::cameraZoneSystem::value().ClearForScriptsRestart();
			}
			auto& audioState = audio::GetScriptAudioState();
			audioState.gameSoundOff = 0;
			audioState.alignmentMusic = 1;
			// the bookmarks on
			if (Locator::cameraBookmarkSystem::has_value())
			{
				Locator::cameraBookmarkSystem::value().SetEnabled(true);
			}
			_game.CleanScriptsForRestart();
		}
		void StartScript(std::string_view script) override
		{
			if (Locator::vm::has_value() && Locator::vm::value().StartScript(std::string(script), lhvm::ScriptType::All) == 0)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The Gods' Playground: no script {}", script);
			}
		}
		void LessonStopped() override { _game._playgroundLessonRunning = false; }
		void InterfaceToNormal() override { help::interface_interaction::Set(0); }

	private:
		Game& _game;
	};
	Restart restart(*this);
	input::playground_escape::RestartOnFreePlay(restart);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The Gods' Playground's scripts restart on its free play");
}

void Game::RestartTurnClock()
{
	_lastGameLoopTime = std::chrono::steady_clock::now();
	_turnDeltaTime = 0ns;
}

void Game::LoadTestbed() noexcept
{
	LoadTestbed(flat_land::k_Altitude);
}

void Game::LoadTestbed(uint8_t planeAltitude) noexcept
{
	const engine::gpu::ScopedPhase loadPhase(engine::gpu::Phase::Load); // a sync point with the draw thread
	// No script runs on the testbed: the story's would set its time of day and stop its clock a few turns in
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StopAllTasks();
	}
	// As every land after the first: both random seeds at 0
	BeginLoadingScreen(loading::LoadingClock::Mode::Tips);
	// a testbed is a game of its own: the scripts start again, with a new camera, and as a launch's first land it has
	// no map clear before it (its scenes start as they did before the clear's status pass was ported). Their program
	// is not read again: no script runs on the testbed
	Land().PrepareNewLand(true, {.scripts = true, .newGame = true, .mapClear = false});
	_firstMapLoaded = true;
	Land().ResetRegistryForNewLand();
	RenderLoadingFrame();
	Script::BeginMapFeatures();
	try
	{
		InitializeLevel(flat_land::Build(planeAltitude));
		SetUpLandscape();
	}
	catch (const std::exception& e)
	{
		// LoadTestbed is noexcept, as LoadMap is
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not make the testbed's land: {}", e.what());
		EndLoadingScreen();
		return;
	}
	RenderLoadingFrame();

	// Looking down over the middle of the map, from the south
	const auto& land = Locator::terrainSystem::value();
	const auto middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	const auto ground = land.GetHeightAt(middle);
	Locator::camera::value()
	    .SetOrigin({middle.x, ground + k_TestbedCameraHeight, middle.y - k_TestbedCameraBack})
	    .SetFocus({middle.x, ground, middle.y});

	// (openblack) the testbed stands for a new game of its own: its creatures find places in its own regions
	Land().StartNewLand("testbed", true, true);
	EndLoadingScreen();

	// The testbed has no temple to give its player influence, so a ring of influence over its middle gives them the
	// reach a citadel would: the miracles cast only in influence may be cast there, and nowhere beyond
	influence::CreateRing({middle.x, ground, middle.y}, PlayerNames::PLAYER_ONE, k_TestbedInfluenceRadius, false);

	// The testbed comes with its window of scenarios to try out on it
	if (Locator::debugGui::has_value())
	{
		Locator::debugGui::value().OpenWindow(debug::gui::k_TestbedScenariosWindow);
	}
}

void Game::UpdateHandInterface()
{
	if (!_interface)
	{
		return;
	}
	// The temple has no fight panel
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// The fighters' health and stamina, while a fight is on (the fight debug window's switch hides it)
	_interface->SetFightPanel(
	    inTemple || !Locator::config::value().drawFightPanel ? std::nullopt : Locator::creatureFightSystem::value().GetPanel());

	// The creature's status panel: none in the temple, nor while a script's wide screen bars are on
	_interface->SetCreaturePanel(std::nullopt);
	const auto* helpSystem = help::Get();
	const bool scriptWideScreen =
	    helpSystem != nullptr &&
	    creature_panel_choice::HiddenByWideScreen(helpSystem->GetWideScreen() != 0, helpSystem->GetWideScreenOwner());
	if (inTemple || scriptWideScreen || !Locator::config::value().drawCreaturePanel)
	{
		return;
	}
	// The creature the hand holds, with the hand's reward; else the one Creature Mode follows, without it
	const auto& creatureHand = Locator::creatureHandSystem::value();
	const auto followed =
	    Locator::creatureModeSystem::has_value() ? Locator::creatureModeSystem::value().GetCreature() : std::nullopt;
	const auto choice = creature_panel_choice::Choose(creatureHand.GetCreature(), followed, creatureHand.GetLastFeedbackSum());
	if (!choice.has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(choice->creature))
	{
		return;
	}
	if (const auto* needs = registry.TryGet<const ecs::components::CreatureNeeds>(choice->creature); needs != nullptr)
	{
		_interface->SetCreaturePanel(creature_panel::FromNeeds(needs->needs, choice->reward));
	}
}

void Game::ProcessTempleTurn()
{
	// The temple's turn while the world is paused inside it, in the game's order. (not ported) the debug messages
	// cleared first
	class GameTempleTurn final: public temple_turn::Steps
	{
	public:
		explicit GameTempleTurn(uint32_t turn)
		    : _turn(turn)
		{
		}
		// the temple's help scripts alone
		void LookInTempleScripts() override
		{
			auto& lhvm = Locator::vm::value();
			lhvm.LookIn(lhvm::ScriptType::TempleHelp | lhvm::ScriptType::TempleSpecial);
		}
		// the audio game turn: the citadel's music
		void AudioTurn() override { audio::ProcessCitadelTurn(); }
		// the room's tooltip
		void RoomToolTip() override
		{
			if (Locator::temple::has_value())
			{
				Locator::temple::value().ProcessTurn();
			}
		}
		// the help system's turn: the hand state, the input prompt icons, the spirits' turn and the tooltips, which age
		// once a temple turn
		void HelpTurn() override { help::Process(chlapi::ScriptVm(), _turn); }

	private:
		uint32_t _turn;
	};
	GameTempleTurn steps(GetTurn());
	temple_turn::Process(steps, game_clock::IsPaused());
	ecs::audio_queries::RunCitadelTestHook(); // OPENBLACK_AUDIO_TEST_CITADEL counts the temple's turns too
}

void Game::ProcessTempleRoomKeys()
{
	if (!Locator::temple::has_value())
	{
		return;
	}
	// The keys for the temple's rooms take the player into the temple at that room's path in: the first key pressed this
	// frame, in their order. Inside the temple each key down cuts to its room, one after the other in the order they
	// came, as the temple's key handler takes them
	const auto& actions = Locator::gameActionSystem::value();
	auto& temple = Locator::temple::value();
	if (temple.Active())
	{
		for (const auto room : input::TempleRoomsInOrder(actions.GetKeyDownsThisFrame()))
		{
			temple.GoToRoom(room);
			// (openblack, a readout at debug) the key's room and the room the player is in after it
			if (const auto log = spdlog::get("game"); log != nullptr)
			{
				log->debug("Temple room key: {} pressed inside the temple, now in {}", TempleRoomName(room),
				           TempleRoomName(temple.GetCurrentRoom()));
			}
		}
		return;
	}
	// Outside it the keys do nothing while a script holds the wide screen, as every bindable key then
	const auto* helpSystem = help::Get();
	const bool scriptWideScreen = helpSystem != nullptr && helpSystem->IsScriptWideScreen();
	const auto pressed = [&actions](input::BindableActionMap action) {
		return actions.Get(action) && actions.GetChanged(action);
	};
	const auto room = input::TempleRoomToEnter(scriptWideScreen, pressed);
	const auto log = spdlog::get("game");
	if (!room)
	{
		// (openblack, a readout at debug) a room's key pressed while a script holds the wide screen does nothing
		if (scriptWideScreen && log != nullptr && log->should_log(spdlog::level::debug))
		{
			if (const auto refused = input::PressedTempleRoom(pressed); refused)
			{
				log->debug("Temple room key: {} pressed outside the temple while a script holds the wide screen: "
				           "nothing done",
				           TempleRoomName(*refused));
			}
		}
		return;
	}
	temple.Activate(*room);
	if (log != nullptr)
	{
		log->debug("Temple room key: {} pressed outside the temple: {}", TempleRoomName(*room),
		           temple.Active()
		               ? fmt::format("the player is taken into the temple at {}", TempleRoomName(temple.GetCurrentRoom()))
		               : std::string("the room is not open, the player stays outside"));
	}
}

void Game::HandleInterfaceAction()
{
	using Action = gui::GameMenu::Action;
	// the SkipBox's answer: 0 none, 1 bit 23, 2 bits 23 and 24, 3 the three; then the game unpaused
	if (const auto answer = _interface->TakeSkipAnswer(); answer)
	{
		TheTutorialSkipSystem().Set(new_game_choice::SkipFor(new_game_choice::ClampChoice(*answer)));
		game_clock::Pause(false);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "SkipBox: answer {}", *answer);
	}
	// F2's question answered: yes goes to the Gods' Playground, no lets the game go on
	if (const auto yes = _interface->TakeGameQuestionAnswer(); yes.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "F2: the Gods' Playground question answered {}", *yes ? "yes" : "no");
		PlaygroundTrip trip(*this);
		input::playground_key::Answer(*yes, trip);
	}
	const auto action = _interface->TakeAction();
	// While the menu is closed its creature help box follows the switch, which SET_CREATURE_HELP may have set; the
	// options pages show the profile's instead as they show, and the switch only without a profile, as the original
	// takes the help system's when the profile has no value
	if (!_interface->GetMenu().IsOpen() && Locator::creatureHelpSystem::has_value())
	{
		_interface->GetMenu().ShowCreatureHelp(Locator::creatureHelpSystem::value().IsHelpOn());
	}
	auto& profiles = Locator::playerProfileSystem::value();
	// The edits on the Players page are kept with the profile played, before anything its buttons do
	if (const auto edits = _interface->GetMenu().TakePlayerEdits(); edits.has_value())
	{
		profiles.EditCurrentProfile(edits->creatureName, edits->symbol);
		help::profile::KeepInProfile();
		NameCreatureAfterProfile();
	}
	// Escape opening the menu writes the profile played back first, its help record too
	if (_interface->TakeMenuOpenedByKey())
	{
		help::profile::KeepInProfile();
	}
	// The menu shows the buttons of a skirmish while one is played, as it lays them out each time it shows
	if (auto& menu = _interface->GetMenu(); menu.IsSkirmish() != ecs::IsSkirmishGame())
	{
		menu.SetSkirmish(ecs::IsSkirmishGame());
	}
	// The land information box's Back ends the skirmish for the skirmish box; Go! (or Escape) only closes it
	switch (_interface->GetLandInfoBox().TakeChoice())
	{
	case gui::LandInfoBox::Choice::Back:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land information box: Back ends the skirmish");
		EndGameForSkirmishBox();
		break;
	case gui::LandInfoBox::Choice::Go:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land information box: Go! (or Escape) closes it");
		break;
	case gui::LandInfoBox::Choice::None:
		break;
	}
	// and the end of a skirmish: the boxes the turns opened, and Leave Game
	FollowGameEnd();
	// A profile made in the new player's box over the game is played from now on
	if (auto& box = _interface->GetNewPlayerBox(); !box.IsActive())
	{
		if (const auto answer = box.TakeAnswer(); answer.has_value())
		{
			profiles.CreateProfile(answer->name, answer->symbol);
			help::profile::KeepInProfile();
			PlayerProfileChanged();
		}
		(void)box.TakeCancelled();
	}
	if (_interface->TakeSettingsChanged())
	{
		// the options box: a slider sets its main volume at once, value x 127 truncated toward zero; Audio applies the
		// configuration's every frame
		const auto& settings = _interface->GetMenu().GetSettings();
		auto& config = Locator::config::value();
		config.audioSampleMainVolume = gui::VolumeOfSlider(settings.sfxVolume);
		config.audioMusicMainVolume = gui::VolumeOfSlider(settings.musicVolume);
		// (pending) Auto Save (a bit of the game's flags). Push Scroll is kept and taken by the camera as it is
		// clicked, below
	}
	// The Push Scroll box is kept with the profile as it is clicked, and the profile written back, its help record too;
	// the write-back gives the camera the push scrolling at once
	if (_interface->GetMenu().TakePushScrollChanged())
	{
		if (auto profile = profiles.GetCurrentProfile(); profile.has_value())
		{
			profile->settings.pushScrolling = _interface->GetMenu().GetSettings().pushScrolling;
			profiles.SetCurrentSettings(profile->settings);
			help::profile::KeepInProfile();
			simulation::TakePushScrolling(profile->settings);
		}
	}
	// Leaving the options' box (the Options or the Advanced page) keeps its choices with the profile, then the help
	// system takes them from it: the help level, the texts shown and where, how fast the player reads them, the
	// tooltips and the creature help
	if (_interface->GetMenu().TakeOptionsLeft())
	{
		const auto kept = gui::ToProfileSettings(_interface->GetMenu().GetSettings());
		profiles.SetCurrentSettings(kept);
		help::profile::KeepInProfile();
		ApplyPlayerSettings(kept);
		// and the main volumes are kept for the computer (read again as either options page shows)
		audio::main_volumes::Keep();
	}
	// The Controls page left: its bindings kept with the profile played
	if (_interface->GetMenu().TakeControlsLeft())
	{
		profiles.SetCurrentKeys(gui::bound_controls::ForProfile(_interface->GetMenu().GetControlBindings()));
	}
	// A page shown takes the profile played again, and the hand the side its left-handed box keeps: the box's tick alone
	// changes nothing, the leave just before keeps it with the profile (without one, the menu's own choice)
	if (_interface->GetMenu().TakeProfileShown())
	{
		// The main page shown takes the profile's bindings again, as the original's does
		if (_interface->GetMenu().GetPage() == gui::GameMenu::Page::Main)
		{
			TakeProfileControls();
		}
		const auto profile = profiles.GetCurrentProfile();
		// Inside the temple only the options' box takes the side: there its Players and Controls pages show on their own
		if (gui::GameMenu::PageTakesHandSide(_interface->GetMenu().GetPage(), _interface->GetMenu().IsInsideTemple()))
		{
			const bool leftHanded =
			    profile.has_value() ? profile->settings.leftHanded : _interface->GetMenu().GetSettings().leftHandedHand;
			Locator::config::value().rightHandedHand = !leftHanded;
		}
		// The main page and the options' box read the profile played again as they show, its help record and its push
		// scrolling too
		using Page = gui::GameMenu::Page;
		if (gui::GameMenu::PageReadsProfileAgain(_interface->GetMenu().GetPage()))
		{
			help::profile::TakeFromProfile();
			if (profile.has_value())
			{
				simulation::TakePushScrolling(profile->settings);
			}
		}
		// The main page and the Players page read the profile's own symbol pictures again as they show
		if (const auto page = _interface->GetMenu().GetPage(); page == Page::Main || page == Page::Players)
		{
			ReadOwnSymbolsAgain();
		}
		// The Players page keeps the profile played as it shows, the one leaving the game saves into
		if (_interface->GetMenu().GetPage() == Page::Players)
		{
			_profileAtPlayersPage = profile.has_value() ? gui::ToUtf16(profile->name) : std::u16string {};
		}
	}
	// The detail: the original takes it at the next start ("detailidx", saved and read at start-up); openblack keeps
	// no settings for a next start, so it takes it as the Options page is left. The villagers', animals' and birds'
	// drawn mesh follows it (their high one at the top level)
	if (const auto change = _interface->GetMenu().TakeDetailChange(); change.has_value())
	{
		auto& config = Locator::config::value();
		config.detailLevel = graphics::detail_level::LevelAfterOptions(config.detailLevel, change->from, change->to);
		if (Locator::rendereringSystem::has_value())
		{
			Locator::rendereringSystem::value().SetLayoutDirty();
		}
	}
	// the menu pauses the game while it is open (Continue: hide and unpause). Opened inside the temple, by its options
	// room, the menu leaves the pause alone: the temple holds it and gives it back as the player leaves
	const bool open = _interface->GetMenu().IsOpen();
	const auto pauseChange = gui::StepMenuPause(open, _menuWasOpen, game_clock::IsInsideCitadel(), _menuPaused);
	if (pauseChange.pause.has_value())
	{
		game_clock::Pause(*pauseChange.pause);
	}
	if (pauseChange.closed)
	{
		_menuClosedTicks = SDL_GetTicks(); // the menu's close event: when, for Escape's debounce
	}
	_menuWasOpen = open;
	switch (action)
	{
	case Action::None:
	case Action::Continue:
		break;
	case Action::Quit:
		Locator::config::value().running = false;
		break;
	case Action::CreatePlayer:
		AskForAnotherPlayer(true);
		break;
	case Action::PickPlayer:
		if (const auto picked = _interface->GetMenu().GetPickedPlayer(); picked.has_value())
		{
			const auto& list = profiles.GetProfiles().profiles;
			if (*picked < list.size())
			{
				profiles.SelectProfile(gui::ToUtf16(list.at(*picked).name));
				PlayerProfileChanged();
			}
		}
		break;
	case Action::DeletePlayer:
	{
		profiles.DeleteProfile(_interface->GetMenu().GetPlayerToDelete());
		const auto next = _interface->GetMenu().GetPickedPlayer();
		const auto& list = profiles.GetProfiles().profiles;
		if (next.has_value() && *next < list.size())
		{
			profiles.SelectProfile(gui::ToUtf16(list.at(*next).name));
			PlayerProfileChanged();
		}
		else
		{
			// With no profile left the new player's box asks for one, and can't be cancelled
			PlayerProfileChanged();
			AskForAnotherPlayer(false);
		}
		break;
	}
	case Action::LoadDefaultControls:
		// Every action takes its default key and button again, as the Controls page shows them
		Locator::gameActionSystem::value().ResetKeyBindings();
		break;
	case Action::RedefineControl:
		// An action of the Controls page bound anew, and those that lost the key or the button to it
		TakeControlBindings();
		break;
	case Action::StartSkirmish:
		EndGameForSkirmishBox();
		break;
	case Action::LeaveSkirmish:
		// Yes to Leave Skirmish Game's question ends the skirmish, and the skirmish box shows again
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Leave Skirmish Game: the skirmish ends");
		EndGameForSkirmishBox();
		break;
	case Action::StartNewGame:
	case Action::Restart:
		// The Players page's start again is for the profile picked there, Start Game's is not
		_restartForProfile = action == Action::Restart;
		// Starting again as the profile picked carries on from its last automatic save, which a new game does not. The
		// game ends after this frame and Run starts the next one
		_autoSaveAsked = action == Action::Restart;
		_interface->GetMenu().Close();
		SDL_StopTextInput();
		_restartRequested = true;
		break;
	default:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "This part of the menu is not available yet");
		break;
	}
}

void Game::EndGameForSkirmishBox()
{
	// The game ends at once, as the original closes it; the next start shows the skirmish box before any
	// land, and its answer picks the land (LoadStartLand)
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ends for the skirmish box");
	_skirmishBoxDue = true;
	_autoSaveAsked = false;
	_restartRequested = true;
}

void Game::FollowGameEnd()
{
	if (!Locator::gameEndSystem::has_value())
	{
		return;
	}
	auto& gameEnd = Locator::gameEndSystem::value();
	auto& endBox = _interface->GetEndGameBox();
	// The end box stops showing itself when it closes, or when the box of its Statistics tab opens over it
	if (!endBox.ShowsItself())
	{
		end_game_box::StoppedShowing(_endGameBoxState);
	}
	// Leave Game ends the skirmish, and the skirmish box shows again
	if (endBox.TakeAction() == gui::EndGameBox::Action::Leave)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box: Leave Game ends the skirmish");
		EndGameForSkirmishBox();
	}
	const auto events = gameEnd.TakeEvents();
	if (events.empty())
	{
		return;
	}
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto opened = end_game_box::Follow(events, local, _endGameBoxState);
	// The name of a god: the profile's name for the god of this computer, "Player[n]" for the others
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	const auto nameOf = [local, &profile](PlayerNames god) {
		std::optional<std::u16string> human;
		if (god == local && profile.has_value())
		{
			human = gui::ToUtf16(profile->name);
		}
		return end_game_box::GodName(god, human.has_value() ? std::optional<std::u16string_view>(*human) : std::nullopt);
	};
	const auto& texts = _interface->GetTexts();
	if (opened.godOut.has_value())
	{
		const auto name = nameOf(*opened.godOut);
		const std::array<std::u16string_view, 1> words {name};
		_interface->ShowGameMessage(end_game_box::Format(texts.Get("HELP_TEXT_WIN_GAME_03"), words));
	}
	if (!opened.endBox.has_value())
	{
		return;
	}
	const auto winner = gameEnd.LastGodIn();
	const auto winnerName = winner.has_value() ? nameOf(*winner) : std::u16string();
	const auto title = end_game_box::Title(opened.endBox->kind, end_game_box::LooksOn(local), winnerName,
	                                       {.lost = texts.Get("HELP_TEXT_DIALOG_ADDITION_152"),
	                                        .won = texts.Get("HELP_TEXT_WIN_GAME_04"),
	                                        .gameOver = texts.Get("HELP_TEXT_GAME_OVER"),
	                                        .winnerFormat = u"%s - %s Won"});
	// The band under the title has the colour of the player's god. A looker-on has no god among the four, and its band
	// is not drawn (the original reads past its four players' colours)
	const auto landNumber = Locator::mapScriptSystem::value().Globals().landNumber;
	const auto colour = end_game_box::LooksOn(local) ? glm::vec4(0.0f) : gui::statistics_box::PlayerColour(local, landNumber);
	_interface->OpenEndGameBox({.box = *opened.endBox, .title = title, .colour = colour});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box opens for player {}", static_cast<int>(local));
}

void Game::RefreshPlayersPage()
{
	if (!_interface || !Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto& profiles = Locator::playerProfileSystem::value().GetProfiles();
	std::vector<std::u16string> names;
	names.reserve(profiles.profiles.size());
	std::optional<size_t> current;
	for (const auto& profile : profiles.profiles)
	{
		if (profile.name == profiles.current)
		{
			current = names.size();
		}
		names.push_back(gui::ToUtf16(profile.name));
	}
	_interface->GetMenu().SetPlayers(std::move(names), current);
}

void Game::TakeControlBindings()
{
	// Every action takes its key and button from the Controls page, in the page's order, which is the action map's; an
	// action already bound so is left as it is, so that it lets go of nothing
	if (!_interface || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	auto& actions = Locator::gameActionSystem::value();
	const auto page = _interface->GetMenu().GetControlBindings();
	const auto bindings = actions.GetKeyBindings();
	const std::vector<input::KeyBinding> current(bindings.begin(), bindings.end());
	for (size_t i = 0; i < current.size() && i < page.size(); ++i)
	{
		if (const auto chord = gui::control_rebind::ChordOf(page[i]); chord != current[i].key)
		{
			actions.SetKeyBinding(current[i].action, chord);
		}
		if (const auto mouse = gui::control_rebind::MouseInputOf(page[i].mouse); mouse != current[i].mouse)
		{
			actions.SetMouseBinding(current[i].action, mouse);
		}
	}
}

void Game::TakeProfileControls()
{
	// The profile played's bindings, the defaults for those it keeps none of, as the original's profile load does
	if (!_interface || !Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	const auto bindings = gui::bound_controls::FromProfile(
	    profile.has_value() ? profile->keys : std::map<uint32_t, player_profiles::KeyBinding> {});
	_interface->GetMenu().SetControlBindings(bindings);
	TakeControlBindings();
}

void Game::PlayerProfileChanged()
{
	RefreshPlayersPage();
	// the Players page shows the new profile, its own symbol pictures read
	ReadOwnSymbolsAgain();
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value() || !_interface)
	{
		return;
	}
	// The page shows the profile's creature's name, symbol and options. Of the options only the push scrolling takes
	// effect at once, as in the original; the rest only as the options' box is next left, which keeps the menu's
	// choices with the profile and applies them: picking a profile gives the help system none of them, the read speed
	// neither
	_interface->GetMenu().ShowPlayer(gui::ToUtf16(profile->creatureName), profile->symbol, profile->settings);
	TakeProfileControls();
	simulation::TakePushScrolling(profile->settings);
	NameCreatureAfterProfile();
	help::profile::TakeFromProfile();
}

void Game::ReadOwnSymbolsAgain()
{
	if (!player_own_symbols::ReadForProfile() || !Locator::filesystem::has_value())
	{
		return;
	}
	// the two files the particles' symbols are drawn from, read again with the pictures pasted in, then the menus'
	auto& fileSystem = Locator::filesystem::value();
	auto& textures = Locator::resources::value().GetTextures();
	for (const std::string_view stem : {"ChooseSymbol", "ChooseSymbola"})
	{
		const auto path = fileSystem.GetPath<filesystem::Path::Textures>() / (std::string(stem) + ".raw");
		if (!fileSystem.Exists(path))
		{
			continue;
		}
		const auto id = fmt::format("raw/{}", stem);
		if (textures.Contains(id))
		{
			textures.Erase(id);
		}
		try
		{
			textures.Load(id, resources::Texture2DLoader::FromDiskTag {}, path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}
	if (_interface)
	{
		_interface->ReloadSymbols();
	}
}

void Game::NameCreatureAfterProfile()
{
	// The player's creature takes the name its profile gives it, kept in its mind
	if (!Locator::playerProfileSystem::has_value() || !Locator::playerSystem::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(PlayerNames::PLAYER_ONE);
	if (!creature.has_value())
	{
		return;
	}
	ecs::player_creature::SetName(Locator::entitiesRegistry::value(), *creature, gui::ToUtf16(profile->creatureName));
}

void Game::AskForAnotherPlayer(bool canCancel)
{
	if (!_interface || !Locator::playerProfileSystem::has_value())
	{
		return;
	}
	auto& profiles = Locator::playerProfileSystem::value();
	SDL_StartTextInput();
	_interface->ShowNewPlayerBoxOverGame(canCancel && profiles.GetProfileCount() > 0,
	                                     [&profiles](std::u16string_view name) { return profiles.CheckNewName(name); });
}

void Game::ApplyPlayerSettings(const player_profiles::Settings& settings)
{
	simulation::ApplyPlayerSettings(settings);
}

void Game::LoadLandscape(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();

	auto fixedName = fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path));

	if (!fileSystem.Exists(fixedName))
	{
		throw std::runtime_error("Could not find landscape " + path.generic_string());
	}
	LoadTimer timer(fmt::format("landscape {}", path.stem().string()));
	// the land's file as it was asked for, which a save writes into its world file
	if (Locator::mapScriptSystem::has_value())
	{
		Locator::mapScriptSystem::value().Globals().landscapeFile = path.string();
	}
	InitializeLevel(fixedName);
	SetUpLandscape();
}

void Game::SetUpLandscape()
{
	// the landscape's open: the creature's walkable mask of the new landscape
	land_avoid::Validate(Locator::terrainSystem::value());
	ecs::route_plan_world::OnLandLoaded();
	land_avoid::DumpIfRequested();

	// There is always a player active
	Locator::playerSystem::value().AddPlayer(ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE));

	Locator::cameraBookmarkSystem::value().Initialize();
	Locator::playerSystem::value().RegisterPlayers();
}

void Game::SetTime(float time) noexcept
{
	// SET_GAME_TIME: the clock keeps running from this script time
	TheDayNightClock().SetScriptTime(time);
	Locator::skySystem::value().SetTime(TheDayNightClock().GetScriptTime());
}
