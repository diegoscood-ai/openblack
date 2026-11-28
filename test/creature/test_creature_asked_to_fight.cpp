/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureAskedToFight.h"

using namespace openblack;
using creature_asked_to_fight::Fight;
using creature_asked_to_fight::Part;

namespace
{
/// The creature asked, with the asker in its face-off at a turn
constexpr Fight AskedToFightAt(uint32_t turns)
{
	return {.askerFacesOff = true, .askerFaceOffTurns = turns};
}
} // namespace

// The asker leads by its turns of the face-off: asked as the fight starts and on the turn it asks, its mind standing still
// from the turn it is told to, and obeying from the first order (docs/wiki/creature/fights.md#a-fight-a-script-forces)
TEST(CreatureAskedToFight, TheAskedCreatureTakesPartByTheAskersTurns)
{
	using creature_asked_to_fight::PartOf;
	EXPECT_EQ(PartOf(AskedToFightAt(0)), Part::Asked);
	EXPECT_EQ(PartOf(AskedToFightAt(creature_fight::k_FaceOffRequestTurn)), Part::Asked);
	EXPECT_EQ(PartOf(AskedToFightAt(creature_fight::k_FaceOffStillTurn)), Part::StandsStill);
	EXPECT_EQ(PartOf(AskedToFightAt(creature_fight::k_FaceOffOrderTurn)), Part::Obeys);
	EXPECT_EQ(PartOf(AskedToFightAt(creature_fight::k_FaceOffOwnTurn)), Part::Obeys);
	// past the face-off, and the asker itself, from the start
	EXPECT_EQ(PartOf({.askerFacesOff = false}), Part::Obeys);
	EXPECT_EQ(PartOf({.askedForIt = true}), Part::Obeys);
}

// Only from its first order does the fight have its body; its mind stands still from the order to stand still to the end
// of the fight, but for a winner made to do something to the loser; only a plan taken while asked calls the fight off
TEST(CreatureAskedToFight, ItsMindStandsStillFromTheOrderToAndOnlyAnEarlyPlanCallsTheFightOff)
{
	using namespace creature_asked_to_fight;
	const auto asked = AskedToFightAt(creature_fight::k_FaceOffRequestTurn);
	const auto still = AskedToFightAt(creature_fight::k_FaceOffStillTurn);
	const auto obeys = AskedToFightAt(creature_fight::k_FaceOffOrderTurn);
	const Fight duel {};
	const Fight won {.won = true};
	const Fight asker {.askedForIt = true};

	EXPECT_FALSE(FightHasBody(asked));
	EXPECT_FALSE(FightHasBody(still));
	EXPECT_TRUE(FightHasBody(obeys));
	EXPECT_TRUE(FightHasBody(duel));
	EXPECT_TRUE(FightHasBody(asker));

	EXPECT_FALSE(MindStandsStill(asked));
	EXPECT_TRUE(MindStandsStill(still));
	EXPECT_TRUE(MindStandsStill(obeys));
	EXPECT_TRUE(MindStandsStill(duel));
	EXPECT_FALSE(MindStandsStill(won));
	EXPECT_FALSE(MindStandsStill(asker));

	EXPECT_TRUE(NewPlanCallsOff(asked));
	EXPECT_TRUE(NewPlanCallsOff(AskedToFightAt(0)));
	EXPECT_FALSE(NewPlanCallsOff(still));
	EXPECT_FALSE(NewPlanCallsOff(obeys));
	EXPECT_FALSE(NewPlanCallsOff(asker));
}
