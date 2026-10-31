/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player system's state (every player's alignment, the magic of the players without an entity, the local player)
// and the magic::players and creature::LocalPlayer functions that reach it

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/LocalPlayer.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/InfluenceCrossing.h"
#include "ECS/Components/InterfaceAlignment.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatureSlot.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/PlayerRivalry.h"
#include "ECS/Components/VirtualInfluence.h"
#include "ECS/PlayerRivalry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::archetypes::PlayerArchetype;
using openblack::ecs::components::Alignment;
using openblack::ecs::components::Creature;
using openblack::ecs::components::InfluenceCrossing;
using openblack::ecs::components::InterfaceAlignment;
using openblack::ecs::components::Player;
using openblack::ecs::components::PlayerCreatures;
using openblack::ecs::components::PlayerCreatureSlot;
using openblack::ecs::components::PlayerMagic;
using openblack::ecs::components::VirtualInfluence;
using openblack::ecs::systems::PlayerSystem;
using openblack::ecs::systems::PlayerSystemInterface;
using openblack::lhscriptx::FeatureScriptCommands;

namespace
{
/// Records which player each call asked for, and hands out its own state
class FakePlayerSystem final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity) override {}
	[[nodiscard]] entt::entity GetPlayer(PlayerNames) const override { return entt::null; }
	void AddCreature(entt::entity) override {}
	// no creatures
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames) const override { return std::nullopt; }
	void SetPrimaryCreature(PlayerNames, std::optional<entt::entity>) override {}
	// keeps nothing from land to land
	void KeepForNextLand() override {}
	void TakeUpKept(entt::entity) override {}
	[[nodiscard]] PlayerNames GetLocalPlayer() const override { return local; }
	void ClearPlayers() override { ++playerClears; }
	[[nodiscard]] ecs::components::Alignment& Alignment(PlayerNames name) override
	{
		alignmentAsked = name;
		return alignment;
	}
	[[nodiscard]] PlayerMagic& MagicWithoutEntity(PlayerNames name) override
	{
		magicAsked = name;
		return perPlayer ? magics.at(static_cast<size_t>(name)) : magic;
	}
	void ClearMagicWithoutEntity() override { ++clears; }
	[[nodiscard]] ecs::components::InterfaceAlignment& InterfaceAlignmentWithoutEntity() override { return interfaceAlignment; }
	[[nodiscard]] ecs::components::InfluenceCrossing& InfluenceCrossingWithoutEntity() override { return crossing; }

	ecs::components::Alignment alignment {};
	PlayerMagic magic {};
	// one magic for each player instead of the one above
	bool perPlayer {false};
	std::array<PlayerMagic, static_cast<size_t>(PlayerNames::_COUNT)> magics {};
	ecs::components::InterfaceAlignment interfaceAlignment {};
	ecs::components::InfluenceCrossing crossing {};
	PlayerNames alignmentAsked {PlayerNames::_COUNT};
	PlayerNames magicAsked {PlayerNames::_COUNT};
	PlayerNames local {PlayerNames::PLAYER_ONE};
	int clears {0};
	int playerClears {0};
};
} // namespace

TEST(PlayerSystem, startsAtTheNewGameValues)
{
	PlayerSystem players;
	for (size_t p = 0; p < PlayerSystem::k_Players; ++p)
	{
		const auto name = static_cast<PlayerNames>(p);
		EXPECT_EQ(players.Alignment(name).value, 0.0f) << p;
		EXPECT_EQ(players.Alignment(name).pending, 0.0f) << p;
		EXPECT_EQ(players.MagicWithoutEntity(name).tribalPower[0], 1.0f) << p;
		// the neutral player has its own type from the start (3 in every save of the original); nobody plays the others
		EXPECT_EQ(players.MagicWithoutEntity(name).playerType,
		          name == PlayerNames::NEUTRAL ? PlayerMagic::k_NeutralPlayerType : 0)
		    << p;
	}
}

TEST(PlayerSystem, theGameSetsUpTheHumanTheNeutralPlayerAndNobodyElse)
{
	using magic::players::TypeAtSetUp;
	// as the original's set-up of the players: the human at this interface 1, the neutral player 3, the others 0
	EXPECT_EQ(TypeAtSetUp(PlayerNames::PLAYER_ONE), 1);
	EXPECT_EQ(TypeAtSetUp(PlayerNames::NEUTRAL), 3);
	EXPECT_EQ(PlayerMagic::k_NeutralPlayerType, 3);
	for (const auto name : {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE, PlayerNames::PLAYER_FOUR,
	                        PlayerNames::PLAYER_FIVE, PlayerNames::PLAYER_SIX, PlayerNames::PLAYER_SEVEN})
	{
		EXPECT_EQ(TypeAtSetUp(name), 0) << static_cast<int>(name);
	}
	// a script switching the neutral player's computer god on makes it a computer player, off keeps its type
	EXPECT_EQ(magic::players::TypeAfterComputerToggle(TypeAtSetUp(PlayerNames::NEUTRAL), true),
	          PlayerMagic::k_ComputerPlayerType);
	EXPECT_EQ(magic::players::TypeAfterComputerToggle(TypeAtSetUp(PlayerNames::NEUTRAL), false), 3);
}

