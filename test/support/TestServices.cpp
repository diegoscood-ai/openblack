/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TestServices.h"

#include <cstdlib>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManager.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Systems/Implementations/AudioState.h"
#include "ECS/Systems/Implementations/BuildingDamageSystem.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/CameraStack.h"
#include "ECS/Systems/Implementations/ChimneySmokeSystem.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"
#include "ECS/Systems/Implementations/FieldSystem.h"
#include "ECS/Systems/Implementations/GameStatsSystem.h"
#include "ECS/Systems/Implementations/HandTapRegistry.h"
#include "ECS/Systems/Implementations/InputState.h"
#include "ECS/Systems/Implementations/LandAvoidSystem.h"
#include "ECS/Systems/Implementations/LogOnceService.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/MapScriptSystem.h"
#include "ECS/Systems/Implementations/MapShapeProvider.h"
#include "ECS/Systems/Implementations/MeshBoxProvider.h"
#include "ECS/Systems/Implementations/MiracleFxSystem.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/RainSystem.h"
#include "ECS/Systems/Implementations/RenderFrameSystem.h"
#include "ECS/Systems/Implementations/RoutePlanStateSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/SkyFrameSystem.h"
#include "ECS/Systems/Implementations/SoundTagSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "ECS/Systems/Implementations/TownCellObjects.h"
#include "ECS/Systems/Implementations/TownStateSystem.h"
#include "ECS/Systems/Implementations/VegetationSystem.h"
#include "ECS/Systems/Implementations/VillagerBuildingSites.h"
#include "ECS/Systems/Implementations/VillagerFields.h"
#include "ECS/Systems/Implementations/VillagerFishFarms.h"
#include "ECS/Systems/Implementations/VillagerStateSystem.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerTentQueries.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/Systems/Implementations/WorshipState.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"

