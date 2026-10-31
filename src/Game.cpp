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
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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
#include "3D/SkyType.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Engine/MusicStream.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/GameMusic.h"
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
#include "Camera/Camera.h"
#include "Camera/CameraHelp.h"
#include "Camera/CameraPathControl.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScreenPoint.h"
#include "Camera/ScriptCamera.h"
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
#include "Debug/StateHash.h"
#include "Debug/TestbedScenarioRegistry.h"
#include "Debug/TraceLog.h"
#include "ECS/AbodeKnock.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Animations.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/AudioQueries.h"
#include "ECS/CarriedProps.h"
#include "ECS/ChallengePictures.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureLoop.h"
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
#include "ECS/Events/Publish.h"
#include "ECS/Events/SaveEvents.h"
#include "ECS/Fields.h"
#include "ECS/FireFlies.h"
#include "ECS/FishShoals.h"
#include "ECS/FloatingNumber.h"
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
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptTimer.h"
#include "ECS/ShowNeeds.h"
#include "ECS/Statistics.h"
#include "ECS/SunDraw.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AbodeSoundSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
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
#include "GameClock.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GestureIconEffect.h"
#include "Graphics/ObjectOrder.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/SuperVillagerFrame.h"
#include "Gui/CinemaBars.h"
#include "Gui/GameInterface.h"
#include "Gui/GameMenu.h"
#include "Gui/LoadingScreen.h"
#include "Gui/StartupRules.h"
#include "Gui/TextDatabase.h"
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
#include "Input/FixedMouse.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GameCursor.h"
#include "Input/GamePackets.h"
#include "Input/HandDemo.h"
#include "Input/InputReplay.h"
#include "Input/InterfaceActive.h"
#include "Input/RealInput.h"
#include "Input/SpeedKeys.h"
#include "Input/TempleRoomKeys.h"
#include "LHScriptX/MapScript.h"
#include "LHScriptX/Script.h"
#include "LandBalance.h"
#include "LandLoad.h"
#include "Locator.h"
#include "LocatorInit.h"
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
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"
#include "Serializer/FotFile.h"
#include "Temple/TempleTurn.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Windowing/WindowAway.h"
#include "Worship/Citadel.h"
#include "Worship/TempleLeash.h"
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

/// The day / night clock (Locator::dayNightClock)
DayNightClock& TheDayNightClock()
{
	return Locator::dayNightClock::value().Clock();
}

/// The script fade and the cinema bars (Locator::cinematicDirectorSystem)
ecs::systems::CinematicDirectorSystemInterface& CinematicDirector()
{
	return Locator::cinematicDirectorSystem::value();
}

/// The miracles (Locator::magicSystem)
ecs::systems::MagicSystemInterface& TheMagicSystem()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("game: no miracles service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value();
}

/// The objects the scripts hold (Locator::scriptObjects)
ecs::systems::ScriptObjectsSystemInterface& TheScriptObjects()
{
	if (!Locator::scriptObjects::has_value())
	{
		std::fputs("game: no script objects service in the locator (Locator::scriptObjects)\n", stderr);
		std::abort();
	}
	return Locator::scriptObjects::value();
}

/// The answer to the start-of-game question (Locator::tutorialSkipSystem)
ecs::systems::TutorialSkipSystemInterface& TheTutorialSkipSystem()
{
	if (!Locator::tutorialSkipSystem::has_value())
	{
		std::fputs("game: no start-of-game answer in the locator (Locator::tutorialSkipSystem)\n", stderr);
		std::abort();
	}
	return Locator::tutorialSkipSystem::value();
}

