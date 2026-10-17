/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A new game in the same process starts from what a new program has: a game's shutdown takes every service a game
// makes (the player system and the map too, which a program's shutdown leaves) and keeps each one a process makes
// once; a new registry gives its entities their first versions, where a cleared one goes on from the
// last game's; and the clock, started again on the same fixed step, reads as a new one. With fakes and the services'
// own objects, never through a game.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/EventManager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "EngineConfig.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "LocatorInit.h"
#include "Resources/Resources.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using openblack::ecs::systems::TimeSystem;
using openblack::test::RestoreService;

namespace
{
/// The services the shutdown tests put in, each put back as it was when the test ends
class NewGameShutDown: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// only what the test puts in: none of the services every test starts with
		test::ResetTestServices();
		Locator::config::emplace();
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::time::emplace<TimeSystem>();
		Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::events::emplace<EventManager>();
	}

private:
	RestoreService<Locator::config> _config;
	RestoreService<Locator::filesystem> _filesystem;
	RestoreService<Locator::time> _time;
	RestoreService<Locator::playerSystem> _playerSystem;
	RestoreService<Locator::entitiesRegistry> _registry;
	RestoreService<Locator::events> _events;
};

/// A clock as the program starts it, on the fixed step: the fixed ticks from 0, the timers reset, paused, and the
/// engine timer started as the renderer's opening starts it
void StartAsTheProgramDoes(TimeSystem& clock, uint32_t frameMs)
{
	clock.SetFixedFrameMs(frameMs);
	clock.Reset();
	clock.Start(true);
	clock.StartEngineTimer();
}

/// A stand-in for a service the shutdown only resets or keeps and never calls: the slot has a value, and nothing is
/// made, read or destroyed through it
template <typename ServiceLocator>
void PutStandIn()
{
	using Service = typename ServiceLocator::type;
	// any address will do, as it is never read through
	alignas(std::max_align_t) static std::byte s_standIn {};
	ServiceLocator::reset(static_cast<Service*>(static_cast<void*>(&s_standIn)), [](Service* /*unused*/) {});
}

/// One Locator slot of a table: its name, a stand-in for it, and what it held before the test, put back when the last
/// copy of the slot goes
struct Slot
{
	std::string_view name;
	bool (*present)();
	void (*putStandIn)();
	std::shared_ptr<void> restore;
};

template <typename ServiceLocator>
Slot MakeSlot(std::string_view name)
{
	return {
	    .name = name,
	    .present = [] { return ServiceLocator::has_value(); },
	    .putStandIn = &PutStandIn<ServiceLocator>,
	    .restore = std::make_shared<RestoreService<ServiceLocator>>(),
	};
}