TEST(PlayerSystem, aNeutralPlayerMadeAsAnEntityHasTheNeutralType)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::playerSystem::emplace<PlayerSystem>();

	const auto neutral = PlayerArchetype::Create(PlayerNames::NEUTRAL);
	const auto human = PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	const auto other = PlayerArchetype::Create(PlayerNames::PLAYER_TWO);

	EXPECT_EQ(registry.Get<PlayerMagic>(neutral).playerType, PlayerMagic::k_NeutralPlayerType);
	EXPECT_EQ(registry.Get<PlayerMagic>(human).playerType, 1);
	EXPECT_EQ(registry.Get<PlayerMagic>(other).playerType, 0);
}

TEST(PlayerSystem, eachPlayerHasItsOwnState)
{
	PlayerSystem players;
	players.Alignment(PlayerNames::PLAYER_TWO).value = 0.5f;
	players.MagicWithoutEntity(PlayerNames::PLAYER_THREE).chantsUsed = 3.0f;
	EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_ONE).value, 0.0f);
	EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_TWO).value, 0.5f);
	EXPECT_EQ(players.MagicWithoutEntity(PlayerNames::PLAYER_ONE).chantsUsed, 0.0f);
	EXPECT_EQ(players.MagicWithoutEntity(PlayerNames::PLAYER_THREE).chantsUsed, 3.0f);
}

TEST(PlayerSystem, aNameOutOfRangeIsTheNeutralPlayer)
{
	PlayerSystem players;
	players.Alignment(PlayerNames::_COUNT).value = -0.25f;
	players.MagicWithoutEntity(PlayerNames::_COUNT).allMagicCheat = true;
	EXPECT_EQ(&players.Alignment(PlayerNames::_COUNT), &players.Alignment(PlayerNames::NEUTRAL));
	EXPECT_EQ(players.Alignment(PlayerNames::NEUTRAL).value, -0.25f);
	EXPECT_TRUE(players.MagicWithoutEntity(PlayerNames::NEUTRAL).allMagicCheat);
}

// As the original (docs/wiki/beings/towns.md#step-20-the-players-alignment-from-the-towns-desires): the people a
// town's desires share the alignment change by are those of the players with an entity and of the neutral player
TEST(PlayerSystem, thePlayersPopulationCountsThePlayingPlayersAndTheNeutralOne)
{
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	// no player service: nobody
	Locator::playerSystem::reset();
	Locator::entitiesRegistry::reset();
	EXPECT_EQ(magic::players::TotalPopulation(PlayerNames::PLAYER_ONE), 0u);
	EXPECT_EQ(magic::players::ActivePlayersPopulation(), 0u);

	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& players = static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_ONE);
	registry.Assign<Player>(registry.Create(), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(players.TotalPopulation(PlayerNames::PLAYER_ONE), 0u);
	magic::players::SetTotalPopulation(PlayerNames::PLAYER_ONE, 120);
	magic::players::SetTotalPopulation(PlayerNames::PLAYER_TWO, 64);
	// a player with no entity on the land is not counted, the neutral player always is
	magic::players::SetTotalPopulation(PlayerNames::PLAYER_THREE, 999);
	magic::players::SetTotalPopulation(PlayerNames::NEUTRAL, 33);
	EXPECT_EQ(players.TotalPopulation(PlayerNames::PLAYER_TWO), 64u);
	EXPECT_EQ(players.TotalPopulation(PlayerNames::_COUNT), 33u);
	EXPECT_EQ(magic::players::ActivePlayersPopulation(), 217u);
}

TEST(PlayerSystem, eachPlayerKeepsTheirRivalryAcrossLands)
{
	PlayerSystem players;
	EXPECT_EQ(players.Rivalry(PlayerNames::PLAYER_TWO).averageInfluencePower, 0.0f);
	EXPECT_EQ(players.Rivalry(PlayerNames::PLAYER_TWO).harmFrom.at(0), 0.0f);
	auto rivalry = players.Rivalry(PlayerNames::PLAYER_TWO);
	rivalry.harmFrom.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 2.5f;
	rivalry.averageInfluencePower = 40.0f;
	players.SetRivalry(PlayerNames::PLAYER_TWO, rivalry);
	// a land's clear and load keep it
	players.KeepForNextLand();
	players.ClearPlayers();
	players.ClearMagicWithoutEntity();
	EXPECT_EQ(players.Rivalry(PlayerNames::PLAYER_TWO).harmFrom.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 2.5f);
	EXPECT_EQ(players.Rivalry(PlayerNames::PLAYER_TWO).averageInfluencePower, 40.0f);
	EXPECT_EQ(players.Rivalry(PlayerNames::PLAYER_THREE).averageInfluencePower, 0.0f);
	// a name out of range is the neutral player's
	players.SetRivalry(PlayerNames::_COUNT, rivalry);
	EXPECT_EQ(players.Rivalry(PlayerNames::NEUTRAL).averageInfluencePower, 40.0f);
}

