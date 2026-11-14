/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <map>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/GameEndRules.h"
#include "ECS/Systems/Implementations/GameEndSystem.h"

using namespace openblack;
using namespace openblack::game_end;
using openblack::ecs::components::GameStanding;

namespace
{
struct FakeGod
{
	bool human {false};
	bool computer {false};
	bool temple {true};
	bool creature {true};
	bool mind {false};
	uint32_t godsDefeated {0};
	/// Its place in the order of the gods that left, 0 for none
	uint32_t place {0};
	GameStanding standing;
};

class FakeWorld final: public World
{
public:
	std::map<PlayerNames, FakeGod> gods;
	uint32_t placesCounted {0};

	[[nodiscard]] std::vector<PlayerNames> ActiveGods() const override
	{
		std::vector<PlayerNames> active;
		for (const auto& [player, god] : gods)
		{
			if (god.human || god.computer)
			{
				active.push_back(player);
			}
		}
		return active;
	}
	[[nodiscard]] bool IsHuman(PlayerNames player) const override { return gods.at(player).human; }
	[[nodiscard]] bool HasTemple(PlayerNames player) const override { return gods.at(player).temple; }
	[[nodiscard]] GameStanding& Standing(PlayerNames player) override { return gods.at(player).standing; }
	void RemoveCreature(PlayerNames player) override { gods.at(player).creature = false; }
	void CountGodDefeated(PlayerNames player) override { ++gods.at(player).godsDefeated; }
	void TakeNextPlace(PlayerNames player) override { gods.at(player).place = ++placesCounted; }
	void StopMind(PlayerNames player) override { gods.at(player).mind = false; }
};

constexpr auto k_One = PlayerNames::PLAYER_ONE;
constexpr auto k_Two = PlayerNames::PLAYER_TWO;
constexpr auto k_Three = PlayerNames::PLAYER_THREE;
constexpr auto k_Five = PlayerNames::PLAYER_FIVE;

/// The player and computer gods, each with its mind going
FakeWorld PlayerAndComputers(size_t computers)
{
	FakeWorld world;
	world.gods[k_One] = {.human = true};
	for (size_t i = 1; i <= computers; ++i)
	{
		world.gods[static_cast<PlayerNames>(i)] = {.computer = true, .mind = true};
	}
	return world;
}
} // namespace

TEST(GameEnd, NothingEndsAGameThatIsNotASkirmish)
{
	auto world = PlayerAndComputers(1);
	world.gods[k_Two].temple = false;
	State state;
	const auto events = ProcessTurn(world, false, state);
	EXPECT_TRUE(events.empty());
	EXPECT_FALSE(world.gods[k_Two].standing.out);
	EXPECT_TRUE(world.gods[k_Two].creature);
	EXPECT_FALSE(state.decided);
}

TEST(GameEnd, AGodWithATempleStaysIn)
{
	auto world = PlayerAndComputers(2);
	State state;
	EXPECT_TRUE(ProcessTurn(world, true, state).empty());
	EXPECT_FALSE(world.gods[k_Two].standing.out);
	EXPECT_TRUE(world.gods[k_Two].mind);
}

