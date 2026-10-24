/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Locator.h"

#include <cstdlib>
#include <ctime>

#include <algorithm>
#include <array>
#include <memory>
#include <type_traits>

#include "LocatorInit.h"

#define LOCATOR_IMPLEMENTATIONS

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/Clouds.h"
#include "3D/Implementations/LandIsland.h"
#include "3D/Implementations/Ocean.h"
#include "3D/Implementations/TempleInterior.h"
#include "3D/Implementations/UnloadedIsland.h"
#include "3D/LandData.h"
#include "Audio/AudioManager.h"
#include "Audio/Device/Device.h"
#include "CHLApi.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomProduction.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManagerProduction.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/CreatureMimic.h"
#include "ECS/LandReseat.h"
#include "ECS/MapProduction.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AbodeKnockSystem.h"
#include "ECS/Systems/Implementations/AbodeSoundSystem.h"
#include "ECS/Systems/Implementations/AdvisorSystem.h"
#include "ECS/Systems/Implementations/AlignmentSystem.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/AnimatedStaticSystem.h"
#include "ECS/Systems/Implementations/ArtefactSystem.h"
#include "ECS/Systems/Implementations/AudioState.h"
#include "ECS/Systems/Implementations/BuildingDamageSystem.h"
#include "ECS/Systems/Implementations/CameraBookmarkSystem.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "ECS/Systems/Implementations/CameraZoneSystem.h"
#include "ECS/Systems/Implementations/ChimneySmokeSystem.h"
#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"
#include "ECS/Systems/Implementations/CloudSystem.h"
#include "ECS/Systems/Implementations/ComputerPlayerSystem.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "ECS/Systems/Implementations/CreatureCaveSystem.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/CreatureFizzSystem.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "ECS/Systems/Implementations/CreatureHelpSystem.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureModeSystem.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/CreatureReactions.h"
#include "ECS/Systems/Implementations/CreatureSkinSystem.h"
#include "ECS/Systems/Implementations/CreatureSpellBodies.h"
#include "ECS/Systems/Implementations/DanceSystem.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "ECS/Systems/Implementations/DialogueControlSystem.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/EditorSystem.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"
#include "ECS/Systems/Implementations/FieldSystem.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/Implementations/FootprintSystem.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/GameStatsSystem.h"
#include "ECS/Systems/Implementations/GestureSystem.h"
#include "ECS/Systems/Implementations/GlintTargets.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/Implementations/HandTapRegistry.h"
#include "ECS/Systems/Implementations/HeartbeatSystem.h"
#include "ECS/Systems/Implementations/HelpProfileSystem.h"
#include "ECS/Systems/Implementations/HelpSpeechSystem.h"
#include "ECS/Systems/Implementations/HelpTextSystem.h"
#include "ECS/Systems/Implementations/HighDetailSystem.h"
#include "ECS/Systems/Implementations/InfluenceSystem.h"
#include "ECS/Systems/Implementations/InputState.h"
#include "ECS/Systems/Implementations/IntroSystem.h"
#include "ECS/Systems/Implementations/LandAvoidSystem.h"
#include "ECS/Systems/Implementations/LandBalanceSystem.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "ECS/Systems/Implementations/LogOnceService.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/MapScriptSystem.h"
#include "ECS/Systems/Implementations/MapShapeProvider.h"
#include "ECS/Systems/Implementations/MeshBoxProvider.h"
#include "ECS/Systems/Implementations/MiracleFxSystem.h"
#include "ECS/Systems/Implementations/MistSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "ECS/Systems/Implementations/ObjectDrawListSystem.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/PathfindingSystem.h"
#include "ECS/Systems/Implementations/PickingSystem.h"
#include "ECS/Systems/Implementations/PlayerProfileSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/RainSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "ECS/Systems/Implementations/RenderFrameSystem.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/ResourceStoreSystem.h"
#include "ECS/Systems/Implementations/RewardSystem.h"
#include "ECS/Systems/Implementations/RoutePlanStateSystem.h"
#include "ECS/Systems/Implementations/SaveGameSystem.h"
#include "ECS/Systems/Implementations/ScreenshotRequestSystem.h"
#include "ECS/Systems/Implementations/ScriptControlSystem.h"
#include "ECS/Systems/Implementations/ScriptObjectsSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/SharkSystem.h"
#include "ECS/Systems/Implementations/SkyFrameSystem.h"
#include "ECS/Systems/Implementations/SkySystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/Implementations/SnowfallSystem.h"
#include "ECS/Systems/Implementations/SoundTagSystem.h"
#include "ECS/Systems/Implementations/TattooEditorSystem.h"
#include "ECS/Systems/Implementations/TeleportSystem.h"
#include "ECS/Systems/Implementations/TempleDestructionSystem.h"
#include "ECS/Systems/Implementations/TempleExteriorSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "ECS/Systems/Implementations/TipBubbleSystem.h"
#include "ECS/Systems/Implementations/ToBeDeletedSystem.h"
#include "ECS/Systems/Implementations/TornadoSystem.h"
#include "ECS/Systems/Implementations/TownCellObjects.h"
#include "ECS/Systems/Implementations/TownDesireSystem.h"
#include "ECS/Systems/Implementations/TownStateSystem.h"
#include "ECS/Systems/Implementations/TownSystem.h"
#include "ECS/Systems/Implementations/TreeSystem.h"
#include "ECS/Systems/Implementations/TutorialSkipSystem.h"
#include "ECS/Systems/Implementations/VegetationSystem.h"
#include "ECS/Systems/Implementations/VideoSystem.h"
#include "ECS/Systems/Implementations/VillageLightSystem.h"
#include "ECS/Systems/Implementations/VillageTotemSystem.h"
#include "ECS/Systems/Implementations/VillagerBuildingSites.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerFields.h"
#include "ECS/Systems/Implementations/VillagerFishFarms.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStateSystem.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerTentQueries.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/Implementations/VortexSystem.h"
#include "ECS/Systems/Implementations/WalkPathSystem.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/Systems/Implementations/WorshipState.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/WeatherLoop.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/GameFont.h"
#include "Graphics/RendererInterface.h"
#include "Input/GameActionMap.h"
#include "LHVM.h"
#include "Magic/Objects/MagicTree.h"
#include "Magic/Spells/SpellClasses.h"
#include "ModLoaderStatus.h"
#include "Particles/Rules/LightningStrike.h"
#include "Profiler.h"
#include "Resources/Resources.h"
#include "Windowing/Sdl2WindowingSystem.h"
#if __ANDROID__
#include "FileSystem/AndroidFileSystem.h"
#else
#include "FileSystem/DefaultFileSystem.h"
#endif

