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
#include <type_traits>

#define LOCATOR_IMPLEMENTATIONS

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/Clouds.h"
#include "3D/Implementations/LandIsland.h"
#include "3D/Implementations/Ocean.h"
#include "3D/Implementations/Sky.h"
#include "3D/Implementations/TempleInterior.h"
#include "3D/Implementations/UnloadedIsland.h"
#include "3D/LandData.h"
#include "Audio/AudioManager.h"
#include "Audio/Device/Device.h"
#include "CHLApi.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomProduction.h"
#include "Common/RandomNumberManagerProduction.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/CreatureMimic.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/LandReseat.h"
#include "ECS/MapProduction.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AlignmentSystem.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/AudioState.h"
#include "ECS/Systems/Implementations/CameraBookmarkSystem.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "ECS/Systems/Implementations/ChimneySmokeSystem.h"
#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"
#include "ECS/Systems/Implementations/CloudSystem.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "ECS/Systems/Implementations/CreatureCaveSystem.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureModeSystem.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/CreatureReactions.h"
#include "ECS/Systems/Implementations/CreatureSkinSystem.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "ECS/Systems/Implementations/DrawListSystem.h"
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
#include "ECS/Systems/Implementations/InfluenceSystem.h"
#include "ECS/Systems/Implementations/InputState.h"
#include "ECS/Systems/Implementations/LandAvoidSystem.h"
#include "ECS/Systems/Implementations/LandBalanceSystem.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/MapScriptSystem.h"
#include "ECS/Systems/Implementations/MapShapeProvider.h"
#include "ECS/Systems/Implementations/MeshBoxProvider.h"
#include "ECS/Systems/Implementations/MiracleFxSystem.h"
#include "ECS/Systems/Implementations/MistSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/PathfindingSystem.h"
#include "ECS/Systems/Implementations/PhysicsObjectsSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/RainSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "ECS/Systems/Implementations/RenderFrameSystem.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/RoutePlanStateSystem.h"
#include "ECS/Systems/Implementations/ScreenshotRequestSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/SkyFrameSystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/Implementations/SnowfallSystem.h"
#include "ECS/Systems/Implementations/SoundTagSystem.h"
#include "ECS/Systems/Implementations/TeleportSystem.h"
#include "ECS/Systems/Implementations/TempleExteriorSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "ECS/Systems/Implementations/ToBeDeletedSystem.h"
#include "ECS/Systems/Implementations/TornadoSystem.h"
#include "ECS/Systems/Implementations/TownCellObjects.h"
#include "ECS/Systems/Implementations/TownDesireSystem.h"
#include "ECS/Systems/Implementations/TownStateSystem.h"
#include "ECS/Systems/Implementations/TownSystem.h"
#include "ECS/Systems/Implementations/TreeSystem.h"
#include "ECS/Systems/Implementations/VegetationSystem.h"
#include "ECS/Systems/Implementations/VideoSystem.h"
#include "ECS/Systems/Implementations/VillageLightSystem.h"
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
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/Systems/Implementations/WorshipState.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownDesire.h"
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
using openblack::TimeSystem;
using openblack::UnloadedIsland;
using openblack::chlapi::CHLApi;
using openblack::debug::gui::DebugGuiInterface;
using openblack::ecs::MapProduction;
using openblack::ecs::Registry;
using openblack::ecs::systems::AlignmentSystem;
using openblack::ecs::systems::AnimalSystem;
using openblack::ecs::systems::CameraBookmarkSystem;
using openblack::ecs::systems::CameraHelpSystem;
using openblack::ecs::systems::CameraPathSystem;
using openblack::ecs::systems::CinematicDirectorSystem;
using openblack::ecs::systems::CreatureAnimationSystem;
using openblack::ecs::systems::CreatureAudioSystem;
using openblack::ecs::systems::CreatureCaveSystem;
using openblack::ecs::systems::CreatureFightSystem;
using openblack::ecs::systems::CreatureHairSystem;
using openblack::ecs::systems::CreatureHandSystem;
using openblack::ecs::systems::CreatureLocomotionSystem;
using openblack::ecs::systems::CreatureMindSystem;
using openblack::ecs::systems::CreatureModeSystem;
using openblack::ecs::systems::CreatureObjectActionSystem;
using openblack::ecs::systems::CreaturePhysiologySystem;
using openblack::ecs::systems::CreatureSkinSystem;
using openblack::ecs::systems::DayNightClockSystem;
using openblack::ecs::systems::DrawListSystem;
using openblack::ecs::systems::DynamicsSystem;
using openblack::ecs::systems::EditorSystem;
using openblack::ecs::systems::FireSystem;
using openblack::ecs::systems::FootprintSystem;
using openblack::ecs::systems::ForestSystem;
using openblack::ecs::systems::GameStatsSystem;
using openblack::ecs::systems::GestureEventsInterface;
using openblack::ecs::systems::GestureSystem;
using openblack::ecs::systems::GlintTargets;
using openblack::ecs::systems::HandSystem;
using openblack::ecs::systems::InfluenceSystem;
using openblack::ecs::systems::InputState;
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
using openblack::ecs::systems::PathfindingSystem;
using openblack::ecs::systems::PlayerSystem;
using openblack::ecs::systems::ReactionSystem;
using openblack::ecs::systems::RenderingSystem;
using openblack::ecs::systems::RoutePlanStateSystem;
using openblack::ecs::systems::ScreenshotRequestSystem;
using openblack::ecs::systems::TeleportSystem;
using openblack::ecs::systems::ToBeDeletedSystem;
using openblack::ecs::systems::TornadoSystem;
using openblack::ecs::systems::TownCellObjects;
using openblack::ecs::systems::TownDesireSystem;
using openblack::ecs::systems::TownStateSystem;
using openblack::ecs::systems::TownSystem;
using openblack::ecs::systems::TreeSystem;
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
using openblack::ecs::systems::WorshipState;
using openblack::graphics::RendererInterface;
using openblack::input::GameActionMap;
using openblack::lhvm::LHVM;
using openblack::resources::Resources;
using openblack::windowing::DisplayMode;
using openblack::windowing::Sdl2WindowingSystem;