namespace openblack::test
{
namespace
{
/// The particle service of the tests: the game's own, which it holds and passes every call to, but for a spell's
/// effect whose particle file the tests have not loaded, which still starts, as an effect the engine does not know
/// (nothing finds, runs or draws it, and its first step ends it). A miracle whose effect cannot start is not cast, as
/// the game's; the tests have no particle files, so without this none of their miracles would be cast
class TestParticleSystem final: public ecs::systems::ParticleSystemInterface
{
public:
	EffectId StartForSpell(std::string_view file, glm::vec3 origin, glm::vec3 direction, float magnitude,
	                       particles::SpellSink* sink, bool synced) override
	{
		const auto id = _game.StartForSpell(file, origin, direction, magnitude, sink, synced);
		return id != k_NoEffect ? id : ++_standIn;
	}
	EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced) override
	{
		return _game.Start(file, origin, magnitude, synced);
	}
	EffectId Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced) override
	{
		return _game.Start(type, origin, magnitude, synced);
	}
	bool ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds) override
	{
		return _game.ProcessForSpell(id, info, seconds);
	}
	bool ProcessByFrame(EffectId id, float seconds) override { return _game.ProcessByFrame(id, seconds); }
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                             float magnitude) override
	{
		return _game.StartSpotVisual(type, position, turns, owner, magnitude);
	}
	void CloseDownSpotVisual(entt::entity object) override { _game.CloseDownSpotVisual(object); }
	void DeleteSpotVisual(entt::entity object) override { _game.DeleteSpotVisual(object); }
	void SetOrigin(EffectId id, glm::vec3 origin) override { _game.SetOrigin(id, origin); }
	void SetProcessInfo(EffectId id, const particles::ProcessInfo& info) override { _game.SetProcessInfo(id, info); }
	void SetMagnitude(EffectId id, float magnitude) override { _game.SetMagnitude(id, magnitude); }
	void SetPlayer(EffectId id, int player) override { _game.SetPlayer(id, player); }
	void SetDrawPath(EffectId id, particles::DrawPath path) override { _game.SetDrawPath(id, path); }
	void SetDrawOffset(EffectId id, glm::vec3 offset) override { _game.SetDrawOffset(id, offset); }
	[[nodiscard]] const particles::ShieldSphere* FindShield(glm::vec3 point, float margin) const override
	{
		return _game.FindShield(point, margin);
	}
	[[nodiscard]] size_t GetSoundCount() const override { return _game.GetSoundCount(); }
	void SetDrawn(EffectId id, bool drawn) override { _game.SetDrawn(id, drawn); }
	void SetInTemple(EffectId id) override { _game.SetInTemple(id); }
	void AddTarget(EffectId id, entt::entity target) override { _game.AddTarget(id, target); }
	void AddTargetPosition(EffectId id, glm::vec3 position) override { _game.AddTargetPosition(id, position); }
	void AddSpotVisualTarget(entt::entity visual, glm::vec3 position) override { _game.AddSpotVisualTarget(visual, position); }
	void AddSpotVisualTargetObject(entt::entity visual, entt::entity target) override
	{
		_game.AddSpotVisualTargetObject(visual, target);
	}
	void SetSpotVisualPlayer(entt::entity visual, int player) override { _game.SetSpotVisualPlayer(visual, player); }
	void SetSpotVisualStrength(entt::entity visual, float strength) override { _game.SetSpotVisualStrength(visual, strength); }
	[[nodiscard]] std::optional<float> GetSpotVisualStrength(entt::entity visual) const override
	{
		return _game.GetSpotVisualStrength(visual);
	}
	bool SetSpotVisualMagnitude(entt::entity visual, float magnitude) override
	{
		return _game.SetSpotVisualMagnitude(visual, magnitude);
	}
	void AddPlasma(EffectId id, const particles::PlasmaCommand& command) override { _game.AddPlasma(id, command); }
	void AddSpotVisualPlasma(entt::entity visual, const particles::PlasmaCommand& command) override
	{
		_game.AddSpotVisualPlasma(visual, command);
	}
	void StartMultiPickup(ParticleType type) override { _game.StartMultiPickup(type); }
	void StopMultiPickup() override { _game.StopMultiPickup(); }
	void StartMultiPutdown(ParticleType type, float lifeSeconds) override { _game.StartMultiPutdown(type, lifeSeconds); }
	void AddBeliefSprite(const particles::BeliefSprite& sprite) override { _game.AddBeliefSprite(sprite); }
	void AddHandManaPathSpark(const particles::mana_path::Spark& spark) override { _game.AddHandManaPathSpark(spark); }
	void AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail) override { _game.AddGestureTrail(trail); }
	void UpdateFrame(float gameSeconds, const HandFrame& hand) override { _game.UpdateFrame(gameSeconds, hand); }
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override { _game.AddLightSheet(sheet); }
	void AddOwnedLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override { _game.AddOwnedLightSheet(sheet); }
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> LightSheets() override { return _game.LightSheets(); }
	void CloseDown(EffectId id) override { _game.CloseDown(id); }
	void Delete(EffectId id) override { _game.Delete(id); }
	[[nodiscard]] bool IsRunning(EffectId id) const override { return _game.IsRunning(id); }
	[[nodiscard]] particles::Effect* Find(EffectId id) override { return _game.Find(id); }
	void ProcessTurn() override { _game.ProcessTurn(); }
	void Reset() override { _game.Reset(); }
	void CollectDrawFrame(float turnFraction, particles::draw::Frame& frame, bool inTemple) const override
	{
		_game.CollectDrawFrame(turnFraction, frame, inTemple);
	}
	[[nodiscard]] DrawStats GetDrawStats() const override { return _game.GetDrawStats(); }
	[[nodiscard]] std::vector<EffectInfo> GetEffects() const override { return _game.GetEffects(); }
	[[nodiscard]] std::vector<std::string> GetFileNames() const override { return _game.GetFileNames(); }
	void SetPaused(bool paused) override { _game.SetPaused(paused); }
	[[nodiscard]] bool IsPaused() const override { return _game.IsPaused(); }
	[[nodiscard]] particles::manager::State& GetState() noexcept override { return _game.GetState(); }

protected:
	[[nodiscard]] entt::registry& ModuleStore() noexcept override { return _modules; }

private:
	/// The files' own state, which the engine reaches through this service, declared first so that it goes after the
	/// effects that point into it
	entt::registry _modules;
	ecs::systems::ParticleSystem _game;
	/// Far past the ids the engine gives, which start at 1
	EffectId _standIn {0x40000000};
};
} // namespace
} // namespace openblack::test

