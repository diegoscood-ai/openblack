/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Japanese totem puzzle (ecs::totem_puzzle_game, through ecs/PuzzleGames.h as the scripts reach it) on a flat dry
// land of the game's systems, its totems the game's model or, on synthetic data, a box: the arc, a move, the sparkles,
// a totem made again and the solution. See docs/wiki/world/totem-puzzle.md

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/TotemPuzzle.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectFlags.h"
#include "ECS/PlacedTurn.h"
#include "ECS/PuzzleGames.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/TotemPuzzleRules.h"
#include "Locator.h"
#include "support/ParticleFakes.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The puzzle's place, the middle of a map cell well inside the map
constexpr glm::vec3 k_Place {1005.0f, 0.0f, 1235.0f};

/// The spot visuals started, each an object of the registry so that it can be deleted
class RecordedSparkles final: public test::InertParticleSystem
{
public:
	struct Started
	{
		SpotVisualType type;
		glm::vec3 position;
		std::optional<int> turns;
		float magnitude;
		entt::entity object;
	};

	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity /*owner*/,
	                             float magnitude) override
	{
		const auto object = Locator::entitiesRegistry::value().Create();
		started.push_back({type, position, turns, magnitude, object});
		return object;
	}

	std::vector<Started> started;
};

class TotemPuzzleWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		if (!GameData())
		{
			// the Japanese totem's row: a totem of the Japanese, its model a box 2 m across and 6 m tall
			auto& row = Info().abode.at(static_cast<size_t>(AbodeInfo::JapaneseTotem));
			row.abodeType = AbodeType::Totem;
			row.abodeNumber = AbodeNumber::Totem;
			row.tribeType = Tribe::JAPANESE;
			row.meshId = MeshId::BuildingJapaneseTotem;
		}
		LoadMesh(Info().abode.at(static_cast<size_t>(AbodeInfo::JapaneseTotem)).meshId, glm::vec3(-1.0f, 0.0f, -1.0f),
		         glm::vec3(1.0f, 6.0f, 1.0f));
		_sparkles = &static_cast<RecordedSparkles&>(Locator::particleSystem::emplace<RecordedSparkles>());
		_puzzle = CreatePuzzleGame(k_Place, script::PuzzleGameType::Totem3, 0.0f, 1.0f);
	}

	void TearDown() override
	{
		Locator::particleSystem::reset();
		WorldHarness::TearDown();
	}

	[[nodiscard]] TotemPuzzle& Arc() const { return Registry().Get<TotemPuzzle>(_puzzle); }
	[[nodiscard]] PuzzleTotem& Piece(size_t index) const { return Registry().Get<PuzzleTotem>(Arc().totems.at(index)); }
	[[nodiscard]] static map_coords::MapCoords Place()
	{
		return {.x = map_coords::ToFixed(k_Place.x), .z = map_coords::ToFixed(k_Place.z), .altitude = k_Place.y};
	}

	entt::entity _puzzle {entt::null};
	RecordedSparkles* _sparkles {nullptr};
};
} // namespace

// Six totems stand in the arc, each sunk with a highest step of 4, unmovable and turned from the middle; every kept
// height is 0 and nothing has begun
TEST_P(TotemPuzzleWorld, TheArcIsMadeOfSixSunkUnmovableTotems)
{
	ASSERT_TRUE(_puzzle != entt::null);
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		const auto totem = Arc().totems.at(index);
		ASSERT_TRUE(Registry().Valid(totem));
		EXPECT_EQ(Piece(index).maxHeight, totem_puzzle::k_Highest);
		EXPECT_EQ(Piece(index).height, 0);
		EXPECT_TRUE(Registry().AllOf<Immovable>(totem));
		const auto at = totem_puzzle::ArcPlace(Place(), 1.0f, index);
		const auto& position = Registry().Get<const Transform>(totem).position;
		EXPECT_EQ(position.x, map_coords::ToMetres(at.x));
		EXPECT_EQ(position.z, map_coords::ToMetres(at.z));
		EXPECT_EQ(placed_turn::AnglesOf(Registry(), totem).y, totem_puzzle::FacingFromMiddle(Place(), at));
		EXPECT_EQ(Arc().heights.at(index), 0);
	}
	EXPECT_EQ(PuzzleGameStatusOf(_puzzle), script::PuzzleGameStatus::GameNotYetBegun);
	EXPECT_TRUE(_sparkles->started.empty());
}