namespace
{
/// Today's date by the local clock, which only the creatures' footprints read (once per land)
openblack::ecs::systems::CalendarDate LocalDate()
{
	const auto now = std::time(nullptr);
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
	// not in the tests' services: without a Game the test hooks request no screenshot
	Locator::screenshotRequest::emplace<ScreenshotRequestSystem>();
}

bool openblack::InitializeEngine(GraphicsBackend backend, bool vsync, std::array<uint8_t, 256>& fontHashPermutation) noexcept
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "EnTT version: {}", ENTT_VERSION);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), GLM_VERSION_COMPLETE);

	Locator::profiler::emplace();
	// OPENBLACK_PROFILE=<seconds>: log each stage's average / worst time per frame every <seconds>.
	if (const char* profile = std::getenv("OPENBLACK_PROFILE"); profile != nullptr)
	{
		Locator::profiler::value().SetSummaryInterval(std::max(0.5f, static_cast<float>(std::atof(profile))));
	}

	Locator::rendererInterface::reset(RendererInterface::Create(backend, vsync).release());
	if (!Locator::rendererInterface::has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to create renderer");
		return false;
	}
	// Opening the renderer starts the engine timer (the frame delta, EngineMs)
	game_clock::StartEngineTimer();
	Locator::debugGui::reset(DebugGuiInterface::Create(graphics::RenderPass::ImGui).release());
	Locator::events::emplace<EventManager>();

#if __ANDROID__
	Locator::filesystem::emplace<AndroidFileSystem>();
#else
	Locator::filesystem::emplace<DefaultFileSystem>();
