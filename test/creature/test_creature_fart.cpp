/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The poo desire's second action, the fart: the planner weighs it beside the poo, and its agenda is the poo's own. At
// the moment the poo would drop, a fart lets out a cloud of puffs instead. The pinned clouds were produced by running
// the original game's own code natively from the campaign's synchronised seed, the random numbers it drew included:
// the same draws must give the same velocities, bit for bit. A body evil enough makes those near it faint.

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFart.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"

using namespace openblack;
using creature_desires::Desire;

namespace
{
/// The poo and the fart by their rows of the game's action table, in the order the poo desire lists them
constexpr uint32_t k_PooRow = 19;
constexpr uint32_t k_FartRow = 177;
/// A desire's distance weight; the poo and the fart are done to no thing, so it weighs nothing here
constexpr float k_DistanceWeight = 0.4f;

creature_planner::ActionCandidate Candidate(uint32_t row, uint32_t turnsSinceDone)
{
	return {.action = row, .needsObject = false, .opinion = 0.0f, .turnsSinceDone = turnsSinceDone};
}

/// Gives the queued numbers, by their bits, in order, and records each bound it was asked for
struct ScriptedRandom
{
	std::vector<uint32_t> values;
	std::vector<float> bounds;
	size_t next {0};

	creature_fart::FloatRandom Get()
	{
		return [this](float bound) {
			bounds.push_back(bound);
			return next < values.size() ? std::bit_cast<float>(values[next++]) : 0.0f;
		};
	}
};

/// A puff's velocity by its bits, x, y, z
using Bits = std::array<uint32_t, 3>;

Bits BitsOf(const glm::vec3& v)
{
	return {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y), std::bit_cast<uint32_t>(v.z)};
}
} // namespace

TEST(CreatureFart, TheFartIsCarriedOutWithThePoosAgenda)
{
	const auto* poo = creature_plan_actions::For("Poo");
	const auto* fart = creature_plan_actions::For("Fart");
	ASSERT_NE(poo, nullptr);
	ASSERT_NE(fart, nullptr);
	EXPECT_EQ(fart->build, poo->build);
	EXPECT_EQ(fart->target, poo->target);
	EXPECT_EQ(fart->activity, poo->activity);
}

TEST(CreatureFart, WatchingTheTellyIsOnlyEverForcedAndIsAPoo)
{
	// no desire lists it, so the planner never chooses it; forced, its agenda is the poo's
	EXPECT_EQ(creature_plan_actions::For("WatchTelly"), nullptr);
	const auto* poo = creature_plan_actions::For("Poo");
	const auto* telly = creature_plan_actions::ForcedFor("WatchTelly");
	ASSERT_NE(poo, nullptr);
	ASSERT_NE(telly, nullptr);
	EXPECT_EQ(telly->build, poo->build);
	EXPECT_EQ(telly->target, poo->target);
	EXPECT_EQ(telly->activity, poo->activity);
}

TEST(CreatureFart, LookingDownACliffIsNeverBuilt)
{
	// the game's builder of the action is empty and fails, so even forced it is not carried out
	EXPECT_EQ(creature_plan_actions::For("LookDownCliff"), nullptr);
	EXPECT_EQ(creature_plan_actions::ForcedFor("LookDownCliff"), nullptr);
}

TEST(CreatureFart, OfAPooAndAFartWantedAsMuchTheFartIsChosen)
{
	// The later of equals in the desire's list wins, and the fart comes after the poo
	const std::array choices {
	    creature_planner::ActionChoice {.action = Candidate(k_PooRow, creature_planner::k_NoveltyTurns)},
	    creature_planner::ActionChoice {.action = Candidate(k_FartRow, creature_planner::k_NoveltyTurns)},
	};
	const auto plan = creature_planner::PlanWithoutGoal(Desire::Poo, 0.5f, choices, k_DistanceWeight);
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, k_FartRow);
}

TEST(CreatureFart, AFartJustDoneGivesWayToAPoo)
{
	const std::array choices {
	    creature_planner::ActionChoice {.action = Candidate(k_PooRow, creature_planner::k_NoveltyTurns)},
	    creature_planner::ActionChoice {.action = Candidate(k_FartRow, 0)},
	};
	const auto plan = creature_planner::PlanWithoutGoal(Desire::Poo, 0.5f, choices, k_DistanceWeight);
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, k_PooRow);
}

TEST(CreatureFart, ThePooMomentIsAFartOnlyWhenThePlanIsTheFart)
{
	EXPECT_TRUE(creature_fart::Farts(k_FartRow, k_FartRow));
	EXPECT_FALSE(creature_fart::Farts(k_PooRow, k_FartRow));
	// Doing nothing that counts as an action, or with no fart in the tables, it is never a fart
	EXPECT_FALSE(creature_fart::Farts(std::nullopt, k_FartRow));
	EXPECT_FALSE(creature_fart::Farts(k_FartRow, std::nullopt));
	EXPECT_FALSE(creature_fart::Farts(std::nullopt, std::nullopt));
}

TEST(CreatureFart, EachPuffIsFourTimesTheDrawnSize)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(creature_fart::PuffSize(std::bit_cast<float>(0x3DF38813u))), 0x3EF38813u);
}

