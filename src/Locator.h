/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <filesystem>
#include <string>

#include <entt/locator/locator.hpp>

namespace openblack
{
enum class GraphicsBackend : uint8_t;
struct EngineConfig;
class Camera;
class EventManager;
class TimeSystemInterface;
class GameRandomInterface;
class LandIslandInterface;
struct LandData;
class OceanInterface;
struct ModLoaderStatus;
class Profiler;
class RandomNumberManagerInterface;
class SkyInterface;
class TempleInteriorInterface;

namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;

namespace chlapi
{
class CHLApi;
}

namespace debug::gui
{
class DebugGuiInterface;
}

namespace filesystem
{
class FileSystemInterface;
}

namespace graphics
{
class RendererInterface;
}

namespace input
{
class GameActionInterface;
}

namespace lhvm
{
class LHVM;
}

namespace resources
{
class ResourcesInterface;
}

namespace windowing
{
enum class DisplayMode : std::uint8_t;
class WindowingInterface;
} // namespace windowing

namespace ecs
{
class Registry;
class MapInterface;
} // namespace ecs

namespace ecs::systems
{
class AlignmentSystemInterface;
class AnimalSystemInterface;
class CameraBookmarkSystemInterface;
class CameraPathSystemInterface;
class DayNightClockSystemInterface;
class CinematicDirectorSystemInterface;
class ScreenshotRequestSystemInterface;
class ParticleSystemInterface;
class MistSystemInterface;
class CloudSystemInterface;
class VillageLightSystemInterface;
class RenderFrameSystemInterface;
class SkyFrameSystemInterface;
class LandAvoidSystemInterface;
class AudioStateInterface;
class SoundTagSystemInterface;
class WorldEffectsInterface;
class WaterRingSystemInterface;
class ChimneySmokeSystemInterface;
class RainSystemInterface;
class VegetationInterface;
class FieldSystemInterface;
class ExplosionSystemInterface;
class ScriptStateInterface;
class CameraHelpSystemInterface;
class DebugHooksInterface;
class HandTapRegistryInterface;
class VideoSystemInterface;
class PhysicsObjectsSystemInterface;
class WeatherSystemInterface;
class SnowSystemInterface;
class SnowfallSystemInterface;
class CreatureModeSystemInterface;
class CreatureCaveSystemInterface;
class DrawListSystemInterface;
class DynamicsSystemInterface;
class EditorSystemInterface;
class FireSystemInterface;
class ForestSystemInterface;
class GameStatsSystemInterface;
class GestureEventsInterface;
class GestureSystemInterface;
class GlintTargetsInterface;
class HandSystemInterface;
class InfluenceSystemInterface;
class InputStateInterface;
class LandBalanceSystemInterface;
class LivingActionSystemInterface;
class MagicShieldSystemInterface;
class MagicSystemInterface;
class MapCellsSystemInterface;
class MapScriptSystemInterface;
class MapShapeProviderInterface;
class MeshBoxProviderInterface;
class MiracleFxSystemInterface;
class ObjectCreationIndexSystemInterface;
class PathfindingSystemInterface;
class PlayerSystemInterface;
class ReactionSystemInterface;
class RenderingSystemInterface;
class RoutePlanStateSystemInterface;
class TeleportSystemInterface;
class TempleExteriorSystemInterface;
class ToBeDeletedSystemInterface;
class TornadoSystemInterface;
class TownDesireSystemInterface;
class TownStateSystemInterface;
class TownCellObjectsInterface;
class TownSystemInterface;
class TreeSystemInterface;
class VillagerBuildingSitesInterface;
class VillagerChildFactoryInterface;
class VillagerDiscipleJobsInterface;
class VillagerFieldsInterface;
class VillagerFishFarmsInterface;
class VillagerRulesInterface;
class VillagerStateSystemInterface;
class VillagerStoresInterface;
class VillagerTentQueriesInterface;
class VillagerWorldQueriesInterface;
class VillagerWorshipCheckInterface;
class WorshipStateInterface;
class CreatureAnimationSystemInterface;
class CreatureMindSystemInterface;
class CreatureLocomotionSystemInterface;
class CreatureHairSystemInterface;
class CreatureAudioSystemInterface;
class CreatureObjectActionSystemInterface;
class CreatureHandSystemInterface;
class FootprintSystemInterface;
class CreatureSkinSystemInterface;
class CreaturePhysiologySystemInterface;
class LeashSystemInterface;
class CreatureFightSystemInterface;
} // namespace ecs::systems

void InitializeWindow(const std::string& title, int width, int height, windowing::DisplayMode displayMode, uint32_t extraFlags);
/// The game clock, first of all: the engine timer, the audio device and the game read it
void InitializeClock();
/// The state the game keeps for its whole life, made fresh when the Game is made (the script fade, the day / night
/// clock, the map script's globals and the frame count with its screenshot request)
void InitializeGameState();
/// fontHashPermutation: the font cache's glyph hash permutation, drawn right after the atmosphere's start-up, for
/// InitializeGame to keep with the resources
bool InitializeEngine(GraphicsBackend backend, bool vsync, std::array<uint8_t, 256>& fontHashPermutation) noexcept;
bool InitializeGame(const std::array<uint8_t, 256>& fontHashPermutation) noexcept;
void InitializeLevel(const std::filesystem::path& path);
/// The same services for a land made in memory, as the testbed's flat land is
void InitializeLevel(const LandData& land);
void ShutDownServices();

namespace audio
{
class AudioManagerInterface;
}

struct Locator
{
	using config = entt::locator<EngineConfig>;
	using infoConstants = entt::locator<const InfoConstants>;
	using profiler = entt::locator<Profiler>;
	using events = entt::locator<EventManager>;
	using windowing = entt::locator<windowing::WindowingInterface>;
	using debugGui = entt::locator<debug::gui::DebugGuiInterface>;
	using filesystem = entt::locator<filesystem::FileSystemInterface>;
	using resources = entt::locator<resources::ResourcesInterface>;
	using rng = entt::locator<RandomNumberManagerInterface>;
	using gameRandom = entt::locator<GameRandomInterface>;
	using time = entt::locator<TimeSystemInterface>;
	using particleSystem = entt::locator<ecs::systems::ParticleSystemInterface>;
	using mistSystem = entt::locator<ecs::systems::MistSystemInterface>;
	using cloudSystem = entt::locator<ecs::systems::CloudSystemInterface>;
	using villageLightSystem = entt::locator<ecs::systems::VillageLightSystemInterface>;
	using renderFrameSystem = entt::locator<ecs::systems::RenderFrameSystemInterface>;
	using skyFrameSystem = entt::locator<ecs::systems::SkyFrameSystemInterface>;
	using landAvoidSystem = entt::locator<ecs::systems::LandAvoidSystemInterface>;
	using debugHooks = entt::locator<ecs::systems::DebugHooksInterface>;
	using scriptState = entt::locator<ecs::systems::ScriptStateInterface>;
	// what the camera lets the player do, as the scripts allow it (raffclar's slot, over our camera help)
	using cameraHelpSystem = entt::locator<ecs::systems::CameraHelpSystemInterface>;
	using handTapRegistry = entt::locator<ecs::systems::HandTapRegistryInterface>;
	using videoSystem = entt::locator<ecs::systems::VideoSystemInterface>;
	using worldEffects = entt::locator<ecs::systems::WorldEffectsInterface>;
	using waterRingSystem = entt::locator<ecs::systems::WaterRingSystemInterface>;
	using chimneySmokeSystem = entt::locator<ecs::systems::ChimneySmokeSystemInterface>;
	using vegetation = entt::locator<ecs::systems::VegetationInterface>;
	using fieldSystem = entt::locator<ecs::systems::FieldSystemInterface>;
	// what explosions leave: the rubble on the land and the camera shaking (raffclar's slot, over our ground marks
	// and camera shakes)
	using explosionSystem = entt::locator<ecs::systems::ExplosionSystemInterface>;
	// the audio service: the functions of Audio.h, over our engine (raffclar's name)
	using audio = entt::locator<::openblack::audio::AudioManagerInterface>;
	using audioState = entt::locator<ecs::systems::AudioStateInterface>;
	using soundTagSystem = entt::locator<ecs::systems::SoundTagSystemInterface>;
	using physicsObjectsSystem = entt::locator<ecs::systems::PhysicsObjectsSystemInterface>;
	using weatherSystem = entt::locator<ecs::systems::WeatherSystemInterface>;
	using snowSystem = entt::locator<ecs::systems::SnowSystemInterface>;
	using snowfallSystem = entt::locator<ecs::systems::SnowfallSystemInterface>;
	using rainSystem = entt::locator<ecs::systems::RainSystemInterface>;
	using terrainSystem = entt::locator<LandIslandInterface>;
	using oceanSystem = entt::locator<OceanInterface>;
	using skySystem = entt::locator<SkyInterface>;
	using camera = entt::locator<Camera>;
	using gameActionSystem = entt::locator<input::GameActionInterface>;
	using rendereringSystem = entt::locator<ecs::systems::RenderingSystemInterface>;
	using rendererInterface = entt::locator<graphics::RendererInterface>;
	using dynamicsSystem = entt::locator<ecs::systems::DynamicsSystemInterface>;
	using editorSystem = entt::locator<ecs::systems::EditorSystemInterface>;
	using creatureModeSystem = entt::locator<ecs::systems::CreatureModeSystemInterface>;
	using creatureCaveSystem = entt::locator<ecs::systems::CreatureCaveSystemInterface>;
	using cameraBookmarkSystem = entt::locator<ecs::systems::CameraBookmarkSystemInterface>;
	using cameraPathSystem = entt::locator<ecs::systems::CameraPathSystemInterface>;
	using livingActionSystem = entt::locator<ecs::systems::LivingActionSystemInterface>;
	using townSystem = entt::locator<ecs::systems::TownSystemInterface>;
	using pathfindingSystem = entt::locator<ecs::systems::PathfindingSystemInterface>;
	using entitiesRegistry = entt::locator<ecs::Registry>;
	using entitiesMap = entt::locator<ecs::MapInterface>;
	using playerSystem = entt::locator<ecs::systems::PlayerSystemInterface>;
	using alignmentSystem = entt::locator<ecs::systems::AlignmentSystemInterface>;
	using influenceSystem = entt::locator<ecs::systems::InfluenceSystemInterface>;
	using handSystem = entt::locator<ecs::systems::HandSystemInterface>;
	using temple = entt::locator<TempleInteriorInterface>;
	using vm = entt::locator<lhvm::LHVM>;
	using chlapi = entt::locator<chlapi::CHLApi>;
	using villagerFields = entt::locator<ecs::systems::VillagerFieldsInterface>;
	using villagerFishFarms = entt::locator<ecs::systems::VillagerFishFarmsInterface>;
	using villagerBuildingSites = entt::locator<ecs::systems::VillagerBuildingSitesInterface>;
	using villagerStores = entt::locator<ecs::systems::VillagerStoresInterface>;
	using villagerTentQueries = entt::locator<ecs::systems::VillagerTentQueriesInterface>;
	using villagerRules = entt::locator<ecs::systems::VillagerRulesInterface>;
	using villagerWorldQueries = entt::locator<ecs::systems::VillagerWorldQueriesInterface>;
	using villagerWorshipCheck = entt::locator<ecs::systems::VillagerWorshipCheckInterface>;
	using villagerChildFactory = entt::locator<ecs::systems::VillagerChildFactoryInterface>;
	using villagerDiscipleJobs = entt::locator<ecs::systems::VillagerDiscipleJobsInterface>;
	using townCellObjects = entt::locator<ecs::systems::TownCellObjectsInterface>;
	using mapShapeProvider = entt::locator<ecs::systems::MapShapeProviderInterface>;
	using meshBoxProvider = entt::locator<ecs::systems::MeshBoxProviderInterface>;
	using glintTargets = entt::locator<ecs::systems::GlintTargetsInterface>;
	using toBeDeletedSystem = entt::locator<ecs::systems::ToBeDeletedSystemInterface>;
	using objectCreationIndexSystem = entt::locator<ecs::systems::ObjectCreationIndexSystemInterface>;
	using mapCellsSystem = entt::locator<ecs::systems::MapCellsSystemInterface>;
	using drawListSystem = entt::locator<ecs::systems::DrawListSystemInterface>;
	using reactionSystem = entt::locator<ecs::systems::ReactionSystemInterface>;
	using fireSystem = entt::locator<ecs::systems::FireSystemInterface>;
	using forestSystem = entt::locator<ecs::systems::ForestSystemInterface>;
	using treeSystem = entt::locator<ecs::systems::TreeSystemInterface>;
	using landBalanceSystem = entt::locator<ecs::systems::LandBalanceSystemInterface>;
	using animalSystem = entt::locator<ecs::systems::AnimalSystemInterface>;
	/// The shield miracles' objects as the miracles' turn and frame and the reactions reach them; no state of its own
	using magicShieldSystem = entt::locator<ecs::systems::MagicShieldSystemInterface>;
	/// The miracles as the game loop and the tools reach them. It owns their state, which has no slot of its own: the
	/// spells, the magic objects, the hand magic state and the falling spell (SpellStore, MagicObjects, HandMagic and
	/// FallingSpellStore)
	using magicSystem = entt::locator<ecs::systems::MagicSystemInterface>;
	using townStateSystem = entt::locator<ecs::systems::TownStateSystemInterface>;
	using townDesireSystem = entt::locator<ecs::systems::TownDesireSystemInterface>;
	using villagerStateSystem = entt::locator<ecs::systems::VillagerStateSystemInterface>;
	/// The teleport stones and what the tornadoes carry, as the miracles, the villagers and the hand reach them
	using teleportSystem = entt::locator<ecs::systems::TeleportSystemInterface>;
	using tornadoSystem = entt::locator<ecs::systems::TornadoSystemInterface>;
	/// The gestures the hand draws (the gesture system), and where the miracles hear of them (the same system)
	using gestureSystem = entt::locator<ecs::systems::GestureSystemInterface>;
	using gestureEvents = entt::locator<ecs::systems::GestureEventsInterface>;
	using miracleFxSystem = entt::locator<ecs::systems::MiracleFxSystemInterface>;
	using routePlanStateSystem = entt::locator<ecs::systems::RoutePlanStateSystemInterface>;
	using templeExteriorSystem = entt::locator<ecs::systems::TempleExteriorSystemInterface>;
	/// The creatures' systems (emplaced, none called yet; the locomotion one is made again with each land)
	using creatureAnimationSystem = entt::locator<ecs::systems::CreatureAnimationSystemInterface>;
	using creatureMindSystem = entt::locator<ecs::systems::CreatureMindSystemInterface>;
	using creatureLocomotionSystem = entt::locator<ecs::systems::CreatureLocomotionSystemInterface>;
	using creatureHairSystem = entt::locator<ecs::systems::CreatureHairSystemInterface>;
	using creatureAudioSystem = entt::locator<ecs::systems::CreatureAudioSystemInterface>;
	using creatureObjectActionSystem = entt::locator<ecs::systems::CreatureObjectActionSystemInterface>;
	using creatureHandSystem = entt::locator<ecs::systems::CreatureHandSystemInterface>;
	using footprintSystem = entt::locator<ecs::systems::FootprintSystemInterface>;
	using creatureSkinSystem = entt::locator<ecs::systems::CreatureSkinSystemInterface>;
	using creaturePhysiologySystem = entt::locator<ecs::systems::CreaturePhysiologySystemInterface>;
	using leashSystem = entt::locator<ecs::systems::LeashSystemInterface>;
	using creatureFightSystem = entt::locator<ecs::systems::CreatureFightSystemInterface>;
	using worshipState = entt::locator<ecs::systems::WorshipStateInterface>;
	using inputState = entt::locator<ecs::systems::InputStateInterface>;
	using gameStatsSystem = entt::locator<ecs::systems::GameStatsSystemInterface>;
	using mapScriptSystem = entt::locator<ecs::systems::MapScriptSystemInterface>;
	// the script fade and the cinema bars (raffclar's slot, over our fade and bars)
	using cinematicDirectorSystem = entt::locator<ecs::systems::CinematicDirectorSystemInterface>;
	using dayNightClock = entt::locator<ecs::systems::DayNightClockSystemInterface>;
	using screenshotRequest = entt::locator<ecs::systems::ScreenshotRequestSystemInterface>;
	/// only when the mod loader library was loaded at start-up
	using modLoader = entt::locator<ModLoaderStatus>;
};
} // namespace openblack
