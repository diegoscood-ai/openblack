/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The start-up in four parts, as raffclar's tree has it: the core services the simulation needs first of all, the
// engine's, the simulation's and the services of the game on the player's computer. The core is made with no window,
// renderer or audio device, and a new game keeps what a launch made once. With the services' own objects, never
// through a game.

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Locator.h"
#include "LocatorInit.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::test::RestoreService;

namespace
{
/// The service a Locator slot holds, kept alive while this lives so that a new service cannot take its address
template <typename ServiceLocator>
struct Held
{
	typename ServiceLocator::node_type handle {ServiceLocator::handle()};
	const typename ServiceLocator::type* service {&ServiceLocator::value()};
};

/// The game's log, which the core writes to, made for the test when nothing has made it yet and dropped again when the
/// test ends, so that a game made later makes its own
class NullGameLog
{
public:
	NullGameLog()
	    : _made(spdlog::get("game") == nullptr)
	{
		if (_made)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
	}
	~NullGameLog()
	{
		if (_made)
		{
			spdlog::drop("game");
		}
	}
	NullGameLog(const NullGameLog&) = delete;
	NullGameLog& operator=(const NullGameLog&) = delete;
	NullGameLog(NullGameLog&&) = delete;
	NullGameLog& operator=(NullGameLog&&) = delete;

private:
	bool _made;
};

/// The slots the core fills, and those of the services that show the world, each empty when the test starts and put
/// back as it was when the test ends
class SimulationCore: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::logOnce::reset();
		Locator::profiler::reset();
		Locator::events::reset();
		Locator::filesystem::reset();
		Locator::rng::reset();
		Locator::gameRandom::reset();
		Locator::windowing::reset();
		Locator::rendererInterface::reset();
		Locator::debugGui::reset();
		Locator::audio::reset();
	}

private:
	NullGameLog _log;
	RestoreService<Locator::logOnce> _logOnce;
	RestoreService<Locator::profiler> _profiler;
	RestoreService<Locator::events> _events;
	RestoreService<Locator::filesystem> _filesystem;
	RestoreService<Locator::rng> _rng;
	RestoreService<Locator::gameRandom> _gameRandom;
	RestoreService<Locator::windowing> _windowing;
	RestoreService<Locator::rendererInterface> _renderer;
	RestoreService<Locator::debugGui> _debugGui;
	RestoreService<Locator::audio> _audio;
};

/// The slots the client fills, each empty when the test starts and put back as it was when the test ends
class Client: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::gameActionSystem::reset();
		Locator::rendereringSystem::reset();
		Locator::editorSystem::reset();
		Locator::videoSystem::reset();
	}

private:
	RestoreService<Locator::gameActionSystem> _gameActionSystem;
	RestoreService<Locator::rendereringSystem> _renderingSystem;
	RestoreService<Locator::editorSystem> _editorSystem;
	RestoreService<Locator::videoSystem> _videoSystem;
};
} // namespace

TEST_F(SimulationCore, IsMadeWithNoWindowRendererOrAudioDevice)
{
	InitializeSimulationCore(StartKind::Launch);

	EXPECT_TRUE(Locator::logOnce::has_value());
	EXPECT_TRUE(Locator::profiler::has_value());
	EXPECT_TRUE(Locator::events::has_value());
	EXPECT_TRUE(Locator::filesystem::has_value());
	EXPECT_TRUE(Locator::rng::has_value());
	EXPECT_TRUE(Locator::gameRandom::has_value());
	EXPECT_FALSE(Locator::windowing::has_value());
	EXPECT_FALSE(Locator::rendererInterface::has_value());
	EXPECT_FALSE(Locator::debugGui::has_value());
	EXPECT_FALSE(Locator::audio::has_value());
}

TEST_F(SimulationCore, ANewGameKeepsWhatALaunchMadeOnce)
{
	InitializeSimulationCore(StartKind::Launch);
	const Held<Locator::logOnce> logOnce;
	const Held<Locator::profiler> profiler;
	const Held<Locator::filesystem> files;
	const Held<Locator::rng> rng;

	InitializeSimulationCore(StartKind::NewGame);

	EXPECT_EQ(&Locator::logOnce::value(), logOnce.service);
	EXPECT_EQ(&Locator::profiler::value(), profiler.service);
	EXPECT_EQ(&Locator::filesystem::value(), files.service);
	EXPECT_EQ(&Locator::rng::value(), rng.service);
}

TEST_F(Client, MakesTheServicesOfTheGameOnThePlayersComputer)
{
	InitializeClient();

	EXPECT_TRUE(Locator::gameActionSystem::has_value());
	EXPECT_TRUE(Locator::rendereringSystem::has_value());
	EXPECT_TRUE(Locator::editorSystem::has_value());
	EXPECT_TRUE(Locator::videoSystem::has_value());
}
