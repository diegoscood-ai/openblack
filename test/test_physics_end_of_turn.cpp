/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The end of a physics turn (Physics/TurnRules EndOfTurn) in the original's order, on a recording fake of its steps,
// the deed of damage by throwing, and the scripts' hit record the physics keeps (docs/wiki/physics/physics.md#manager).

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "Physics/TurnRules.h"
#include "support/DynamicsFakes.h"

using namespace openblack;
using openblack::test::RecordingEndOfTurnSteps;
using Body = RecordingEndOfTurnSteps::Body;

namespace
{
/// The bodies the walk reached, in order
std::vector<std::string> Reached(const std::vector<std::string>& log)
{
	std::vector<std::string> reached;
	for (const auto& line : log)
	{
		if (line.starts_with("push "))
		{
			reached.push_back(line.substr(5));
		}
	}
	return reached;
}
} // namespace

TEST(PhysicsEndOfTurn, TwoBodiesAreWorkedOneAfterTheOtherInTheOriginalsOrder)
{
	// A was hit by B, B by nothing; both knocks count. The second body's pushes end only after the first body's
	// reaction, deed and hit record: one walk, not one pass per step
	RecordingEndOfTurnSteps steps({Body {.name = 'A', .counts = true, .hitter = 'B'}, Body {.name = 'B', .counts = true}});
	physics::turn::EndOfTurn(steps);
	const std::vector<std::string> expected {
	    "push A", "topple A", "follow A", "impact A", "hitter A", "sound A", "react A", "deed A", "record A<-B", "deed-at A",
	    "push B", "topple B", "follow B", "impact B", "hitter B", "sound B", "react B", "deed B", "age",
	};
	EXPECT_EQ(steps.log, expected);
}

TEST(PhysicsEndOfTurn, OnlyABodyWhoseForceCountsSoundsReactsAndIsRecorded)
{
	// A's force is too small to count: the first steps only, and its hitter is not looked up (it keeps the one it had).
	// B, hit by A, gets the rest, its hit record after its reaction
	RecordingEndOfTurnSteps steps(
	    {Body {.name = 'A', .counts = false, .hitter = 'B'}, Body {.name = 'B', .counts = true, .hitter = 'A'}});
	physics::turn::EndOfTurn(steps);
	const std::vector<std::string> expected {
	    "push A",   "topple A", "follow A", "impact A", "push B",      "topple B",  "follow B", "impact B",
	    "hitter B", "sound B",  "react B",  "deed B",   "record B<-A", "deed-at B", "age",
	};
	EXPECT_EQ(steps.log, expected);
}

TEST(PhysicsEndOfTurn, TheLastBodyHitInListOrderIsRecordedLast)
{
	RecordingEndOfTurnSteps steps({Body {.name = 'A', .counts = true, .hitter = 'C'}, Body {.name = 'B', .counts = true},
	                               Body {.name = 'C', .counts = true, .hitter = 'A'}});
	physics::turn::EndOfTurn(steps);
	std::vector<std::string> records;
	std::ranges::copy_if(steps.log, std::back_inserter(records),
	                     [](const std::string& line) { return line.starts_with("record "); });
	EXPECT_EQ(records, (std::vector<std::string> {"record A<-C", "record C<-A"}));
}

TEST(PhysicsEndOfTurn, TheWalkTakesTheListAsTheReactionsLeaveIt)
{
	// A's reaction adds D at the end. B's knocks C, resting, into flight: C's resting body goes, the last (D) moves
	// into its place, and C's moving body E is added. The walk goes on in list order, as the original's does: A, B, then
	// D in C's place, then E. The steps after B's reaction are still about B
	RecordingEndOfTurnSteps steps({Body {.name = 'A', .counts = true, .adds = {'D'}},
	                               Body {.name = 'B', .counts = true, .hitter = 'C', .adds = {'E'}, .removes = {'C'}},
	                               Body {.name = 'C', .counts = false}});
	physics::turn::EndOfTurn(steps);
	EXPECT_EQ(Reached(steps.log), (std::vector<std::string> {"A", "B", "D", "E"}));
	const auto react = std::ranges::find(steps.log, std::string("react B"));
	ASSERT_NE(react, steps.log.end());
	ASSERT_GE(steps.log.end() - react, 4);
	EXPECT_EQ(*(react + 1), "deed B");
	EXPECT_EQ(*(react + 2), "record B<-C");
	EXPECT_EQ(*(react + 3), "deed-at B");
	EXPECT_EQ(steps.log.back(), "age");
}

TEST(PhysicsEndOfTurn, DamageByThrowingNeedsTheHandACreditAndALivingThingOrARock)
{
	using physics::turn::IsDamageByThrowing;
	EXPECT_TRUE(IsDamageByThrowing(true, true, true, false, false));
	EXPECT_TRUE(IsDamageByThrowing(true, true, false, true, false));
	EXPECT_FALSE(IsDamageByThrowing(false, true, true, false, false)); // not from the hand
	EXPECT_FALSE(IsDamageByThrowing(true, false, true, false, false)); // nobody's credit
	EXPECT_FALSE(IsDamageByThrowing(true, true, false, false, false)); // neither living nor a rock
	EXPECT_FALSE(IsDamageByThrowing(true, true, false, true, true));   // a kind of tree
}

TEST(PhysicsHitRecord, KeepsOnlyWhatIsAvailableAsItIsWrittenAndReadsItBackAsKept)
{
	ecs::Registry registry;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<openblack::test::FakeDynamicsWorld>(registry));
	EXPECT_EQ(dynamics.GetHitObject(), entt::entity {entt::null});
	EXPECT_EQ(dynamics.GetObjectWhichHit(), entt::entity {entt::null});

	const auto hit = registry.Create();
	const auto hitter = registry.Create();
	dynamics.RecordHit(hit, hitter);
	EXPECT_EQ(dynamics.GetHitObject(), hit);
	EXPECT_EQ(dynamics.GetObjectWhichHit(), hitter);

	// marked for deletion after it was written: still read back
	registry.Assign<ecs::components::Unavailable>(hitter);
	EXPECT_EQ(dynamics.GetObjectWhichHit(), hitter);

	// marked, or gone, as it is written: none
	const auto gone = registry.Create();
	registry.Destroy(gone);
	dynamics.RecordHit(gone, hitter);
	EXPECT_EQ(dynamics.GetHitObject(), entt::entity {entt::null});
	EXPECT_EQ(dynamics.GetObjectWhichHit(), entt::entity {entt::null});

	// the scripts clear it
	dynamics.RecordHit(hit, entt::null);
	EXPECT_EQ(dynamics.GetHitObject(), hit);
	dynamics.RecordHit(entt::null, entt::null);
	EXPECT_EQ(dynamics.GetHitObject(), entt::entity {entt::null});
}