TEST(PlayerRivalry, HarmAddsUpByPlayerAndTheAverageGoesHalfwayToThePower)
{
	ecs::components::PlayerRivalry rivalry;
	ecs::player_rivalry::AddHarm(rivalry, PlayerNames::PLAYER_THREE, 0.25f);
	ecs::player_rivalry::AddHarm(rivalry, PlayerNames::PLAYER_THREE, 0.5f);
	ecs::player_rivalry::AddHarm(rivalry, PlayerNames::NEUTRAL, 1.0f);
	ecs::player_rivalry::AddHarm(rivalry, PlayerNames::_COUNT, 9.0f);
	EXPECT_EQ(rivalry.harmFrom.at(static_cast<size_t>(PlayerNames::PLAYER_THREE)), 0.75f);
	EXPECT_EQ(rivalry.harmFrom.at(static_cast<size_t>(PlayerNames::NEUTRAL)), 1.0f);
	EXPECT_EQ(rivalry.harmFrom.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0.0f);
	ecs::player_rivalry::NoteInfluencePower(rivalry, 860.0f);
	EXPECT_EQ(rivalry.averageInfluencePower, 430.0f);
	ecs::player_rivalry::NoteInfluencePower(rivalry, 860.0f);
	EXPECT_EQ(rivalry.averageInfluencePower, 645.0f);
	ecs::player_rivalry::NoteInfluencePower(rivalry, 0.0f);
	EXPECT_EQ(rivalry.averageInfluencePower, 322.5f);
}

TEST(PlayerSystem, aLandLoadClearsTheMagicButKeepsTheAlignment)
{
	PlayerSystem players;
	auto& magic = players.MagicWithoutEntity(PlayerNames::NEUTRAL);
	magic.remainder[3] = 2;
	magic.everEnabled[3] = true;
	magic.tribalPower[1] = 0.5f;
	magic.maxTribalPower[1] = 4.0f;
	players.Alignment(PlayerNames::NEUTRAL) = Alignment {.value = 0.75f, .pending = 0.125f};

	players.ClearMagicWithoutEntity();

	// the neutral player's type is not a land's: it stays
	EXPECT_EQ(magic.playerType, PlayerMagic::k_NeutralPlayerType);
	EXPECT_EQ(magic.remainder[3], 0);
	EXPECT_FALSE(magic.everEnabled[3]);
	EXPECT_EQ(magic.tribalPower[1], 1.0f);
	EXPECT_EQ(magic.maxTribalPower[1], 1.0f);
	EXPECT_EQ(players.Alignment(PlayerNames::NEUTRAL).value, 0.75f);
	EXPECT_EQ(players.Alignment(PlayerNames::NEUTRAL).pending, 0.125f);
}

TEST(PlayerSystem, thePlayerFunctionsUseTheLocatorsPlayerSystem)
{
	const test::RestoreService<Locator::playerSystem> restore;
	auto& fake = static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
	const test::RestoreService<Locator::entitiesRegistry> noRegistry;
	Locator::entitiesRegistry::reset();

	EXPECT_EQ(&magic::players::AlignmentOf(PlayerNames::PLAYER_FOUR), &fake.alignment);
	EXPECT_EQ(fake.alignmentAsked, PlayerNames::PLAYER_FOUR);
	// no player entity: the magic is the player system's
	EXPECT_EQ(&magic::players::MagicOf(PlayerNames::NEUTRAL), &fake.magic);
	EXPECT_EQ(fake.magicAsked, PlayerNames::NEUTRAL);
	magic::players::Reset();
	EXPECT_EQ(fake.clears, 1);
	// a land load also empties the list of the player entities
	EXPECT_EQ(fake.playerClears, 1);
}

