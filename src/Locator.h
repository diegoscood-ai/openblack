/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/locator/locator.hpp>

namespace openblack
{
struct EngineConfig;
class Camera;
class EventManager;
class GameRandomInterface;
class LandIslandInterface;
class OceanInterface;
struct ModLoaderStatus;
class Profiler;
class RandomNumberManagerInterface;
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
class WindowingInterface;
} // namespace windowing

namespace ecs
{
class Registry;
class MapInterface;
} // namespace ecs

namespace ecs::systems
{
class AdvisorSystemInterface;
class AlignmentSystemInterface;
class AnimalSystemInterface;
class CameraBookmarkSystemInterface;
class CameraPathSystemInterface;
class DayNightClockSystemInterface;
class CinematicDirectorSystemInterface;
class DialogueControlSystemInterface;
class RewardSystemInterface;
class ScriptControlSystemInterface;
class ScreenshotRequestSystemInterface;
class ParticleSystemInterface;
class MistSystemInterface;
class CloudSystemInterface;
class VillageLightSystemInterface;
class VillageTotemSystemInterface;
class ArtefactSystemInterface;
class HeartbeatSystemInterface;
class RenderFrameSystemInterface;
class SkyFrameSystemInterface;
class SkySystemInterface;
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
class ScriptObjectsSystemInterface;
class CameraHelpSystemInterface;
class DebugHooksInterface;
class HandTapRegistryInterface;
class VideoSystemInterface;
class DynamicsSystemInterface;
class BuildingDamageSystemInterface;
class WeatherSystemInterface;
class SnowSystemInterface;
class SnowfallSystemInterface;
class CreatureModeSystemInterface;
class CreatureCaveSystemInterface;
class DrawListSystemInterface;
class PickingSystemInterface;
class AnimatedStaticSystemInterface;
class EditorSystemInterface;
class FireSystemInterface;
class ForestSystemInterface;
class GameStatsSystemInterface;
class GestureEventsInterface;
class GestureSystemInterface;
class GlintTargetsInterface;
class CreatureSpellBodiesInterface;
class HandGrabSystemInterface;
class HandSystemInterface;
class HelpProfileSystemInterface;
class HelpSpeechSystemInterface;
class HelpTextSystemInterface;
class HighDetailSystemInterface;
class WalkPathSystemInterface;
class SharkSystemInterface;
class DanceSystemInterface;
class AbodeKnockSystemInterface;
class CameraZoneSystemInterface;
class CreatureCarryOverSystemInterface;
class CreatureFizzSystemInterface;
class CreaturePenSystemInterface;
class FireflySystemInterface;
class FishFarmSystemInterface;
class HandDemoSystemInterface;
class InspectorSystemInterface;
class ResourceStoreSystemInterface;
class ScriptHighlightSystemInterface;
class TattooEditorSystemInterface;
class TempleDestructionSystemInterface;
class WorshipSiteSystemInterface;
class InfluenceSystemInterface;
class InputStateInterface;
class IntroSystemInterface;
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
class PlayerProfileSystemInterface;
class PlayerSystemInterface;
class ReactionSystemInterface;
class RenderingSystemInterface;
class RoutePlanStateSystemInterface;
class TeleportSystemInterface;
class TempleExteriorSystemInterface;
class TipBubbleSystemInterface;
class ToBeDeletedSystemInterface;
class TornadoSystemInterface;
class TownDesireSystemInterface;
class TownStateSystemInterface;
class TownCellObjectsInterface;
class TownSystemInterface;
class TreeSystemInterface;
class TutorialSkipSystemInterface;
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
class VortexSystemInterface;
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
class TimeSystemInterface;
} // namespace ecs::systems

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
	using time = entt::locator<ecs::systems::TimeSystemInterface>;
	using particleSystem = entt::locator<ecs::systems::ParticleSystemInterface>;
	using mistSystem = entt::locator<ecs::systems::MistSystemInterface>;
	using cloudSystem = entt::locator<ecs::systems::CloudSystemInterface>;
	using villageLightSystem = entt::locator<ecs::systems::VillageLightSystemInterface>;
	using villageTotemSystem = entt::locator<ecs::systems::VillageTotemSystemInterface>;
	using artefactSystem = entt::locator<ecs::systems::ArtefactSystemInterface>;
	/// The players' heartbeats as the temples' columns of light read them (the guidance's heart beat)
	using heartbeatSystem = entt::locator<ecs::systems::HeartbeatSystemInterface>;
	using renderFrameSystem = entt::locator<ecs::systems::RenderFrameSystemInterface>;
	using skyFrameSystem = entt::locator<ecs::systems::SkyFrameSystemInterface>;
	using landAvoidSystem = entt::locator<ecs::systems::LandAvoidSystemInterface>;
	using debugHooks = entt::locator<ecs::systems::DebugHooksInterface>;
	using scriptState = entt::locator<ecs::systems::ScriptStateInterface>;
	// the objects the scripts hold (raffclar's slot, over our ScriptHeld logic)
	using scriptObjects = entt::locator<ecs::systems::ScriptObjectsSystemInterface>;
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
	/// The reward chests the scripts give (raffclar's slot)
	using rewardSystem = entt::locator<ecs::systems::RewardSystemInterface>;
	// the audio service: the functions of Audio.h, over our engine (raffclar's name)
	using audio = entt::locator<audio::AudioManagerInterface>;
	using audioState = entt::locator<ecs::systems::AudioStateInterface>;
	using soundTagSystem = entt::locator<ecs::systems::SoundTagSystemInterface>;
	using dynamicsSystem = entt::locator<ecs::systems::DynamicsSystemInterface>;
	// the broken buildings and the fragments knocked off them (raffclar's slot, over our damage meshes)
	using buildingDamageSystem = entt::locator<ecs::systems::BuildingDamageSystemInterface>;
	using weatherSystem = entt::locator<ecs::systems::WeatherSystemInterface>;
	using snowSystem = entt::locator<ecs::systems::SnowSystemInterface>;
	using snowfallSystem = entt::locator<ecs::systems::SnowfallSystemInterface>;
	using rainSystem = entt::locator<ecs::systems::RainSystemInterface>;
	using terrainSystem = entt::locator<LandIslandInterface>;
	using oceanSystem = entt::locator<OceanInterface>;
	using skySystem = entt::locator<ecs::systems::SkySystemInterface>;
	using camera = entt::locator<Camera>;
	using gameActionSystem = entt::locator<input::GameActionInterface>;
	using rendereringSystem = entt::locator<ecs::systems::RenderingSystemInterface>;
	using rendererInterface = entt::locator<graphics::RendererInterface>;
	using pickingSystem = entt::locator<ecs::systems::PickingSystemInterface>;
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
	using handGrabSystem = entt::locator<ecs::systems::HandGrabSystemInterface>;
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
	using creatureSpellBodies = entt::locator<ecs::systems::CreatureSpellBodiesInterface>;
	using toBeDeletedSystem = entt::locator<ecs::systems::ToBeDeletedSystemInterface>;
	using objectCreationIndexSystem = entt::locator<ecs::systems::ObjectCreationIndexSystemInterface>;
	using mapCellsSystem = entt::locator<ecs::systems::MapCellsSystemInterface>;
	using drawListSystem = entt::locator<ecs::systems::DrawListSystemInterface>;
	using animatedStaticSystem = entt::locator<ecs::systems::AnimatedStaticSystemInterface>;
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
	/// The vortices between the lands (raffclar's slot, over our ECS/Vortex)
	using vortexSystem = entt::locator<ecs::systems::VortexSystemInterface>;
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
	/// The creatures fizzing out of sight and back in (raffclar's slot and system)
	using creatureFizzSystem = entt::locator<ecs::systems::CreatureFizzSystemInterface>;
	using worshipState = entt::locator<ecs::systems::WorshipStateInterface>;
	using inputState = entt::locator<ecs::systems::InputStateInterface>;
	using gameStatsSystem = entt::locator<ecs::systems::GameStatsSystemInterface>;
	using mapScriptSystem = entt::locator<ecs::systems::MapScriptSystemInterface>;
	/// The answer to the start-of-game question, which the story's scripts ask about, and the player profiles it is
	/// asked from (raffclar's slots, over our flags and our profile's creature file)
	using tutorialSkipSystem = entt::locator<ecs::systems::TutorialSkipSystemInterface>;
	using playerProfileSystem = entt::locator<ecs::systems::PlayerProfileSystemInterface>;
	/// The help texts' spoken lines the scripts have said (raffclar's slot, over our voices)
	using helpSpeechSystem = entt::locator<ecs::systems::HelpSpeechSystemInterface>;
	/// The help texts the scripts show (raffclar's slot, over our help system's texts)
	using helpTextSystem = entt::locator<ecs::systems::HelpTextSystemInterface>;
	/// The opening's specials the scripts play: the falling light, the chasing camera and the god's hand (raffclar's
	/// slot, over our opening specials)
	using introSystem = entt::locator<ecs::systems::IntroSystemInterface>;
	/// The villagers scripts draw in high detail, such as the opening's family (raffclar's slot, over our SuperVillagers)
	using highDetailSystem = entt::locator<ecs::systems::HighDetailSystemInterface>;
	/// The things the scripts walk along the camera editor's tracks (raffclar's slot, over our walking list)
	using walkPathSystem = entt::locator<ecs::systems::WalkPathSystemInterface>;
	/// The opening's sharks (raffclar's slot, over our sharks)
	using sharkSystem = entt::locator<ecs::systems::SharkSystemInterface>;
	/// The dances: the scripts', the towns' and the worship sites' (raffclar's slot and system)
	using danceSystem = entt::locator<ecs::systems::DanceSystemInterface>;
	/// The help profile: what the player has done, counted for the help (raffclar's slot, over our help profile)
	using helpProfileSystem = entt::locator<ecs::systems::HelpProfileSystemInterface>;
	/// The did-you-know bubble over the sign last tapped (raffclar's slot, over our bubble)
	using tipBubbleSystem = entt::locator<ecs::systems::TipBubbleSystemInterface>;
	/// The player's two advisors (raffclar's slot, over our advisor spirits)
	using advisorSystem = entt::locator<ecs::systems::AdvisorSystemInterface>;
	// the script fade and the cinema bars (raffclar's slot, over our fade and bars)
	using cinematicDirectorSystem = entt::locator<ecs::systems::CinematicDirectorSystemInterface>;
	// which script task has the dialogue (raffclar's slot, over the owner our help system held)
	using dialogueControlSystem = entt::locator<ecs::systems::DialogueControlSystemInterface>;
	// which script task has the camera and which the game speed (raffclar's slot, over our script camera)
	using scriptControlSystem = entt::locator<ecs::systems::ScriptControlSystemInterface>;
	using dayNightClock = entt::locator<ecs::systems::DayNightClockSystemInterface>;
	using screenshotRequest = entt::locator<ecs::systems::ScreenshotRequestSystemInterface>;
	using resourceStoreSystem = entt::locator<ecs::systems::ResourceStoreSystemInterface>;
	// raffclar's slots whose systems are not ported yet, declared ahead so that porting one does not touch this header
	using abodeKnockSystem = entt::locator<ecs::systems::AbodeKnockSystemInterface>;
	using cameraZoneSystem = entt::locator<ecs::systems::CameraZoneSystemInterface>;
	using creatureCarryOverSystem = entt::locator<ecs::systems::CreatureCarryOverSystemInterface>;
	using creaturePenSystem = entt::locator<ecs::systems::CreaturePenSystemInterface>;
	using fireflySystem = entt::locator<ecs::systems::FireflySystemInterface>;
	using fishFarmSystem = entt::locator<ecs::systems::FishFarmSystemInterface>;
	using handDemoSystem = entt::locator<ecs::systems::HandDemoSystemInterface>;
	using inspector = entt::locator<ecs::systems::InspectorSystemInterface>;
	using scriptHighlightSystem = entt::locator<ecs::systems::ScriptHighlightSystemInterface>;
	using tattooEditorSystem = entt::locator<ecs::systems::TattooEditorSystemInterface>;
	using templeDestructionSystem = entt::locator<ecs::systems::TempleDestructionSystemInterface>;
	using worshipSiteSystem = entt::locator<ecs::systems::WorshipSiteSystemInterface>;
	/// only when the mod loader library was loaded at start-up
	using modLoader = entt::locator<ModLoaderStatus>;
};
} // namespace openblack
