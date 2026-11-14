/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The services of the game on the player's computer: the window, the renderer, the debug GUI, the audio and the
// services made among them, and the drawing of the world

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <string>

#include <spdlog/spdlog.h>

#include "Audio/AudioManager.h"
#include "Audio/Device/Device.h"
#include "Common/GameRandom.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Systems/Implementations/AbodeKnockSystem.h"
#include "ECS/Systems/Implementations/AbodeSoundSystem.h"
#include "ECS/Systems/Implementations/AudioState.h"
#include "ECS/Systems/Implementations/BuildingDamageSystem.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/CameraZoneSystem.h"
#include "ECS/Systems/Implementations/ChimneySmokeSystem.h"
#include "ECS/Systems/Implementations/CloudSystem.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"
#include "ECS/Systems/Implementations/FieldSystem.h"
#include "ECS/Systems/Implementations/HandTapRegistry.h"
#include "ECS/Systems/Implementations/LandAvoidSystem.h"
#include "ECS/Systems/Implementations/LandSystem.h"
#include "ECS/Systems/Implementations/MistSystem.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/RainSystem.h"
#include "ECS/Systems/Implementations/RenderFrameSystem.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/RewardSystem.h"
#include "ECS/Systems/Implementations/ScriptObjectsSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/SkyFrameSystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/Implementations/SnowfallSystem.h"
#include "ECS/Systems/Implementations/SoundTagSystem.h"
#include "ECS/Systems/Implementations/VegetationSystem.h"
#include "ECS/Systems/Implementations/VillageLightSystem.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/WeatherLoop.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Graphics/GameFont.h"
#include "Graphics/RendererInterface.h"
#include "Locator.h"
#include "Particles/Rules/LightningStrike.h"
#include "Visual/DetailLevel.h"
#include "Windowing/Sdl2WindowingSystem.h"

using openblack::debug::gui::DebugGuiInterface;
using openblack::ecs::systems::CameraHelpSystem;
using openblack::ecs::systems::RenderingSystem;
using openblack::graphics::RendererInterface;
using openblack::windowing::DisplayMode;
using openblack::windowing::Sdl2WindowingSystem;

void openblack::InitializeWindow(const std::string& title, int width, int height, DisplayMode displayMode, uint32_t extraFlags)
{
	Locator::windowing::emplace<Sdl2WindowingSystem>(title, width, height, displayMode, extraFlags);
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
	// the camera zones, which lived in the script state too, made once: the map's clear and the scripts' restart clear
	// them
	Locator::cameraZoneSystem::emplace<ecs::systems::CameraZoneSystem>();
	Locator::landSystem::emplace<ecs::systems::LandSystem>();
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

void openblack::InitializeClient()
{
	// The player's input, the drawing of the world, the editor and the films
	InitializePlayerInput();
	Locator::rendereringSystem::emplace<RenderingSystem>();
	InitializeOnlineAndTools();
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