TEST(PlayerSystem, eachTurnTheMostTribalPowerAtLeastOneBecomesTheTribalPower)
{
	// the turn's rule: the most when above 1, otherwise (and when not a number) 1
	EXPECT_EQ(magic::players::TribalPowerOfTurn(3.5f), 3.5f);
	EXPECT_EQ(magic::players::TribalPowerOfTurn(1.0f), 1.0f);
	EXPECT_EQ(magic::players::TribalPowerOfTurn(0.25f), 1.0f);
	EXPECT_EQ(magic::players::TribalPowerOfTurn(-2.0f), 1.0f);
	EXPECT_EQ(magic::players::TribalPowerOfTurn(std::numeric_limits<float>::quiet_NaN()), 1.0f);

	const test::RestoreService<Locator::playerSystem> restore;
	auto& fake = static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
	const test::RestoreService<Locator::entitiesRegistry> noRegistry;
	Locator::entitiesRegistry::reset();
	// a wonder of tribe 4 raised the most power by 2.5; one of tribe 1 was taken off after the turn kept 1
	fake.magic.maxTribalPower[4] = 3.5f;
	fake.magic.maxTribalPower[1] = 0.25f;

	magic::players::ProcessTribalPowers(PlayerNames::PLAYER_ONE);

	EXPECT_EQ(fake.magicAsked, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(fake.magic.tribalPower[4], 3.5f);
	EXPECT_EQ(fake.magic.maxTribalPower[4], 3.5f);
	EXPECT_EQ(fake.magic.tribalPower[1], 1.0f);
	EXPECT_EQ(fake.magic.maxTribalPower[1], 1.0f);
	EXPECT_EQ(fake.magic.tribalPower[0], 1.0f);
}

TEST(PlayerSystem, aWonderGivenToAPlayerWhoseTurnHasGoneRaisesItsTribalPowerAtItsNextTurn)
{
	const test::RestoreService<Locator::playerSystem> restore;
	auto& fake = static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
	fake.perPlayer = true;
	const test::RestoreService<Locator::entitiesRegistry> noRegistry;
	Locator::entitiesRegistry::reset();
	auto& first = fake.magics.at(static_cast<size_t>(PlayerNames::PLAYER_ONE));
	auto& neutral = fake.magics.at(static_cast<size_t>(PlayerNames::NEUTRAL));
	// a Norse wonder of power 2 stands in a neutral town
	neutral.maxTribalPower[7] = 3.0f;

	// the turn: the first player's turn ends, then the neutral one's takes the town over and its wonder goes to the
	// first player, then the neutral player's turn ends
	magic::players::ProcessTribalPowers(PlayerNames::PLAYER_ONE);
	neutral.maxTribalPower[7] = neutral.maxTribalPower[7] - 2.0f;
	first.maxTribalPower[7] = 2.0f + first.maxTribalPower[7];
	magic::players::ProcessTribalPowers(PlayerNames::NEUTRAL);

	// the neutral player has lost it at once; the first player has the most already, its tribal power waits
	EXPECT_EQ(neutral.tribalPower[7], 1.0f);
	EXPECT_EQ(neutral.maxTribalPower[7], 1.0f);
	EXPECT_EQ(first.maxTribalPower[7], 3.0f);
	EXPECT_EQ(first.tribalPower[7], 1.0f);

	// until the first player's next turn
	magic::players::ProcessTribalPowers(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(first.tribalPower[7], 3.0f);
	EXPECT_EQ(first.maxTribalPower[7], 3.0f);
	// another player's turn leaves the first one's alone
	first.maxTribalPower[0] = 4.0f;
	magic::players::ProcessTribalPowers(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(first.tribalPower[0], 1.0f);
}

TEST(PlayerSystem, theCreaturesLocalPlayerIsThePlayerSystems)
{
	const test::RestoreService<Locator::playerSystem> restore;
	// the game's player service: the first player's interface
	EXPECT_EQ(PlayerSystem().GetLocalPlayer(), PlayerNames::PLAYER_ONE);
	// with no player service, the first player
	Locator::playerSystem::reset();
	EXPECT_EQ(creature::LocalPlayer(), PlayerNames::PLAYER_ONE);
	// with one, whichever player it names
	auto& fake = static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
	EXPECT_EQ(creature::LocalPlayer(), PlayerNames::PLAYER_ONE);
	fake.local = PlayerNames::PLAYER_THREE;
	EXPECT_EQ(creature::LocalPlayer(), PlayerNames::PLAYER_THREE);
}

TEST(PlayerSystem, aLandLoadListsOnlyThePlayersTheNewLandMakes)
{
	const test::RestoreService<Locator::entitiesRegistry> restore;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	PlayerSystem players;
	const auto lastLand = registry.Create();
	registry.Assign<Player>(lastLand, PlayerNames::PLAYER_ONE);
	players.AddPlayer(lastLand);
	ASSERT_EQ(players.GetPlayer(PlayerNames::PLAYER_ONE), lastLand);

	// the next land: the registry is cleared and the land makes its own PLAYER_ONE
	players.ClearPlayers();
	registry.Reset();
	const auto newLand = registry.Create();
	registry.Assign<Player>(newLand, PlayerNames::PLAYER_ONE);
	players.AddPlayer(newLand);
	players.RegisterPlayers();

	EXPECT_EQ(players.GetPlayer(PlayerNames::PLAYER_ONE), newLand);
	EXPECT_THROW(static_cast<void>(players.GetPlayer(PlayerNames::PLAYER_TWO)), std::out_of_range);
}

TEST(PlayerSystem, aComputerPlayerTheLandScriptMakesIsListed)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& players = static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());

	// Land 2's TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1), after LOAD_LANDSCAPE
	const auto human = registry.Create();
	registry.Assign<Player>(human, PlayerNames::PLAYER_ONE);
	players.AddPlayer(human);
	FeatureScriptCommands::ToggleComputerPlayer("PLAYER_TWO", 1);

	const auto entity = players.GetPlayer(PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(registry.Valid(entity));
	EXPECT_EQ(registry.Get<Player>(entity).name, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(registry.Get<PlayerMagic>(entity).playerType, PlayerMagic::k_ComputerPlayerType);
	EXPECT_EQ(players.GetPlayer(PlayerNames::PLAYER_ONE), human);
}

TEST(PlayerSystem, switchingTheComputerGodOnMakesAComputerPlayerAndOffLeavesTheType)
{
	using magic::players::TypeAfterComputerToggle;
	// on: a player not playing, or a human one, becomes a computer player
	EXPECT_EQ(TypeAfterComputerToggle(0, true), PlayerMagic::k_ComputerPlayerType);
	EXPECT_EQ(TypeAfterComputerToggle(1, true), PlayerMagic::k_ComputerPlayerType);
	// on twice: still a computer player
	EXPECT_EQ(TypeAfterComputerToggle(TypeAfterComputerToggle(0, true), true), PlayerMagic::k_ComputerPlayerType);
	// off: the type stays as it was
	EXPECT_EQ(TypeAfterComputerToggle(0, false), 0);
	EXPECT_EQ(TypeAfterComputerToggle(1, false), 1);
	EXPECT_EQ(TypeAfterComputerToggle(PlayerMagic::k_ComputerPlayerType, false), PlayerMagic::k_ComputerPlayerType);
}

TEST(PlayerSystem, theComputerPlayerToggleReusesAPlayerThatExists)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& players = static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());

	// off on a player no script has named yet: the player is made and is not a computer player
	FeatureScriptCommands::ToggleComputerPlayer("PLAYER_THREE", 0);
	const auto three = players.GetPlayer(PlayerNames::PLAYER_THREE);
	EXPECT_EQ(registry.Get<PlayerMagic>(three).playerType, 0);

	// on, then on again: the same player, a computer player, and no second entity
	FeatureScriptCommands::ToggleComputerPlayer("PLAYER_THREE", 1);
	FeatureScriptCommands::ToggleComputerPlayer("PLAYER_THREE", 1);
	EXPECT_EQ(players.GetPlayer(PlayerNames::PLAYER_THREE), three);
	EXPECT_EQ(registry.Get<PlayerMagic>(three).playerType, PlayerMagic::k_ComputerPlayerType);
	EXPECT_EQ(registry.Size<Player>(), 1u);

	// off again: still a computer player
	FeatureScriptCommands::ToggleComputerPlayer("PLAYER_THREE", 0);
	EXPECT_EQ(registry.Get<PlayerMagic>(three).playerType, PlayerMagic::k_ComputerPlayerType);
	EXPECT_EQ(registry.Size<Player>(), 1u);
}

TEST(PlayerSystem, theLocalPlayersInterfaceStateLastsAcrossLands)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& players = static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());
	auto& keptAlignment = players.InterfaceAlignmentWithoutEntity();
	auto& keptCrossing = players.InfluenceCrossingWithoutEntity();

	// before the first land no entity carries them: they are the player system's
	EXPECT_EQ(&magic::players::LocalInterfaceAlignment(), &keptAlignment);
	EXPECT_EQ(&magic::players::LocalInfluenceCrossing(), &keptCrossing);
	keptAlignment.value = 0.25f;
	keptCrossing.haveState = true;
	keptCrossing.wasInside[2] = true;
	keptCrossing.previousPoint = glm::vec3(3.0f, 0.0f, 4.0f);

	// a computer player's entity takes none; the local player's takes the values kept for it
	const auto computer = PlayerArchetype::Create(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(registry.TryGet<InterfaceAlignment>(computer), nullptr);
	EXPECT_EQ(registry.TryGet<InfluenceCrossing>(computer), nullptr);
	const auto local = PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	ASSERT_NE(registry.TryGet<InterfaceAlignment>(local), nullptr);
	ASSERT_NE(registry.TryGet<InfluenceCrossing>(local), nullptr);
	EXPECT_EQ(&magic::players::LocalInterfaceAlignment(), registry.TryGet<InterfaceAlignment>(local));
	EXPECT_EQ(&magic::players::LocalInfluenceCrossing(), registry.TryGet<InfluenceCrossing>(local));
	EXPECT_EQ(magic::players::LocalInterfaceAlignment().value, 0.25f);
	EXPECT_TRUE(magic::players::LocalInfluenceCrossing().haveState);
	EXPECT_TRUE(magic::players::LocalInfluenceCrossing().wasInside[2]);
	EXPECT_EQ(magic::players::LocalInfluenceCrossing().previousPoint.z, 4.0f);
	// a second entity of the local player takes none: there is one copy only
	const auto second = PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(registry.TryGet<InterfaceAlignment>(second), nullptr);
	EXPECT_EQ(registry.TryGet<InfluenceCrossing>(second), nullptr);

	// the land's clear moves the entity's values to the player system, before the magic's reset
	magic::players::LocalInterfaceAlignment().value = 0.75f;
	magic::players::LocalInfluenceCrossing().previousPoint = glm::vec3(5.0f, 0.0f, 6.0f);
	players.KeepForNextLand();
	magic::players::Reset();
	EXPECT_EQ(registry.TryGet<InterfaceAlignment>(local), nullptr);
	EXPECT_EQ(registry.TryGet<InfluenceCrossing>(local), nullptr);
	EXPECT_EQ(&magic::players::LocalInterfaceAlignment(), &keptAlignment);
	EXPECT_EQ(&magic::players::LocalInfluenceCrossing(), &keptCrossing);
	EXPECT_EQ(keptAlignment.value, 0.75f);
	EXPECT_EQ(keptCrossing.previousPoint.x, 5.0f);
	EXPECT_EQ(keptCrossing.previousPoint.z, 6.0f);
	EXPECT_TRUE(keptCrossing.wasInside[2]);

	// the next land's local player takes them back
	registry.Reset();
	const auto nextLand = PlayerArchetype::Create(PlayerNames::PLAYER_ONE);
	ASSERT_NE(registry.TryGet<InterfaceAlignment>(nextLand), nullptr);
	EXPECT_EQ(registry.TryGet<InterfaceAlignment>(nextLand)->value, 0.75f);
	EXPECT_EQ(registry.TryGet<InfluenceCrossing>(nextLand)->previousPoint.z, 6.0f);
}

