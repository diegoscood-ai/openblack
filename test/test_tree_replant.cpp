/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/TreeReplant.h"

using namespace openblack;
using namespace openblack::tree_replant;

namespace
{
std::vector<Step> StepsRun(const Facts& facts)
{
	std::vector<Step> steps;
	std::ranges::copy_if(k_Order, std::back_inserter(steps), [&facts](Step step) { return Runs(step, facts); });
	return steps;
}
} // namespace

TEST(TreeReplant, TheStepsRunInTheOriginalsOrder)
{
	// The smoke and the immersion come before the forest search; the alignment comes after the creature's mimicry
	const auto steps = StepsRun({.dropped = true, .inTown = false, .player = true});
	const std::vector<Step> expected {Step::Smoke,      Step::Immersion,     Step::FindForest, Step::JoinForest,
	                                  Step::SpotVisual, Step::CreatureMimic, Step::Alignment,  Step::BackInMap};
	EXPECT_EQ(steps, expected);
}

TEST(TreeReplant, InATownThereIsNoSpotVisual)
{
	const auto steps = StepsRun({.dropped = true, .inTown = true, .player = true});
	EXPECT_EQ(std::ranges::count(steps, Step::SpotVisual), 0);
	EXPECT_EQ(steps.size(), k_Order.size() - 1);
}

TEST(TreeReplant, WithoutAPlayerNeitherTheMimicryNorTheAlignment)
{
	// The puff, the search, the forest and the map always run
	const auto steps = StepsRun({.dropped = false, .inTown = false, .player = false});
	const std::vector<Step> expected {Step::Smoke, Step::FindForest, Step::JoinForest, Step::SpotVisual, Step::BackInMap};
	EXPECT_EQ(steps, expected);
}

TEST(TreeReplant, TheImmersionIsThePlantObjectFeel)
{
	EXPECT_EQ(static_cast<int>(k_Immersion), 0x2E);
	EXPECT_EQ(k_ImmersionFlags, 0x80000000u);
}

TEST(TreeReplant, AForestMiraclesTreePutDownGentlyStaysWhereItFellWhereverItIs)
{
	// on land or in the sea, burning or not: neither planted again nor dead
	EXPECT_EQ(EndOfFlight(true, true, true, false), End::StaysWhereItFell);
	EXPECT_EQ(EndOfFlight(true, true, false, false), End::StaysWhereItFell);
	EXPECT_EQ(EndOfFlight(true, true, true, true), End::StaysWhereItFell);
}

TEST(TreeReplant, AForestMiraclesTreeThatDidNotLandGentlyDiesAsAnyTree)
{
	EXPECT_EQ(EndOfFlight(false, true, true, false), End::Dead);
	EXPECT_EQ(EndOfFlight(false, false, true, false), End::Dead);
}

TEST(TreeReplant, AnyOtherTreeIsPlantedAgainOnlyPutDownGentlyOnLandWithNoFire)
{
	EXPECT_EQ(EndOfFlight(true, false, true, false), End::Replanted);
	EXPECT_EQ(EndOfFlight(true, false, false, false), End::Dead);
	EXPECT_EQ(EndOfFlight(true, false, true, true), End::Dead);
}
