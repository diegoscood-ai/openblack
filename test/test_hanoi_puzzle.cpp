/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The beach temple puzzle (ecs::hanoi_puzzle, through ecs/PuzzleGames.h as the scripts reach it) in a small world: the
// test's registry, the world's lists with the real map cells, zeroed tables and no land (the ground at 0). Each service
// is put back when the test ends. See docs/wiki/world/beach-temple-puzzle.md

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>

#include <array>
#include <memory>
#include <optional>

#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/HanoiPuzzle.h"
#include "ECS/HanoiRules.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Land 2's beach temple: the puzzle's spot, as HanoiFlood makes it, and its half turn
constexpr glm::vec3 k_Spot {2386.968f, 0.0f, 2633.286f};
/// The bases stand 20 m apart towards -z from the spot
constexpr float k_BaseSpacing = 20.0f;
/// How near a drawn position must be to the one worked out (the map's 16.16 grid)
constexpr float k_Near = 0.01f;

class HanoiPuzzleTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"game", "scripting"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_mt>(name);
			}
		}
		test::EmplaceMapAndVillagerDefaults();
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<Registry>();
		test::EmplaceWorldSystems();
		puzzle = CreatePuzzleGame(k_Spot, script::PuzzleGameType::Hanoi, glm::radians(180.0f), 1.0f);
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] PuzzleGame& Game() const { return Reg().Get<PuzzleGame>(puzzle); }
	static HanoiBlock& Block(entt::entity piece) { return Reg().Get<HanoiBlock>(piece); }
	static glm::vec3 At(entt::entity piece) { return Reg().Get<const Transform>(piece).position; }

	/// A ring taken into the hand and let go of somewhere else, where it lies until the puzzle's next turn
	static void Carry(entt::entity ring, glm::vec3 to)
	{
		hanoi_puzzle::TakenInHand(ring);
		map_cells::MoveMapObject(ring, to);
	}

	entt::entity puzzle {entt::null};
};
} // namespace

// The three bases stand in a row along the half turn, and the four rings on the first, the largest at the bottom, each
// raised by the height of the piece under it
TEST_F(HanoiPuzzleTest, ThePiecesAreMadeStackedOnTheFirstBase)
{
	ASSERT_NE(puzzle, entt::entity(entt::null));
	const auto& game = Game();
	for (size_t base = 0; base < hanoi::k_Bases; ++base)
	{
		const auto piece = game.bases.at(base);
		ASSERT_TRUE(Reg().Valid(piece));
		EXPECT_EQ(Reg().Get<const MobileObject>(piece).type, hanoi::k_BaseKind);
		EXPECT_NEAR(At(piece).x, k_Spot.x, k_Near);
		EXPECT_NEAR(At(piece).z, k_Spot.z - k_BaseSpacing * static_cast<float>(base), k_Near);
		EXPECT_TRUE(object_flags::IsImmovable(piece));
		EXPECT_TRUE(Reg().AllOf<Indestructible>(piece));
		EXPECT_EQ(Block(piece).puzzle, puzzle);
	}
	constexpr std::array<float, hanoi::k_Rings> k_Heights {0.0f, 4.0f, 7.0f, 9.0f};
	auto below = game.bases.front();
	for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
	{
		const auto piece = game.rings.at(ring);
		ASSERT_TRUE(Reg().Valid(piece));
		EXPECT_EQ(Reg().Get<const MobileObject>(piece).type, hanoi::k_RingKinds.at(ring));
		EXPECT_NEAR(At(piece).y, k_Heights.at(ring), k_Near);
		EXPECT_NEAR(At(piece).z, k_Spot.z, k_Near);
		EXPECT_EQ(Block(piece).below, below);
		EXPECT_EQ(Block(below).above, piece);
		EXPECT_FALSE(object_flags::IsImmovable(piece));
		below = piece;
	}
	EXPECT_EQ(Block(game.rings.back()).above, entt::entity(entt::null));
	EXPECT_EQ(game.calmTurns, hanoi::k_CalmTop);
}