TEST(PlayerSystem, whatIsKeptGoesOnlyToTheLocalPlayersFirstEntity)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	PlayerSystem players;
	players.InterfaceAlignmentWithoutEntity().value = 0.5f;
	players.InfluenceCrossingWithoutEntity().wasInside[1] = true;

	// neither a thing that is no player nor a computer player takes them
	players.TakeUpKept(registry.Create());
	const auto computer = registry.Create();
	registry.Assign<Player>(computer, PlayerNames::PLAYER_TWO);
	players.TakeUpKept(computer);
	EXPECT_EQ(registry.Size<InterfaceAlignment>(), 0u);
	EXPECT_EQ(registry.Size<InfluenceCrossing>(), 0u);

	const auto local = registry.Create();
	registry.Assign<Player>(local, PlayerNames::PLAYER_ONE);
	players.TakeUpKept(local);
	ASSERT_NE(registry.TryGet<InterfaceAlignment>(local), nullptr);
	EXPECT_EQ(registry.Get<InterfaceAlignment>(local).value, 0.5f);
	EXPECT_TRUE(registry.Get<InfluenceCrossing>(local).wasInside[1]);

	// kept again as the land is cleared; with none carried, or no registry, the copy stays as it is
	registry.Get<InterfaceAlignment>(local).value = -0.25f;
	players.KeepForNextLand();
	EXPECT_EQ(registry.TryGet<InterfaceAlignment>(local), nullptr);
	EXPECT_EQ(registry.TryGet<InfluenceCrossing>(local), nullptr);
	EXPECT_EQ(players.InterfaceAlignmentWithoutEntity().value, -0.25f);
	players.KeepForNextLand();
	Locator::entitiesRegistry::reset();
	players.KeepForNextLand();
	EXPECT_EQ(players.InterfaceAlignmentWithoutEntity().value, -0.25f);
	EXPECT_TRUE(players.InfluenceCrossingWithoutEntity().wasInside[1]);
}