/// Every slot a game's shutdown resets, but for the three it reads first (the particle system, the audio's state and
/// the weather), which the test leaves empty
std::vector<Slot> GameSlots()
{
	return {
	    MakeSlot<Locator::temple>("temple"),
	    MakeSlot<Locator::videoSystem>("videoSystem"),
	    MakeSlot<Locator::rendereringSystem>("rendereringSystem"),
	    MakeSlot<Locator::pickingSystem>("pickingSystem"),
	    MakeSlot<Locator::creatureCaveSystem>("creatureCaveSystem"),
	    MakeSlot<Locator::creatureModeSystem>("creatureModeSystem"),
	    MakeSlot<Locator::editorSystem>("editorSystem"),
	    MakeSlot<Locator::cameraBookmarkSystem>("cameraBookmarkSystem"),
	    MakeSlot<Locator::cameraPathSystem>("cameraPathSystem"),
	    MakeSlot<Locator::livingActionSystem>("livingActionSystem"),
	    MakeSlot<Locator::townSystem>("townSystem"),
	    MakeSlot<Locator::resourceStoreSystem>("resourceStoreSystem"),
	    MakeSlot<Locator::handSystem>("handSystem"),
	    MakeSlot<Locator::handGrabSystem>("handGrabSystem"),
	    MakeSlot<Locator::pathfindingSystem>("pathfindingSystem"),
	    MakeSlot<Locator::creatureLocomotionSystem>("creatureLocomotionSystem"),
	    MakeSlot<Locator::terrainSystem>("terrainSystem"),
	    MakeSlot<Locator::gameActionSystem>("gameActionSystem"),
	    MakeSlot<Locator::oceanSystem>("oceanSystem"),
	    MakeSlot<Locator::skySystem>("skySystem"),
	    MakeSlot<Locator::debugGui>("debugGui"),
	    MakeSlot<Locator::creatureFightSystem>("creatureFightSystem"),
	    MakeSlot<Locator::leashSystem>("leashSystem"),
	    MakeSlot<Locator::creatureMindSystem>("creatureMindSystem"),
	    MakeSlot<Locator::creaturePhysiologySystem>("creaturePhysiologySystem"),
	    MakeSlot<Locator::creatureSkinSystem>("creatureSkinSystem"),
	    MakeSlot<Locator::creatureHairSystem>("creatureHairSystem"),
	    MakeSlot<Locator::footprintSystem>("footprintSystem"),
	    MakeSlot<Locator::creatureAudioSystem>("creatureAudioSystem"),
	    MakeSlot<Locator::creatureObjectActionSystem>("creatureObjectActionSystem"),
	    MakeSlot<Locator::creatureHandSystem>("creatureHandSystem"),
	    MakeSlot<Locator::creatureAnimationSystem>("creatureAnimationSystem"),
	    MakeSlot<Locator::entitiesRegistry>("entitiesRegistry"),
	    MakeSlot<Locator::entitiesMap>("entitiesMap"),
	    MakeSlot<Locator::mistSystem>("mistSystem"),
	    MakeSlot<Locator::cloudSystem>("cloudSystem"),
	    MakeSlot<Locator::villageLightSystem>("villageLightSystem"),
	    MakeSlot<Locator::renderFrameSystem>("renderFrameSystem"),
	    MakeSlot<Locator::skyFrameSystem>("skyFrameSystem"),
	    MakeSlot<Locator::landAvoidSystem>("landAvoidSystem"),
	    MakeSlot<Locator::debugHooks>("debugHooks"),
	    MakeSlot<Locator::scriptState>("scriptState"),
	    MakeSlot<Locator::scriptObjects>("scriptObjects"),
	    MakeSlot<Locator::cameraHelpSystem>("cameraHelpSystem"),
	    MakeSlot<Locator::handTapRegistry>("handTapRegistry"),
	    MakeSlot<Locator::abodeKnockSystem>("abodeKnockSystem"),
	    MakeSlot<Locator::worldEffects>("worldEffects"),
	    MakeSlot<Locator::waterRingSystem>("waterRingSystem"),
	    MakeSlot<Locator::chimneySmokeSystem>("chimneySmokeSystem"),
	    MakeSlot<Locator::vegetation>("vegetation"),
	    MakeSlot<Locator::fieldSystem>("fieldSystem"),
	    MakeSlot<Locator::explosionSystem>("explosionSystem"),
	    MakeSlot<Locator::audio>("audio"),
	    MakeSlot<Locator::soundTagSystem>("soundTagSystem"),
	    MakeSlot<Locator::dynamicsSystem>("dynamicsSystem"),
	    MakeSlot<Locator::buildingDamageSystem>("buildingDamageSystem"),
	    MakeSlot<Locator::snowSystem>("snowSystem"),
	    MakeSlot<Locator::snowfallSystem>("snowfallSystem"),
	    MakeSlot<Locator::rainSystem>("rainSystem"),
	    MakeSlot<Locator::villagerFields>("villagerFields"),
	    MakeSlot<Locator::villagerFishFarms>("villagerFishFarms"),
	    MakeSlot<Locator::villagerBuildingSites>("villagerBuildingSites"),
	    MakeSlot<Locator::villagerStores>("villagerStores"),
	    MakeSlot<Locator::villagerTentQueries>("villagerTentQueries"),
	    MakeSlot<Locator::villagerRules>("villagerRules"),
	    MakeSlot<Locator::villagerWorldQueries>("villagerWorldQueries"),
	    MakeSlot<Locator::villagerWorshipCheck>("villagerWorshipCheck"),
	    MakeSlot<Locator::villagerChildFactory>("villagerChildFactory"),
	    MakeSlot<Locator::villagerDiscipleJobs>("villagerDiscipleJobs"),
	    MakeSlot<Locator::townCellObjects>("townCellObjects"),
	    MakeSlot<Locator::mapShapeProvider>("mapShapeProvider"),
	    MakeSlot<Locator::meshBoxProvider>("meshBoxProvider"),
	    MakeSlot<Locator::glintTargets>("glintTargets"),
	    MakeSlot<Locator::toBeDeletedSystem>("toBeDeletedSystem"),
	    MakeSlot<Locator::objectCreationIndexSystem>("objectCreationIndexSystem"),
	    MakeSlot<Locator::mapCellsSystem>("mapCellsSystem"),
	    MakeSlot<Locator::drawListSystem>("drawListSystem"),
	    MakeSlot<Locator::animatedStaticSystem>("animatedStaticSystem"),
	    MakeSlot<Locator::reactionSystem>("reactionSystem"),
	    MakeSlot<Locator::fireSystem>("fireSystem"),
	    MakeSlot<Locator::forestSystem>("forestSystem"),
	    MakeSlot<Locator::treeSystem>("treeSystem"),
	    MakeSlot<Locator::landBalanceSystem>("landBalanceSystem"),
	    MakeSlot<Locator::animalSystem>("animalSystem"),
	    MakeSlot<Locator::magicShieldSystem>("magicShieldSystem"),
	    MakeSlot<Locator::teleportSystem>("teleportSystem"),
	    MakeSlot<Locator::tornadoSystem>("tornadoSystem"),
	    MakeSlot<Locator::vortexSystem>("vortexSystem"),
	    MakeSlot<Locator::playerProfileSystem>("playerProfileSystem"),
	    MakeSlot<Locator::helpSpeechSystem>("helpSpeechSystem"),
	    MakeSlot<Locator::helpTextSystem>("helpTextSystem"),
	    MakeSlot<Locator::introSystem>("introSystem"),
	    MakeSlot<Locator::highDetailSystem>("highDetailSystem"),
	    MakeSlot<Locator::helpProfileSystem>("helpProfileSystem"),
	    MakeSlot<Locator::tipBubbleSystem>("tipBubbleSystem"),
	    MakeSlot<Locator::advisorSystem>("advisorSystem"),
	    MakeSlot<Locator::townStateSystem>("townStateSystem"),
	    MakeSlot<Locator::townDesireSystem>("townDesireSystem"),
	    MakeSlot<Locator::villagerStateSystem>("villagerStateSystem"),
	    MakeSlot<Locator::miracleFxSystem>("miracleFxSystem"),
	    MakeSlot<Locator::gestureEvents>("gestureEvents"),
	    MakeSlot<Locator::gestureSystem>("gestureSystem"),
	    MakeSlot<Locator::routePlanStateSystem>("routePlanStateSystem"),
	    MakeSlot<Locator::magicSystem>("magicSystem"),
	    MakeSlot<Locator::templeExteriorSystem>("templeExteriorSystem"),
	    MakeSlot<Locator::worshipState>("worshipState"),
	    MakeSlot<Locator::villageTotemSystem>("villageTotemSystem"),
	    MakeSlot<Locator::gameStatsSystem>("gameStatsSystem"),
	    MakeSlot<Locator::alignmentSystem>("alignmentSystem"),
	    MakeSlot<Locator::playerSystem>("playerSystem"),
	    MakeSlot<Locator::influenceSystem>("influenceSystem"),
	    MakeSlot<Locator::inputState>("inputState"),
	    MakeSlot<Locator::events>("events"),
	    MakeSlot<Locator::camera>("camera"),
	    MakeSlot<Locator::gameRandom>("gameRandom"),
	    MakeSlot<Locator::vm>("vm"),
	    MakeSlot<Locator::dialogueControlSystem>("dialogueControlSystem"),
	    MakeSlot<Locator::scriptControlSystem>("scriptControlSystem"),
	    MakeSlot<Locator::screenshotRequest>("screenshotRequest"),
	    MakeSlot<Locator::tutorialSkipSystem>("tutorialSkipSystem"),
	    MakeSlot<Locator::mapScriptSystem>("mapScriptSystem"),
	    MakeSlot<Locator::dayNightClock>("dayNightClock"),
	    MakeSlot<Locator::cinematicDirectorSystem>("cinematicDirectorSystem"),
	};
}