// A turn settles a ring let go of near another base onto that base: it is made again at its new rest place, linked to
// the base, and the play has begun
TEST_F(HanoiPuzzleTest, ATurnSettlesARingLetGoOfNearABase)
{
	const auto smallest = Game().rings.back();
	const auto target = Game().bases.back();
	Carry(smallest, At(target) + glm::vec3(1.0f, 3.0f, 1.0f));
	EXPECT_EQ(PuzzleGameStatusOf(puzzle), script::PuzzleGameStatus::GameNotYetBegun);

	ProcessPuzzleGamesTurn();

	const auto made = Game().rings.back();
	EXPECT_NE(made, smallest);
	EXPECT_FALSE(IsAvailable(smallest));
	ASSERT_TRUE(Reg().Valid(made));
	EXPECT_EQ(Block(made).below, target);
	EXPECT_EQ(Block(target).above, made);
	EXPECT_NEAR(At(made).x, At(target).x, k_Near);
	EXPECT_NEAR(At(made).z, At(target).z, k_Near);
	EXPECT_NEAR(At(made).y, 0.0f, k_Near);
	EXPECT_TRUE(Game().begun);
	EXPECT_EQ(PuzzleGameStatusOf(puzzle), script::PuzzleGameStatus::GameInProgress);
	// nothing was restless: the count stays at its top
	EXPECT_EQ(Game().calmTurns, hanoi::k_CalmTop);
}

// A turn puts back a base that moved off its place: a new base stands on the place
TEST_F(HanoiPuzzleTest, ATurnPutsBackABaseThatMoved)
{
	const auto middle = Game().bases.at(1);
	const auto place = At(middle);
	map_cells::MoveMapObject(middle, place + glm::vec3(30.0f, 0.0f, 0.0f));

	ProcessPuzzleGamesTurn();

	const auto made = Game().bases.at(1);
	EXPECT_NE(made, middle);
	EXPECT_FALSE(IsAvailable(middle));
	EXPECT_NEAR(At(made).x, place.x, k_Near);
	EXPECT_NEAR(At(made).z, place.z, k_Near);
	EXPECT_EQ(Block(made).above, entt::entity(entt::null));
}

// A base and a covered ring refuse the hand; the top ring does not, until a ring lies off its place and unsettled, which
// makes the puzzle restless
TEST_F(HanoiPuzzleTest, ACoveredOrUnsettledPieceRefusesTheHand)
{
	const auto& rings = Game().rings;
	EXPECT_TRUE(object_flags::IsCannotBePickedUp(Game().bases.front()));
	EXPECT_TRUE(object_flags::IsCannotBePickedUp(rings.at(2)));
	EXPECT_FALSE(object_flags::IsCannotBePickedUp(rings.at(3)));

	// the smallest ring taken off and let go of far from every stack: the third ring is uncovered, but the turn is
	// restless
	Carry(rings.at(3), k_Spot + glm::vec3(200.0f, 0.0f, 0.0f));
	ProcessPuzzleGamesTurn();

	EXPECT_EQ(Game().calmTurns, hanoi::k_CalmTop - 1);
	EXPECT_EQ(Block(Game().rings.at(2)).above, entt::entity(entt::null));
	EXPECT_TRUE(object_flags::IsCannotBePickedUp(Game().rings.at(2)));
	// it still lies where it was let go of: the count has not yet gone below 0
	EXPECT_NEAR(At(Game().rings.at(3)).x, k_Spot.x + 200.0f, k_Near);
}