TEST(GameEnd, TheLastComputerGodWithoutATempleDecidesTheGame)
{
	auto world = PlayerAndComputers(1);
	world.gods[k_Two].temple = false;
	State state;
	const auto events = ProcessTurn(world, true, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_EQ(events[0].god, k_Two);
	EXPECT_TRUE(state.decided);
	// out, its creature gone, counted out once and its mind stopped
	EXPECT_TRUE(world.gods[k_Two].standing.out);
	EXPECT_TRUE(world.gods[k_Two].standing.finished);
	EXPECT_FALSE(world.gods[k_Two].creature);
	EXPECT_FALSE(world.gods[k_Two].mind);
	// in a skirmish the player counts a computer god defeated, and as the last god in takes the first place
	EXPECT_EQ(world.gods[k_One].godsDefeated, 1u);
	EXPECT_EQ(world.gods[k_One].place, 1u);
	// a computer god takes no place
	EXPECT_EQ(world.gods[k_Two].place, 0u);
}

TEST(GameEnd, WithThreeGodsTheGameGoesOnAfterTheFirstIsOut)
{
	auto world = PlayerAndComputers(2);
	world.gods[k_Three].temple = false;
	State state;
	auto events = ProcessTurn(world, true, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOut);
	EXPECT_EQ(events[0].god, k_Three);
	EXPECT_FALSE(state.decided);
	EXPECT_EQ(world.gods[k_One].godsDefeated, 1u);
	EXPECT_EQ(world.gods[k_One].place, 0u);
	// a god is counted out once only
	EXPECT_TRUE(ProcessTurn(world, true, state).empty());
	EXPECT_EQ(world.gods[k_One].godsDefeated, 1u);
	// then the second computer god goes
	world.gods[k_Two].temple = false;
	events = ProcessTurn(world, true, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_EQ(events[0].god, k_Two);
	EXPECT_EQ(world.gods[k_One].godsDefeated, 2u);
	EXPECT_EQ(world.gods[k_One].place, 1u);
}

TEST(GameEnd, ThePlayerOutWithTwoGodsLeftLetsTheGameGoOn)
{
	auto world = PlayerAndComputers(2);
	world.gods[k_One].temple = false;
	State state;
	const auto events = ProcessTurn(world, true, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOut);
	EXPECT_EQ(events[0].god, k_One);
	EXPECT_FALSE(state.decided);
	// the player takes the first place out; no human is left in to count it
	EXPECT_EQ(world.gods[k_One].place, 1u);
	EXPECT_TRUE(world.gods[k_Two].mind);
}

TEST(GameEnd, ThePlayerOutAgainstOneGodDecidesTheGame)
{
	auto world = PlayerAndComputers(1);
	world.gods[k_One].temple = false;
	State state;
	const auto events = ProcessTurn(world, true, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_EQ(events[0].god, k_One);
	EXPECT_TRUE(state.decided);
	EXPECT_EQ(world.gods[k_One].place, 1u);
}

TEST(GameEnd, NothingRunsOnceTheGameIsDecided)
{
	auto world = PlayerAndComputers(2);
	world.gods[k_Two].temple = false;
	world.gods[k_Three].temple = false;
	State state;
	const auto events = ProcessTurn(world, true, state);
	// both are out in the same turn, so each is counted out with one god left
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_EQ(events[1].kind, Event::Kind::GodOutAndDecided);
	world.gods[k_One].temple = false;
	EXPECT_TRUE(ProcessTurn(world, true, state).empty());
	EXPECT_FALSE(world.gods[k_One].standing.out);
}

TEST(GameEnd, PlayersFiveToSevenNeverLoseByTheTempleRule)
{
	auto world = PlayerAndComputers(1);
	world.gods[k_Five] = {.computer = true, .temple = false, .mind = true};
	State state;
	EXPECT_TRUE(ProcessTurn(world, true, state).empty());
	EXPECT_FALSE(world.gods[k_Five].standing.out);
	EXPECT_TRUE(world.gods[k_Five].creature);
}

TEST(GameEnd, AnUnplayedPlayerTakesNoPart)
{
	auto world = PlayerAndComputers(1);
	world.gods[k_Three] = {.temple = false};
	State state;
	EXPECT_TRUE(ProcessTurn(world, true, state).empty());
	EXPECT_FALSE(world.gods[k_Three].standing.out);
}

TEST(GameEnd, OutsideASkirmishAComputerGodOutCountsForNoHuman)
{
	auto world = PlayerAndComputers(2);
	CountOut(world, k_Three, false);
	EXPECT_EQ(world.gods[k_One].godsDefeated, 0u);
	CountOut(world, k_Three, true);
	EXPECT_EQ(world.gods[k_One].godsDefeated, 1u);
}

TEST(GameEndSystem, KeepsTheEventsUntilTakenAndForgetsThemOnANewLand)
{
	// each god out is logged to the game's log
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	auto owned = std::make_unique<FakeWorld>(PlayerAndComputers(2));
	auto& world = *owned;
	ecs::systems::GameEndSystem system(std::move(owned), true);
	world.gods[k_Three].temple = false;
	system.ProcessTurn();
	world.gods[k_Two].temple = false;
	system.ProcessTurn();
	EXPECT_TRUE(system.IsDecided());
	const auto events = system.TakeEvents();
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].god, k_Three);
	EXPECT_EQ(events[1].god, k_Two);
	EXPECT_TRUE(system.TakeEvents().empty());
	system.Reset();
	EXPECT_FALSE(system.IsDecided());
}