using namespace openblack::audio;
using namespace openblack::filesystem;
using openblack::GameRandomProduction;
using openblack::LandIsland;
using openblack::RandomNumberManagerProduction;
using openblack::TempleInterior;
using openblack::UnloadedIsland;
using openblack::chlapi::CHLApi;
using openblack::debug::gui::DebugGuiInterface;
using openblack::ecs::MapProduction;
using openblack::ecs::Registry;
using openblack::ecs::systems::AdvisorSystem;
using openblack::ecs::systems::AlignmentSystem;
using openblack::ecs::systems::AnimalSystem;
using openblack::ecs::systems::AnimatedStaticSystem;
using openblack::ecs::systems::ArtefactSystem;
using openblack::ecs::systems::CameraBookmarkSystem;
using openblack::ecs::systems::CameraHelpSystem;
using openblack::ecs::systems::CameraPathSystem;
using openblack::ecs::systems::CinematicDirectorSystem;
using openblack::ecs::systems::CreatureAnimationSystem;
using openblack::ecs::systems::CreatureAudioSystem;
using openblack::ecs::systems::CreatureCaveSystem;
using openblack::ecs::systems::CreatureFightSystem;
using openblack::ecs::systems::CreatureFizzSystem;
using openblack::ecs::systems::CreatureHairSystem;
using openblack::ecs::systems::CreatureHandSystem;
using openblack::ecs::systems::CreatureHelpSystem;
using openblack::ecs::systems::CreatureLocomotionSystem;
using openblack::ecs::systems::CreatureMindSystem;
using openblack::ecs::systems::CreatureModeSystem;
using openblack::ecs::systems::CreatureObjectActionSystem;
using openblack::ecs::systems::CreaturePhysiologySystem;
using openblack::ecs::systems::CreatureSkinSystem;
using openblack::ecs::systems::CreatureSpellBodies;
using openblack::ecs::systems::DanceSystem;
using openblack::ecs::systems::DayNightClockSystem;
using openblack::ecs::systems::DialogueControlSystem;
using openblack::ecs::systems::EditorSystem;
using openblack::ecs::systems::FireSystem;
using openblack::ecs::systems::FootprintSystem;
using openblack::ecs::systems::ForestSystem;
using openblack::ecs::systems::GameStatsSystem;
using openblack::ecs::systems::GestureEventsInterface;
using openblack::ecs::systems::GestureSystem;
using openblack::ecs::systems::GlintTargets;
using openblack::ecs::systems::HandGrabSystemInterface;
using openblack::ecs::systems::HandSystem;
using openblack::ecs::systems::HeartbeatSystem;
using openblack::ecs::systems::HelpProfileSystem;
using openblack::ecs::systems::HelpSpeechSystem;
using openblack::ecs::systems::HelpTextSystem;
using openblack::ecs::systems::HighDetailSystem;
using openblack::ecs::systems::InfluenceSystem;
using openblack::ecs::systems::InputState;
using openblack::ecs::systems::IntroSystem;
using openblack::ecs::systems::LandBalanceSystem;
using openblack::ecs::systems::LeashSystem;
using openblack::ecs::systems::LivingActionSystem;
using openblack::ecs::systems::MagicShieldSystem;
using openblack::ecs::systems::MagicSystem;
using openblack::ecs::systems::MapCellsSystem;
using openblack::ecs::systems::MapScriptSystem;
using openblack::ecs::systems::MapShapeProvider;
using openblack::ecs::systems::MeshBoxProvider;
using openblack::ecs::systems::MiracleFxSystem;
using openblack::ecs::systems::ObjectCreationIndexSystem;
using openblack::ecs::systems::ObjectDrawListSystem;
using openblack::ecs::systems::PathfindingSystem;
using openblack::ecs::systems::PickingSystem;
using openblack::ecs::systems::PlayerProfileSystem;
using openblack::ecs::systems::PlayerSystem;
using openblack::ecs::systems::ReactionSystem;
using openblack::ecs::systems::RenderingSystem;
using openblack::ecs::systems::ResourceStoreSystem;
using openblack::ecs::systems::RoutePlanStateSystem;
using openblack::ecs::systems::SaveGameSystem;
using openblack::ecs::systems::ScreenshotRequestSystem;
using openblack::ecs::systems::ScriptControlSystem;
using openblack::ecs::systems::SharkSystem;
using openblack::ecs::systems::TattooEditorSystem;
using openblack::ecs::systems::TeleportSystem;
using openblack::ecs::systems::TimeSystem;
using openblack::ecs::systems::TipBubbleSystem;
using openblack::ecs::systems::ToBeDeletedSystem;
using openblack::ecs::systems::TornadoSystem;
using openblack::ecs::systems::TownCellObjects;
using openblack::ecs::systems::TownDesireSystem;
using openblack::ecs::systems::TownStateSystem;
using openblack::ecs::systems::TownSystem;
using openblack::ecs::systems::TreeSystem;
using openblack::ecs::systems::TutorialSkipSystem;
using openblack::ecs::systems::VillagerBuildingSites;
using openblack::ecs::systems::VillagerChildFactory;
using openblack::ecs::systems::VillagerDiscipleJobs;
using openblack::ecs::systems::VillagerFields;
using openblack::ecs::systems::VillagerFishFarms;
using openblack::ecs::systems::VillagerRules;
using openblack::ecs::systems::VillagerStateSystem;
using openblack::ecs::systems::VillagerStores;
using openblack::ecs::systems::VillagerTentQueries;
using openblack::ecs::systems::VillagerWorldQueries;
using openblack::ecs::systems::VillagerWorshipCheck;
using openblack::ecs::systems::VillageTotemSystem;
using openblack::ecs::systems::VortexSystem;
using openblack::ecs::systems::WalkPathSystem;
using openblack::ecs::systems::WorshipState;
using openblack::graphics::RendererInterface;
using openblack::input::GameActionMap;
using openblack::lhvm::LHVM;
using openblack::resources::Resources;
using openblack::windowing::DisplayMode;
using openblack::windowing::Sdl2WindowingSystem;

namespace
{
/// Today's date by the local clock, which only the creatures' footprints read (once per land); the date as the game
/// reads it, pinned in a seeded run
openblack::ecs::systems::CalendarDate LocalDate()
{
	const auto now = static_cast<std::time_t>(openblack::machine_clock::UnixTime());
	std::tm local {};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return {.month = local.tm_mon + 1, .day = local.tm_mday};
}
} // namespace

void openblack::InitializeWindow(const std::string& title, int width, int height, DisplayMode displayMode, uint32_t extraFlags)
{
	Locator::windowing::emplace<Sdl2WindowingSystem>(title, width, height, displayMode, extraFlags);
}