// A ring left unsettled goes back onto the stack at its rest place once the count of calm turns goes below 0
TEST_F(HanoiPuzzleTest, AnUnsettledRingGoesBackAfterAWhile)
{
	const auto smallest = Game().rings.back();
	const auto third = Game().rings.at(2);
	Carry(smallest, k_Spot + glm::vec3(200.0f, 0.0f, 0.0f));
	for (int turn = 0; turn < hanoi::k_CalmTop + 1; ++turn)
	{
		ProcessPuzzleGamesTurn();
	}
	const auto made = Game().rings.back();
	EXPECT_NE(made, smallest);
	EXPECT_EQ(Block(made).below, third);
	EXPECT_EQ(Block(third).above, made);
	EXPECT_NEAR(At(made).x, k_Spot.x, k_Near);
}

// A piece taken into the hand comes off the piece it rested on; anything that is no piece is left as it is
TEST_F(HanoiPuzzleTest, ATakenPieceIsUnlinked)
{
	const auto smallest = Game().rings.back();
	const auto third = Game().rings.at(2);
	hanoi_puzzle::TakenInHand(smallest);
	EXPECT_EQ(Block(smallest).below, entt::entity(entt::null));
	EXPECT_EQ(Block(smallest).above, entt::entity(entt::null));
	EXPECT_EQ(Block(third).above, entt::entity(entt::null));
	// a ring resting on nothing may fly; a base and a stacked ring may not, and anything else is not the puzzle's
	EXPECT_EQ(hanoi_puzzle::CanBecomePhysicsObject(smallest), std::optional<bool>(true));
	EXPECT_EQ(hanoi_puzzle::CanBecomePhysicsObject(third), std::optional<bool>(false));
	EXPECT_EQ(hanoi_puzzle::CanBecomePhysicsObject(Game().bases.front()), std::optional<bool>(false));
	EXPECT_FALSE(hanoi_puzzle::CanBecomePhysicsObject(puzzle).has_value());
	const auto other = Reg().Create();
	hanoi_puzzle::TakenInHand(other);
	EXPECT_FALSE(hanoi_puzzle::RefusesPickUp(other));
}

// The scripts' PLAYED and GET_OBJECT_STATE read the solution: false and not begun while the rings stand on the first
// base, true and won once they stand in order on the last, which fixes the rings in place
TEST_F(HanoiPuzzleTest, TheSolvedSignalReachesTheScripts)
{
	EXPECT_FALSE(IsPuzzleGamePlayed(puzzle));
	EXPECT_EQ(PuzzleGameStatusOf(puzzle), script::PuzzleGameStatus::GameNotYetBegun);

	// the stack moved whole onto the last base, as fifteen moves would leave it
	auto& game = Game();
	const auto largest = game.rings.front();
	Block(game.bases.front()).above = entt::null;
	Block(largest).below = game.bases.back();
	Block(game.bases.back()).above = largest;

	EXPECT_TRUE(IsPuzzleGamePlayed(puzzle));
	EXPECT_TRUE(Game().played);
	EXPECT_EQ(PuzzleGameStatusOf(puzzle), script::PuzzleGameStatus::Won);
	for (const auto ring : Game().rings)
	{
		EXPECT_TRUE(object_flags::IsCannotBePickedUp(ring));
		EXPECT_TRUE(object_flags::IsImmovable(ring));
	}
	// a solved puzzle takes no more turns: a ring moved now stays where it is
	const auto smallest = Game().rings.back();
	map_cells::MoveMapObject(smallest, k_Spot + glm::vec3(100.0f, 0.0f, 0.0f));
	ProcessPuzzleGamesTurn();
	EXPECT_EQ(Game().rings.back(), smallest);
}

// A puzzle that goes leaves its rings with no puzzle, and they then refuse the hand
TEST_F(HanoiPuzzleTest, APuzzleThatGoesLeavesItsRingsWithNoPuzzle)
{
	const auto smallest = Game().rings.back();
	const auto first = Game().bases.front();
	ToBeDeleted(puzzle);
	ProcessPuzzleGamesTurn();
	EXPECT_EQ(Block(smallest).puzzle, entt::entity(entt::null));
	EXPECT_EQ(Block(first).puzzle, puzzle);
	EXPECT_TRUE(object_flags::IsCannotBePickedUp(smallest));
}