// The first totem raised a step is the move: the second goes to 4 and the sixth to 2, blending from where they were,
// a sparkle stands over the second, and the puzzle has begun
TEST_P(TotemPuzzleWorld, ARaisedTotemMovesTheOthersByTheTable)
{
	Piece(0).height = 1;
	ProcessPuzzleGame(_puzzle);
	EXPECT_EQ(Arc().heights, (std::array<int32_t, totem_puzzle::k_Totems> {1, 4, 0, 0, 0, 2}));
	EXPECT_EQ(Piece(0).height, 1);
	EXPECT_EQ(Piece(0).previousHeight, 1);
	EXPECT_EQ(Piece(0).blendCountdown, 0);
	EXPECT_EQ(Piece(1).height, 4);
	EXPECT_EQ(Piece(1).previousHeight, 0);
	// the totem's place on the move counted one read off the blend
	EXPECT_EQ(Piece(1).blendCountdown, puzzle_totem::k_BlendReads - 1);
	EXPECT_EQ(Piece(5).height, 2);
	ASSERT_EQ(_sparkles->started.size(), 1U);
	const auto& sparkle = _sparkles->started.front();
	EXPECT_EQ(sparkle.type, SpotVisualType::CreatureTarget);
	EXPECT_EQ(sparkle.magnitude, totem_puzzle::k_SparkleMagnitude);
	EXPECT_FALSE(sparkle.turns.has_value());
	EXPECT_EQ(sparkle.position.x, map_coords::ToMetres(totem_puzzle::ArcPlace(Place(), 1.0f, 1).x));
	EXPECT_EQ(Arc().sparkles.at(1), sparkle.object);
	EXPECT_EQ(PuzzleGameStatusOf(_puzzle), script::PuzzleGameStatus::GameInProgress);
	// a turn with no move changes nothing
	ProcessPuzzleGame(_puzzle);
	EXPECT_EQ(Arc().heights, (std::array<int32_t, totem_puzzle::k_Totems> {1, 4, 0, 0, 0, 2}));
	EXPECT_EQ(_sparkles->started.size(), 1U);
}

// A totem lowered from its highest loses its sparkle: every move takes the sparkles away and makes those at the highest
// again
TEST_P(TotemPuzzleWorld, ATotemLoweredFromItsHighestLosesItsSparkle)
{
	Piece(0).height = 1;
	ProcessPuzzleGame(_puzzle);
	const auto first = Arc().sparkles.at(1);
	Piece(1).height = 3;
	ProcessPuzzleGame(_puzzle);
	EXPECT_EQ(Arc().heights, (std::array<int32_t, totem_puzzle::k_Totems> {1, 3, 0, 0, 0, 2}));
	EXPECT_FALSE(IsAvailable(first));
	EXPECT_TRUE(Arc().sparkles.at(1) == entt::null);
	EXPECT_EQ(_sparkles->started.size(), 1U);
}

// A totem that has gone is made again where it stood, with its highest step and height, held by the scripts, unmovable
// and facing the way a new totem faces
TEST_P(TotemPuzzleWorld, ATotemThatWentIsMadeAgain)
{
	const auto old = Arc().totems.at(2);
	const auto& oldPosition = Registry().Get<const Transform>(old).position;
	const float x = oldPosition.x;
	const float z = oldPosition.z;
	ToBeDeleted(old, true);
	ProcessPuzzleGame(_puzzle);
	const auto remade = Arc().totems.at(2);
	ASSERT_NE(remade, old);
	ASSERT_TRUE(IsAvailable(remade));
	EXPECT_EQ(Piece(2).maxHeight, totem_puzzle::k_Highest);
	EXPECT_EQ(Piece(2).height, 0);
	EXPECT_TRUE(Registry().AllOf<Immovable>(remade));
	EXPECT_TRUE(script_held::IsControlledByScript(remade));
	EXPECT_EQ(Registry().Get<const Transform>(remade).position.x, x);
	EXPECT_EQ(Registry().Get<const Transform>(remade).position.z, z);
	EXPECT_EQ(placed_turn::AnglesOf(Registry(), remade).y, 0.0f);
	// it was no move
	EXPECT_EQ(PuzzleGameStatusOf(_puzzle), script::PuzzleGameStatus::GameNotYetBegun);
}

// Every kept height at the highest is the solution: the sparkles go, a sparkle of three turns stands over each totem's
// place, and the puzzle is played; each later test makes those sparkles again, as the original's
TEST_P(TotemPuzzleWorld, EveryTotemAtItsHighestIsTheSolution)
{
	Piece(0).height = 1;
	ProcessPuzzleGame(_puzzle);
	const auto kept = Arc().sparkles.at(1);
	Arc().heights.fill(totem_puzzle::k_Highest);
	EXPECT_TRUE(IsPuzzleGamePlayed(_puzzle));
	EXPECT_TRUE(Registry().Get<const PuzzleGame>(_puzzle).played);
	EXPECT_FALSE(IsAvailable(kept));
	ASSERT_EQ(_sparkles->started.size(), 1U + totem_puzzle::k_Totems);
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		const auto& sparkle = _sparkles->started.at(1 + index);
		EXPECT_EQ(sparkle.turns, totem_puzzle::k_SolvedSparkleTurns);
		EXPECT_EQ(sparkle.position.z, map_coords::ToMetres(totem_puzzle::ArcPlace(Place(), 1.0f, index).z));
	}
	EXPECT_EQ(PuzzleGameStatusOf(_puzzle), script::PuzzleGameStatus::Won);
	EXPECT_EQ(_sparkles->started.size(), 1U + 2 * totem_puzzle::k_Totems);
}

INSTANTIATE_TEST_SUITE_P(Data, TotemPuzzleWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