TEST(CreatureFart, TheCloudIsBlownBehindTheBodyAsTheOriginalBlowsIt)
{
	struct Case
	{
		uint32_t yaw;
		uint32_t drawnSize;
		/// The cloud's size, which every draw is bounded by
		uint32_t cloud;
		std::vector<uint32_t> draws;
		std::vector<Bits> velocities;
	};
	const std::array cases {
	    Case {
	        .yaw = 0x3F333333, // 0.7
	        .drawnSize = 0x3DF38813,
	        .cloud = 0x3FE44F92,
	        .draws = {0x3F9CC80D, 0x3FA8BB1B, 0x3F84FF5F, 0x3FB38611, 0x3F159106, 0x3F83BABD, 0x3F780137, 0x3FD16306,
	                  0x3F6D9BED},
	        .velocities = {{0xBF803D61, 0x3EDA4D48, 0x3FD93F6E},
	                       {0xBF818203, 0xBE9D7D18, 0x3FEFFD72},
	                       {0xBF8E6ECA, 0x3F3E767A, 0x3FB877FC}},
	    },
	    Case {
	        .yaw = 0xC0200000, // -2.5
	        .drawnSize = 0x3F800000,
	        .cloud = 0x41700000,
	        .draws = {0x4124CEF5, 0x41315EA1, 0x410BCE8C, 0x413CB70D, 0x409D395D, 0x410A794A},
	        .velocities = {{0x412370AE, 0x40657A84, 0xC113774E}, {0x41221B6C, 0xC0258D46, 0xC0F71E6C}},
	    },
	    Case {
	        .yaw = 0x40400000, // 3
	        .drawnSize = 0x3F19999A,
	        .cloud = 0x41100000,
	        .draws = {0x40C5C526, 0x40D4D7F5, 0x40A7C4A8},
	        .velocities = {{0xBF06FE88, 0x4009AFEA, 0xC0E75906}},
	    },
	};
	for (const auto& c : cases)
	{
		ScriptedRandom random {.values = c.draws};
		const auto velocities =
		    creature_fart::PuffVelocities(std::bit_cast<float>(c.yaw), std::bit_cast<float>(c.drawnSize), random.Get());
		// Three draws for every puff, each bounded by the cloud's size
		ASSERT_EQ(random.bounds.size(), 3 * creature_fart::k_Puffs);
		for (const auto bound : random.bounds)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(bound), c.cloud);
		}
		for (size_t i = 0; i < c.velocities.size(); ++i)
		{
			EXPECT_EQ(BitsOf(velocities.at(i)), c.velocities.at(i)) << "yaw " << std::bit_cast<float>(c.yaw) << ", puff " << i;
		}
	}
}

TEST(CreatureFart, TheFirstDrawOfAPuffGoesAlongZAndTheLastAlongX)
{
	// Behind a body turned 0 is +z. With every draw at the middle of the spread, the puff flies straight behind; a draw
	// at the top of the spread moves it only along its own axis
	ScriptedRandom middle {.values = std::vector<uint32_t>(3 * creature_fart::k_Puffs, std::bit_cast<uint32_t>(7.5f))};
	const auto straight = creature_fart::PuffVelocities(0.0f, 1.0f, middle.Get());
	EXPECT_NEAR(straight[0].x, 0.0f, 1e-5f);
	EXPECT_EQ(straight[0].y, 0.0f);
	EXPECT_EQ(straight[0].z, 15.0f);

	ScriptedRandom first {
	    .values = {std::bit_cast<uint32_t>(15.0f), std::bit_cast<uint32_t>(7.5f), std::bit_cast<uint32_t>(7.5f)}};
	const auto alongZ = creature_fart::PuffVelocities(0.0f, 1.0f, first.Get());
	EXPECT_EQ(alongZ[0].y, 0.0f);
	EXPECT_EQ(alongZ[0].z, 22.5f);

	ScriptedRandom last {
	    .values = {std::bit_cast<uint32_t>(7.5f), std::bit_cast<uint32_t>(7.5f), std::bit_cast<uint32_t>(15.0f)}};
	const auto alongX = creature_fart::PuffVelocities(0.0f, 1.0f, last.Get());
	EXPECT_NEAR(alongX[0].x, 7.5f, 1e-5f);
	EXPECT_EQ(alongX[0].y, 0.0f);
	EXPECT_EQ(alongX[0].z, 15.0f);
}

TEST(CreatureFart, OnlyABodyMoreEvilThanHalfMakesThoseNearItFaint)
{
	EXPECT_FALSE(creature_fart::MakesFaint(-0.5f));
	EXPECT_TRUE(creature_fart::MakesFaint(std::nextafter(-0.5f, -1.0f)));
	EXPECT_TRUE(creature_fart::MakesFaint(-1.0f));
	EXPECT_FALSE(creature_fart::MakesFaint(0.0f));
	EXPECT_FALSE(creature_fart::MakesFaint(1.0f));
	// The game's comparison lets an alignment that is no number through
	EXPECT_TRUE(creature_fart::MakesFaint(std::numeric_limits<float>::quiet_NaN()));
}
