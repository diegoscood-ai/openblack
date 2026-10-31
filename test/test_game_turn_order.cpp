/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "GameTurnOrder.h"

using namespace openblack::game_turn;

namespace
{
/// Records the steps that the turn does, in the order that it does them
class TurnRecorder
{
public:
	void operator()(Step step) { _steps.push_back(step); }
	[[nodiscard]] const std::vector<Step>& Steps() const { return _steps; }
	/// Where the step is in the record
	[[nodiscard]] std::ptrdiff_t IndexOf(Step step) const
	{
		return std::distance(_steps.begin(), std::ranges::find(_steps, step));
	}
	[[nodiscard]] bool Before(Step first, Step second) const { return IndexOf(first) < IndexOf(second); }

private:
	std::vector<Step> _steps;
};

TurnRecorder PlayOneTurn()
{
	TurnRecorder recorder;
	PlayTurn([&recorder](Step step) { recorder(step); });
	return recorder;
}
} // namespace

TEST(GameTurnOrder, EachStepOnce)
{
	const auto turn = PlayOneTurn();
	ASSERT_EQ(turn.Steps().size(), static_cast<size_t>(Step::_Count));
	for (size_t i = 0; i < static_cast<size_t>(Step::_Count); ++i)
	{
		EXPECT_EQ(std::ranges::count(turn.Steps(), static_cast<Step>(i)), 1) << i;
	}
}

TEST(GameTurnOrder, FollowsTheGamesStages)
{
	EXPECT_TRUE(InGameOrder(k_Turn));
	// a step out of the stages' order is caught
	constexpr std::array k_Swapped = {Place {.step = Step::Scripts, .stage = Stage::Scripts},
	                                  Place {.step = Step::Living, .stage = Stage::Living}};
	EXPECT_FALSE(InGameOrder(k_Swapped));
	// a step with no stage keeps its place and is not checked
	constexpr std::array k_Unknown = {Place {.step = Step::Living, .stage = Stage::Living},
	                                  Place {.step = Step::DeadList, .stage = std::nullopt},
	                                  Place {.step = Step::Scripts, .stage = Stage::Scripts}};
	EXPECT_TRUE(InGameOrder(k_Unknown));
}

// The order the game loop has always had: the table plays it as it was written in the loop
TEST(GameTurnOrder, OpenblacksOrder)
{
	const auto turn = PlayOneTurn();
	const std::vector<Step> k_Expected = {
	    Step::PlayerCommands, Step::HandInterface,    Step::PowerUpInputs, Step::Clock,          Step::MapCells,
	    Step::ReactionClock,  Step::Sharks,           Step::TurnStart,     Step::Fields,         Step::FishFarms,
	    Step::WalkPaths,      Step::MistFades,        Step::PuzzleGames,   Step::Footpaths,      Step::ForestGrowth,
	    Step::Living,         Step::Miracles,         Step::DebugHooks,    Step::Particles,      Step::Fireflies,
	    Step::Physics,        Step::ParticleEnd,      Step::Scripts,       Step::WeatherThings,  Step::ScriptHighlights,
	    Step::Climates,       Step::TownBelief,       Step::HeldThing,     Step::DeadList,       Step::Rewards,
	    Step::Guidance,       Step::InfluenceBorders, Step::CameraChecks,  Step::BuildingPieces, Step::Sounds,
	    Step::AudioTestHooks, Step::AutoSave,         Step::GameOver,      Step::ReactionsEnd,   Step::GlobalStatistics,
	};
	EXPECT_EQ(turn.Steps(), k_Expected);
	// the inputs come before the turn number goes up, and the statistics last
	EXPECT_TRUE(turn.Before(Step::PowerUpInputs, Step::Clock));
	EXPECT_EQ(turn.Steps().back(), Step::GlobalStatistics);
}