/// The help texts (Locator::helpTextSystem)
ecs::systems::HelpTextSystemInterface& TheHelpTextSystem()
{
	if (!Locator::helpTextSystem::has_value())
	{
		std::fputs("game: no help texts in the locator (Locator::helpTextSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpTextSystem::value();
}

/// The dialogue the help system made, which the help texts hold: nullptr before the help system's start
help::DialogueText* TheDialogue()
{
	return Locator::helpTextSystem::has_value() ? Locator::helpTextSystem::value().GetDialogue() : nullptr;
}

/// The opening's specials (Locator::introSystem)
ecs::systems::IntroSystemInterface& TheIntroSystem()
{
	if (!Locator::introSystem::has_value())
	{
		std::fputs("game: no opening specials in the locator (Locator::introSystem)\n", stderr);
		std::abort();
	}
	return Locator::introSystem::value();
}

/// The villagers drawn in high detail (Locator::highDetailSystem)
ecs::systems::HighDetailSystemInterface& TheHighDetailSystem()
{
	if (!Locator::highDetailSystem::has_value())
	{
		std::fputs("game: no high-detail villagers in the locator (Locator::highDetailSystem)\n", stderr);
		std::abort();
	}
	return Locator::highDetailSystem::value();
}

/// The creatures fizzing out of sight and back in (Locator::creatureFizzSystem)
ecs::systems::CreatureFizzSystemInterface& TheCreatureFizzSystem()
{
	if (!Locator::creatureFizzSystem::has_value())
	{
		std::fputs("game: no creature fizz in the locator (Locator::creatureFizzSystem)\n", stderr);
		std::abort();
	}
	return Locator::creatureFizzSystem::value();
}

/// The help profile (Locator::helpProfileSystem)
ecs::systems::HelpProfileSystemInterface& TheHelpProfileSystem()
{
	if (!Locator::helpProfileSystem::has_value())
	{
		std::fputs("game: no help profile in the locator (Locator::helpProfileSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpProfileSystem::value();
}

/// The player's two advisors (Locator::advisorSystem)
ecs::systems::AdvisorSystemInterface& TheAdvisorSystem()
{
	if (!Locator::advisorSystem::has_value())
	{
		std::fputs("game: no advisors in the locator (Locator::advisorSystem)\n", stderr);
		std::abort();
	}
	return Locator::advisorSystem::value();
}

/// The did-you-know bubble over the sign last tapped (Locator::tipBubbleSystem)
ecs::systems::TipBubbleSystemInterface& TheTipBubbleSystem()
{
	if (!Locator::tipBubbleSystem::has_value())
	{
		std::fputs("game: no tip bubble in the locator (Locator::tipBubbleSystem)\n", stderr);
		std::abort();
	}
	return Locator::tipBubbleSystem::value();
}

/// The owners of the camera and of the game's speed (Locator::scriptControlSystem)
ecs::systems::ScriptControlSystemInterface& TheScriptControlSystem()
{
	if (!Locator::scriptControlSystem::has_value())
	{
		std::fputs("game: no script control in the locator (Locator::scriptControlSystem)\n", stderr);
		std::abort();
	}
	return Locator::scriptControlSystem::value();
}

/// The sky and its clock of day and night (Locator::skySystem)
ecs::systems::SkySystemInterface& TheSkySystem()
{
	if (!Locator::skySystem::has_value())
	{
		std::fputs("game: no sky in the locator (Locator::skySystem)\n", stderr);
		std::abort();
	}
	return Locator::skySystem::value();
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

/// The read-out's little people over the drawn buildings, as world triangles in the plane of the screen: the sprite's
/// cell 57 of misc0, its half size, no turn. One row per building, which the renderer draws right after the building
void FillKnockReadout(std::span<const entt::entity> drawn, const graphics::billboard::CameraFrame& frame,
                      std::vector<graphics::SpiritQuadVertex>& out, std::vector<graphics::KnockRow>& rows)
{
	out.clear();
	rows.clear();
	if (!Locator::abodeKnockSystem::has_value())
	{
		return;
	}
	const auto& knocks = Locator::abodeKnockSystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	for (const auto object : drawn)
	{
		if (!registry.AllOf<ecs::components::Abode>(object))
		{
			continue;
		}
		const auto first = static_cast<uint32_t>(out.size());
		for (const auto& person : knocks.PeopleOver(object))
		{
			const graphics::billboard::Sprite sprite {
			    .position = person.position,
			    .size = person.size,
			    .argb = person.colour,
			    .cell = ecs::abode_knock::k_PersonCell,
			};
			const auto quad = graphics::billboard::SpriteQuad(sprite, frame);
			if (!quad.has_value())
			{
				continue;
			}
			const uint32_t abgr = argb_colour::ToAbgr(person.colour);
			for (const int k : graphics::billboard::k_SpriteTriangles)
			{
				const auto& p = quad->corners.at(static_cast<size_t>(k));
				const auto& uv = quad->uv.at(static_cast<size_t>(k));
				out.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
			}
		}
		if (const auto count = static_cast<uint32_t>(out.size()) - first; count != 0)
		{
			rows.push_back({.building = object, .first = first, .count = count});
		}
	}
}

/// The gesture icons (Magic/Gestures/GestureIcons.h), drawn with the world's particles: none inside the temple. Their
/// fade reads whether a help text was drawn on the screen the frame before, as the help text is drawn after them; this
/// frame's is kept for the next. Each icon's symbol and sparkles are sprites at their pixels, a fixed depth along the
/// view; its model (Magic/Gestures/GestureIconModels.h: a leash, the bubble, or a seed's graphic of
/// Magic/Gestures/GestureIconSeeds.h, whose holder effect steps here) is placed in the models' camera's space and moved
/// on the screen to the box above it
void FillGestureIcons(graphics::OverlayFrame& out)
{
	namespace gestures = magic::gestures;
	out.gestureIcons.clear();
	if (!Locator::magicSystem::has_value())
	{
		return;
	}
	auto& icons = gestures::IconState();
	const bool shownBefore = icons.helpTextShown;
	const auto* dialogue = TheDialogue();
	icons.helpTextShown = out.helpText.active && dialogue != nullptr &&
	                      dialogue->GetDisplay().IsTextDrawn(dialogue->GetTextDraw()) && dialogue->GetDisplay().HasNewestText();
	if (game_clock::IsInsideCitadel() || !Locator::camera::has_value() || out.width == 0 || out.height == 0)
	{
		return;
	}
	const auto draws =
	    gestures::StepIcons(icons.list.Icons(), icons.alpha, help::Get() != nullptr, shownBefore, game_clock::FrameGameMs(),
	                        [](gestures::Gesture g) -> const std::vector<glm::vec3>& { return gestures::ShapeOf(g).points; });
	const auto& camera = Locator::camera::value();
	const auto plane = screen_point::NearPlaneOf(glm::ivec2(out.width, out.height), camera.GetHorizontalFieldOfView(),
	                                             camera.GetPixelNearClip());
	const auto worldToCamera = affine::WorldToCamera(camera.GetOrigin(), camera.GetFocus());
	const auto eye = camera.GetEye();
	const auto at = [&](glm::ivec2 pixel) {
		return screen_point::PixelPoint(worldToCamera, eye, plane, pixel, gestures::k_IconDepth);
	};
	const auto screen = glm::ivec2(out.width, out.height);
	constexpr auto k_LeashMesh = resources::shared_assets::k_LeashCollarMesh.value();
	const auto bubbleMesh = resources::HashIdentifier(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName);
	const float bubbleHalfDiagonal = ecs::object::MeshHalfDiagonal(bubbleMesh);
	for (const auto& draw : draws)
	{
		graphics::GestureIconOverlay icon {.page = draw.page};
		const auto& listed = icons.list.Icons()[draw.icon];
		icon.depthBox = gestures::IconModelDepthBox(listed.box);
		icon.screenOffset = gestures::IconModelScreenOffset(listed.box, screen);
		// the model first, as the icon's draw does: a leash's spin turns only while its icon is drawn
		const auto model = gestures::StepIconModel(listed, gestures::IconLeashSpins(), icons.alpha, game_clock::FrameGameMs(),
		                                           bubbleHalfDiagonal);
		if (model.kind == gestures::IconModelKind::Leash || model.kind == gestures::IconModelKind::Bubble)
		{
			icon.models.push_back({
			    .mesh = model.kind == gestures::IconModelKind::Leash ? k_LeashMesh : bubbleMesh,
			    .matrix = model.matrix,
			    .uv = {0.0f, model.band},
			    .alpha = model.alpha,
			});
		}
		else if (model.kind == gestures::IconModelKind::SeedGraphic)
		{
			// the seed's graphic: its mesh (lit, its colour set), its holder effect drawn now, its bands
			if (auto seed = gestures::DrawIconSeed(listed, model.alpha, game_clock::FrameGameMs()); seed.has_value())
			{
				for (const auto& mesh : seed->draw.meshes)
				{
					if (!mesh.inReach)
					{
						continue;
					}
					icon.models.push_back({
					    .mesh = seed->mesh,
					    .matrix = mesh.matrix,
					    .uv = mesh.uv,
					    .alpha = static_cast<uint8_t>(mesh.colour >> 24u),
					    .colour = mesh.colour & 0xFFFFFFu,
					    .specular = mesh.specular,
					    .lit = true,
					    .landLight = mesh.landLight,
					    .fading = mesh.fading,
					});
				}
				if (!seed->sprites.empty())
				{
					icon.effect = std::make_shared<const graphics::GestureIconEffect>(
					    graphics::GestureIconEffect {.sprites = std::move(seed->sprites)});
				}
				for (const auto& band : seed->draw.bands)
				{
					if (!band.inReach)
					{
						continue;
					}
					// the band's own material: additive, two-sided, no Z write; its colour set, unlit
					icon.bands.push_back({
					    .mesh = resources::shared_assets::k_PowerUpBandMesh.value(),
					    .matrix = band.matrix,
					    .alpha = static_cast<uint8_t>(band.colour >> 24u),
					    .colour = band.colour & 0xFFFFFFu,
					    .specular = gestures::k_IconSeedBandSpecular,
					});
				}
			}
		}
		icon.symbol = {.position = at(draw.symbolPixel),
		               .size = gestures::k_IconSymbolHalfSize,
		               .argb = draw.symbolArgb,
		               .cell = draw.cell,
		               .cellsPerRow = gestures::k_IconSymbolCellsPerRow};
		for (int i = 0; i < draw.sparkles; ++i)
		{
			const auto k = static_cast<size_t>(i);
			icon.sparkles.push_back({.position = at(draw.sparklePixels.at(k)),
			                         .size = gestures::k_IconSparkleHalfSize,
			                         .argb = draw.sparkleArgb.at(k),
			                         .cell = gestures::k_IconSparkleCell});
		}
		out.gestureIcons.push_back(std::move(icon));
	}
}

/// The floating numbers drawn this frame (ecs/FloatingNumber.h): those still showing whose point falls in a pixel of
/// the screen, the furthest from the camera first, as the transparent queue draws them. None while the help system
/// has the wide screen on, nor inside the temple
void FillFloatingNumbers(int width, int height, std::vector<graphics::FloatingNumberOverlay>& out)
{
	out.clear();
	const auto* helpSystem = help::Get();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (width == 0 || height == 0 || !Locator::camera::has_value() || inTemple ||
	    (helpSystem != nullptr && helpSystem->GetWideScreen() != 0))
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto eye = camera.GetOrigin();
	std::vector<std::pair<float, graphics::FloatingNumberOverlay>> placed;
	Locator::entitiesRegistry::value().Each<const ecs::components::FloatingNumber>(
	    [&](entt::entity /*unused*/, const ecs::components::FloatingNumber& number) {
		    const auto alpha = ecs::floating_number::Alpha(number.life);
		    glm::vec3 screen;
		    if (!alpha.has_value() ||
		        !camera.ProjectWorldToScreen(number.position, glm::vec4(0.0f, 0.0f, width, height), screen))
		    {
			    return;
		    }
		    const auto pixel = ecs::floating_number::ScreenPixel(screen, {width, height});
		    if (!pixel.has_value())
		    {
			    return;
		    }
		    const glm::vec3 d = number.position - eye;
		    placed.push_back({glm::dot(d, d),
		                      {.screen = *pixel,
		                       .text = std::u16string(number.text.begin(), number.text.end()),
		                       .colour = number.colour,
		                       .alpha = *alpha}});
	    });
	std::ranges::stable_sort(placed, std::ranges::greater {}, &std::pair<float, graphics::FloatingNumberOverlay>::first);
	out.reserve(placed.size());
	for (auto& each : placed)
	{
		out.push_back(std::move(each.second));
	}
}

/// The villagers' names the land's draw placed this frame (Gui/VillagerLabels.h), laid out for the draw: each a box,
/// a pointer and its lines, drawn in the world at the depth the help system's bubbles take, in front of the point and
/// pulled towards the camera as it looks down
void FillVillagerNames(std::span<const gui::villager_labels::Label> labels, std::vector<graphics::VillagerNameOverlay>& out)
{
	out.clear();
	if (labels.empty() || !Locator::camera::has_value())
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	float heading = 0.0f;
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(camera.GetOrigin(), camera.GetFocus(), heading, pitch);
	const float nearClip = camera.GetPixelNearClip();
	out.reserve(labels.size());
	for (const auto& label : labels)
	{
		graphics::VillagerNameOverlay name {.keyPoint = label.point, .depth = help::DrawDepth(label.depth, pitch, nearClip)};
		// the box: black, its alpha the box's at its top and bottom, its corners rounded by a quarter of its height
		const auto shape =
		    help::BubbleShape(label.left, label.top, label.right, label.bottom, (label.bottom - label.top) * 0.25f, 0.0f, false,
		                      0xFF000000u, false, label.boxAlpha, label.boxAlpha);
		name.box.reserve(shape.size());
		for (const auto& v : shape)
		{
			const uint32_t abgr = (v.argb & 0xFF00FF00u) | ((v.argb >> 16) & 0xFFu) | ((v.argb & 0xFFu) << 16);
			name.box.push_back({v.x, v.y, name.depth, v.u, v.v, abgr});
		}
		const auto pointer = gui::villager_labels::PointerShape(label);
		for (size_t i = 0; i < pointer.size(); ++i)
		{
			const auto& piece = pointer.at(i);
			name.pointer.at(i) = {static_cast<float>(piece.x0),
			                      static_cast<float>(piece.y0),
			                      static_cast<float>(piece.x1),
			                      static_cast<float>(piece.y1),
			                      piece.uv0.x,
			                      piece.uv0.y,
			                      piece.uv1.x,
			                      piece.uv1.y};
		}
		name.pointerAlpha = label.boxAlpha;
		// the lines, one under another from the box's top, set in from its left and top by the inset
		for (size_t line = 0; line < label.lines.size(); ++line)
		{
			name.lines.push_back(
			    {label.lines[line], label.left + label.inset,
			     ((static_cast<float>(static_cast<int32_t>(line)) * label.lineHeight) + label.inset) + label.top});
		}
		name.lineHeight = label.lineHeight;
		name.wordsAbgr = (static_cast<uint32_t>(label.wordsAlpha) << 24) | (static_cast<uint32_t>(label.colour.b) << 16) |
		                 (static_cast<uint32_t>(label.colour.g) << 8) | label.colour.r;
		out.push_back(std::move(name));
	}
}

/// The PreDraw for the overlays (Graphics/OverlayFrame.h): what Renderer::DrawFinishFrameOverlays and
/// DrawHelpText draw, read from the game and laid out once a frame before DrawScene, at the Main view's resolution.
/// boxActive: a box with a background panel takes the input (GameInterface::IsBoxActive)
void FillOverlayFrame(const ecs::systems::CinematicDirectorSystemInterface& director, bool boxActive,
                      graphics::OverlayFrame& out)
{
	const auto& renderer = Locator::rendererInterface::value();
	const auto resolution = renderer.GetResolution();
	out.width = resolution.x;
	out.height = resolution.y;
	// drawn at the end of the frame: the script fade's colour and the wide screen bars
	out.fade.colour = director.GetFadeColour();
	out.fade.barPixels = gui::CinemaBars::BarHeight(out.width, out.height, director.GetWideScreenFraction());

	// the help text's frame: not drawn while a full screen film is open, from its start to the end of its fade (the
	// input prompts below still are)
	auto& helpText = out.helpText;
	auto& helpTexts = TheHelpTextSystem();
	helpText.active = helpTexts.IsStarted() && out.width != 0 && out.height != 0 && !video::IsPlaying();
	helpText.text = {};
	helpText.songBox.reset();
	if (helpText.active)
	{
		const help::WidthFn widthOf = [&renderer](help::TextFont font, std::u16string_view text, float size) {
			return renderer.MeasureText(font, text, size);
		};
		// the song's karaoke: the language follows it, one of its verses is the script's music, and the verse's beat
		{
			const auto lock = audio::game_music::Lock();
			const auto* music = audio::game_music::Get();
			helpTexts.SetSong({.bouncingBall = help::karaoke::UsesBouncingBall(helptext::Language()),
			                   .versePlaying = music != nullptr && help::karaoke::IsVerseMusic(music->GetScriptType()),
			                   .beat = audio::GetScriptAudioState().musicBeat.load()});
		}
		if (const auto* frame = helpTexts.Layout(glm::ivec2(out.width, out.height), out.fade.barPixels, widthOf);
		    frame != nullptr)
		{
			helpText.text = *frame;
			// the newest text's $M codes ask for their control icons as it is drawn (help::control_icon)
			if (auto* dialogue = TheDialogue(); dialogue != nullptr && !frame->controlIcons.empty())
			{
				dialogue->ShowControlIcons(frame->controlIcons, frame->box, out.width);
			}
			// the box glides to the sung word once a frame the texts are drawn, by the frame's clamped ms
			helpText.songBox = helpTexts.FollowSong(helpText.text, game_clock::ClampedFrameMs(game_clock::IsInsideCitadel()));
		}
	}
	// the game speed's words (paused, slow or fast) near the top of the screen: the help text draws them right after the
	// dialogue texts, so not while a film is open either
	out.speedIndicator = {};
	if (helpText.active)
	{
		const auto state = help::StateOf(game_clock::IsPaused(), game_clock::Speed());
		const help::SpeedIndicatorGates gates {
		    .gameRunning = true, // a land is up whenever the overlays are filled
		    .sequenceMode = game_clock::SequenceMode() != game_clock::k_SequenceModeNone,
		    // task 0 is no task: it may set the speed only while no script has it
		    .scriptOwnsSpeed = !TheScriptControlSystem().MaySetGameSpeed(0),
		    .editorMode = false,                // openblack has no such mode
		    .videoPlaying = video::IsPlaying(), // false here: the help text draws nothing while a film is open
		    .setupBoxFlag = boxActive,          // the menu open or the SkipBox shown
		};
		const auto frame = help::Get()->GetSpeedIndicator().Update(state, game_clock::FrameRealMs(), gates);
		if (frame.has_value() && Locator::camera::has_value())
		{
			const auto& camera = Locator::camera::value();
			const auto& text = helptext::Get(help::SpeedText(frame->shown));
			// the text up to its first null, its spaces counted
			const std::u16string_view letters(text.c_str());
			const auto frames =
			    help::PlaceLetters(graphics::billboard::CameraFrame::From(camera),
			                       screen_point::LensOf(out.width, out.height, camera.GetHorizontalFieldOfView()), out.width,
			                       out.height, letters.size(), frame->depth, frame->phase);
			out.speedIndicator.alpha = static_cast<uint8_t>(static_cast<int32_t>(frame->t * 255.0f));
			for (size_t i = 0; i < letters.size(); ++i)
			{
				const auto letter = letters.substr(i, 1);
				const float width = renderer.MeasureText(help::TextFont::J0, letter, help::k_SpeedLetterSize);
				out.speedIndicator.letters.push_back({letter.front(), frames[i], width * -0.5f});
			}
		}
	}
	// the key / mouse icons' draw half (Renderer::DrawInputPrompts): the input prompt icons as the two updates left them,
	// newest first; the hand's point on the screen for DrawKeyOrMouse (the hand's position projected; (inferred) its ints are
	// truncated); the tick count of the icons' update. Under an active box no icon is drawn, though their fades go on
	help::input_prompt::Snapshot(out.inputPrompts.icons, boxActive);
	out.inputPrompts.tickCount = game_clock::TickCount();
	out.inputPrompts.hand.reset();
	if (Locator::handSystem::has_value() && Locator::camera::has_value() && out.width != 0 && out.height != 0)
	{
		const auto hands = Locator::handSystem::value().GetPlayerHands();
		const auto& registry = Locator::entitiesRegistry::value();
		const auto position = !hands.empty() && registry.Valid(hands[0])
		                          ? std::optional<glm::vec3>(registry.Get<ecs::components::Transform>(hands[0]).position)
		                          : std::nullopt;
		const glm::vec4 viewport(0.0f, 0.0f, out.width, out.height);
		out.inputPrompts.hand = help::input_prompt::HandAnchor(position, [&viewport](const glm::vec3& point) {
			glm::vec3 screen;
			return Locator::camera::value().ProjectWorldToScreen(point, viewport, screen) ? std::optional<glm::vec3>(screen)
			                                                                              : std::nullopt;
		});
	}

	// the control pictures over the temple's scroll seen close up: each line's points projected by the frame's camera as
	// the game projects them (none nearer than the near clip; on the screen within its pixels), in the temple's place
	out.templeScrollIcons.clear();
	if (Locator::temple::has_value() && Locator::temple::value().Active() && Locator::camera::has_value() && out.width != 0 &&
	    out.height != 0)
	{
		const auto& temple = Locator::temple::value();
		const auto& camera = Locator::camera::value();
		const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Projection::Normal);
		const float nearClip = camera.GetPixelNearClip();
		const auto width = static_cast<float>(out.width);
		const auto height = static_cast<float>(out.height);
		const glm::vec3 templePlace = temple.GetPosition();
		const temple_scroll_pictures::Project project =
		    [&](const glm::vec3& point) -> std::optional<temple_scroll_pictures::ScreenPoint> {
			const auto clip = viewProjection * glm::vec4(point + templePlace, 1.0f);
			if (clip.w < nearClip)
			{
				return std::nullopt;
			}
			const float inverse = 1.0f / clip.w;
			const float x = ((clip.x * inverse) + 1.0f) * (width * 0.5f);
			const float y = (1.0f - (clip.y * inverse)) * (height * 0.5f);
			return temple_scroll_pictures::ScreenPoint {
			    .x = x, .y = y, .onScreen = x >= 0.0f && y >= 0.0f && x < width && y < height};
		};
		for (const auto& line : temple.GetScrollPictures())
		{
			const auto place =
			    temple_scroll_pictures::ControlPicture(line.frame, line.across, line.down, line.lineX, line.lineY, project);
			int32_t animType = 0;
			int32_t clickType = 0;
			int32_t row = 0;
			std::u16string keyName;
			if (place.has_value() && help::input_prompt::ResolveAction(line.control, animType, clickType, row, keyName))
			{
				out.templeScrollIcons.push_back(
				    {animType, clickType, row, std::move(keyName), place->x, place->y, place->size, 0xFF});
			}
		}
	}

	// the did-you-know bubble, which the help system's 3D draw puts among the world's blended objects, sorted by the sign's
	// point, so that it is drawn before (under) the dialogue texts
	// (note) the bubble's life (Age) and scroll (ClampScroll) change here, in the fill, as the original changes them in
	// the draw; FillOverlayFrame runs once a frame on the logic side, so the Renderer still only reads the copy
	out.didYouKnow = {};
	auto& tipBubble = TheTipBubbleSystem();
	if (tipBubble.IsUp() && Locator::camera::has_value() && out.width != 0 && out.height != 0)
	{
		auto& bubble = tipBubble.GetBubble();
		const auto& camera = Locator::camera::value();
		if (const auto anchor = tipBubble.GetAnchor(); anchor.has_value() && tipBubble.GetDisplayTime() > 0.0f)
		{
			const glm::vec3 point = *anchor;
			const glm::vec3 origin = camera.GetOrigin();
			const glm::vec3 forward = glm::normalize(camera.GetFocus() - origin);
			const float depth = glm::dot(point - origin, forward);
			// the sign's point on the screen wherever it falls, past the screen's edges too; given up only nearer than
			// the near clip
			const glm::vec3 projected = glm::project(point, camera.GetViewMatrix(Camera::Interpolation::Current),
			                                         camera.GetProjectionMatrix(Camera::Projection::Normal),
			                                         glm::vec4(0.0f, 0.0f, out.width, out.height));
			if (const auto screen =
			        help::AnchorOnScreen(projected, depth, camera.GetPixelNearClip(), static_cast<float>(out.height)))
			{
				const auto tail = help::TailFor(screen->x, out.width);
				if (const auto layout = help::LayoutBubble(bubble.Sizes(), depth, *screen, tail.offset, tail.place))
				{
					// in the world at its draw depth, in front of the sign and pulled toward the camera as it looks down
					float heading = 0.0f;
					float pitch = 0.0f;
					script_camera::HeadingAndPitchFromPoints(origin, camera.GetFocus(), heading, pitch);
					const float drawDepth = help::DrawDepth(layout->depth, pitch, camera.GetPixelNearClip());
					const auto depths = help::DepthsOf(drawDepth);
					out.didYouKnow.keyPoint = point;
					out.didYouKnow.depth = drawDepth;
					// the dark colour at 255 x alpha
					constexpr uint32_t k_Colour = 0xFF3BBDED;
					const uint32_t dark = ((k_Colour >> 1) & 0x7F7F7Fu) | 0xFF000000u;
					const float alpha = std::min(bubble.Life(), 1.0f) * layout->fade;
					const auto a = static_cast<uint32_t>(std::clamp(static_cast<int>(alpha * 255.0f), 0, 255));
					// the scroll clamped with the last draw's text height, before it is taken in pixels for this
					// frame's draw (scroll x text size); the drag changes the scroll only after that
					const float size = layout->textSize;
					const auto marks = bubble.ClampScroll(layout->height, layout->margin, size);
					const float scrollPixels = bubble.Scroll() * size;
					// the words (help::LayOutBubbleWords): the tip, a blank line and the title, each part wrapped
					// again only when the part, the width or the size changed
					const auto parts = help::BubbleParts(helptext::Get(tipBubble.GetText()), helptext::Get(0x1016));
					const auto words = help::LayOutBubbleWords(
					    *layout, scrollPixels, alpha, dark, parts,
					    [&bubble, &renderer](size_t part, std::u16string_view text, float maxWidth,
					                         float lineSize) -> const std::vector<std::u16string>& {
						    return bubble.PartWrap(part).Wrap(text, maxWidth, lineSize,
						                                      [&renderer, lineSize](std::u16string_view t) {
							                                      return renderer.MeasureText(help::TextFont::J0, t, lineSize);
						                                      });
					    },
					    [&renderer](std::u16string_view text, float lineSize) {
						    return renderer.MeasureText(help::TextFont::J0, text, lineSize);
					    },
					    [](int action) -> std::optional<help::BubbleKeyBinding> {
						    help::BubbleKeyBinding binding;
						    if (!help::input_prompt::ResolveAction(action, binding.animType, binding.clickType, binding.row,
						                                           binding.keyName))
						    {
							    return std::nullopt;
						    }
						    return binding;
					    });
					for (const auto& line : words.lines)
					{
						out.didYouKnow.lines.push_back({line.run, line.alphaBottom, line.shadow ? depths.shadow : depths.text});
					}
					for (const auto& icon : words.keyIcons)
					{
						out.didYouKnow.keyIcons.push_back({icon.binding.animType, icon.binding.clickType, icon.binding.row,
						                                   icon.binding.keyName, icon.x, icon.y, icon.size, icon.alpha});
					}
					for (const auto& gesture : words.gestures)
					{
						auto& quads = out.didYouKnow.gestures.at(gesture.page);
						const float x = gesture.x;
						const float qy = gesture.y;
						const float w = gesture.width;
						const float h = gesture.height;
						const float uu = gesture.u;
						const float z = gesture.shadow ? depths.shadow : depths.text;
						const graphics::SpiritQuadVertex p0 {x, qy, z, uu, gesture.vTop, gesture.abgr};
						const graphics::SpiritQuadVertex p1 {x + w, qy, z, uu + 0.25f, gesture.vTop, gesture.abgr};
						const graphics::SpiritQuadVertex p2 {x + w, qy + h, z, uu + 0.25f, gesture.vBottom, gesture.abgr};
						const graphics::SpiritQuadVertex p3 {x, qy + h, z, uu, gesture.vBottom, gesture.abgr};
						quads.insert(quads.end(), {p0, p1, p2, p0, p2, p3});
					}
					// the text height (0, then each part's and icon's height added) is written by the z object's draw
					// only: none is queued while alpha x the fade is 0 or less, and the height keeps the last value
					if (alpha > 0.0f)
					{
						bubble.SetTextHeight(words.textHeight);
					}
					// the scroll arrows, while the tick count % 750 < 400, with h the box's height and s = 0.2 h, k = s
					// / 9: the below mark's arrow at the top right, the above mark's one at the bottom right, squares
					// of 0.5 s + 2 k in atmos.raw; colour {0, 0xFF, 0xFF, 0xFF}, alpha = the dark colour's alpha / 2
					if (game_clock::TickCount() % 750u < 400u)
					{
						const float boxTop = layout->topLeft.y;
						const float boxBottom = layout->topLeft.y + layout->height;
						const float boxRight = layout->topLeft.x + layout->width;
						const float s5 = 0.2f * layout->height;
						const float k = s5 / 9.0f;
						const float xr = boxRight - 0.8f * s5;
						// the alpha of the z object's dark colour, stored at 0xFF and never faded: 0xFF >> 1
						out.didYouKnow.arrowAlpha = 0x7F;
						const auto ftolf = [](float v) { return static_cast<float>(static_cast<int>(v)); };
						if (marks.below)
						{
							out.didYouKnow.arrows.push_back({ftolf(xr + 0.25f * s5 - k), ftolf(boxTop + 0.5f * s5 - k),
							                                 ftolf(xr + k + 0.75f * s5), ftolf(boxTop + s5 + k), 0.87890625f,
							                                 0.12890625f, 0.99609375f, 0.24609375f});
						}
						if (marks.above)
						{
							out.didYouKnow.arrows.push_back({ftolf(xr + 0.25f * s5 - k), ftolf(boxBottom - s5 - k),
							                                 ftolf(xr + k + 0.75f * s5), ftolf(boxBottom - 0.5f * s5 + k),
							                                 0.75390625f, 0.00390625f, 0.87109375f, 0.12109375f});
						}
					}
					const auto shape = help::BubbleShape(layout->topLeft.x, layout->topLeft.y,
					                                     layout->topLeft.x + layout->width, layout->topLeft.y + layout->height,
					                                     layout->margin, layout->topLeft.x + layout->width * layout->tailPlace,
					                                     layout->tailRight, (dark & 0x00FFFFFFu) | (a << 24), true, 0, 0xFF);
					// queued only while its alpha times the distance fade is above 0
					out.didYouKnow.active = alpha > 0.0f && !shape.empty();
					for (const auto& v : shape)
					{
						const uint32_t abgr = (v.argb & 0xFF00FF00u) | ((v.argb >> 16) & 0xFFu) | ((v.argb & 0xFFu) << 16);
						out.didYouKnow.shape.push_back({v.x, v.y, depths.box, v.u, v.v, abgr});
					}
					// the hand's drag of the bubble and its hover
					{
						const auto mouse = input::GameCursor();
						const auto id = entt::to_integral(tipBubble.GetSign());
						auto* hand = Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr;
						bubble.Drag(hand != nullptr && hand->DraggedScreenObject() == id, mouse.y, size);
						const float x0 = layout->topLeft.x;
						const float y0 = layout->topLeft.y;
						const auto mx = static_cast<float>(mouse.x);
						const auto my = static_cast<float>(mouse.y);
						if (mx > x0 && mx < x0 + layout->width && my > y0 && my < y0 + layout->height)
						{
							if (hand != nullptr) // (openblack guard) the original always has its interface
							{
								static_cast<void>(hand->OfferScreenObject(id, layout->depth));
							}
							tipBubble.Hover(); // life back to 3.0
						}
					}
				}
			}
		}
		tipBubble.UpdateFrame(static_cast<float>(game_clock::FrameGameMs())); // the frame's game ms x 0.0002
	}

	// the advisor spirits (Locator::advisorSystem): the spirits and their trails. Not while the film covers the screen
	// or the falling spell hides the world: DrawScene draws no spirit then, and the halo clock stays (as when the
	// Renderer asked the runtime itself)
	out.spirits.clear();
	out.spiritTrails.clear();
	if (auto& advisors = TheAdvisorSystem();
	    advisors.IsLoaded() && !video::Get().CoversScreen() && !video::GetFallingSpell().HidesWorld())
	{
		advisors.FillOverlay(out);
	}
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
	    .doubleClick = event.button.clicks == 2,
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

/// What a jump to a camera bookmark is made from: the interface status's place of the last turn, the camera's origin,
/// and whether the game is inside the citadel
ecs::systems::BookmarkJumpFrom JumpFromCamera(const Camera& camera)
{
	return {
	    .statusPlace =
	        Locator::handSystem::has_value() ? Locator::handSystem::value().GetSyncedPlace() : map_coords::MapCoords {},
	    .cameraOrigin = camera.GetOrigin(),
	    .insideCitadel = game_clock::IsInsideCitadel(),
	};
}

/// A flight to a camera bookmark is the player's camera's: Creature Mode hands it back first
void FlyToBookmark(Camera& camera, const ecs::systems::BookmarkFlight& flight)
{
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	camera.GetModel().SetFlight(flight.origin, flight.focus);
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
	const float frameSeconds = game_clock::FrameGameSeconds();
	const glm::vec3 eye = camera.GetOrigin();
	const ecs::draw_list::Consumer consumer = [&view, &registry, &animatedStatics, &abodeSounds, &townDesires, &eye, frameMs,
	                                           realMs, frameSeconds](entt::entity object) -> std::optional<bool> {
		if (ecs::script_highlight::IsHighlight(object))
		{
			return ecs::script_highlight::Draw(object, view, frameMs);
		}
		// A town desire flag's Draw steps it; only a flag that shows is drawn, and so tested on the screen
		if (std::as_const(registry).AllOf<ecs::components::TownDesireFlag>(object))
		{
			if (!townDesires.DrawFlag(object, frameSeconds, view.eye))
			{
				return true;
			}
			return ecs::draw_list::DrawOnScreen(ecs::draw_list::OnScreenInputsOf(registry, object), view);
		}
		// A workshop's and a worship site's Draw draw their need signs first (the frame's game ms x 0.001)
		ecs::show_needs::DrawFor(object, static_cast<float>(frameMs) * 0.001f, eye);
		// An animated static's Draw plays its clip on first
		animatedStatics.Draw(object, frameMs);
		// A creature's Draw places the glows on its body, on the screen or not
		if (std::as_const(registry).AllOf<ecs::components::Creature>(object) && Locator::creatureAnimationSystem::has_value())
		{
			Locator::creatureAnimationSystem::value().PlaceHandGlows(object, realMs, frameMs);
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
	}
}

/// The middle button let go: out of relative mode, the cursor back where it went down
void LetGoOfMiddleButton(const glm::ivec2& pressPosition)
{
	SDL_SetRelativeMouseMode(SDL_FALSE);
	if (Locator::windowing::has_value())
	{
		SDL_WarpMouseInWindow(static_cast<SDL_Window*>(Locator::windowing::value().GetHandle()), pressPosition.x,
		                      pressPosition.y);
	}
}

/// The hand's mesh from its file. The hand's good or evil shape is drawn into it as the player's alignment moves, so a
/// new game, whose hand starts neutral, reads it afresh as the launch did
void LoadHandMesh()
{
	auto& meshes = Locator::resources::value().GetMeshes();
	meshes.Erase(ecs::components::Hand::k_MeshId);
	meshes.Load(ecs::components::Hand::k_MeshId, resources::L3DLoader::FromDiskTag {},
	            Locator::filesystem::value().GetPath<filesystem::Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
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
	[[nodiscard]] std::chrono::duration<float, std::milli> GetDeltaTime() const override { return _game.GetDeltaTime(); }
	[[nodiscard]] const hand_tricons::Fades& GetHandTriconFades() const override { return _game.GetHandTriconFades(); }
	[[nodiscard]] const graphics::ObjectListFrame* GetObjectListFrame() const override { return _game.GetObjectListFrame(); }

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
    , _workingFolder(StartingWorkingFolder())
    , _testMenuAt(TestMenuAtFromEnvironment())
{
	// with the members, before anything else: the script fade, the day / night clock, the map script's globals, the
	// frame count with its screenshot request (--screenshot-frame's, if given) and the camera
	StartGameState(args.requestScreenshot);
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
	// the corner frame counter stays off in the checks' runs, so that their screenshots do not change
	config.checkRun = debug::frame_counter::IsCheckRun(input::IgnoreRealInput(), _startTestbed, args.numFramesToSimulate);
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
	LoadHandMesh();
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
		return false;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == window.GetID())
		{
			return false;
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
		case SDLK_f:
			window.SetDisplayMode(windowing::DisplayMode::Fullscreen);
			break;
		case SDLK_p:
			game_clock::Pause(!game_clock::IsPaused());
			break;
		case SDLK_F11: // F1 is the game's Help key
			Locator::rendererInterface::value().SetDebug(!Locator::rendererInterface::value().GetDebug());
			break;
		case SDLK_BACKSPACE:
			// Backspace goes back to the view before the last jump, while no script holds the wide screen and the key
			// shortcuts are on (the bookmarks' switch does not gate it)
			if (!input::BeforeJumpKeyActs({
			        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			        .keyShortcutsEnabled = help::interface_interaction::KeyShortcutsEnabled(),
			        .bookmarksEnabled = true,
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
		case SDLK_1:
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
		case SDLK_5:
		case SDLK_6:
		case SDLK_7:
		case SDLK_8:
			// the keys 1..Tab are skipped while a script holds the wide screen or a SET_INTERFACE_INTERACTION level has
			// a ControlMap switch off, and the bookmark keys while they are disabled (PLAY_JC_SPECIAL 14 / 15)
			// the key 8 is not a bookmark key
			if (!input::IsBookmarkKey(static_cast<int>(event.key.keysym.sym - SDLK_1) + 1))
			{
				break;
			}
			if (!input::BookmarkKeyActs({
			        .scriptWideScreen = help::Get() != nullptr && help::Get()->IsScriptWideScreen(),
			        .keyShortcutsEnabled = help::interface_interaction::KeyShortcutsEnabled(),
			        .bookmarksEnabled = Locator::cameraBookmarkSystem::value().IsEnabled(),
			    }))
			{
				break;
			}
			{
				const auto mod = event.key.keysym.mod;
				const auto index = static_cast<uint8_t>(event.key.keysym.sym - SDLK_1);
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
		break;
	case SDL_MOUSEMOTION:
	{
		auto& cursor = input::GameCursorRef();
		// an input recording playing moves the game's cursor, not the real pointer
		if (input::replay::IsPlaying())
		{
			cursor = {event.motion.x, event.motion.y};
		}
		else
		{
			SDL_GetMouseState(&cursor.x, &cursor.y);
		}
		break;
	}
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		switch (event.button.button)
		{
		case SDL_BUTTON_MIDDLE:
		{
			// Relative mode while held: the cursor stays put and only the motion drives the camera.
			// (the press position is kept by ApplyMouseButton above)
			const bool pressed = event.type == SDL_MOUSEBUTTONDOWN;
			SDL_SetRelativeMouseMode(pressed ? SDL_TRUE : SDL_FALSE);
			// (a box that let go of it brought the cursor back already)
			if (!pressed && middleWasHeld)
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
	game_turn::PlayTurn([this, &turn](game_turn::Step step) { PlayTurnStep(step, turn); });
	// after the game code the original also runs the wall huggers' lookahead and the repair of missing mothers

	// OPENBLACK_STATE_HASH: the state of this turn, for the replay test (Debug/StateHash.h)
	state_hash::OnTurnEnd(turn);

	return false;
}

void Game::PlayTurnStep(game_turn::Step step, uint32_t& turn) noexcept
{
	using namespace ecs::components;
	using namespace ecs::systems;
	using game_turn::Step;

	switch (step)
	{
	case Step::PlayerCommands:
	{
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
		const auto currentTime = std::chrono::steady_clock::now();
		_turnDeltaTime = currentTime - _lastGameLoopTime;
		_lastGameLoopTime = currentTime;
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
		// the reactions' clock (the game turn) for the whole turn, and the ones whose initiator went
		// (ECS/Effects/Reactions)
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
			help::Process(chlapi::ScriptVm(), GetTurn());
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
		// the players' sparkles and the mobile objects' checksum: (not ported)
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
		// the camera's stacked modes are checked: the script camera's things that have gone are dropped
		script_camera::Validate();
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
			game_stats::TrackFrameRate(_statisticsFrameRate.Rate());
		}
		break;
	}
	case Step::_Count:
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
				                     Locator::handSystem::value().GetInterfaceHandState() == k_GripLandscapeHandState});
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
			// the quick save key, not while the debug windows have the keyboard
			if (!Locator::debugGui::value().StealsFocus())
			{
				const auto& actions = Locator::gameActionSystem::value();
				if (actions.Get(input::BindableActionMap::QUICK_SAVE) &&
				    actions.GetChanged(input::BindableActionMap::QUICK_SAVE))
				{
					ecs::save_triggers::QuickSaveKey();
				}
			}
		}
		break;
	}
	case Step::StopCheck:
	{
		auto& config = Locator::config::value();

		// a restart asked for in the menu ends this game's frames too (Run starts the next). Leaving the game saves it
		// into the autosave's slot first, as the original's play loop does as it ends (a run of the checks keeps no
		// profile, so nothing is written)
		if (!config.running || _restartRequested)
		{
			ecs::save_triggers::Exiting();
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
			if (!script_camera::UpdateCamera(camera, static_cast<float>(game_clock::CameraFrameMs()) * 0.001f,
			                                 game_clock::FrameGameMs(), game_clock::FrameGameSeconds()))
			{
				// Creature Mode keeps the camera on its creature, and takes it or gives it back before it moves
				if (Locator::creatureModeSystem::has_value())
				{
					auto creatureMode = profiler.BeginScoped(Profiler::Stage::CreatureModeUpdate);
					Locator::creatureModeSystem::value().Update(
					    deltaTime, {.handGripping = input::HandGripping(_mouseButtons),
					                // the hand's pick of its last update: the camera moves before the hand
					                .creatureUnderHand = Locator::creatureHandSystem::has_value()
					                                         ? Locator::creatureHandSystem::value().CreatureUnderHand()
					                                         : std::nullopt,
					                .doubleClickFlies = Locator::cameraHelpSystem::value().Get().IsFeatureEnabled(
					                    camera_help::Feature::DoubleClickFly)});
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
			else if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
			{
				// A script's camera ends Creature Mode
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
		auto& profiler = Locator::profiler::value();
		auto& camera = Locator::camera::value();
		auto& deltaTime = frameState.deltaTime;
		// with the landscape's draw: the physics objects drawn between their last two turn poses (before the
		// trees' bending after the hand, which takes them as sources, and before the drawing), and the dust
		// the profile's "Frame Updaters" (no return before its End below: Profiler::End checks the level)
		profiler.Begin(Profiler::Stage::FrameUpdaters);
		Locator::dynamicsSystem::value().UpdateFrame(GetTurnFraction(), game_clock::FrameGameSeconds());

		// the wind the ripe fields and the trees sway in, once per frame like the trees' pre-draw, by the frame's whole game
		// ms (none in pause)
		{
			auto sway = profiler.BeginScoped(Profiler::Stage::VegetationUpdate);
			Locator::vegetation::value().Update(
			    std::chrono::duration<float, std::milli>(static_cast<float>(game_clock::FrameGameMs())));
		}
		// Fields: visibility and sinking with their food (as their draw does)
		Locator::fieldSystem::value().Update(std::chrono::duration<float>(deltaTime));
		// the trees' draw: their brightness this frame and the rustle of the tall ones by the camera (their bends come
		// after the hand)
		ecs::UpdateTrees(std::chrono::duration<float>(deltaTime).count());

		// Fireflies (as their draw does): orbit and fade, in game time
		ecs::UpdateFireFlies(game_clock::FrameGameSeconds(), camera.GetOrigin());
		// the script highlights: the hidden ones' meshes taken away and given back (their pose is set by their Draw, from
		// the object draw list)
		ecs::script_highlight::SyncMeshes();
		// the town desire flags over the stores: a new store takes them (they step only as the object draw list draws them)
		Locator::townDesireSystem::value().UpdateFlags();

		// Water rings: the frame's game time, in milliseconds
		Locator::waterRingSystem::value().Update(
		    std::chrono::duration<float, std::milli>(static_cast<float>(game_clock::FrameGameMs())));
		// the static the creatures' eyes fizz through slides on, once a frame as the frame starts, by the frame's game time
		TheCreatureFizzSystem().UpdateFrame(game_clock::FrameGameSeconds());
		// the designed scenery of Land 3 (waterfall) and Land 4 (ark, dinosaur), by land number
		ecs::designed_scenery::Update(static_cast<float>(game_clock::FrameGameMs()));
		// The marks on the ground, the blasts' rubble and the uprooted trees' craters (just before the smoky stuff): fade
		// and go
		{
			auto explosions = profiler.BeginScoped(Profiler::Stage::ExplosionUpdate);
			Locator::explosionSystem::value().Update(static_cast<float>(game_clock::FrameGameMs()));
		}
		// the reward chests from the sky fall and thump down in the world's draw, by the frame's game time
		Locator::rewardSystem::value().Update(static_cast<float>(game_clock::FrameGameMs()));
		// the missionaries' boat before its draw, the smoky stuff, after its draw (ecs/MissionaryBoat.h). The smoky stuff also
		// moves the smoke an object leaves when it goes (ecs/DisappearSmoke.h), in game time, boat or not.
		ecs::missionary_boat::Update(static_cast<float>(game_clock::FrameGameMs()));
		// in the landscape's draw, before the SuperVillager loop: the intro hand's grip, from the hand as the last frame
		// left it; then, just before the boat's after-draw, and the light's Z-object callback of this frame: the intro
		// light, the debug camera's points and the intro hand (Locator::introSystem). (approximate) after the boat's update
		// here. The light's CRT Random draws: (approximate, pending): the original draws in the Z drain, after the clouds,
		// night lights and smoke; to move to the end of Renderer::PreDraw
		TheIntroSystem().Update(game_clock::FrameGameMs());

		// in the landscape's draw: the SuperVillagers go without a script wide screen; the others take the on-screen
		// test and set this frame's fade and yaw parameters of the smooth drawing (ecs/SuperVillager.h)
		TheHighDetailSystem().Update();
		// Villagers and animals drawn between turns, turning smoothly, on the slope (ecs/MobileDrawing.h)
		ecs::UpdateMobileDrawing(GetTurnFraction(), static_cast<float>(game_clock::FrameGameMs()));
		// after the villagers' draw: a SuperVillager with feature 7 at the intro hand's grip
		ecs::super_villager::FollowHand();
		// the disciples' glows, and the symbol of the one in the hand or under the pointer, over the villagers where they
		// are drawn this frame (as the original draws them with the villager's own drawing)
		ecs::disciple_marks::Update(camera.GetOrigin(), Locator::windowing::has_value()
		                                                    ? static_cast<float>(Locator::windowing::value().GetSize().x)
		                                                    : 0.0f);
		// Skeletal animation of villagers and animals (ecs/Animations.h), in milliseconds of game time
		ecs::UpdateVillagerAnimations();
		ecs::UpdateAnimalAnimations();
		ecs::UpdateAnimations(static_cast<float>(game_clock::FrameGameMs()));
		ecs::UpdateCarriedProps();
		// the scaffold the local hand holds, as it is drawn: its phantom's appear and disappear sounds (ECS/Scaffolds.h)
		if (const auto held = Locator::handSystem::value().GetHeldObject(); held.has_value())
		{
			ecs::scaffolds::PlayHeldPhantomSound(*held);
		}
		// the creatures between turns, which stand still with the world while it is paused in the citadel
		// (ECS/CreatureLoop.h)
		if (!ecs::creature_loop::FrozenInCitadel(game_clock::IsPaused(), game_clock::IsInsideCitadel()))
		{
			ecs::creature_loop::UpdateFrame(GetTurnFraction(), game_clock::FrameGameMs(), profiler);
		}
		// the sharks drawn between turns, heading, wake rings (the shark system)
		Locator::sharkSystem::value().Update(
		    std::chrono::duration<float, std::milli>(static_cast<float>(game_clock::FrameGameMs())), GetTurnFraction());

		// FishFarm shoals, moved with the frame's game time
		ecs::UpdateFishShoals(game_clock::FrameGameSeconds(), camera.GetOrigin());
		// after the fish and the boat's after-draw: each SuperVillager's eyes after its body, on screen only
		// (ecs/SuperVillagerEyes.h); the swim rings' Random(0, 2 pi) of the driver loop came before them, in Update.
		// Neither argument is read by ours, which steps the eyes by the frame's game milliseconds
		TheHighDetailSystem().PlaceEyes(game_clock::VisualMs(), camera.GetViewProjectionMatrix());
		// the town centres' draw: the town belief symbols' PSys step, once a rendered frame
		particles::town_belief::Step();
		// the floating numbers rise and fade by the frame's game time, and go once their time is up
		ecs::floating_number::StepAll(Locator::entitiesRegistry::value(), game_clock::FrameGameSeconds());
		// the forest miracle's camera particle's draw: its drawn frame is its camera path's time, after the camera's update
		particles::forest_camera::UpdateFrame(GetTurnFraction());

		// from the help system's Draw3D: the cinema bars slide with the game time of this frame
		CinematicDirector().Update(std::chrono::duration<float, std::milli>(static_cast<float>(game_clock::FrameGameMs())));
		// the scripts' words on the screen fade by the game's time of the frame, and stand still while a video plays
		if (!video::IsPlaying())
		{
			CinematicDirector().UpdateTexts(static_cast<int32_t>(game_clock::FrameGameMs()));
		}
		profiler.End(Profiler::Stage::FrameUpdaters);
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
					input::HandButtonsRef() = {.gripping = demo->grip, .action = demo->action};
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
			// then the bookmarks' shared pulse moves on by the frame's game ms and sizes the markers, which a script's wide
			// screen hides
			Locator::cameraBookmarkSystem::value().Update(game_clock::FrameGameMs(),
			                                              help::Get() != nullptr && help::Get()->IsScriptWideScreen());
		}
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
				// the help system's interface pass, every frame: the click since the last one, and its key, held, which cuts
				// the text with no click
				if (auto* dialogue = TheDialogue(); dialogue != nullptr)
				{
					dialogue->ProcessClick(std::exchange(_dialogueClick, false));
				}
				else
				{
					_dialogueClick = false;
				}
				const bool middleWasHeld = _mouseButtons.middle;
				const bool menuUp = _interface && _interface->IsBoxActive();
				bool boxHasMouse = false;
				if (input::BoxTakesInput(_mouseButtons, menuUp))
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

	auto& resources = Locator::resources::value();
	auto& meshManager = resources.GetMeshes();
	auto& textureManager = resources.GetTextures();
	auto& animationManager = resources.GetAnimations();
	auto& levelManager = resources.GetLevels();
	auto& glowManager = resources.GetGlows();
	auto& cameraPathManager = resources.GetCameraPaths();

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

	_startupTimer->Step("temple outside meshes");

	fileSystem.Iterate( //
	    fileSystem.GetPath<filesystem::Path::Citadel>() / "engine", false,
	    [&meshManager, &glowManager, &cameraPathManager](const std::filesystem::path& f) {
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
		    else if (f.extension() == ".cam")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple cam: {}", f.stem().string());
			    try
			    {
				    cameraPathManager.Load(fmt::format("temple/{}", f.stem().string()),
				                           resources::CameraPathLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });
	// The main room's markers of the temples, creatures and challenges on its map
	for (const auto* icon : {"I_citadel_on_map", "I_creature_on_map", "I_challenge_on_map"})
	{
		try
		{
			meshManager.Load(fmt::format("temple/icons/{}", icon), resources::L3DLoader::FromDiskTag {},
			                 fileSystem.GetPath<Path::Citadel>() / "icons" / fmt::format("{}.l3d", icon));
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	_startupTimer->Step("temple interior meshes, glows, cameras and icons");

	pack::PackFile pack;

	// read once into the byte cache: the exploding objects parse the same copy later
	auto packResult = pack::PackResult::ErrCantOpen;
	try
	{
		const auto& packBytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                                            fileSystem.FindPath(fileSystem.GetPath<Path::Data>() / "AllMeshes.g3d"));
		packResult = pack.ReadFile(*resources::BlobStream(packBytes));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "AllMeshes.g3d: {}", e.what());
	}
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllMeshes.g3d: {}", pack::ResultToStr(packResult));
		return false;
	}

	_startupTimer->Step("mesh pack read");
	const auto& meshes = pack.GetMeshes();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& mesh : meshes)
	{
		const auto meshId = static_cast<MeshId>(i);
		// a pack may have more meshes than openblack has names for
		const auto name = i < k_MeshNames.size() ? k_MeshNames[i] : fmt::format("Mesh{}", i);
		meshManager.Load(meshId, resources::L3DLoader::FromBufferTag {}, name, mesh);
		++i;
	}

	_startupTimer->Step("mesh pack meshes");
	const auto& textures = pack.GetTextures();
	for (auto const& [name, g3dTexture] : textures)
	{
		textureManager.Load(g3dTexture.header.id, resources::Texture2DLoader::FromPackTag {}, name, g3dTexture);
	}

	_startupTimer->Step("mesh pack textures");
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
	_startupTimer->Step("animation pack");

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
			if (!meshId.has_value())
			{
				// A file whose name names no appearance is not a species' body; loading it would take the id of the
				// real mesh of that species
				return;
			}
			meshManager.Load(*meshId, resources::L3DLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});
	_startupTimer->Step("creature meshes");

	// Load loose one-off assets
	{
		// the ids of the loose assets (the coffre's animation and mesh share one id, in different caches)
		constexpr auto k_CoffreId = entt::hashed_string("coffre");
		constexpr auto k_ConeMesh = entt::hashed_string("cone");
		constexpr auto k_MarkerMesh = entt::hashed_string("marker");
		constexpr auto k_RiverMesh = entt::hashed_string("river");
		constexpr auto k_River2Mesh = entt::hashed_string("river2");
		constexpr auto k_MetreSphereMesh = entt::hashed_string("metre_sphere");

		using AFromDiskTag = resources::L3DAnimLoader::FromDiskTag;
		animationManager.Load(k_CoffreId.value(), AFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.anm");

		using LFromDiskTag = resources::L3DLoader::FromDiskTag;
		LoadHandMesh();
		_startupTimer->Step("hand mesh");
		// What moves each species' body, and the meshes whose skins are painted
		resources::LoadCreatureRigs(Locator::resources::value());
		_startupTimer->Step("creature rigs");
		// The eyes every creature is drawn with
		for (const auto& [id, file] : {std::pair {ecs::components::CreatureEyes::k_EyeballMeshId, "Eyeball.l3d"},
		                               std::pair {ecs::components::CreatureEyes::k_EyelidMeshId, "Eyelid.l3d"}})
		{
			if (const auto path = fileSystem.GetPath<Path::Data>() / file; fileSystem.Exists(path))
			{
				meshManager.Load(id, LFromDiskTag {}, path);
			}
		}
		meshManager.Load(k_CoffreId.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.l3d");
		// The closed reward chest
		if (const auto path = fileSystem.GetPath<Path::Misc>() / "chest0.l3d"; fileSystem.Exists(path))
		{
			meshManager.Load(entt::hashed_string("misc/chest0").value(), LFromDiskTag {}, path);
		}
		meshManager.Load(k_ConeMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "cone.l3d");
		meshManager.Load(k_MarkerMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "marker.l3d");
		meshManager.Load(k_RiverMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river.l3d");
		meshManager.Load(k_River2Mesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river2.l3d");
		meshManager.Load(k_MetreSphereMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "metre_sphere.l3d");
		_startupTimer->Step("one-off meshes");
		// the in-game menus
		CreateInterface();
		_startupTimer->Step("game interface");
		// the dispensers' bubble, made with the one-shot orb: .\data\spells\meshes\O_Bibble_up.l3d (not in the
		// test data)
		try
		{
			meshManager.Load(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName, LFromDiskTag {},
			                 fileSystem.GetPath<Path::Data>() / "Spells" / "Meshes" / "O_bibble_up.l3d");
			// the shared mesh with MaterialProperties {1, 1, 0, 1, 1}: the change flag rewrites every primitive:
			// the cap's AlphaTextured (4) -> 6 -> additive 13 -> with Z write 12 (SRCALPHA / ONE, alpha = texture x
			// diffuse), and the double-sided bit cleared. The object alpha's mode table keeps mode 12, so the bubble
			// ADDS its texture x 0x95 to what is behind it: the bright, pearly bubble of the original
			meshManager.Handle(resources::HashIdentifier(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName))
			    ->SetMaterialProperties(
			        {.additive = true, .zWrite = true, .doubleSided = false, .change = true, .alpha = true});
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
		// the temples' leash posts: their collar in the rope's texture (the game loads it with the first temple)
		resources::shared_assets::LoadLeashCollar("Leash posts");
	}

	_startupTimer->Step("spell seed bubble and leash collar");

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

	_startupTimer->Step("level list");

	StartAudio();

	_startupTimer->Step("sound and music banks");
	// The pre-intro, once there is audio for its music
	ContinueStartupScreens();
	{
		InfoFile infoFile;
		auto result = infoFile.LoadFromFile(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "info.dat");
		if (!result)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to load game info data.");
			return false;
		}
		Locator::infoConstants::reset(result.release());
	}

	_startupTimer->Step("info tables");

	StartHelp();
	_startupTimer->Step("help");

	fileSystem.Iterate(fileSystem.GetPath<Path::Textures>(), false, [&textureManager](const std::filesystem::path& f) {
		// The game ships a grey ice map it never loads, cut short of a whole texture
		constexpr std::string_view k_UnusedTexture = "s_iceenvmapgrey.raw";
		if (string_utils::LowerCase(f.extension().string()) == ".raw" &&
		    string_utils::LowerCase(f.filename().string()) != k_UnusedTexture)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading raw texture: {}", f.stem().string());
			try
			{
				textureManager.Load(fmt::format("raw/{}", f.stem().string()), resources::Texture2DLoader::FromDiskTag {}, f);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			}
		}
	});
	_startupTimer->Step("raw textures");

	// The noise that makes the snow's edges on the land ragged, read now into the byte cache for the land's prints step
	// (ecs::land_prints)
	(void)resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(),
	                                  fileSystem.GetPath<Path::WeatherSystem>() / "snowmap.raw", "snow's noise");

	// The texture every creature's hair is drawn with, and its alpha beside it
	for (const auto& [id, name] : {std::pair {ecs::components::CreatureHair::k_TextureId, "C_Ape_Hair.raw"},
	                               std::pair {ecs::components::CreatureHair::k_AlphaTextureId, "C_Ape_Haira.raw"}})
	{
		const auto path = fileSystem.GetPath<Path::Data>() / name;
		if (!fileSystem.Exists(path))
		{
			continue;
		}
		try
		{
			textureManager.Load(id, resources::Texture2DLoader::FromDiskTag {}, path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	// What creatures' tattoos and marks are painted with. The symbols the game ships with fill the cells no player's
	// symbol has been written into.
	try
	{
		const auto dataDirectory = fileSystem.GetPath<Path::Data>();
		const auto texturesDirectory = fileSystem.GetPath<Path::Textures>();
		Locator::resources::value().GetCreatureSkinArt().Load(
		    creature_skin::k_ArtId, resources::CreatureSkinArtLoader::FromDiskTag {},
		    resources::CreatureSkinArtLoader::Paths {
		        .symbols = texturesDirectory / "PlayersSymbols.raw",
		        .defaultSymbols = texturesDirectory / "OriginalChooseSymbol.raw",
		        .freshDamage = dataDirectory / "damage_new256.raw",
		        .freshDamageAlpha = dataDirectory / "damage_new256A.raw",
		        .oldDamage = dataDirectory / "damage_old256.raw",
		        .oldDamageAlpha = dataDirectory / "damage_old256A.raw",
		        .palette = dataDirectory / "tattoocols.raw",
		    });
	}
	catch (const std::exception& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Creature skin art: {}", err.what());
	}
	_startupTimer->Step("snow, hair and skin art textures");

	return true;
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
}

void Game::StartAudio()
{
	// the audio's start: the sample main volume and the channels' queries, then its banks (every .sad of the Audio
	// directory, audio::banks)
	audio::Init(MakeMusicQueries(*this));

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
		// the voice bank is loaded
		queries.voiceBankLoaded = [](audio::SfxBank bank) { return audio::voices::BankRegistered(bank); };
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
		// after that text is shown (Game::Loop takes it, see _textShotAtMs)
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
	// A save that could not be written is told by the game's message box
	events.AddHandler(std::function([this](const ecs::events::SaveProblem& /*problem*/) {
		if (_interface)
		{
			_interface->ShowSaveProblem();
		}
	}));
	events.AddHandler(std::function([this, &config](const SDL_Event& event) {
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
			// Inside the temple, Escape goes back to its main room and out, as the temple's keys do
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::temple::has_value() && Locator::temple::value().Active())
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
			// the escape menu takes Escape, and the keyboard and mouse while a box is open. Escape opens no menu with
			// a modifier held (Shift or Ctrl), over a full screen film (it skips the film instead) or while a script
			// holds the wide screen. (pending) a help task's dialogue stopped first, and the 300 ms debounce.
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
	EndGame();
	if (!StartNewGame(std::nullopt, _startConfig.numFramesToSimulate, std::nullopt))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game could not start again");
		return false;
	}
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
	const bool startTestbed = _startTestbed && !_landRun.has_value();
	auto startMap = _landRun.has_value() ? fileSystem.GetPath<filesystem::Path::Scripts>() / _landRun->land : _startMap;
	if (!_landRun.has_value() && _startOnMapScriptLand)
	{
		startMap = MapScriptLand(fileSystem).value_or(_startMap);
	}

	// Each game started: its first film can be skipped, unless no player has a profile yet (the new player's first game)
	video::Get().SetNoSkip(startup::FilmsUnskippable(HasPlayerProfiles()));
	if (startTestbed)
	{
		LoadTestbed();
	}
	else
	{
		// A first start: the new player is asked their name before the first land
		AskForNewPlayer();
		if (!LoadMap(startMap))
		{
			return false;
		}
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

	auto challengePath = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
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
			if (!startTestbed && !questRun)
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
	// --replay-input / --record-input (test runs): from the run's first frame
	if (_replayInput.has_value())
	{
		input::replay::StartPlayback(*_replayInput, 0);
	}
	if (_recordInput.has_value())
	{
		const auto window = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
		input::replay::StartRecording(*_recordInput, 0, window.x, window.y);
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
	return RunStart::Play;
}

bool Game::RunFrame() noexcept
{
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
	return true;
}

void Game::EndRun() noexcept
{
	if (input::replay::IsRecording())
	{
		input::replay::FinishRecording();
	}
	_objectListFrame = nullptr;
	_frameLoop.reset();
}

void Game::FillHandTricons(bool boxActive, graphics::OverlayFrame& out)
{
	out.handTricons = {};
	out.handDemoMouse.reset();
	// A turn whose hand state holds the red cross back keeps it away for at least 250 ms, counted down every frame by
	// the frame's time ((inferred) the wall clock's), whether or not the icons are drawn
	if (Locator::handSystem::has_value())
	{
		constexpr int32_t k_CrossHeldMs = 250;
		const auto holdBacks = Locator::handSystem::value().GetCameraIconFacts().crossHoldBacks;
		if (holdBacks != _handCrossHoldBacks)
		{
			_handCrossHoldBacks = holdBacks;
			_handCrossHeldMs = std::max(_handCrossHeldMs, k_CrossHeldMs);
		}
		_handCrossHeldMs = std::max(_handCrossHeldMs - static_cast<int32_t>(game_clock::FrameRealMs()), 0);
	}
	// Faded and drawn only with the camera and the hand, the interface active, out of the temple and with no box up;
	// otherwise the strengths stay where they were
	if (!Locator::camera::has_value() || !Locator::handSystem::has_value() || !Locator::gameActionSystem::has_value() ||
	    out.width == 0 || out.height == 0 || !interface_active::IsActive() || game_clock::IsInsideCitadel() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()) || boxActive)
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto cues = camera.GetModel().GetHandCues();
	// The player's own camera picks them, or a hand demo's recording while it plays, whoever has the camera
	const auto demoHints = hand_demo::PlayingHints();
	const bool playerCamera = !script_camera::Drives() &&
	                          !(Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera());
	if (!demoHints.has_value() && (!cues.icons.has_value() || !playerCamera))
	{
		return;
	}
	auto& handSystem = Locator::handSystem::value();
	const auto hands = handSystem.GetPlayerHands();
	const auto& registry = Locator::entitiesRegistry::value();
	if (hands.empty() || !registry.Valid(hands[0]))
	{
		return;
	}
	// At the hand's point projected, the point the tooltip's icon is drawn by; not with the hand behind the camera
	const auto& transform = registry.Get<ecs::components::Transform>(hands[0]);
	glm::vec3 onScreen;
	if (!camera.ProjectWorldToScreen(transform.position, glm::vec4(0.0f, 0.0f, out.width, out.height), onScreen))
	{
		return;
	}
	const glm::ivec2 hand {static_cast<int>(std::trunc(onScreen.x)), static_cast<int>(std::trunc(onScreen.y))};
	const auto facts = handSystem.GetCameraIconFacts();
	const auto& actions = Locator::gameActionSystem::value();
	hand_tricons::FadeFrame fade {
	    .seconds = static_cast<float>(game_clock::CameraFrameMs(demoHints.has_value())) * 0.001f,
	    // the cross goes at once, and stays away, while a hand demo plays or while it is held back
	    .demonstration = demoHints.has_value() || _handCrossHeldMs > 0,
	};
	if (demoHints.has_value())
	{
		// The recording's icons and the rotate arrow's turn
		fade.icons = *demoHints;
		_handTriconAngle = hand_demo::PlayingHintAngle();
	}
	else
	{
		auto frame = *cues.icons;
		frame.handInInfluence = facts.inInfluence;
		frame.tickMs = game_clock::TickCount();
		const auto icons = hand_tricons::WorldCamera(frame, _handTriconAngle);
		_handTriconAngle = icons.rotateAngle;
		fade.icons = icons.icons;
		fade.cameraBusy =
		    frame.gripping || frame.rotatingAroundMouse || frame.turn != 0.0f || frame.tilt != 0.0f || frame.zoom != 0.0f;
		// The cross brightens with a thing under the hand, and shows at once with the action button
		if (actions.Get(input::BindableActionMap::ACTION))
		{
			fade.crossNudge = hand_tricons::CrossNudge::Action;
		}
		else if (facts.overObject)
		{
			fade.crossNudge = hand_tricons::CrossNudge::OverThing;
		}
	}
	// The camera icons go where the tooltips are switched off or the hand's state doesn't show them; the cross stays
	fade.icons = hand_tricons::Shown(fade.icons, help::tooltips::Level() != 0, facts.showsCameraIcons);
	// what the scripts ask of the icons shown
	Locator::camera::value().SetShownHandIcons(fade.icons);
	hand_tricons::Fade(_handTriconFades, fade);
	// A quarter of the way to the cursor the hand follows, unless the clear view is coming in
	const glm::ivec2 screen {out.width, out.height};
	const auto centre = hand_tricons::Place(
	    hand,
	    hand_tricons::FollowedCursor(hand_demo::IsPlaying(), input::GameCursorRef(), glm::ivec2(actions.GetMousePosition())),
	    screen, CinematicDirector().IsWideScreenOn(), cues.clearView <= hand_tricons::k_ClearViewLeans);
	const float halfSize = hand_tricons::IconHalfSize(camera.GetProjectionMatrix()[0][0], screen);
	out.handTricons = hand_tricons::Sprites(glm::vec2(centre), halfSize, _handTriconFades, _handTriconAngle);
	// While a hand demo plays, its mouse and the word "Demo" by the icons, on the side it keeps until it is past a third
	// of the screen, with the buttons whose press the hand took lit until what they started ended
	if (hand_demo::IsPlaying())
	{
		_handDemoMouseOnLeft = hand_tricons::LabelOnLeft(centre.x, out.width, _handDemoMouseOnLeft);
		const auto place = hand_tricons::PlaceDemoMouse(centre, _handDemoMouseOnLeft);
		graphics::OverlayFrame::HandDemoMouse mouse {
		    .at = place.at, .align = place.align, .text = helptext::Get(hand_tricons::k_DemoText)};
		std::u16string keyName;
		const int32_t action = hand_tricons::DemoMouseAction(facts.moveLit, facts.actionLit);
		if (action == -1)
		{
			// no button: the mouse row while the move action is on the mouse, else none
			int32_t animType = 0;
			int32_t clickType = 0;
			int32_t row = 3;
			const bool moveOnMouse = help::input_prompt::ResolveAction(1, animType, clickType, row, keyName) && animType == 0;
			mouse.row = moveOnMouse ? help::input_prompt::MouseRow() : 3;
		}
		else if (!help::input_prompt::ResolveAction(action, mouse.animType, mouse.clickType, mouse.row, keyName))
		{
			mouse.animType = 0;
			mouse.clickType = 0;
			mouse.row = help::input_prompt::MouseRow();
		}
		out.handDemoMouse = std::move(mouse);
	}
}

void Game::PlaceVillagerNames(bool listDrew, bool landDrawn)
{
	namespace labels = gui::villager_labels;
	_villagerNames.clear();
	const auto fade = labels::StepNameFadeFrame(_villagerNameFade, Locator::config::value().showVillagerNames,
	                                            game_clock::FrameGameMs(), landDrawn);
	auto* helpSystem = help::Get();
	if (listDrew && Locator::camera::has_value() && Locator::rendererInterface::has_value())
	{
		const auto& camera = Locator::camera::value();
		const auto& renderer = Locator::rendererInterface::value();
		const auto resolution = renderer.GetResolution();
		const auto requests = ecs::villager_label_source::Gather({
		    .drawn = Locator::objectDrawListSystem::value().DrawnThisFrame(),
		    .worldToClipping = graphics::billboard::CameraFrame::From(camera).clipMatrices.worldToClipping,
		    .nearClip = camera.GetPixelNearClip(),
		    .screen = glm::ivec2(resolution),
		    .nameFade = fade.drawn,
		    .wideScreen = helpSystem != nullptr && helpSystem->GetWideScreen() != 0,
		    .inHand = Locator::handSystem::has_value() ? Locator::handSystem::value().GetRenderHandObject() : std::nullopt,
		    .landNumber = ecs::player_colours::LandNumber(),
		});
		labels::Board board;
		const labels::WidthFn width = [&renderer](std::u16string_view text, float lineHeight) {
			return renderer.MeasureText(help::TextFont::J0, text, lineHeight);
		};
		for (const auto& request : requests)
		{
			board.Add(request, resolution.x, width);
		}
		_villagerNames = board.GetLabels();
	}
	_villagerNameFade = fade.next;
	// a script that holds the wide screen drops the names without drawing them
	if (helpSystem != nullptr && helpSystem->IsScriptWideScreen())
	{
		_villagerNames.clear();
	}
}

void Game::LogicFrame(engine::FrameSnapshot& out, uint32_t timeMs)
{
	auto& profiler = Locator::profiler::value();
	const auto& config = Locator::config::value();
	// the PreDraw (the end of the frame's logic, nothing between it and the draw): the overlays read here, the
	// draw reads only the copy
	FillOverlayFrame(CinematicDirector(), _interface && _interface->IsBoxActive(), out.overlay);
	FillGestureIcons(out.overlay);
	FillFloatingNumbers(out.overlay.width, out.overlay.height, out.overlay.floatingNumbers);
	FillVillagerNames(_villagerNames, out.overlay.villagerNames);
	FillHandTricons(_interface && _interface->IsBoxActive(), out.overlay);
	ecs::super_villager::FillFrame(out.superVillagers);
	// what the object draw list leaves out of this frame's draw, for the renderer's row mask
	ecs::draw_gate::FillFrame(
	    _objectListRan,
	    _objectListRan ? Locator::objectDrawListSystem::value().DrawnThisFrame() : std::span<const entt::entity> {},
	    _objectListRan ? Locator::objectDrawListSystem::value().OnScreenThisFrame() : std::span<const entt::entity> {},
	    Locator::entitiesRegistry::value(),
	    [](entt::entity object) {
		    // the town desire flags too, even one off the map: only the list draws a flag
		    return ecs::map_cells::IsObjectInMap(object) ||
		           std::as_const(Locator::entitiesRegistry::value()).AllOf<ecs::components::TownDesireFlag>(object);
	    },
	    out.objectList);
	out.objectList.applied = config.drawFollowsList;
	// the list's tail, as the original draws it after the list's passes: the falling roots, the souls, the chests falling
	// from the sky (the frame one lands too), then the ground marks, each the newest first. Read only
	out.objectList.tail.clear();
	out.objectList.physics.clear();
	out.objectList.sunk.clear();
	if (!config.objectTailUnplaced)
	{
		auto roots = Locator::handSystem::has_value() ? Locator::handSystem::value().FallingRootsNewestFirst()
		                                              : std::vector<entt::entity> {};
		std::vector<std::pair<uint32_t, entt::entity>> falling;
		std::as_const(Locator::entitiesRegistry::value())
		    .Each<const ecs::components::Reward>([&falling](entt::entity chest, const ecs::components::Reward& reward) {
			    if (reward.state == ecs::components::Reward::State::Falling || reward.goesIntoMap)
			    {
				    falling.emplace_back(reward.serial, chest);
			    }
		    });
		std::ranges::sort(falling, std::greater {});
		std::vector<entt::entity> chests;
		chests.reserve(falling.size());
		for (const auto& [serial, chest] : falling)
		{
			chests.push_back(chest);
		}
		// the physics objects the physics list draws, in its order: the awake ones not wholly under the water (centre y
		// above minus the radius); the others stay where they were drawn
		if (Locator::dynamicsSystem::has_value())
		{
			const bool drawSunk = config.drawSunkPhysicsObjects;
			Locator::dynamicsSystem::value().ForEachEntry([&out, drawSunk](const ecs::PhysicsEntry& entry) {
				if (entry.entity == entt::null || entry.body.resting)
				{
					return;
				}
				// a flying one wholly under the water is out of the map and the physics list skips it: nothing draws it
				if (entry.body.Centre().y > -entry.body.Radius())
				{
					out.objectList.physics.push_back(entry.entity);
				}
				else if (!drawSunk)
				{
					out.objectList.sunk.push_back(entry.entity);
				}
			});
		}
		out.objectList.tail = graphics::object_order::TailOrder(roots, {}, ecs::villager_soul::NewestFirst(), chests,
		                                                        ecs::ground_marks::NewestFirst());
	}
	_objectListFrame = &out.objectList;
	// the knocked town's read-out over the buildings whose Draw the object list called this frame
	FillKnockReadout(
	    _objectListRan ? Locator::objectDrawListSystem::value().DrawnThisFrame() : std::span<const entt::entity> {},
	    graphics::billboard::CameraFrame::From(Locator::camera::value()), out.overlay.knockReadout, out.overlay.knockRows);
	TheIntroSystem().FillFrame(out.overlay.introLight);

	profiler.Begin(Profiler::Stage::SceneDraw); // ends in DrawFrame, after DrawScene (as the scope did)
	// the draw reads a copy of the camera and the frame's clocks, taken here: nothing between this and the
	// draw moves them (PreDraw, which may read the live ones, gets the same values)
	Locator::camera::value().CopyViewTo(*_drawCamera);
	out.camera = _drawCamera.get();
	out.hand.position.reset();
	if (Locator::handSystem::has_value())
	{
		out.hand.position = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	out.time = timeMs; // TODO(#481): get actual time
	out.timeOfDay = Locator::skySystem::value().GetTime();
	out.clock = {.frameGameMs = game_clock::FrameGameMs(),
	             .turn = game_clock::Turn(),
	             .turnFraction = game_clock::TurnFraction(),
	             .paused = game_clock::IsPaused()};
	out.bumpMapStrength = config.bumpMapStrength;
	out.smallBumpMapStrength = config.smallBumpMapStrength;
	out.drawSky = config.drawSky;
	out.drawWater = config.drawWater;
	out.drawIsland = config.drawIsland;
	out.drawEntities = config.drawEntities;
	out.drawSprites = config.drawSprites;
	out.drawBoundingBoxes = config.drawBoundingBoxes;
	out.wireframe = config.wireframe;
	const auto& screenshots = Locator::screenshotRequest::value();
	out.frameCount = screenshots.Frame();
	out.screenshot = screenshots.Due();
	out.screenPictures.clear();
	// The challenges' pictures asked for this frame, of the frame now drawn
	for (auto& name : ecs::challenge_pictures::TakeRequests(Locator::entitiesRegistry::value()))
	{
		out.screenPictures.push_back({.name = std::move(name), .picturer = &ecs::challenge_pictures::Take});
	}
	// While a running script can take a challenge's picture, every frame is kept aside, as a picture is of the frame
	// already on the screen when the script runs
	const auto snapshotTask =
	    Locator::vm::has_value() ? ecs::challenge_pictures::TaskThatSnapshots(_snapshotScripts, Locator::vm::value().GetTasks())
	                             : std::nullopt;
	out.keepShownFrame = snapshotTask.has_value();
	// (debug) a line each time the keeping starts or stops, with the task that starts it
	if (out.keepShownFrame != std::exchange(_keepingShownFrames, out.keepShownFrame))
	{
		if (snapshotTask.has_value())
		{
			const auto& task = Locator::vm::value().GetTasks().at(*snapshotTask);
			spdlog::get("game")->debug("Challenge pictures: the frames are kept from frame {}, task {} running {} can take one",
			                           out.frameCount, *snapshotTask, task.name);
		}
		else
		{
			spdlog::get("game")->debug("Challenge pictures: the frames are no longer kept from frame {}, no running script can "
			                           "take one",
			                           out.frameCount);
		}
	}
	{
		auto preDraw = profiler.BeginScoped(Profiler::Stage::PreDraw);
		Locator::rendererInterface::value().PreDraw(out.Desc(Locator::entitiesRegistry::value()));
	}
}

void Game::DrawFrame(const engine::FrameSnapshot& in)
{
	auto& profiler = Locator::profiler::value();
	{
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw); // what the draw thread will run
		auto desc = in.Desc(Locator::entitiesRegistry::value());
		// the interface's panels go in the end of the frame, between the help text and the fade
		if (_interface && Locator::windowing::has_value())
		{
			const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
			desc.interfaceLayers = {
			    .fightPanel = [this, size]() { _interface->DrawFightPanelLayer(size); },
			    .scriptTexts = [this, size]() { _interface->DrawScriptTextLayer(size); },
			    .creaturePanel = [this, size]() { _interface->DrawCreaturePanelLayer(size); },
			};
		}
		Locator::rendererInterface::value().DrawScene(desc);
	}
	profiler.End(Profiler::Stage::SceneDraw); // begun in LogicFrame

	// the in-game menus over the scene, timed by the game's tick count as the original's pointer is. (pending) the
	// real mouse read here: into the snapshot
	if (_interface && Locator::windowing::has_value())
	{
		glm::ivec2 mouse;
		SDL_GetMouseState(&mouse.x, &mouse.y);
		if (input::FixedMouse().has_value())
		{
			mouse = *input::FixedMouse();
		}
		else if (input::IgnoreRealInput() || input::replay::IsPlaying())
		{
			// the game's cursor (OPENBLACK_MOUSE_AT, a hand demo or an input recording), not the real one
			mouse = input::GameCursorRef();
		}
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		// Not under the ignored real input: the real mouse over a debug window would put a pointer in the shots
		_interface->Draw(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), mouse, game_clock::TickCount(),
		                 !input::IgnoreRealInput() && Locator::debugGui::value().IsMouseOverWindow());
	}

	{
		auto section = profiler.BeginScoped(Profiler::Stage::GuiDraw);
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		// The pictures kept in memory are of the screen as the player sees it: no debug windows in them
		for (const auto& request : in.screenPictures)
		{
			Locator::rendererInterface::value().RequestScreenPicture(request.name, request.picturer);
		}
		const bool picturesAsked = !in.screenPictures.empty();
		// Skip drawing Debug UI for screenshots
		if (in.screenshot.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Requesting a screenshot at frame {}...", in.frameCount);
			Locator::rendererInterface::value().RequestScreenshot(*in.screenshot);
			// test hook OPENBLACK_SCREENSHOT_GUI=1: the debug UI (menus, windows) in the screenshot too
			if (std::getenv("OPENBLACK_SCREENSHOT_GUI") != nullptr && !picturesAsked)
			{
				Locator::debugGui::value().Draw();
			}
		}
		else if (!picturesAsked)
		{
			// (pending) ImGui is still drawn from the logic's state: the single-thread mode only, until its draw data
			// is copied
			Locator::debugGui::value().Draw();
		}
		// The frame kept aside for a later turn's picture: the window shows it pixel for pixel, the debug windows over it
		if (in.keepShownFrame)
		{
			Locator::rendererInterface::value().KeepShownFrame();
		}
	}

	if (std::getenv("OPENBLACK_DRAW_STATS") != nullptr && (in.frameCount % 30 == 0 || in.frameCount < 8))
	{
		const auto* stats = bgfx::getStats();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "DRAWSTATS frame {} draws {}", in.frameCount, stats->numDraw);
	}
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
	     .failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); }});

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

bool Game::ChangeLand(const std::filesystem::path& scriptPath) noexcept
{
	const auto path = Locator::filesystem::value().GetGamePath() / scriptPath;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "LOAD_MAP: changing land to {}", path.generic_string());
	// a playground's start restarts the scripts before the map's clear, which takes the local player's creature out of a
	// lesson script before it is saved
	if (land_load::ResetsFor(!_firstMapLoaded, path.parent_path().filename() == "Playgrounds").scripts)
	{
		ecs::player_creature::EndMyCreaturesDevScript();
	}
	// the map's clear: first the local player's creature is saved to the profile's mind file, which the next land's
	// LOAD_MY_CREATURE reads back (the creature goes with the land)
	ecs::player_creature::SaveMyCreature();
	// then the things no script variable holds any more are released, as after each turn's scripts, while
	// the old land is still there (before the seeds go back). The things still held go with the land; the variables
	// themselves are kept
	TheScriptObjects().ReleaseUnreferenced();
	return LoadMap(path, loading::LoadingClock::Mode::PleaseWait);
}

bool Game::LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look) noexcept
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
	const auto resets = land_load::ResetsFor(firstLand, inPlaygrounds);
	PrepareNewLand(playgroundStart, resets);

	// the map script from the byte cache, read afresh on every map load so that a script edited while the game runs is
	// the one loaded (the debug menu's map list)
	auto& blobs = Locator::resources::value().GetBlobs();
	blobs.Erase(resources::BlobId(path));
	const auto& data = resources::LoadBlob(blobs, path);
	const auto source = std::string(reinterpret_cast<const char*>(data.data()), data.size());

	ResetRegistryForNewLand();
	timer.Step("reset");
	RenderLoadingFrame();

	// The folder the vortices' crossing file is kept in, under the game's folder, before the map script can open one. A
	// run of the checks reads a game folder that is never written to, so there the way out of a land writes nothing
	ecs::vortex_save::SetSaveFolder(ecs::vortex_save::SaveFolderUnder(fileSystem.GetGamePath()));
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
	StartNewLand(path.stem().string(), false, resets.newGame);
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
	BeginLoadingScreen(loading::LoadingClock::Mode::Tips);
	// As the original's load: the scripts restart, the map is cleared and the saved land's file opens
	PrepareNewLand(true, {.scripts = true, .newGame = false, .mapClear = true});
	ResetRegistryForNewLand();
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
	// the world's objects, made in the order the original puts their links back once all are read; the classes made so
	// far are listed on the saved-games page
	ecs::saved_world_write::LoadedObjects loadedObjects;
	_savedGameCensus =
	    ecs::saved_world_load::Apply(ecs::saved_world_load::PlanOf(*read), ecs::saved_world_load::GameMakers(), &loadedObjects);
	// the players' values the game models, each player listed for the next save's players' writer
	ecs::saved_players::Load(*read, &loadedObjects);
	// with every object and player back, each totem wears the icon of its town's player's creature, as at its creation
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().WorldLoaded();
	}
	// and the loaded objects no entity stands for that the writers write from what openblack keeps
	ecs::saved_world_write::KeepLoadedSources(*read, loadedObjects);
	// the world read and what was made of it, kept for the next save, which writes from them what openblack does not
	// model yet
	saves.KeepLoadedWorld(read, std::move(loadedObjects));
	// the rivers' landscape footprints, once their points are back
	ecs::CreateRiverFootprints();
	timer.Step("objects");
	// the help record, from the profile's file, read just before the scripts' machine
	help::profile::TakeFromProfile();
	// the scripts' machine, from its file beside the world file
	if (Locator::vm::has_value())
	{
		const auto script = world->parent_path() / gamesave::VanillaScriptFileName(slot);
		const auto path = script.is_absolute() ? script : fileSystem.GetGamePath() / script;
		if (Locator::vm::value().RestoreState(path) != EXIT_SUCCESS)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The scripts' state {} could not be read", path.generic_string());
		}
		FindSnapshotScripts();
	}
	timer.Step("scripts");
	StartNewLand(std::filesystem::path(filesystem::FileSystemInterface::FixPath(state->landscape)).stem().string(), false,
	             false);
	// the game object's values, and the clock started again at the saved turn's time
	ecs::saved_game_load::Apply(*state);
	saves.EndLoad(*world);
	EndLoadingScreen();
	timer.Step("start");
	return true;
}

void Game::PrepareNewLand(bool playgroundStart, land_load::Resets resets)
{
	// A new land keeps nothing of the last load's world: a load keeps its own again once its objects are made
	if (Locator::saveGame::has_value())
	{
		Locator::saveGame::value().ForgetLoadedWorld();
	}
	// A land loaded while the player is inside the temple takes them out of it first, rooms and all
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		Locator::temple::value().Deactivate();
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
	// and the camera features back to all but the self-tilting camera, with the self-tilt's start angle and height
	camera_help::Reset();
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
	// the map's clear: the script highlights (after the bookmarks)
	ecs::script_highlight::OnClearMap();
	// the map's clear: the camera help icons by the hand faded out, the rotate arrow unturned and the red cross no longer
	// held back
	if (resets.mapClear)
	{
		hand_tricons::Restart(_handTriconFades, _handTriconAngle);
		_handCrossHeldMs = 0;
		if (Locator::camera::has_value())
		{
			Locator::camera::value().SetShownHandIcons(0);
		}
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
	else
	{
		game_random::Reset();
	}
	_firstMapLoaded = true;
	// a new game starts in no sequence mode; a land change keeps the one it is in (the temple's exit above has left the
	// citadel's)
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
		ResetScripts();
	}
	ecs::designed_scenery::OnLoadMap();
}

void Game::ResetScripts()
{
	// the audio switches: the creatures' sounds, the dialogue-only sound, the alignment music, the music line and beats
	audio::GetScriptAudioState().Reset();
	// the camera control's switches: the leashes and the highlights drawn. The script control's owners of the camera
	// and the game speed are kept, as the dialogue control's is
	help::script_control::GetCameraControl().Reset();
	// SET_INTERFACE_CITADEL's value back to 1 (the citadel entrance may be tapped)
	worship::citadel::ResetInterfaceCitadel();
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

void Game::ResetRegistryForNewLand()
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
	Locator::entitiesRegistry::value().Reset();
	Locator::entitiesRegistry::value().Context().explorationRegions = std::move(explorationRegions);
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
}

void Game::StartNewLand(std::string_view land, bool startsAfresh, bool newGame)
{
	// With the land and its towns ready, the game's first land gives the creatures' map its regions, which every later
	// land keeps (a land that starts afresh makes them again)
	ecs::creature_exploration_land::MakeMap(land, startsAfresh);
	_lastGameLoopTime = std::chrono::steady_clock::now();
	_turnDeltaTime = 0ns;
	// the game speed is kept: a land change leaves it as it was, and the clock's start below keeps its speed
	// a new game from turn 0 (inferred: openblack has no saved games, so this stands for a loaded game too), then the
	// start of the game loop: the timer from 0 and the local game timer reset
	game_clock::SetTurn(0);
	game_clock::OnLoad();
	// a new game starts the guidance with its interface status (its turn is the new land's); a land change keeps it,
	// and what it remembers of the last land's things matches nothing. The spooky voices take the player's name
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
	// no map clear before it (its scenes start as they did before the clear's status pass was ported)
	PrepareNewLand(true, {.scripts = true, .newGame = true, .mapClear = false});
	ResetRegistryForNewLand();
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
	StartNewLand("testbed", true, true);
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
	// The keys for the temple's rooms take the player into the temple at that room's path in, and inside the temple
	// they cut to the room: the first key pressed this frame, in their order
	const auto& actions = Locator::gameActionSystem::value();
	auto& temple = Locator::temple::value();
	const auto room = input::PressedTempleRoom(
	    [&actions](input::BindableActionMap action) { return actions.Get(action) && actions.GetChanged(action); });
	if (!room)
	{
		return;
	}
	if (temple.Active())
	{
		temple.GoToRoom(*room);
	}
	else
	{
		temple.Activate(*room);
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
		// (pending) Auto Save (a bit of the game's flags) and Push Scroll (a profile setting): openblack has neither yet
	}
	// The Push Scroll box is kept with the profile as it is clicked, and the profile written back, its help record too
	if (_interface->GetMenu().TakePushScrollChanged())
	{
		if (auto profile = profiles.GetCurrentProfile(); profile.has_value())
		{
			profile->settings.pushScrolling = _interface->GetMenu().GetSettings().pushScrolling;
			profiles.SetCurrentSettings(profile->settings);
			help::profile::KeepInProfile();
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
	// A page shown takes the profile played again, and the hand the side its left-handed box keeps: the box's tick alone
	// changes nothing, the leave just before keeps it with the profile (without one, the menu's own choice)
	if (_interface->GetMenu().TakeProfileShown())
	{
		const auto profile = profiles.GetCurrentProfile();
		const bool leftHanded =
		    profile.has_value() ? profile->settings.leftHanded : _interface->GetMenu().GetSettings().leftHandedHand;
		Locator::config::value().rightHandedHand = !leftHanded;
		// The main page and the options' box read the profile played again as they show, its help record too
		using Page = gui::GameMenu::Page;
		if (const auto page = _interface->GetMenu().GetPage();
		    page == Page::Main || page == Page::Options || page == Page::Advanced)
		{
			help::profile::TakeFromProfile();
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
		// Every action takes its default key and button again. The Controls page lists the defaults, so it shows the same
		Locator::gameActionSystem::value().ResetKeyBindings();
		break;
	case Action::StartNewGame:
	case Action::Restart:
		// Starting again as the profile picked would carry on from its last automatic save; with none, which openblack
		// can't make yet, a new game starts, as Start a New Game does. The game ends after this frame and Run starts the
		// next one
		_interface->GetMenu().Close();
		SDL_StopTextInput();
		_restartRequested = true;
		break;
	default:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "This part of the menu is not available yet");
		break;
	}
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

void Game::PlayerProfileChanged()
{
	RefreshPlayersPage();
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value() || !_interface)
	{
		return;
	}
	// The page shows the profile's creature's name, symbol and options. The options take effect only as the options'
	// box is next left, which keeps the menu's choices with the profile and applies them, as in the original: picking a
	// profile gives the help system none of them, the read speed neither
	_interface->GetMenu().ShowPlayer(gui::ToUtf16(profile->creatureName), profile->symbol, profile->settings);
	NameCreatureAfterProfile();
	help::profile::TakeFromProfile();
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
	// The help system, the dialogue and the creature help, each passed over while not made
	help::ApplyPlayerChoices(settings, help::Get(), TheDialogue(),
	                         Locator::creatureHelpSystem::has_value() ? &Locator::creatureHelpSystem::value() : nullptr);
	// The hand stays as the menu's box sets it, until the mirror is checked
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