class PlayerSystemLands: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Player>(entity, name);
		return entity;
	}

	/// The land goes, its players with it, as a new land is loaded
	static void ClearLand() { Locator::entitiesRegistry::value().Reset(); }

	// the registry the tests before had is put back after the fixture's own
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	PlayerSystem _players;
};

TEST_F(PlayerSystemLands, KnowsTheLandsPlayers)
{
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_TWO), two);
}

TEST_F(PlayerSystemLands, TheFirstPlayerMadeKeepsItsName)
{
	// The land is set up with its player added, then every player on it registered
	const auto added = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(added);
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), added);
}

TEST_F(PlayerSystemLands, ANewLandsPlayersTakeTheLastLandsPlaces)
{
	MakePlayer(PlayerNames::PLAYER_ONE);
	MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();

	ClearLand();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(one);
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();

	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_TWO), two);
	EXPECT_TRUE(registry.Valid(_players.GetPlayer(PlayerNames::PLAYER_ONE)));
	EXPECT_TRUE(registry.Valid(_players.GetPlayer(PlayerNames::PLAYER_TWO)));
}

TEST_F(PlayerSystemLands, APlayerNotOnTheNewLandIsForgotten)
{
	MakePlayer(PlayerNames::PLAYER_THREE);
	_players.RegisterPlayers();

	ClearLand();
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();

	EXPECT_THROW(static_cast<void>(_players.GetPlayer(PlayerNames::PLAYER_THREE)), std::out_of_range);
}

TEST_F(PlayerSystemLands, TheHandsVirtualInfluenceGoesOnToTheNextLandAsItWasLeft)
{
	// Land 1's script switched it off: no strength left, and the last place the hand was in influence
	auto& registry = Locator::entitiesRegistry::value();
	const auto land1 = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(land1);
	_players.AddPlayer(land1);
	const map_coords::MapCoords anchor {.x = 123456, .z = 654321, .altitude = 2.5f};
	registry.AssignState<VirtualInfluence>(
	    land1, VirtualInfluence {.state = {.anchor = anchor, .lastInsideTurn = 40, .fraction = 0.0f, .disabled = true}});
	_players.RegisterPlayers();

	// As the land is cleared the players keep what is theirs: it moves off the last land's entity, then the land goes
	_players.KeepForNextLand();
	EXPECT_EQ(registry.TryGet<VirtualInfluence>(land1), nullptr);
	_players.ClearPlayers();
	ClearLand();
	// Land 2's computer player takes none; its local player takes it as it was, still with no strength
	const auto computer = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.TakeUpKept(computer);
	_players.AddPlayer(computer);
	EXPECT_EQ(registry.TryGet<VirtualInfluence>(computer), nullptr);
	const auto land2 = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(land2);
	_players.AddPlayer(land2);
	const auto* carried = registry.TryGet<VirtualInfluence>(land2);
	ASSERT_NE(carried, nullptr);
	EXPECT_EQ(carried->state.fraction, 0.0f);
	EXPECT_TRUE(carried->state.disabled);
	EXPECT_EQ(carried->state.anchor, anchor);
	EXPECT_EQ(carried->state.lastInsideTurn, 40u);
	// a second entity of the local player takes none: there is one copy only
	const auto second = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(second);
	EXPECT_EQ(registry.TryGet<VirtualInfluence>(second), nullptr);
}

TEST_F(PlayerSystemLands, AFirstLandsLocalPlayerHasNoVirtualInfluenceKeptForIt)
{
	// the first land's start keeps what the players have, and nothing is there yet
	_players.KeepForNextLand();
	const auto local = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(local);
	_players.AddPlayer(local);
	EXPECT_EQ(Locator::entitiesRegistry::value().TryGet<VirtualInfluence>(local), nullptr);
}