void openblack::InitializeClock()
{
	// A clock set up before the game (a test's fixed step) is kept, as the old global clock was
	if (!Locator::time::has_value())
	{
		Locator::time::emplace<TimeSystem>();
	}
}

void openblack::InitializeGameState()
{
	// Always new, as each Game started with its own (a test's is replaced too); in the order the Game made them
	Locator::cinematicDirectorSystem::emplace<CinematicDirectorSystem>();
	Locator::dayNightClock::emplace<DayNightClockSystem>();
	Locator::mapScriptSystem::emplace<MapScriptSystem>();
	// the start-of-game question's answer, which lived in the map script's globals: new with them, none skipped
	Locator::tutorialSkipSystem::emplace<TutorialSkipSystem>();
	// not in the tests' services: without a Game the test hooks request no screenshot
	Locator::screenshotRequest::emplace<ScreenshotRequestSystem>();
	// the dialogue's owner, as each Game's help system held it; a test's own is kept
	if (!Locator::dialogueControlSystem::has_value())
	{
		Locator::dialogueControlSystem::emplace<DialogueControlSystem>();
	}
	// and the camera's and the game speed's, as each Game's script state held them
	if (!Locator::scriptControlSystem::has_value())
	{
		Locator::scriptControlSystem::emplace<ScriptControlSystem>();
	}
}

void openblack::InitializeSimulationCore(StartKind kind)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "EnTT version: {}", ENTT_VERSION);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), GLM_VERSION_COMPLETE);

	// The warnings logged once, before anything that may warn (the renderer's shaders): kept for the whole process, so
	// a new game logs none of them again; a test's own is kept
	if (!Locator::logOnce::has_value())
	{
		Locator::logOnce::emplace<ecs::systems::LogOnceService>();
	}

	// Made at a launch, as always; a new game in the same process keeps the profiler
	const bool launch = kind == StartKind::Launch;
	if (launch || !Locator::profiler::has_value())
	{
		Locator::profiler::emplace();
		// OPENBLACK_PROFILE=<seconds>: log each stage's average / worst time per frame every <seconds>.
		if (const char* profile = std::getenv("OPENBLACK_PROFILE"); profile != nullptr)
		{
			Locator::profiler::value().SetSummaryInterval(std::max(0.5f, static_cast<float>(std::atof(profile))));
		}
	}
	Locator::events::emplace<EventManager>();
	// The file system and the random number manager too
	if (launch || !Locator::filesystem::has_value())
	{
#if __ANDROID__
		Locator::filesystem::emplace<AndroidFileSystem>();
#else
		Locator::filesystem::emplace<DefaultFileSystem>();
#endif
	}
	if (launch || !Locator::rng::has_value())
	{
		Locator::rng::emplace<RandomNumberManagerProduction>();
	}
	// The game's own random streams, new with every game
	Locator::gameRandom::emplace<GameRandomProduction>();
}

bool openblack::InitializeEngine(StartKind kind, GraphicsBackend backend, bool vsync,
                                 std::array<uint8_t, 256>& fontHashPermutation) noexcept
{
	// Made at a launch, as always; a new game in the same process keeps the renderer
	const bool launch = kind == StartKind::Launch;
	if (launch || !Locator::rendererInterface::has_value())
	{
		Locator::rendererInterface::reset(RendererInterface::Create(backend, vsync).release());
		if (!Locator::rendererInterface::has_value())
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to create renderer");
			return false;
		}
	}
	// Opening the renderer starts the engine timer (the frame delta, EngineMs); a new game starts it again
	game_clock::StartEngineTimer();
	Locator::debugGui::reset(DebugGuiInterface::Create(graphics::RenderPass::ImGui).release());

	Locator::particleSystem::emplace<ecs::systems::ParticleSystem>();
	// a test's fake mist system stays
	if (!Locator::mistSystem::has_value())
	{
		Locator::mistSystem::emplace<ecs::systems::MistSystem>();
	}
	// a test's fake cloud system stays
	if (!Locator::cloudSystem::has_value())
	{
		Locator::cloudSystem::emplace<ecs::systems::CloudSystem>();
	}
	Locator::villageLightSystem::emplace<ecs::systems::VillageLightSystem>();
	Locator::renderFrameSystem::emplace<ecs::systems::RenderFrameSystem>();
	Locator::skyFrameSystem::emplace<ecs::systems::SkyFrameSystem>();
	Locator::landAvoidSystem::emplace<ecs::systems::LandAvoidSystem>();
	Locator::debugHooks::emplace<ecs::systems::DebugHooks>();
	Locator::scriptState::emplace<ecs::systems::ScriptState>();
	Locator::scriptObjects::emplace<ecs::systems::ScriptObjectsSystem>();
	// the camera help, which lived in the script state: made and gone with it
	Locator::cameraHelpSystem::emplace<CameraHelpSystem>();
	// the camera zones, which lived in the script state too; each land starts its own
	Locator::cameraZoneSystem::emplace<ecs::systems::CameraZoneSystem>();
	Locator::handTapRegistry::emplace<ecs::systems::HandTapRegistry>();
	Locator::abodeKnockSystem::emplace<ecs::systems::AbodeKnockSystem>();
	Locator::abodeSoundSystem::emplace<ecs::systems::AbodeSoundSystem>();
	Locator::worldEffects::emplace<ecs::systems::WorldEffects>();
	Locator::waterRingSystem::emplace<ecs::systems::WaterRingSystem>();
	Locator::chimneySmokeSystem::emplace<ecs::systems::ChimneySmokeSystem>();
	Locator::vegetation::emplace<ecs::systems::VegetationSystem>();
	Locator::fieldSystem::emplace<ecs::systems::FieldSystem>();
	Locator::explosionSystem::emplace<ecs::systems::ExplosionSystem>();
	Locator::rewardSystem::emplace<ecs::systems::RewardSystem>();
	// a test's fake audio service stays
	if (!Locator::audio::has_value())
	{
		Locator::audio::emplace<audio::AudioManager>();
	}
	Locator::audioState::emplace<ecs::systems::AudioState>();
	Locator::soundTagSystem::emplace<ecs::systems::SoundTagSystem>();
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>();
	Locator::buildingDamageSystem::emplace<ecs::systems::BuildingDamageSystem>();
	Locator::weatherSystem::emplace<ecs::systems::WeatherSystem>();
	// A storm's fork lightning queues its point for the particle system's strike effect, as the particle system's
	// start installs it in the original
	weather::storms::SetForkCallback([](const weather::storms::Storm& /*storm*/, const glm::vec3& point, float radius) {
		particles::lightning_strike::Queue(point, radius);
	});
	// the snow lying on the island, which the weather's storms lay
	Locator::snowSystem::emplace<ecs::systems::SnowSystem>();
	// the snow drawn falling, and the rain
	Locator::snowfallSystem::emplace<ecs::systems::SnowfallSystem>();
	Locator::rainSystem::emplace<ecs::systems::RainSystem>();
	// The atmosphere's start-up, once a process and before any land: the first draws of the CRT stream. The Weather
	// detail setting is read here only; without a config it is the default level's
	const uint8_t detailLevel =
	    Locator::config::has_value() ? Locator::config::value().detailLevel : graphics::detail_level::k_Default;
	weather::StartAtmos(graphics::detail_level::Weather(detailLevel), Locator::snowfallSystem::value(),
	                    Locator::rainSystem::value());
	// Then the font cache's glyph hash permutation, whose 255 draws come next on the CRT stream, with or without fonts.
	// InitializeSimulation keeps it with the resources
	fontHashPermutation = graphics::GameFont::MakeHashPermutation([] { return game_random::crt::Rand(); });
	// The audio system's wave device: without one nothing plays
	audio::device::Open();
	return true;
}