/// What a process makes once and a game's shutdown keeps, but for the resources, which it reads (their samples go),
/// and the configuration, the file system and the clock, which the fixture puts in
std::vector<Slot> KeptSlots()
{
	return {
	    MakeSlot<Locator::rng>("rng"),
	    MakeSlot<Locator::chlapi>("chlapi"),
	    MakeSlot<Locator::infoConstants>("infoConstants"),
	    MakeSlot<Locator::profiler>("profiler"),
	    MakeSlot<Locator::modLoader>("modLoader"),
	    MakeSlot<Locator::rendererInterface>("rendererInterface"),
	    MakeSlot<Locator::windowing>("windowing"),
	};
}
} // namespace

TEST_F(NewGameShutDown, AGamesShutDownTakesItsServicesAndTheirPlayers)
{
	ShutDownServices(ShutDownScope::Game);
	// the next game's start makes these again: a player system kept would carry the last game's players into it
	EXPECT_FALSE(Locator::playerSystem::has_value());
	EXPECT_FALSE(Locator::entitiesRegistry::has_value());
	EXPECT_FALSE(Locator::events::has_value());
	// what a process makes once stays
	EXPECT_TRUE(Locator::config::has_value());
	EXPECT_TRUE(Locator::filesystem::has_value());
	EXPECT_TRUE(Locator::time::has_value());
}