#endif
	Locator::rng::emplace<RandomNumberManagerProduction>();
	Locator::gameRandom::emplace<GameRandomProduction>();
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
	// the camera help, which lived in the script state: made and gone with it
	Locator::cameraHelpSystem::emplace<CameraHelpSystem>();
	Locator::handTapRegistry::emplace<ecs::systems::HandTapRegistry>();
	Locator::videoSystem::emplace<ecs::systems::VideoSystem>();
	Locator::worldEffects::emplace<ecs::systems::WorldEffects>();
	Locator::waterRingSystem::emplace<ecs::systems::WaterRingSystem>();
	Locator::chimneySmokeSystem::emplace<ecs::systems::ChimneySmokeSystem>();
	Locator::vegetation::emplace<ecs::systems::VegetationSystem>();
	Locator::fieldSystem::emplace<ecs::systems::FieldSystem>();
	Locator::explosionSystem::emplace<ecs::systems::ExplosionSystem>();
	// a test's fake audio service stays
	if (!Locator::audio::has_value())
	{
		Locator::audio::emplace<audio::AudioManager>();
	}
	Locator::audioState::emplace<ecs::systems::AudioState>();
	Locator::soundTagSystem::emplace<ecs::systems::SoundTagSystem>();
	Locator::physicsObjectsSystem::emplace<ecs::systems::PhysicsObjectsSystem>();
	Locator::weatherSystem::emplace<ecs::systems::WeatherSystem>();
	// A storm's fork lightning queues its point for the particle system's strike effect, as the particle system's
	// start installs it in the original
	weather::storms::SetForkCallback([](const weather::storms::Storm& /*storm*/, const glm::vec3& point, float radius) {
		psys::lightning_strike::Queue(point, radius);
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
	// InitializeGame keeps it with the resources
	fontHashPermutation = graphics::GameFont::MakeHashPermutation([] { return game_random::crt::Rand(); });
	// The audio system's wave device: without one nothing plays
	audio::device::Open();

	Locator::chlapi::emplace<CHLApi>();
	Locator::vm::emplace<LHVM>();
	return true;
}

bool openblack::InitializeGame(const std::array<uint8_t, 256>& fontHashPermutation) noexcept
{
	Locator::terrainSystem::emplace<UnloadedIsland>();
	Locator::resources::emplace<Resources>();
	Locator::resources::value().SetFontHashPermutation(fontHashPermutation);
	// The players' alignment lasts the whole game, across lands: an existing player system (a test's, or one a
	// previous game left, as it is not reset at shutdown) is kept
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
	// Every player's GameStats and the shared statics, kept for the whole game (the game never clears them)
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
	Locator::gameActionSystem::emplace<GameActionMap>();
	Locator::rendereringSystem::emplace<RenderingSystem>();
	Locator::entitiesRegistry::emplace<Registry>();
	Locator::handSystem::emplace<HandSystem>();
	Locator::temple::emplace<TempleInterior>();
	Locator::oceanSystem::emplace<Ocean>();
	Locator::skySystem::emplace<Sky>();
	// The in-game editor (F2), kept for the whole game: while it drives the camera it holds the player's camera model
	Locator::editorSystem::emplace<EditorSystem>();
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
	// the engine, before this) until it goes. A test's own is kept
	if (!Locator::drawListSystem::has_value())
	{
		Locator::drawListSystem::emplace<DrawListSystem>(Locator::events::has_value() ? &Locator::events::value() : nullptr);
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
	// player's creature is the one the leash service knows
	Locator::creatureModeSystem::emplace<CreatureModeSystem>([](PlayerNames player) -> std::optional<entt::entity> {
		return Locator::leashSystem::has_value() ? Locator::leashSystem::value().PlayersCreature(player) : std::nullopt;
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
	Locator::creatureFightSystem::emplace<CreatureFightSystem>();
	// the creature's physics class (thrown at, mass 1000), as the living things register theirs
	openblack::ecs::creature_physics::RegisterPhysicsHandlers();
	// The game's handlers of what the villagers' deaths and the towns' belief and desires report: help sprites,
	// tooltips, the smoke and the souls; the players' deeds their creatures watch; and the things that follow the land
	// when the vortex or the temple flattens it
	if (Locator::events::has_value())
	{
		auto& events = Locator::events::value();
		openblack::ecs::villager::AddDeathEventHandlers(events);
		openblack::ecs::town_belief::AddBeliefEventHandlers(events);
		openblack::ecs::town_desire::AddDesireEventHandlers(events);
		openblack::ecs::creature_mimic::AddMimicEventHandlers(events);
		openblack::ecs::land_reseat::AddLandReseatEventHandlers(events);
	}

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
	Locator::entitiesMap::emplace<MapProduction>();
	Locator::dynamicsSystem::emplace<DynamicsSystem>();
	Locator::livingActionSystem::emplace<LivingActionSystem>();
	Locator::townSystem::emplace<TownSystem>();
	Locator::pathfindingSystem::emplace<PathfindingSystem>();
	Locator::creatureLocomotionSystem::emplace<CreatureLocomotionSystem>();
	Locator::cameraBookmarkSystem::emplace<CameraBookmarkSystem>();
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

void openblack::InitializeLevel(const std::filesystem::path& path)
{
	InitializeLevelWith(path);
}

void openblack::InitializeLevel(const LandData& land)
{
	InitializeLevelWith(land);
}

void openblack::ShutDownServices()
{
	// the particle effects first, as a map load clears them: their destructors still find the services they use
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().Reset();
	}
	// Manually delete the assets here before BGFX renderer clears its buffers resulting in invalid handles in our assets
	if (Locator::resources::has_value())
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
	audio::device::Close();

	Locator::rendereringSystem::reset();
	Locator::dynamicsSystem::reset();
	// the cave asks Creature Mode for the player's creature
	Locator::creatureCaveSystem::reset();
	// before the camera, whose model it may hold
	Locator::creatureModeSystem::reset();
	Locator::editorSystem::reset();
	Locator::cameraBookmarkSystem::reset();
	Locator::cameraPathSystem::reset();
	Locator::livingActionSystem::reset();
	Locator::townSystem::reset();
	Locator::handSystem::reset();
	Locator::pathfindingSystem::reset();
	Locator::creatureLocomotionSystem::reset();
	Locator::terrainSystem::reset();
	Locator::filesystem::reset();
	Locator::gameActionSystem::reset();

	Locator::oceanSystem::reset();
	Locator::skySystem ::reset();
	Locator::debugGui::reset();
	// after the debug GUI, whose menu reads it
	Locator::modLoader::reset();
	// The creatures' systems may go before the registry: none connects a registry signal, and no creature component's
	// destructor reaches a service
	Locator::creatureFightSystem::reset();
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
	Locator::cameraHelpSystem::reset();
	Locator::handTapRegistry::reset();
	Locator::worldEffects::reset();
	Locator::waterRingSystem::reset();
	Locator::chimneySmokeSystem::reset();
	Locator::vegetation::reset();
	Locator::fieldSystem::reset();
	Locator::explosionSystem::reset();
	Locator::audio::reset();
	Locator::audioState::reset();
	Locator::soundTagSystem::reset();
	Locator::physicsObjectsSystem::reset();
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
	Locator::toBeDeletedSystem::reset();
	Locator::objectCreationIndexSystem::reset();
	Locator::mapCellsSystem::reset();
	// before the event manager, which outlives its handler
	Locator::drawListSystem::reset();
	Locator::reactionSystem::reset();
	Locator::fireSystem::reset();
	Locator::forestSystem::reset();
	Locator::treeSystem::reset();
	Locator::landBalanceSystem::reset();
	Locator::animalSystem::reset();
	Locator::magicShieldSystem::reset();
	Locator::teleportSystem::reset();
	Locator::tornadoSystem::reset();
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
	Locator::worshipState::reset();
	Locator::gameStatsSystem::reset();
	Locator::alignmentSystem::reset();
	Locator::influenceSystem::reset();
	// after the hand system: its destructor clears its packet handlers
	Locator::inputState::reset();
	Locator::rendererInterface::reset();
	Locator::windowing::reset();
	Locator::events::reset();
	Locator::camera::reset();
	Locator::config::reset();
	Locator::infoConstants::reset();
	Locator::profiler::reset();
	Locator::gameRandom::reset();
	// Last: the audio device, closed above, was the other thread reading the clock
	Locator::time::reset();

	Locator::vm::reset();
	// Last, as the Game's own members went after all of the above (after the renderer), in the reverse order
	Locator::screenshotRequest::reset();
	Locator::mapScriptSystem::reset();
	Locator::dayNightClock::reset();
	Locator::cinematicDirectorSystem::reset();
}