bool openblack::InitializeSimulation(StartKind kind, const std::array<uint8_t, 256>& fontHashPermutation) noexcept
{
	// The script machine: the scripts' table of native functions, made at a launch and kept by a new game, and the VM,
	// new with every game
	if (kind == StartKind::Launch || !Locator::chlapi::has_value())
	{
		Locator::chlapi::emplace<CHLApi>();
	}
	Locator::vm::emplace<LHVM>();
	Locator::terrainSystem::emplace<UnloadedIsland>();
	// The asset caches, made at a launch with its permutation: a new game in the same process keeps them (its own
	// start-up draws the same permutation again)
	if (kind == StartKind::Launch || !Locator::resources::has_value())
	{
		Locator::resources::emplace<Resources>();
		Locator::resources::value().SetFontHashPermutation(fontHashPermutation);
	}
	// The players' alignment lasts the whole game, across lands: an existing player system (a test's, or one a
	// previous Game left, as a process's shutdown does not reset it) is kept
	if (!Locator::playerSystem::has_value())
	{
		Locator::playerSystem::emplace<PlayerSystem>();
	}
	// A new game's interface alignment and hand crossing start again (no entity carries them yet): neutral, and no
	// influence edge crossed
	Locator::playerSystem::value().InterfaceAlignmentWithoutEntity() = {};
	Locator::playerSystem::value().InfluenceCrossingWithoutEntity() = {};
	// The players', the camera's and the sky's alignment as one service, over the player system's
	Locator::alignmentSystem::emplace<AlignmentSystem>();
	// The input modules' state (the game packets and their handlers, the hand demo, the interface's flags), kept for
	// the whole game. Made before the hand system, which registers its packet handlers when it is made
	if (!Locator::inputState::has_value())
	{
		Locator::inputState::emplace<InputState>();
	}
	// The worship modules' state, kept for the whole game; each module clears its own part on a land load as before
	if (!Locator::worshipState::has_value())
	{
		Locator::worshipState::emplace<WorshipState>();
	}
	// The town totems, over the worship percentage; a test's fake stays
	if (!Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::emplace<VillageTotemSystem>();
	}
	// The artefacts the towns' people dance round; a test's fake stays
	if (!Locator::artefactSystem::has_value())
	{
		Locator::artefactSystem::emplace<ArtefactSystem>();
	}
	// The heartbeats the temples' columns of light read, the guidance's; it holds no state. A test's fake stays
	if (!Locator::heartbeatSystem::has_value())
	{
		Locator::heartbeatSystem::emplace<HeartbeatSystem>();
	}
	// Every player's GameStats and the shared statics, kept for the whole game: each land's load clears the counters
	// (game_stats::ClearAll) and the histories run on
	if (!Locator::gameStatsSystem::has_value())
	{
		Locator::gameStatsSystem::emplace<GameStatsSystem>();
	}
	// The players' influence as one service, over the influence state each land keeps in the registry. It keeps nothing
	// of its own, so a test's is kept
	if (!Locator::influenceSystem::has_value())
	{
		Locator::influenceSystem::emplace<InfluenceSystem>();
	}
	Locator::entitiesRegistry::emplace<Registry>();
	// The hand grab's slot is the same system, which stays the owner
	auto& hand = static_cast<HandSystem&>(Locator::handSystem::emplace<HandSystem>());
	Locator::handGrabSystem::reset(static_cast<HandGrabSystemInterface*>(&hand), [](HandGrabSystemInterface* /*unowned*/) {});
	Locator::temple::emplace<TempleInterior>();
	Locator::oceanSystem::emplace<Ocean>();
	Locator::skySystem::emplace<ecs::systems::SkySystem>();
	// What the villagers ask of the fields, fish farms, building sites, stores and tent spots. A test may have put
	// its fakes in before the game: they are kept
	if (!Locator::villagerFields::has_value())
	{
		Locator::villagerFields::emplace<VillagerFields>();
	}
	if (!Locator::villagerFishFarms::has_value())
	{
		Locator::villagerFishFarms::emplace<VillagerFishFarms>();
	}
	if (!Locator::villagerBuildingSites::has_value())
	{
		Locator::villagerBuildingSites::emplace<VillagerBuildingSites>();
	}
	if (!Locator::villagerStores::has_value())
	{
		Locator::villagerStores::emplace<VillagerStores>();
	}
	if (!Locator::villagerTentQueries::has_value())
	{
		Locator::villagerTentQueries::emplace<VillagerTentQueries>();
	}
	// The villagers' rules, world queries, worship check, child maker and disciple jobs. A test's fakes are kept the
	// same way
	if (!Locator::villagerRules::has_value())
	{
		Locator::villagerRules::emplace<VillagerRules>();
	}
	if (!Locator::villagerWorldQueries::has_value())
	{
		Locator::villagerWorldQueries::emplace<VillagerWorldQueries>();
	}
	if (!Locator::villagerWorshipCheck::has_value())
	{
		Locator::villagerWorshipCheck::emplace<VillagerWorshipCheck>();
	}
	if (!Locator::villagerChildFactory::has_value())
	{
		Locator::villagerChildFactory::emplace<VillagerChildFactory>();
	}
	if (!Locator::villagerDiscipleJobs::has_value())
	{
		Locator::villagerDiscipleJobs::emplace<VillagerDiscipleJobs>();
	}
	// What the map cells and the town read of the meshes and the cells' objects. A test's fakes are kept the same way
	if (!Locator::townCellObjects::has_value())
	{
		Locator::townCellObjects::emplace<TownCellObjects>();
	}
	if (!Locator::mapShapeProvider::has_value())
	{
		Locator::mapShapeProvider::emplace<MapShapeProvider>();
	}
	if (!Locator::meshBoxProvider::has_value())
	{
		Locator::meshBoxProvider::emplace<MeshBoxProvider>();
	}
	// What the glints on a target read of the objects they are given. A test's fake is kept
	if (!Locator::glintTargets::has_value())
	{
		Locator::glintTargets::emplace<GlintTargets>();
	}
	// What the creature spells' particles read of the creature a spell was cast on. A test's fake is kept
	if (!Locator::creatureSpellBodies::has_value())
	{
		Locator::creatureSpellBodies::emplace<CreatureSpellBodies>();
	}
	// The game's world lists, kept for the whole game: the dead list, the object creation counter and the map cells
	// (each land starts the counter again through object_index::OnLoadMap and empties the cells through
	// magic::OnLoadMap). A test's own are kept the same way
	if (!Locator::toBeDeletedSystem::has_value())
	{
		Locator::toBeDeletedSystem::emplace<ToBeDeletedSystem>();
	}
	if (!Locator::objectCreationIndexSystem::has_value())
	{
		Locator::objectCreationIndexSystem::emplace<ObjectCreationIndexSystem>();
	}
	if (!Locator::mapCellsSystem::has_value())
	{
		Locator::mapCellsSystem::emplace<MapCellsSystem>();
	}
	// The object draw list, kept for the whole game. It hears the rebuild requests through the event manager (made with
	// the core services, before this) until it goes. A test's own is kept
	if (!Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::emplace<ObjectDrawListSystem>(Locator::events::has_value() ? &Locator::events::value()
		                                                                                          : nullptr);
	}
	// The animated scenery the scripts open and close (the creature gate, the gate stone plinth), kept for the whole game
	if (!Locator::animatedStaticSystem::has_value())
	{
		Locator::animatedStaticSystem::emplace<AnimatedStaticSystem>();
	}
	// The game's reactions, kept for the whole game (each land empties them through magic::OnLoadMap). The animals',
	// the villagers' and the creatures' handlers are set as soon as they exist, before any map load
	if (!Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::emplace<ReactionSystem>();
		openblack::ecs::animal_ai::RegisterReactionHandler();
		openblack::ecs::villager_reactions::RegisterHandlers();
		openblack::ecs::creature_reactions::RegisterHandlers();
	}
	// The game's fires, their graphics and their crackle, kept for the whole game (each land empties them through
	// magic::OnLoadMap: FireSystem::Reset)
	if (!Locator::fireSystem::has_value())
	{
		Locator::fireSystem::emplace<FireSystem>();
	}
	// The game's forests (each land empties them: Game::LoadMap, ecs::ClearForests), what every tree shares (with the
	// magic trees' deletion listener, before any tree), the land balance (reset by Game::LoadMap) and what the animals
	// share; all kept for the whole game
	if (!Locator::forestSystem::has_value())
	{
		Locator::forestSystem::emplace<ForestSystem>();
	}
	if (!Locator::treeSystem::has_value())
	{
		Locator::treeSystem::emplace<TreeSystem>();
		openblack::magic::magic_tree::RegisterTreeListener();
	}
	if (!Locator::landBalanceSystem::has_value())
	{
		Locator::landBalanceSystem::emplace<LandBalanceSystem>();
	}
	if (!Locator::animalSystem::has_value())
	{
		// with the flock miracles' species dying: the spell classes register it only once per spell system
		Locator::animalSystem::emplace<AnimalSystem>();
		openblack::magic::RegisterFlockSpeciesDying();
	}
	// The miracles' service, which owns their stores for the whole game (a test's fake stays): the spells and their
	// sinks, the map shields and the fireballs (each land empties both through magic::OnLoadMap), the hand's magic
	// modules' state (gestures, casting, mouse sampling, power-up bands, grain sprinkle; each module clears its own
	// part on a land load), and the falling spell (made on its first use) with its sparks flag. The stores'
	// constructors do no work
	if (!Locator::magicSystem::has_value())
	{
		Locator::magicSystem::emplace<MagicSystem>();
	}
	// The shields' service, which holds no state of its own either: the shield lists are the magic objects' and the
	// spells' (a test's fake stays)
	if (!Locator::magicShieldSystem::has_value())
	{
		Locator::magicShieldSystem::emplace<MagicShieldSystem>();
	}
	// The teleport stones' and the tornadoes' services, which hold no state of their own (a test's fake stays)
	if (!Locator::teleportSystem::has_value())
	{
		Locator::teleportSystem::emplace<TeleportSystem>();
	}
	if (!Locator::tornadoSystem::has_value())
	{
		Locator::tornadoSystem::emplace<TornadoSystem>();
	}
	// The vortices' service, which holds no state of its own: the vortices' list is the script state's (a test's fake
	// stays)
	if (!Locator::vortexSystem::has_value())
	{
		Locator::vortexSystem::emplace<VortexSystem>();
	}
	// The player profiles: the game makes them at its launch, before this (Game::Initialize), and keeps them through a
	// restart and a new game; without a game, a run profile of the game's own that keeps nothing (a test's fake stays)
	if (!Locator::playerProfileSystem::has_value())
	{
		Locator::playerProfileSystem::emplace<PlayerProfileSystem>();
	}
	// The saved games of whatever profile is current when they are used: with no profile kept (a run of the checks) it
	// reads and writes nothing (a test's fake stays)
	if (!Locator::saveGame::has_value())
	{
		auto* fileSystem = Locator::filesystem::has_value() ? &Locator::filesystem::value() : nullptr;
		Locator::saveGame::emplace<SaveGameSystem>(fileSystem, []() -> std::optional<std::filesystem::path> {
			if (!Locator::playerProfileSystem::has_value() || !Locator::playerProfileSystem::value().IsKept())
			{
				return std::nullopt;
			}
			return Locator::playerProfileSystem::value().CurrentPlayerFolder();
		});
	}
	// The help texts' spoken lines, which hold no state of their own: the voices are the audio's (a test's fake stays)
	if (!Locator::helpSpeechSystem::has_value())
	{
		Locator::helpSpeechSystem::emplace<HelpSpeechSystem>();
	}
	// The help texts, which hold no state of their own: the help system is the script state's (a test's fake stays)
	if (!Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::emplace<HelpTextSystem>();
	}
	// The opening's specials, which hold no state of their own: it is the world effects' (a test's fake stays)
	if (!Locator::introSystem::has_value())
	{
		Locator::introSystem::emplace<IntroSystem>();
	}
	// The high-detail villagers, which hold no state of their own: the SuperVillager list is ours (a test's fake stays)
	if (!Locator::highDetailSystem::has_value())
	{
		Locator::highDetailSystem::emplace<HighDetailSystem>();
	}
	// The things walking tracks, whose walks are their WalkPath components (a test's fake stays)
	if (!Locator::walkPathSystem::has_value())
	{
		Locator::walkPathSystem::emplace<WalkPathSystem>();
	}
	// The sharks, whose wake timer is the system's (a test's fake stays)
	if (!Locator::sharkSystem::has_value())
	{
		Locator::sharkSystem::emplace<SharkSystem>();
	}
	// The dances, whose state is their Dance entities (a test's fake stays)
	if (!Locator::danceSystem::has_value())
	{
		Locator::danceSystem::emplace<DanceSystem>();
	}
	// The help profile, which holds no state of its own: its counters are the script state's (a test's fake stays)
	if (!Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::emplace<HelpProfileSystem>();
	}
	// The good advisor's messages about the player's creature, whose memory is a component on the creature (a test's
	// fake stays)
	if (!Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::emplace<CreatureHelpSystem>();
	}
	// The did-you-know bubble over the sign last tapped (a test's fake stays)
	if (!Locator::tipBubbleSystem::has_value())
	{
		Locator::tipBubbleSystem::emplace<TipBubbleSystem>();
	}
	// The advisors, which hold no state of their own: the spirits are the script state's (a test's fake stays)
	if (!Locator::advisorSystem::has_value())
	{
		Locator::advisorSystem::emplace<AdvisorSystem>();
	}
	// The towns' and the villagers' shared lists, kept for the whole game (the vagrants are emptied by a script reboot,
	// the mourning takers by magic::OnLoadMap; the belief sprites are only ever popped)
	if (!Locator::townStateSystem::has_value())
	{
		Locator::townStateSystem::emplace<TownStateSystem>();
	}
	// The towns' desires (their data stays on each town; the town process works them out)
	if (!Locator::townDesireSystem::has_value())
	{
		Locator::townDesireSystem::emplace<TownDesireSystem>();
	}
	if (!Locator::villagerStateSystem::has_value())
	{
		Locator::villagerStateSystem::emplace<VillagerStateSystem>();
	}
	// The gestures, over the hand magic state's gesture modules; a test's own is kept. The miracles hear of the
	// gestures through the same system, which stays the owner
	if (!Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::emplace<GestureSystem>();
	}
	Locator::gestureEvents::reset(static_cast<GestureEventsInterface*>(&Locator::gestureSystem::value()),
	                              [](GestureEventsInterface* /*unowned*/) {});
	// The miracles' looks on the globes, the hand and the piles, run on the hand's magic state above (a test's fake stays)
	if (!Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::emplace<MiracleFxSystem>();
	}
	// The route planner's callbacks and obstacle hook, and the footpaths' holder pool, kept for the whole game (the
	// callbacks are installed again at every land load; the pool never shrinks)
	if (!Locator::routePlanStateSystem::has_value())
	{
		Locator::routePlanStateSystem::emplace<RoutePlanStateSystem>();
	}
	// The temples' outsides, their meshes' files read on the first blend and kept for the whole game
	if (!Locator::templeExteriorSystem::has_value())
	{
		Locator::templeExteriorSystem::emplace<openblack::ecs::systems::TempleExteriorSystem>();
	}
	// The temples being destroyed, their hearts' turns from their citadels' and the game's end at the end of a turn
	if (!Locator::templeDestructionSystem::has_value())
	{
		Locator::templeDestructionSystem::emplace<openblack::ecs::systems::TempleDestructionSystem>();
	}
	// The players' computer minds, which a land script switches on and which start in the players' turn
	if (!Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::emplace<openblack::ecs::systems::ComputerPlayerSystem>();
	}
	// The creatures' systems (called from ECS/CreatureLoop): each made new with the game, none reaches the registry
	// when made
	Locator::creatureAnimationSystem::emplace<CreatureAnimationSystem>();
	Locator::creatureMindSystem::emplace<CreatureMindSystem>();
	Locator::creaturePhysiologySystem::emplace<CreaturePhysiologySystem>();
	Locator::creatureHairSystem::emplace<CreatureHairSystem>();
	Locator::creatureAudioSystem::emplace<CreatureAudioSystem>();
	Locator::creatureObjectActionSystem::emplace<CreatureObjectActionSystem>();
	Locator::creatureHandSystem::emplace<CreatureHandSystem>();
	Locator::footprintSystem::emplace<FootprintSystem>(&LocalDate);
	Locator::creatureSkinSystem::emplace<CreatureSkinSystem>();
	Locator::leashSystem::emplace<LeashSystem>();
	// Creature Mode, kept for the whole game: while it follows a creature it holds the player's camera model. The
	// player's creature is their primary one
	Locator::creatureModeSystem::emplace<CreatureModeSystem>([](PlayerNames player) -> std::optional<entt::entity> {
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	});
	// The Creature Cave, which the temple's creature room shows: of the player's creature, through Creature Mode. The
	// mind's tables and the tattoos come from the mind and skin services
	Locator::creatureCaveSystem::emplace<CreatureCaveSystem>(CreatureCaveSystem::Services {
	    .mindTables = []() -> const creature_mind_tables::Tables* {
		    return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	    },
	    .setTattoo =
	        [](entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) {
		        if (Locator::creatureSkinSystem::has_value())
		        {
			        Locator::creatureSkinSystem::value().SetTattoo(creature, slot, tattoo);
		        }
	        },
	});
	// The tattoo editor, which the cave opens on the player's creature; it paints the skin through the skin service
	Locator::tattooEditorSystem::emplace<TattooEditorSystem>();
	Locator::creatureFightSystem::emplace<CreatureFightSystem>();
	Locator::creatureFizzSystem::emplace<CreatureFizzSystem>();
	// the game's kinds of thing in the physics: villagers, animals, trees, dead trees, pots, scaffolds and the creature
	// (thrown at, mass 1000)
	Locator::dynamicsSystem::value().SetClassHooks(std::make_unique<openblack::ecs::PhysicsGameHooks>());
	// The game's handlers of what the villagers' deaths and the towns' belief and desires report: help sprites,
	// tooltips, the smoke and the souls; the players' deeds their creatures watch; the things that follow the land
	// when the vortex or the temple flattens it; and the walkers that let go of a thing's circles when they go
	if (Locator::events::has_value())
	{
		auto& events = Locator::events::value();
		openblack::ecs::villager::AddDeathEventHandlers(events);
		openblack::ecs::town_belief::AddBeliefEventHandlers(events);
		openblack::ecs::town_desire::AddDesireEventHandlers(events);
		openblack::ecs::creature_mimic::AddMimicEventHandlers(events);
		openblack::ecs::land_reseat::AddLandReseatEventHandlers(events);
		openblack::ecs::systems::AddPathfindingEventHandlers(events);
		openblack::ecs::animal_ai::AddAnimalEventHandlers(events);
	}

	return true;
}

