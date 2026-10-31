/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand with a spell seed in it as the game sets it up: the harness of support/HandSeedHarness.h with the hand
// initialised on the game's own data (its model, animations and packet handlers), drawn by bgfx's renderer that draws
// nothing.
// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <chrono>
#include <memory>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>

#include "ECS/Components/Hand.h"
#include "FileSystem/DefaultFileSystem.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Input/GamePackets.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "Resources/Loaders.h"
#include "support/HandSeedHarness.h"

using namespace openblack;
using HandResult = openblack::ecs::systems::MagicSystemInterface::HandResult;

namespace
{
constexpr std::chrono::milliseconds k_Frame {33};

class HandSeedOnGameData: public test::hand_seed::HandSeedHarness
{
protected:
	void SetUp() override
	{
		const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH");
		if (game == nullptr)
		{
			GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
		}
		// the hand's model builds bgfx buffers
		bgfx::renderFrame(); // single-threaded
		bgfx::Init init {};
		init.type = bgfx::RendererType::Noop;
		ASSERT_TRUE(bgfx::init(init));
		_bgfx = true;
		HandSeedHarness::SetUp();
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::filesystem::value().SetGamePath(game);
		// the hand's model, as the game loads it, then the hand's own start
		Locator::resources::value().GetMeshes().Load(ecs::components::Hand::k_MeshId, resources::L3DLoader::FromDiskTag {},
		                                             Locator::filesystem::value().GetPath<filesystem::Path::CreatureMesh>() /
		                                                 "Hand_Boned_Base2.l3d");
		static_cast<void>(Hand().Initialize());
		game_packets::Reset();
		game_clock::SetTurn(1);
	}
	void TearDown() override
	{
		if (!_bgfx)
		{
			return;
		}
		game_packets::Reset();
		game_clock::SetTurn(0);
		HandSeedHarness::TearDown();
		// the model goes before bgfx does
		Locator::resources::reset();
		bgfx::shutdown();
	}

	/// The turn's packets handled, then the hand's turn
	void EndTurn()
	{
		game_packets::Flush();
		game_packets::DispatchQueuedPackets();
		Hand().ProcessTurn();
		game_clock::SetTurn(game_clock::Turn() + 1);
	}

	[[nodiscard]] test::FakeMagicWorld& World() const
	{
		return static_cast<test::InertMagicSystem&>(Locator::magicSystem::value()).FakeWorld();
	}

private:
	test::RestoreService<Locator::filesystem> _restoreFileSystem;
	bool _bgfx {false};
};
} // namespace

// A miracle cast while held over a place where it can't be cast skips that turn's apply, with no failure, and stays
// locked: back over a good place it applies again, and only letting go ends it
TEST_F(HandSeedOnGameData, ALockedMiracleOverAPlaceItCantApplySkipsThatTurnButStaysLocked)
{
	// a seed of the lightning bolt, kept in the hand while it is cast, that may be cast only on land
	auto& seedRow = Info().spellSeed.at(static_cast<size_t>(SpellSeedType::Fire));
	seedRow.magicTypes.fill(MagicType::LightningBolt);
	seedRow.isKeptInHand = 1;
	auto& bolt = const_cast<GMagicInfo&>(magic::GetMagicInfo(Info(), MagicType::LightningBolt));
	bolt.magicType = MagicType::LightningBolt;
	bolt.castRuleType = CastRuleType::OnLand;
	const auto seed = HoldSeed(SpellSeedType::Fire, SpellCastType::SpellCastInHand);
	ASSERT_EQ(magic::seed::MagicTypeOf(Registry().Get<const ecs::components::SpellSeed>(seed)), MagicType::LightningBolt);
	ASSERT_EQ(Hand().PressSeedAction(), HandResult::CastHeld);
	EXPECT_GT(game_packets::Pending(), 0u);
	EndTurn();
	ASSERT_EQ(game_packets::Pending(), 0u);
	// over water: the turn's apply is skipped, and no failure is shown
	World().wetBelowX = 1000.0f;
	Hand().Update(k_Frame, glm::vec2(0.0f), false, true);
	EXPECT_EQ(game_packets::Pending(), 0u);
	EndTurn();
	// back over land: still locked, it applies again
	World().wetBelowX = -1.0f;
	Hand().Update(k_Frame, glm::vec2(0.0f), false, true);
	EXPECT_GT(game_packets::Pending(), 0u);
	EndTurn();
	// letting go ends it
	EXPECT_EQ(Hand().ReleaseSeedAction(), HandResult::Released);
}