void openblack::test::EmplaceTestServices()
{
	using namespace openblack::ecs::systems;
	// The game's hidden state, as the program starts it. A service belongs here only when (1) it replaces global state
	// that always existed without anyone setting it up, (2) it is set up empty, exactly as production starts it, and (3)
	// it has no production behaviour a test should fake. Defaults of production behaviour do not belong here.
	// The game's loggers, which the program always makes before anything runs (Game's logging subsystems): a test
	// that has not made one gets it writing nowhere, so code that logs through it runs alone as with the rest; a test's
	// own logger of that name is kept
	for (const auto* name : {"game", "input", "graphics", "scripting", "audio", "pathfinding", "ai"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::register_logger(std::make_shared<spdlog::logger>(name, std::make_shared<spdlog::sinks::null_sink_mt>()));
		}
	}
	// The random streams (both seeds 0, the C runtime seed 1, no hooks) and the clock (reset)
	Locator::gameRandom::emplace<GameRandomTesting>();
	Locator::time::emplace<TimeSystem>();
	// the warnings logged once, none yet
	Locator::logOnce::emplace<LogOnceService>();
	// the particle system's state, empty
	Locator::particleSystem::emplace<TestParticleSystem>();
	// the RenderFrame state, empty
	Locator::renderFrameSystem::emplace<RenderFrameSystem>();
	// the SkyFrame state, empty
	Locator::skyFrameSystem::emplace<SkyFrameSystem>();
	// the LandAvoid state, empty
	Locator::landAvoidSystem::emplace<LandAvoidSystem>();
	// the debug hooks' state, empty
	Locator::debugHooks::emplace<DebugHooks>();
	// the script side's state, empty
	Locator::scriptState::emplace<ScriptState>();
	// the camera's stack of modes, empty, as the script state held it
	Locator::cameraStack::emplace<CameraStack>();
	// the camera help at its start values
	Locator::cameraHelpSystem::emplace<CameraHelpSystem>();
	// the hand-tap handlers, none yet
	Locator::handTapRegistry::emplace<HandTapRegistry>();
	// the world effects modules' state, empty
	Locator::worldEffects::emplace<WorldEffects>();
	// the world effects services, empty
	Locator::waterRingSystem::emplace<WaterRingSystem>();
	Locator::chimneySmokeSystem::emplace<ChimneySmokeSystem>();
	Locator::vegetation::emplace<VegetationSystem>();
	Locator::fieldSystem::emplace<FieldSystem>();
	Locator::explosionSystem::emplace<ExplosionSystem>();
	// the audio modules' state, empty
	Locator::audio::emplace<audio::AudioManager>();
	Locator::audioState::emplace<AudioState>();
	// the sound tags, none yet
	Locator::soundTagSystem::emplace<SoundTagSystem>();
	// the weather's state, empty
	Locator::weatherSystem::emplace<WeatherSystem>();
	// the rain, its streaks not placed yet
	Locator::rainSystem::emplace<RainSystem>();
	// the physics objects' state, empty (the base game's constants)
	Locator::dynamicsSystem::emplace<DynamicsSystem>();
	// the broken buildings' service (it holds no state)
	Locator::buildingDamageSystem::emplace<BuildingDamageSystem>();
	// the players' alignment and the magic of the players without an entity, at their new game values
	Locator::playerSystem::emplace<PlayerSystem>();
	// the miracles' service, which owns the stores of the spells and their sinks, the map shields and the fireballs, the
	// hand's magic modules' state and the falling spell, all empty
	Locator::magicSystem::emplace<MagicSystem>();
	// the shields' service over those lists (it holds no state)
	Locator::magicShieldSystem::emplace<MagicShieldSystem>();
	// the towns' and the villagers' shared lists, empty
	Locator::townStateSystem::emplace<TownStateSystem>();
	Locator::villagerStateSystem::emplace<VillagerStateSystem>();
	// the miracles' looks, which keep their state in the hand's magic state
	Locator::miracleFxSystem::emplace<MiracleFxSystem>();
	// the route planner's callbacks, hook and holder pool, empty
	Locator::routePlanStateSystem::emplace<RoutePlanStateSystem>();
	// the worship modules' state, empty
	Locator::worshipState::emplace<WorshipState>();
	// the input modules' state, empty (no packet handlers)
	Locator::inputState::emplace<InputState>();
	// every player's GameStats and the shared statics, at their start values
	Locator::gameStatsSystem::emplace<GameStatsSystem>();
	// the map script's globals at their start values, where the tests without a Game had a static copy (a Game made by
	// a test puts in its own). They hold the land balance's values too, so it is emplaced before any land balance
	// (WorldSystems.h) and reset after it
	Locator::mapScriptSystem::emplace<MapScriptSystem>();
}