void openblack::InitializeClient()
{
	// The player's input, the drawing of the world, the editor and the films
	Locator::gameActionSystem::emplace<GameActionMap>();
	Locator::rendereringSystem::emplace<RenderingSystem>();
	// The in-game editor (F2), kept for the whole game: while it drives the camera it holds the player's camera model
	Locator::editorSystem::emplace<EditorSystem>();
	Locator::videoSystem::emplace<ecs::systems::VideoSystem>();
}

bool openblack::InitializeGame(StartKind kind, const std::array<uint8_t, 256>& fontHashPermutation) noexcept
{
	if (!InitializeSimulation(kind, fontHashPermutation))
	{
		return false;
	}
	InitializeClient();
	return true;
}

namespace openblack
{
namespace
{
/// The land's services, from a landscape file or from land made in memory
template <typename LandSource>
void InitializeLevelWith(const LandSource& land)
{
	// The game's own streams start again from the run's seed with every map, so that the same land loaded with the
	// same seed draws the same numbers
	if (Locator::rng::has_value())
	{
		auto& rng = Locator::rng::value();
		rng.SetRunSeed(rng.GetRunSeed());
	}
	// In a seeded run the machine's clock, as the game reads it, counts from 0 again with every map, from the pinned date
	if (Locator::time::has_value())
	{
		if (const auto date = Locator::time::value().GetPinnedDate(); date.has_value())
		{
			Locator::time::value().RestartClock(*date);
		}
	}
	Locator::entitiesMap::emplace<MapProduction>();
	Locator::pickingSystem::emplace<PickingSystem>();
	Locator::livingActionSystem::emplace<LivingActionSystem>();
	Locator::townSystem::emplace<TownSystem>();
	Locator::resourceStoreSystem::emplace<ResourceStoreSystem>();
	Locator::pathfindingSystem::emplace<PathfindingSystem>();
	Locator::creatureLocomotionSystem::emplace<CreatureLocomotionSystem>();
	Locator::cameraBookmarkSystem::emplace<CameraBookmarkSystem>();
	// A land starts with no camera zones: the game clears them as it clears the last map
	Locator::cameraZoneSystem::emplace<ecs::systems::CameraZoneSystem>();
	if constexpr (std::is_same_v<LandSource, LandData>)
	{
		// The last land's island goes before a generated one is built, as at the game's start: a generated land fills
		// the whole map, and its blocks and the last land's would not fit bgfx's buffer handles together. Here, after
		// the services above that hold the last island's rigid bodies and routes are made again
		Locator::terrainSystem::emplace<UnloadedIsland>();
		// bgfx gives a destroyed buffer's handle back only once the frames after it are submitted, as the island's own
		// build submits one when its meshes are made
		bgfx::frame();
		bgfx::frame();
	}
	Locator::terrainSystem::emplace<LandIsland>(land);
	Locator::cameraPathSystem::emplace<CameraPathSystem>();
	// Opening a landscape opens its sky too: a new sky of clouds for every land, laid out here, once the island is
	// loaded and before its walkable mask, while the map script is still on its LOAD_LANDSCAPE line
	Clouds::OnLandscapeOpened();
}
} // namespace
} // namespace openblack

