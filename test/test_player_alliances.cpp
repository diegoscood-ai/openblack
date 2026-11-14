/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The players' alliances: the pure rules over one player's strengths, where the game keeps them (the player's entity,
// else the player system, from land to land), and the script natives SET_PLAYER_ALLY and GET_PLAYER_ALLY

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerAlliances.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::archetypes::PlayerArchetype;
using openblack::ecs::components::Player;
using openblack::ecs::components::PlayerAlliances;
using openblack::ecs::systems::PlayerSystem;
namespace alliances = openblack::ecs::alliances;

namespace
{
/// Whether the registry has a storage of alliances: one is made only once an alliance is set
bool HasAllianceStorage(const ecs::Registry& registry)
{
	bool found = false;
	registry.EachStorage(
	    [&found](entt::id_type id, const auto&) { found = found || id == entt::type_hash<PlayerAlliances>::value(); });
	return found;
}

PlayerAlliances With(PlayerNames other, float strength)
{
	PlayerAlliances of {};
	of.strength.at(static_cast<size_t>(other)) = strength;
	return of;
}
} // namespace

TEST(PlayerAlliances, AnAllyIsAStrengthAboveZero)
{
	EXPECT_FALSE(alliances::IsAllied(PlayerAlliances {}, PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(alliances::IsAllied(With(PlayerNames::PLAYER_TWO, 0.0f), PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(alliances::IsAllied(With(PlayerNames::PLAYER_TWO, -0.5f), PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(alliances::IsAllied(With(PlayerNames::PLAYER_TWO, 0.01f), PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(alliances::IsAllied(With(PlayerNames::PLAYER_TWO, 1.0f), PlayerNames::PLAYER_TWO));
	// only towards that player, and not towards a slot out of range
	EXPECT_FALSE(alliances::IsAllied(With(PlayerNames::PLAYER_TWO, 1.0f), PlayerNames::PLAYER_THREE));
	EXPECT_FALSE(alliances::IsAllied(With(PlayerNames::PLAYER_TWO, 1.0f), PlayerNames::_COUNT));
	EXPECT_EQ(alliances::Strength(With(PlayerNames::NEUTRAL, 0.75f), PlayerNames::NEUTRAL), 0.75f);
	EXPECT_EQ(alliances::Strength(With(PlayerNames::NEUTRAL, 0.75f), PlayerNames::_COUNT), 0.0f);
}

TEST(PlayerAlliances, AnAllysInfluenceIsUsedAboveATenthComparedAsADouble)
{
	EXPECT_FALSE(alliances::MayUseInfluenceOf(With(PlayerNames::PLAYER_TWO, 0.05f), PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(alliances::MayUseInfluenceOf(With(PlayerNames::PLAYER_TWO, 0.0999f), PlayerNames::PLAYER_TWO));
	// 0.1f is a little above the double 0.1
	EXPECT_TRUE(alliances::MayUseInfluenceOf(With(PlayerNames::PLAYER_TWO, 0.1f), PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(alliances::MayUseInfluenceOf(With(PlayerNames::PLAYER_TWO, 1.0f), PlayerNames::PLAYER_TWO));
}

TEST(PlayerAlliances, TheAlliesInfluenceIsTheFirstAllysAboveZeroInSlotOrder)
{
	std::vector<PlayerNames> asked;
	const auto rawOf = [&asked](PlayerNames ally) {
		asked.push_back(ally);
		switch (ally)
		{
		case PlayerNames::PLAYER_THREE:
			return 0.0f;
		case PlayerNames::PLAYER_FIVE:
			return 0.4f;
		case PlayerNames::NEUTRAL:
			return 0.9f;
		default:
			return 0.6f;
		}
	};
	PlayerAlliances of {};
	// player two is an ally too weak to lend its influence; three has none there; five is the first with some
	of.strength.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)) = 0.1f - 0.01f;
	of.strength.at(static_cast<size_t>(PlayerNames::PLAYER_THREE)) = 1.0f;
	of.strength.at(static_cast<size_t>(PlayerNames::PLAYER_FIVE)) = 0.5f;
	of.strength.at(static_cast<size_t>(PlayerNames::NEUTRAL)) = 1.0f;
	EXPECT_EQ(alliances::AlliesInfluence(of, rawOf), 0.4f);
	EXPECT_EQ(asked, (std::vector<PlayerNames> {PlayerNames::PLAYER_THREE, PlayerNames::PLAYER_FIVE}));
	// no ally with influence there: 0
	asked.clear();
	EXPECT_EQ(alliances::AlliesInfluence(With(PlayerNames::PLAYER_THREE, 1.0f), rawOf), 0.0f);
	EXPECT_EQ(alliances::AlliesInfluence(PlayerAlliances {}, rawOf), 0.0f);
	EXPECT_EQ(asked, (std::vector<PlayerNames> {PlayerNames::PLAYER_THREE}));
}

class PlayerAlliancesKept: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_players = &static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());
	}

	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::playerSystem> _restorePlayers;
	ecs::Registry* _registry {nullptr};
	PlayerSystem* _players {nullptr};
};

TEST_F(PlayerAlliancesKept, EveryStrengthStartsAtZero)
{
	for (size_t p = 0; p < PlayerAlliances::k_Players; ++p)
	{
		for (size_t q = 0; q < PlayerAlliances::k_Players; ++q)
		{
			EXPECT_EQ(alliances::Strength(static_cast<PlayerNames>(p), static_cast<PlayerNames>(q)), 0.0f) << p << q;
		}
	}
	// nothing is kept or carried until a script sets one
	EXPECT_FALSE(_players->AlliancesWithoutEntity(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FALSE(HasAllianceStorage(*_registry));
}

TEST_F(PlayerAlliancesKept, SettingAnAllianceSetsItBothWaysAsItIs)
{
	const auto one = PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	const auto two = PlayerArchetype::Create(PlayerNames::PLAYER_TWO);
	alliances::SetAlliance(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 1.0f);
	EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO), 1.0f);
	EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), 1.0f);
	EXPECT_TRUE(alliances::IsAllied(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(alliances::IsAllied(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	// on the players' entities
	EXPECT_EQ(_registry->Get<PlayerAlliances>(one).strength.at(1), 1.0f);
	EXPECT_EQ(_registry->Get<PlayerAlliances>(two).strength.at(0), 1.0f);
	// a percentage is not scaled, and 0 ends the alliance
	alliances::SetAlliance(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, 50.0f);
	EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), 50.0f);
	alliances::SetAlliance(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, 0.0f);
	EXPECT_FALSE(alliances::IsAllied(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	// the same player twice: their strength towards themselves
	alliances::SetAlliance(PlayerNames::PLAYER_THREE, PlayerNames::PLAYER_THREE, 0.5f);
	EXPECT_TRUE(alliances::IsAllied(PlayerNames::PLAYER_THREE, PlayerNames::PLAYER_THREE));
	// a slot out of range: nothing
	alliances::SetAlliance(PlayerNames::_COUNT, PlayerNames::PLAYER_ONE, 1.0f);
	EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_ONE, PlayerNames::NEUTRAL), 0.0f);
}

TEST_F(PlayerAlliancesKept, APlayerWithoutAnEntityKeepsThemInThePlayerSystemUntilTheirEntityIsMade)
{
	// the neutral player never has an entity
	alliances::SetAlliance(PlayerNames::NEUTRAL, PlayerNames::PLAYER_FOUR, 0.25f);
	ASSERT_TRUE(_players->AlliancesWithoutEntity(PlayerNames::NEUTRAL).has_value());
	ASSERT_TRUE(_players->AlliancesWithoutEntity(PlayerNames::PLAYER_FOUR).has_value());
	EXPECT_EQ(_players->AlliancesWithoutEntity(PlayerNames::NEUTRAL)->strength.at(3), 0.25f);
	EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_FOUR, PlayerNames::NEUTRAL), 0.25f);
	// player four's entity, when a script makes it, carries them from then on
	const auto four = PlayerArchetype::Create(PlayerNames::PLAYER_FOUR);
	EXPECT_FALSE(_players->AlliancesWithoutEntity(PlayerNames::PLAYER_FOUR).has_value());
	EXPECT_EQ(_registry->Get<PlayerAlliances>(four).strength.at(7), 0.25f);
	EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_FOUR, PlayerNames::NEUTRAL), 0.25f);
}

TEST_F(PlayerAlliancesKept, ALandChangeKeepsEveryPlayersAlliances)
{
	PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	PlayerArchetype::Create(PlayerNames::PLAYER_TWO);
	alliances::SetAlliance(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, 1.0f);

	// the land is cleared: the entities' alliances move to the player system
	_players->KeepForNextLand();
	EXPECT_EQ(_registry->Size<PlayerAlliances>(), 0u);
	_players->ClearPlayers();
	_players->ClearMagicWithoutEntity();
	_registry->Reset();
	EXPECT_TRUE(alliances::IsAllied(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(alliances::IsAllied(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));

	// the next land's players take them back, the computer player's as well as the local player's
	const auto two = PlayerArchetype::Create(PlayerNames::PLAYER_TWO);
	const auto one = PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_registry->Get<PlayerAlliances>(two).strength.at(0), 1.0f);
	EXPECT_EQ(_registry->Get<PlayerAlliances>(one).strength.at(1), 1.0f);
	EXPECT_FALSE(_players->AlliancesWithoutEntity(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FALSE(_players->AlliancesWithoutEntity(PlayerNames::PLAYER_TWO).has_value());
}

TEST_F(PlayerAlliancesKept, ALandWithoutAlliancesMakesNoStorage)
{
	PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(alliances::IsAllied(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO));
	_players->KeepForNextLand();
	_registry->Reset();
	PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(alliances::IsAllied(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(HasAllianceStorage(*_registry));
}

TEST(PlayerAlliances, WithoutAPlayerSystemNobodyIsAllied)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	Locator::entitiesRegistry::reset();
	Locator::playerSystem::reset();
	EXPECT_FALSE(alliances::IsAllied(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO));
	EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), 0.0f);
}

class PlayerAllyNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives log to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::vm::emplace<lhvm::LHVM>();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::playerSystem::emplace<PlayerSystem>();
	}

	const lhvm::NativeFunction& Native(std::string_view name)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end());
		return *native;
	}

	/// SET_PLAYER_ALLY as a script calls it, with a value under its three that it must leave there
	void SetPlayerAlly(float player1, float player2, float percentage)
	{
		const auto& native = Native("SET_PLAYER_ALLY");
		EXPECT_EQ(native.stackIn, 3);
		EXPECT_EQ(native.stackOut, 0u);
		auto& vm = Locator::vm::value();
		vm.Pushi(77);
		vm.Pushf(player1);
		vm.Pushf(player2);
		vm.Pushf(percentage);
		native.impl();
		EXPECT_EQ(vm.Pop().intVal, 77);
	}

	float GetPlayerAlly(float player1, float player2)
	{
		const auto& native = Native("GET_PLAYER_ALLY");
		EXPECT_EQ(native.stackIn, 2);
		EXPECT_EQ(native.stackOut, 1u);
		auto& vm = Locator::vm::value();
		vm.Pushi(77);
		vm.Pushf(player1);
		vm.Pushf(player2);
		native.impl();
		const auto strength = vm.Popf();
		EXPECT_EQ(vm.Pop().intVal, 77);
		return strength;
	}

	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::playerSystem> _restorePlayers;
	chlapi::CHLApi _api;
};

TEST_F(PlayerAllyNatives, Land2AlliesThePlayerWithKhazarAndTheLaterLandsEndIt)
{
	// land2ComputerAI: SET_PLAYER_ALLY(1, 2, 1); script player n is game player n - 1
	SetPlayerAlly(1.0f, 2.0f, 1.0f);
	EXPECT_TRUE(alliances::IsAllied(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(alliances::IsAllied(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(GetPlayerAlly(1.0f, 2.0f), 1.0f);
	EXPECT_EQ(GetPlayerAlly(2.0f, 1.0f), 1.0f);
	EXPECT_EQ(GetPlayerAlly(1.0f, 3.0f), 0.0f);
	// SetupLand3, SetupLand5, beginland5 and killkhazar: SET_PLAYER_ALLY(1, 2, 0)
	SetPlayerAlly(1.0f, 2.0f, 0.0f);
	EXPECT_FALSE(alliances::IsAllied(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO));
	EXPECT_EQ(GetPlayerAlly(2.0f, 1.0f), 0.0f);
}

TEST_F(PlayerAllyNatives, TheStrengthIsTheScriptsValueAsItIs)
{
	SetPlayerAlly(2.0f, 3.0f, 75.0f);
	EXPECT_EQ(GetPlayerAlly(3.0f, 2.0f), 75.0f);
	SetPlayerAlly(2.0f, 3.0f, -1.0f);
	EXPECT_EQ(GetPlayerAlly(2.0f, 3.0f), -1.0f);
	EXPECT_FALSE(alliances::IsAllied(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE));
}

TEST_F(PlayerAllyNatives, ScriptPlayerZeroIsTheNeutralPlayerAndANumberOfNoPlayerDoesNothing)
{
	SetPlayerAlly(0.0f, 1.0f, 0.5f);
	EXPECT_EQ(alliances::Strength(PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE), 0.5f);
	EXPECT_EQ(GetPlayerAlly(1.0f, 0.0f), 0.5f);
	// 9 and -1 name no player: nothing is set, and the strength asked is 0
	SetPlayerAlly(9.0f, 1.0f, 1.0f);
	SetPlayerAlly(1.0f, -1.0f, 1.0f);
	for (size_t p = 0; p < PlayerAlliances::k_Players; ++p)
	{
		EXPECT_EQ(alliances::Strength(PlayerNames::PLAYER_ONE, static_cast<PlayerNames>(p)), p == 7 ? 0.5f : 0.0f) << p;
	}
	EXPECT_EQ(GetPlayerAlly(9.0f, 1.0f), 0.0f);
	EXPECT_EQ(GetPlayerAlly(1.0f, 9.0f), 0.0f);
}