void openblack::test::ResetTestServices()
{
	Locator::mapScriptSystem::reset();
	Locator::gameStatsSystem::reset();
	Locator::inputState::reset();
	Locator::worshipState::reset();
	Locator::routePlanStateSystem::reset();
	Locator::miracleFxSystem::reset();
	Locator::villagerStateSystem::reset();
	Locator::townStateSystem::reset();
	Locator::magicShieldSystem::reset();
	Locator::magicSystem::reset();
	Locator::playerSystem::reset();
	Locator::dynamicsSystem::reset();
	Locator::buildingDamageSystem::reset();
	Locator::rainSystem::reset();
	Locator::weatherSystem::reset();
	Locator::audio::reset();
	Locator::audioState::reset();
	Locator::soundTagSystem::reset();
	Locator::explosionSystem::reset();
	Locator::fieldSystem::reset();
	Locator::vegetation::reset();
	Locator::chimneySmokeSystem::reset();
	Locator::waterRingSystem::reset();
	Locator::worldEffects::reset();
	Locator::handTapRegistry::reset();
	Locator::cameraStack::reset();
	Locator::scriptState::reset();
	Locator::cameraHelpSystem::reset();
	Locator::debugHooks::reset();
	Locator::landAvoidSystem::reset();
	Locator::skyFrameSystem::reset();
	Locator::renderFrameSystem::reset();
	Locator::particleSystem::reset();
	Locator::time::reset();
	Locator::gameRandom::reset();
	Locator::logOnce::reset();
}

void openblack::test::EmplaceMapAndVillagerDefaults()
{
	using namespace openblack::ecs::systems;
	Locator::villagerFields::emplace<VillagerFields>();
	Locator::villagerFishFarms::emplace<VillagerFishFarms>();
	Locator::villagerBuildingSites::emplace<VillagerBuildingSites>();
	Locator::villagerStores::emplace<VillagerStores>();
	Locator::villagerTentQueries::emplace<VillagerTentQueries>();
	Locator::townCellObjects::emplace<TownCellObjects>();
	Locator::mapShapeProvider::emplace<MapShapeProvider>();
	Locator::meshBoxProvider::emplace<MeshBoxProvider>();
}

void openblack::test::ResetMapAndVillagerDefaults()
{
	Locator::meshBoxProvider::reset();
	Locator::mapShapeProvider::reset();
	Locator::townCellObjects::reset();
	Locator::villagerTentQueries::reset();
	Locator::villagerStores::reset();
	Locator::villagerBuildingSites::reset();
	Locator::villagerFishFarms::reset();
	Locator::villagerFields::reset();
}

openblack::test::ScopedDefaultFileSystem::ScopedDefaultFileSystem()
    : _previous(Locator::filesystem::handle())
{
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
}

openblack::test::ScopedDefaultFileSystem::~ScopedDefaultFileSystem()
{
	Locator::filesystem::reset(_previous);
}

namespace
{
/// Every test starts with the same services, whether it runs alone or with the rest of its executable. Only the
/// hidden-state services above are set here; a test injects any other dependency itself
class TestServices final: public ::testing::EmptyTestEventListener
{
	void OnTestStart(const ::testing::TestInfo& /*info*/) override { openblack::test::EmplaceTestServices(); }
	void OnTestEnd(const ::testing::TestInfo& /*info*/) override { openblack::test::ResetTestServices(); }
};

/// The game's trace switches (OPENBLACK_*TRACE*) come from the environment the tests run in, and they make code under
/// test log to loggers a test has not made. Every test executable starts with none of them; a test that wants one sets
/// it itself. The data paths (OPENBLACK_GAME_PATH, OPENBLACK_TEST_BW_ROOT) and the other switches are kept
const bool k_TracesCleared = [] {
	std::vector<std::string> names;
#ifdef _WIN32
	char** environment = _environ;
#else
	extern char** environ;
	char** environment = environ;
#endif
	for (char** entry = environment; entry != nullptr && *entry != nullptr; ++entry)
	{
		const std::string variable(*entry);
		const auto equals = variable.find('=');
		const auto name = variable.substr(0, equals);
		if (name.rfind("OPENBLACK_", 0) == 0 && name.find("TRACE") != std::string::npos)
		{
			names.push_back(name);
		}
	}
	for (const auto& name : names)
	{
#ifdef _WIN32
		_putenv_s(name.c_str(), "");
#else
		unsetenv(name.c_str());
#endif
	}
	return true;
}();

/// Registered before main() runs the tests (gtest_main's main calls InitGoogleTest, which keeps the listeners). gtest
/// takes ownership of the listener
const bool k_Registered = [] {
	::testing::UnitTest::GetInstance()->listeners().Append(std::make_unique<TestServices>().release());
	return true;
}();
} // namespace