TEST_F(NewGameShutDown, AGamesShutDownTakesEverySlotAGameMakesAndNoneAProcessMakes)
{
	// the three slots the shutdown reads before it resets them stay empty
	const RestoreService<Locator::particleSystem> particleSystem;
	const RestoreService<Locator::audioState> audioState;
	const RestoreService<Locator::weatherSystem> weatherSystem;
	Locator::particleSystem::reset();
	Locator::audioState::reset();
	Locator::weatherSystem::reset();
	const RestoreService<Locator::resources> keptResources;
	const auto gameSlots = GameSlots();
	const auto keptSlots = KeptSlots();
	for (const auto& slot : gameSlots)
	{
		slot.putStandIn();
	}
	for (const auto& slot : keptSlots)
	{
		slot.putStandIn();
	}
	// real, as the shutdown clears its samples
	Locator::resources::emplace<resources::Resources>();

	ShutDownServices(ShutDownScope::Game);
	// a slot left would carry the last game into the next one, which makes it again only when it is not there
	for (const auto& slot : gameSlots)
	{
		EXPECT_FALSE(slot.present()) << slot.name;
	}
	for (const auto& slot : keptSlots)
	{
		EXPECT_TRUE(slot.present()) << slot.name;
	}
	EXPECT_TRUE(Locator::resources::has_value());
	EXPECT_TRUE(Locator::config::has_value());
	EXPECT_TRUE(Locator::filesystem::has_value());
	EXPECT_TRUE(Locator::time::has_value());
}

TEST_F(NewGameShutDown, AProgramsShutDownLeavesThePlayerSystem)
{
	ShutDownServices(ShutDownScope::Process);
	EXPECT_FALSE(Locator::entitiesRegistry::has_value());
	EXPECT_FALSE(Locator::config::has_value());
	EXPECT_FALSE(Locator::filesystem::has_value());
	EXPECT_FALSE(Locator::time::has_value());
	// as before: the program's end leaves it, which is why a new game's shutdown takes it itself
	EXPECT_TRUE(Locator::playerSystem::has_value());
}

TEST(NewGameRegistry, ANewRegistryGivesTheFirstVersionsAgain)
{
	ecs::Registry cleared;
	const auto first = cleared.Create();
	cleared.Reset();
	const auto again = cleared.Create();
	// a cleared registry gives the same entity with its version moved on: the state hash and the script objects would
	// see another entity than a new program's
	EXPECT_EQ(entt::to_entity(again), entt::to_entity(first));
	EXPECT_EQ(static_cast<uint32_t>(entt::to_version(again)), static_cast<uint32_t>(entt::to_version(first)) + 1u);

	ecs::Registry fresh;
	EXPECT_EQ(fresh.Create(), first);
}

TEST(NewGameClock, AClockStartedAgainReadsAsANewOne)
{
	constexpr uint32_t k_FrameMs = 16;
	TimeSystem fresh;
	StartAsTheProgramDoes(fresh, k_FrameMs);

	TimeSystem played;
	StartAsTheProgramDoes(played, k_FrameMs);
	// a game played for a while: frames, a land's start and its turns, a pause
	played.SetTurn(0);
	played.OnLoad();
	played.Start(false);
	for (int frame = 0; frame < 120; ++frame)
	{
		played.AdvanceFixedFrame();
		while (played.IsTurnScheduled())
		{
			played.StartTurn();
		}
		played.UpdateFrame();
	}
	played.SetPaused(true);
	ASSERT_NE(played.TickCount(), 0u);
	ASSERT_NE(played.GetTurn(), 0u);

	// the new game's start: the same fixed step again, then the clock as the program starts it
	StartAsTheProgramDoes(played, played.FixedFrameMs());
	EXPECT_EQ(played.FixedFrameMs(), k_FrameMs);
	EXPECT_EQ(played.TickCount(), 0u);
	EXPECT_EQ(played.TickCount(), fresh.TickCount());
	EXPECT_EQ(played.GetTurn(), 0u);
	EXPECT_TRUE(played.IsPaused());
	EXPECT_EQ(played.IsPaused(), fresh.IsPaused());
	EXPECT_EQ(played.EngineMs(), fresh.EngineMs());
	EXPECT_EQ(played.GetFrameGameTime(), fresh.GetFrameGameTime());
	EXPECT_EQ(played.GetSpeed(), fresh.GetSpeed());
	EXPECT_EQ(played.SequenceMode(), fresh.SequenceMode());
	EXPECT_FLOAT_EQ(played.GetTurnFraction(), fresh.GetTurnFraction());

	// and the same frames from there on read the same
	for (int frame = 0; frame < 10; ++frame)
	{
		fresh.AdvanceFixedFrame();
		played.AdvanceFixedFrame();
	}
	EXPECT_EQ(played.TickCount(), fresh.TickCount());
	EXPECT_EQ(played.EngineMs(), fresh.EngineMs());
}