TEST_F(PlayerSystemLands, TheMapsClearReachesTheKeptVirtualInfluenceAndANewGameMakesItAfresh)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a land whose hand had none yet: the clear finds none to work on
	_players.KeepForNextLand();
	EXPECT_EQ(_players.VirtualInfluenceAtClearMap(false), nullptr);
	// a land change: the clear works on the one kept off the last land's entity, and the next land's local player takes
	// it up as the clear left it
	const auto land1 = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(land1);
	registry.AssignState<VirtualInfluence>(land1, VirtualInfluence {.state = {.lastInsideTurn = 40, .fraction = 0.5f}});
	_players.KeepForNextLand();
	auto* kept = _players.VirtualInfluenceAtClearMap(false);
	ASSERT_NE(kept, nullptr);
	EXPECT_EQ(kept->state.lastInsideTurn, 40u);
	kept->state.fraction = 0.25f;
	_players.ClearPlayers();
	ClearLand();
	const auto land2 = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(land2);
	ASSERT_NE(registry.TryGet<VirtualInfluence>(land2), nullptr);
	EXPECT_EQ(registry.Get<VirtualInfluence>(land2).state.fraction, 0.25f);
	// a new game: the clear works on one with the start values, whatever the last land left
	_players.AddPlayer(land2);
	registry.Get<VirtualInfluence>(land2).state = {.lastInsideTurn = 70, .fraction = 0.0f, .disabled = true};
	_players.KeepForNextLand();
	kept = _players.VirtualInfluenceAtClearMap(true);
	ASSERT_NE(kept, nullptr);
	EXPECT_EQ(kept->state.fraction, 1.0f);
	EXPECT_EQ(kept->state.lastInsideTurn, 0u);
	EXPECT_FALSE(kept->state.disabled);
}

/// The primary creature is the one creature the game keeps for each player: on the player's entity, or in the player
/// service for a player without one (SetPrimaryCreature)
class PlayerSystemCreatures: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	static entt::entity MakeCreature(PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Creature>(entity, Creature {.owner = owner});
		return entity;
	}
	static entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Player>(entity, name);
		return entity;
	}

	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	PlayerSystem _players;
};

TEST_F(PlayerSystemCreatures, aPlayersCreatureIsKeptOnTheirEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto player = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(player);
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::PLAYER_ONE).has_value());

	_players.SetPrimaryCreature(PlayerNames::PLAYER_ONE, creature);
	ASSERT_NE(registry.TryGet<PlayerCreatureSlot>(player), nullptr);
	EXPECT_EQ(registry.Get<PlayerCreatureSlot>(player).creature, std::optional(creature));
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(creature));
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::PLAYER_TWO).has_value());

	_players.SetPrimaryCreature(PlayerNames::PLAYER_ONE, std::nullopt);
	EXPECT_FALSE(registry.Get<PlayerCreatureSlot>(player).creature.has_value());
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::PLAYER_ONE).has_value());
}

TEST_F(PlayerSystemCreatures, aPlayerWithoutAnEntityKeepsTheirCreatureInTheService)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = MakeCreature(PlayerNames::PLAYER_TWO);
	const auto wild = MakeCreature(PlayerNames::NEUTRAL);
	_players.SetPrimaryCreature(PlayerNames::PLAYER_TWO, creature);
	_players.SetPrimaryCreature(PlayerNames::NEUTRAL, wild);

	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_TWO), std::optional(creature));
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::NEUTRAL), std::optional(wild));
	EXPECT_EQ(registry.Size<PlayerCreatureSlot>(), 0u);
}

TEST_F(PlayerSystemCreatures, theCreatureMovesOntoAPlayerEntityMadeLater)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = MakeCreature(PlayerNames::PLAYER_TWO);
	const auto later = MakeCreature(PlayerNames::PLAYER_THREE);
	_players.SetPrimaryCreature(PlayerNames::PLAYER_TWO, creature);
	_players.SetPrimaryCreature(PlayerNames::PLAYER_THREE, later);

	// a script's TOGGLE_COMPUTER_PLAYER adds the entity, a land's start lists the ones it finds
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.AddPlayer(two);
	const auto three = MakePlayer(PlayerNames::PLAYER_THREE);
	_players.RegisterPlayers();

	ASSERT_NE(registry.TryGet<PlayerCreatureSlot>(two), nullptr);
	EXPECT_EQ(registry.Get<PlayerCreatureSlot>(two).creature, std::optional(creature));
	ASSERT_NE(registry.TryGet<PlayerCreatureSlot>(three), nullptr);
	EXPECT_EQ(registry.Get<PlayerCreatureSlot>(three).creature, std::optional(later));
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_TWO), std::optional(creature));
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_THREE), std::optional(later));
	// taken up, not kept twice: emptied on the entity, it stays empty
	_players.SetPrimaryCreature(PlayerNames::PLAYER_TWO, std::nullopt);
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::PLAYER_TWO).has_value());
}

TEST_F(PlayerSystemCreatures, aLandLoadForgetsEveryPlayersCreature)
{
	const auto creature = MakeCreature(PlayerNames::PLAYER_TWO);
	const auto wild = MakeCreature(PlayerNames::NEUTRAL);
	_players.SetPrimaryCreature(PlayerNames::PLAYER_TWO, creature);
	_players.SetPrimaryCreature(PlayerNames::NEUTRAL, wild);

	_players.ClearPlayers();

	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::NEUTRAL).has_value());
}

TEST_F(PlayerSystemCreatures, aNameOutOfRangeIsTheNeutralPlayersCreature)
{
	const auto wild = MakeCreature(PlayerNames::NEUTRAL);
	_players.SetPrimaryCreature(PlayerNames::_COUNT, wild);
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::NEUTRAL), std::optional(wild));
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::_COUNT), std::optional(wild));
}

TEST_F(PlayerSystemCreatures, APrimaryGoneOrGivenAwayLeavesNone)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto given = MakeCreature(PlayerNames::PLAYER_ONE);
	_players.SetPrimaryCreature(PlayerNames::PLAYER_ONE, given);
	registry.Get<Creature>(given).owner = PlayerNames::PLAYER_TWO;
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::PLAYER_ONE).has_value());

	const auto gone = MakeCreature(PlayerNames::PLAYER_THREE);
	_players.SetPrimaryCreature(PlayerNames::PLAYER_THREE, gone);
	registry.Destroy(gone);
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::PLAYER_THREE).has_value());
}