void openblack::InitializePlayerProfiles(const player_profiles::Store& store)
{
	if (!Locator::playerProfileSystem::has_value())
	{
		auto* fileSystem = Locator::filesystem::has_value() ? &Locator::filesystem::value() : nullptr;
		Locator::playerProfileSystem::emplace<PlayerProfileSystem>(fileSystem, store);
	}
}

void openblack::InitializeLevel(const std::filesystem::path& path)
{
	InitializeLevelWith(path);
}

void openblack::InitializeLevel(const LandData& land)
{
	InitializeLevelWith(land);
}

void openblack::ShutDownServices(ShutDownScope scope)
{
	const bool process = scope == ShutDownScope::Process;
	// the particle effects first, as a map load clears them: their destructors still find the services they use
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().Reset();
	}
	// A game's samples go with it: the audio's start loads them again with the next game's banks
	if (!process && Locator::resources::has_value())
	{
		Locator::resources::value().GetSounds().Clear();
		// and the meshes made for its entities (partly built, damaged, fragments): the registry goes without the
		// destroy signals that erase them at a land change, and the next game numbers its own from 0
		Locator::resources::value().ForgetGeneratedMeshes();
	}
	// Manually delete the assets here before BGFX renderer clears its buffers resulting in invalid handles in our assets
	if (process && Locator::resources::has_value())
	{
		auto& resources = Locator::resources::value();
		resources.GetMeshes().Clear();
		resources.GetTextures().Clear();
		resources.GetAnimations().Clear();
		resources.GetSounds().Clear();
		resources.GetFonts().Clear();
		resources.GetBlobs().Clear();
	}

	// The temple interior before the audio device, the renderer and the registry it uses while active; its scrolls and
	// signs have already gone with the interface (Game's shutdown), so what is left is plain state
	Locator::temple::reset();

	// The audio resources have been cleared and all sounds have been stopped (audio::Shutdown): the channels' sources,
	// the wave buffers and the OpenAL context go
	// the films before the device: the player's sound goes with it
	Locator::videoSystem::reset();
	if (Locator::audioState::has_value())
	{
		audio::device::Close();
	}

	Locator::rendereringSystem::reset();
	Locator::pickingSystem::reset();
	Locator::tattooEditorSystem::reset();
	// the cave asks Creature Mode for the player's creature
	Locator::creatureCaveSystem::reset();
	// before the camera, whose model it may hold
	Locator::creatureModeSystem::reset();
	Locator::editorSystem::reset();
	Locator::cameraBookmarkSystem::reset();
	Locator::cameraPathSystem::reset();
	Locator::livingActionSystem::reset();
	Locator::townSystem::reset();
	Locator::handGrabSystem::reset();
	Locator::handSystem::reset();
	Locator::pathfindingSystem::reset();
	Locator::creatureLocomotionSystem::reset();
	Locator::terrainSystem::reset();
	if (process)
	{
		Locator::filesystem::reset();
	}
	Locator::gameActionSystem::reset();

	Locator::oceanSystem::reset();
	Locator::skySystem ::reset();
	Locator::debugGui::reset();
	// after the debug GUI, whose menu reads it
	if (process)
	{
		Locator::modLoader::reset();
	}
	// The creatures' systems may go before the registry: none connects a registry signal, and no creature component's
	// destructor reaches a service
	Locator::creatureFightSystem::reset();
	Locator::creatureFizzSystem::reset();
	Locator::leashSystem::reset();
	Locator::creatureMindSystem::reset();
	Locator::creaturePhysiologySystem::reset();
	Locator::creatureSkinSystem::reset();
	Locator::creatureHairSystem::reset();
	Locator::footprintSystem::reset();
	Locator::creatureAudioSystem::reset();
	Locator::creatureObjectActionSystem::reset();
	Locator::creatureHandSystem::reset();
	Locator::creatureAnimationSystem::reset();
	Locator::entitiesRegistry::reset();
	// after the registry: an entity going away may still reach the stores
	Locator::resourceStoreSystem::reset();
	// the land's map, after the entities it holds; the next land load makes it again
	if (!process)
	{
		Locator::entitiesMap::reset();
	}
	// no storm may queue a strike once the particle system is gone
	if (Locator::weatherSystem::has_value())
	{
		weather::storms::SetForkCallback({});
	}
	// after the registry: an entity's destruction may still close its spot visual
	Locator::particleSystem::reset();
	Locator::mistSystem::reset();
	Locator::cloudSystem::reset();
	Locator::villageLightSystem::reset();
	Locator::renderFrameSystem::reset();
	Locator::skyFrameSystem::reset();
	Locator::landAvoidSystem::reset();
	Locator::debugHooks::reset();
	Locator::scriptState::reset();
	Locator::scriptObjects::reset();
	Locator::cameraHelpSystem::reset();
	Locator::cameraZoneSystem::reset();
	Locator::handTapRegistry::reset();
	Locator::abodeKnockSystem::reset();
	Locator::abodeSoundSystem::reset();
	Locator::worldEffects::reset();
	Locator::waterRingSystem::reset();
	Locator::chimneySmokeSystem::reset();
	Locator::vegetation::reset();
	Locator::fieldSystem::reset();
	Locator::rewardSystem::reset();
	Locator::explosionSystem::reset();
	Locator::audio::reset();
	Locator::audioState::reset();
	Locator::soundTagSystem::reset();
	Locator::dynamicsSystem::reset();
	Locator::buildingDamageSystem::reset();
	Locator::weatherSystem::reset();
	Locator::snowSystem::reset();
	Locator::snowfallSystem::reset();
	Locator::rainSystem::reset();
	// After the registry: anything an entity's destruction asks of these still finds them
	Locator::villagerFields::reset();
	Locator::villagerFishFarms::reset();
	Locator::villagerBuildingSites::reset();
	Locator::villagerStores::reset();
	Locator::villagerTentQueries::reset();
	Locator::villagerRules::reset();
	Locator::villagerWorldQueries::reset();
	Locator::villagerWorshipCheck::reset();
	Locator::villagerChildFactory::reset();
	Locator::villagerDiscipleJobs::reset();
	Locator::townCellObjects::reset();
	Locator::mapShapeProvider::reset();
	Locator::meshBoxProvider::reset();
	Locator::glintTargets::reset();
	Locator::creatureSpellBodies::reset();
	Locator::toBeDeletedSystem::reset();
	Locator::objectCreationIndexSystem::reset();
	Locator::mapCellsSystem::reset();
	// before the event manager, which outlives its handler
	Locator::objectDrawListSystem::reset();
	Locator::animatedStaticSystem::reset();
	Locator::reactionSystem::reset();
	Locator::fireSystem::reset();
	Locator::forestSystem::reset();
	Locator::treeSystem::reset();
	Locator::landBalanceSystem::reset();
	Locator::animalSystem::reset();
	Locator::magicShieldSystem::reset();
	Locator::teleportSystem::reset();
	Locator::tornadoSystem::reset();
	Locator::vortexSystem::reset();
	// the profile played stays through a restart and a new game: only the program's end takes it
	if (process)
	{
		Locator::playerProfileSystem::reset();
	}
	Locator::saveGame::reset();
	Locator::helpSpeechSystem::reset();
	Locator::helpTextSystem::reset();
	Locator::introSystem::reset();
	Locator::highDetailSystem::reset();
	Locator::walkPathSystem::reset();
	Locator::sharkSystem::reset();
	Locator::danceSystem::reset();
	Locator::helpProfileSystem::reset();
	Locator::creatureHelpSystem::reset();
	Locator::tipBubbleSystem::reset();
	Locator::advisorSystem::reset();
	Locator::townStateSystem::reset();
	Locator::townDesireSystem::reset();
	Locator::villagerStateSystem::reset();
	Locator::miracleFxSystem::reset();
	Locator::gestureEvents::reset();
	Locator::gestureSystem::reset();
	Locator::routePlanStateSystem::reset();
	// after the registry: a spell's sink may still be asked for while its effect goes. It releases its stores spells
	// first, then the magic objects, the hand magic state and the falling spell
	Locator::magicSystem::reset();
	Locator::templeExteriorSystem::reset();
	Locator::templeDestructionSystem::reset();
	Locator::computerPlayerSystem::reset();
	Locator::worshipState::reset();
	Locator::villageTotemSystem::reset();
	Locator::artefactSystem::reset();
	Locator::heartbeatSystem::reset();
	Locator::gameStatsSystem::reset();
	Locator::alignmentSystem::reset();
	// the players' alignment, after the alignment service over it: a new game starts with none
	if (!process)
	{
		Locator::playerSystem::reset();
	}
	Locator::influenceSystem::reset();
	// after the hand system: its destructor clears its packet handlers
	Locator::inputState::reset();
	if (process)
	{
		Locator::rendererInterface::reset();
		Locator::windowing::reset();
	}
	Locator::events::reset();
	Locator::camera::reset();
	if (process)
	{
		Locator::config::reset();
		Locator::infoConstants::reset();
		Locator::profiler::reset();
	}
	Locator::gameRandom::reset();
	// Last: the audio device, closed above, was the other thread reading the clock
	if (process)
	{
		Locator::time::reset();
	}

	Locator::vm::reset();
	// after the VM, whose task-stop callback is their only other user (the VM stops no task as it goes)
	Locator::dialogueControlSystem::reset();
	Locator::scriptControlSystem::reset();
	// Last, as the Game's own members went after all of the above (after the renderer), in the reverse order
	Locator::screenshotRequest::reset();
	Locator::tutorialSkipSystem::reset();
	Locator::mapScriptSystem::reset();
	Locator::dayNightClock::reset();
	Locator::cinematicDirectorSystem::reset();
	// Last of all, with the process: anything above may still warn as it goes
	if (process)
	{
		Locator::logOnce::reset();
	}
}