TEST_F(PlayerSystemCreatures, ThePlayersCreaturesAreListedInTheOrderMade)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto player = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(player);
	const auto first = MakeCreature(PlayerNames::PLAYER_ONE);
	const auto second = MakeCreature(PlayerNames::PLAYER_ONE);
	_players.AddCreature(second);
	_players.AddCreature(first);
	_players.AddCreature(second);
	// a player not made on the land keeps no list
	_players.AddCreature(MakeCreature(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(registry.Get<PlayerCreatures>(player).acquired, (std::vector<entt::entity> {second, first}));
	// the list does not decide the primary: the last one added is theirs
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(second));
}

TEST_F(PlayerSystemCreatures, theNewestCreatureMadeIsThePlayers)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto player = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(player);
	const auto first = MakeCreature(PlayerNames::PLAYER_ONE);
	_players.AddCreature(first);
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(first));
	const auto second = MakeCreature(PlayerNames::PLAYER_ONE);
	_players.AddCreature(second);
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(second));
	EXPECT_EQ(registry.Get<PlayerCreatureSlot>(player).creature, std::optional(second));
	// a player without an entity the same way
	const auto theirs = MakeCreature(PlayerNames::PLAYER_TWO);
	const auto later = MakeCreature(PlayerNames::PLAYER_TWO);
	_players.AddCreature(theirs);
	_players.AddCreature(later);
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::PLAYER_TWO), std::optional(later));
	// a creature of no player is nobody's
	_players.AddCreature(MakeCreature(PlayerNames::_COUNT));
	EXPECT_FALSE(_players.GetPrimaryCreature(PlayerNames::NEUTRAL).has_value());
}

TEST_F(PlayerSystemCreatures, theNeutralPlayerHasACreatureToo)
{
	const auto first = MakeCreature(PlayerNames::NEUTRAL);
	const auto last = MakeCreature(PlayerNames::NEUTRAL);
	_players.AddCreature(first);
	_players.AddCreature(last);
	EXPECT_EQ(_players.GetPrimaryCreature(PlayerNames::NEUTRAL), std::optional(last));
}

TEST(PlayerSystem, aScriptsPlayerNameIsReadIgnoringCaseAndAnUnknownOneIsTheNeutralPlayer)
{
	EXPECT_EQ(FeatureScriptCommands::PlayerFromText("PLAYER_ONE"), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(FeatureScriptCommands::PlayerFromText("player_two"), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(FeatureScriptCommands::PlayerFromText("Neutral"), PlayerNames::NEUTRAL);
	EXPECT_EQ(FeatureScriptCommands::PlayerFromText("NEUTRAL"), PlayerNames::NEUTRAL);
	EXPECT_EQ(FeatureScriptCommands::PlayerFromText("NOBODY"), PlayerNames::NEUTRAL);
	EXPECT_EQ(FeatureScriptCommands::PlayerFromText(""), PlayerNames::NEUTRAL);
}

namespace
{
/// Records what a land script asks of the computer players
class FakeComputerPlayers final: public ecs::systems::ComputerPlayerSystemInterface
{
public:
	void SwitchOn(PlayerNames, bool) override {}
	void SetCreatureLike(PlayerNames, PlayerNames) override {}
	void ProcessTurn() override {}
	void SetPersonality(PlayerNames player, std::string_view name, float value) override
	{
		personalityPlayer = player;
		personalityName = name;
		personality = value;
	}
	void LoadPersonality(PlayerNames player, std::string_view path) override
	{
		loadedPlayer = player;
		loadedPath = path;
	}

	std::optional<PlayerNames> personalityPlayer;
	std::string personalityName;
	float personality {0.0f};
	std::optional<PlayerNames> loadedPlayer;
	std::string loadedPath;
};
} // namespace

TEST(PlayerSystem, theLandScriptsPersonalityKeywordsReachTheComputerPlayers)
{
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputerPlayers;
	auto& fake = static_cast<FakeComputerPlayers&>(Locator::computerPlayerSystem::emplace<FakeComputerPlayers>());

	// the player by its number, and the second argument's text as the file's name
	FeatureScriptCommands::LoadComputerPlayerPersonality(1, "Khazar.personality");
	EXPECT_EQ(fake.loadedPlayer, std::optional<PlayerNames>(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(fake.loadedPath, "Khazar.personality");
	// a number that names no player reads nothing
	fake.loadedPlayer.reset();
	FeatureScriptCommands::LoadComputerPlayerPersonality(8, "other");
	FeatureScriptCommands::LoadComputerPlayerPersonality(-1, "other");
	EXPECT_FALSE(fake.loadedPlayer.has_value());

	// the player by its name, the node's name as it is and the weight
	FeatureScriptCommands::SetComputerPlayerPersonality("PLAYER_TWO", "ExpandInfluence", 0.2f);
	EXPECT_EQ(fake.personalityPlayer, std::optional<PlayerNames>(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(fake.personalityName, "ExpandInfluence");
	EXPECT_EQ(fake.personality, 0.2f);
	// any other name is the neutral player's
	FeatureScriptCommands::SetComputerPlayerPersonality("NOBODY", "ExpandInfluence", 0.5f);
	EXPECT_EQ(fake.personalityPlayer, std::optional<PlayerNames>(PlayerNames::NEUTRAL));
}
