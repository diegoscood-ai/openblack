/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature locomotion system on our route follower and walkable mask, with a synthetic Giant Ape on an open land:
// what it refuses, a walk planned, followed and ended, stopping and turning, running away on the synced stream, the
// Transform moving only once a turn and the body drawn between turns, and the jumps back onto the land it can stand on;
// a step off a turn behind its follower, and walking on in the turn it is over; where a route re-planned on the way turns
// off

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/sparse_set.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "3D/ObjectMatrix.h"
#include "Audio/AnimEffectKeys.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/Point2D.h"
#include "RoutePlanner/RouteFollower.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureLocomotionSystem;
using MoveResult = openblack::ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
using Pace = openblack::ecs::systems::CreatureLocomotionSystemInterface::Pace;
using Motion = CreatureLocomotion::Motion;

namespace
{
/// 64 x 64 cells of land, 640 m a side
constexpr int32_t k_MaskSize = 64;

/// Whether a circle's box reaches into a 10 m map cell: a thing is listed in every cell it covers, so its circle comes
/// into a plan with the first square read that holds any of them
bool CoversCell(const route_planner::Point2D& centre, float radius, int32_t cellX, int32_t cellZ)
{
	const auto low = [radius](float at) { return static_cast<int32_t>(std::floor((at - radius) / 10.0f)); };
	const auto high = [radius](float at) { return static_cast<int32_t>(std::floor((at + radius) / 10.0f)); };
	return low(centre.x) <= cellX && cellX <= high(centre.x) && low(centre.z) <= cellZ && cellZ <= high(centre.z);
}

class CreatureLocomotionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		auto& state = Locator::landAvoidSystem::value().GetState();
		state.size = k_MaskSize;
		state.avoid.assign(static_cast<size_t>(k_MaskSize * k_MaskSize), land_avoid::k_Land);
		// the ape's speeds
		auto& ape = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
		ape.slowSpeed = 0.5f;
		ape.walkSpeed = 0.7f;
		ape.runSpeed = 0.9f;
		ape.runAwayDistance = 50.0f;
	}

	/// The ape's stand, walk and run, the walk and run moving it along
	void LoadWalkingRig()
	{
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {.durationMs = 1000},
		               test::creature_block::Clip {.durationMs = 1000, .displacement = {0.0f, 0.0f, -20.0f}},
		               test::creature_block::Clip {.durationMs = 600, .displacement = {0.0f, 0.0f, -25.0f}}};
		_world.LoadApeRig(block, {{"move", {"Cstand", "Wwalk", "Wrun"}}});
	}

	static void Avoid(int32_t x, int32_t z)
	{
		Locator::landAvoidSystem::value().GetState().avoid[static_cast<size_t>(z * k_MaskSize + x)] = land_avoid::k_Avoid;
	}

	static CreatureLocomotion& Of(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureLocomotion>(creature);
	}
	static const Transform& TransformOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<Transform>(creature);
	}

	/// A walking ape at (100, 100), started on the first turn
	entt::entity StartedApe()
	{
		LoadWalkingRig();
		const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
		_system.ProcessTurn();
		return creature;
	}

	/// An ape at (100, 100) with its six steps off, each going 5 along in a second, sent along +x to (300, 100) and
	/// turned until it steps off round to the way (at most 100 turns)
	entt::entity SteppingApe()
	{
		// the ape's stand, walk and run, then the rest of its moves
		test::creature_block::Block block;
		block.clips.assign(16, test::creature_block::Clip {.durationMs = 1000});
		block.clips.at(1)->displacement = {0.0f, 0.0f, -20.0f};
		block.clips.at(2) = test::creature_block::Clip {.durationMs = 600, .displacement = {0.0f, 0.0f, -25.0f}};
		for (size_t step = 10; step < 16; ++step)
		{
			block.clips.at(step)->displacement = {0.0f, 0.0f, -5.0f};
			block.clips.at(step)->strideLength = 5.0f;
		}
		std::vector<std::string> names {"Cstand", "Wwalk", "Wrun"};
		for (size_t move = 3; move < 16; ++move)
		{
			names.push_back("Cmove" + std::to_string(move));
		}
		_world.LoadApeRig(block, {{"move", names}});
		const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
		_system.ProcessTurn();
		EXPECT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
		for (int turn = 0; turn < 100 && Of(creature).motion != Motion::Stepping; ++turn)
		{
			_system.ProcessTurn();
		}
		return creature;
	}

	/// Turns until the creature stands again, at most `turns`; how many it took
	int RunUntilStanding(entt::entity creature, int turns)
	{
		for (int turn = 1; turn <= turns; ++turn)
		{
			_system.ProcessTurn();
			if (Of(creature).motion == Motion::Standing)
			{
				return turn;
			}
		}
		return -1;
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	CreatureLocomotionSystem _system;
};
} // namespace

TEST_F(CreatureLocomotionSystemTest, BusyWithoutWalkAndRunAnimations)
{
	const auto creature = test::creature_world::World::MakeCreature();
	// not started before its first turn
	EXPECT_EQ(_system.MoveTo(creature, {200.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Busy);
	_system.ProcessTurn();
	EXPECT_TRUE(Of(creature).started);
	// no rig: nothing to walk with
	EXPECT_EQ(_system.MoveTo(creature, {200.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Busy);
	// what is no creature
	EXPECT_EQ(_system.MoveTo(test::creature_world::World::Registry().Create(), {200.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f),
	          MoveResult::Busy);
	EXPECT_FALSE(_system.IsMoving(creature));
}

TEST_F(CreatureLocomotionSystemTest, NowhereToStandIsAnInvalidDestination)
{
	const auto creature = StartedApe();
	Avoid(30, 10);
	EXPECT_FALSE(_system.IsValidPosition({305.0f, 105.0f}, land_avoid::k_CreatureRadius));
	EXPECT_EQ(_system.MoveTo(creature, {305.0f, 105.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::InvalidDestination);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_EQ(Of(creature).follower, nullptr);
}

TEST_F(CreatureLocomotionSystemTest, AWalkIsPlannedFollowedAndEnded)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	EXPECT_EQ(Of(creature).motion, Motion::Planning);
	ASSERT_NE(Of(creature).follower, nullptr);
	EXPECT_EQ(Of(creature).follower->GetContext(), static_cast<int32_t>(entt::to_integral(creature)));
	EXPECT_TRUE(_system.IsMoving(creature));

	// planned within a few turns, then it turns to face the way and walks
	bool walked = false;
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Standing; ++turn)
	{
		_system.ProcessTurn();
		walked = walked || Of(creature).motion == Motion::Walking;
	}
	EXPECT_TRUE(walked);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_FALSE(Of(creature).failed);
	EXPECT_FALSE(Of(creature).destination.has_value());
	EXPECT_LT(glm::distance(glm::xz(TransformOf(creature).position), glm::vec2(300.0f, 100.0f)), 6.0f);
	// it faces the way it went, along +x
	EXPECT_NEAR(ecs::creature_pose::ReadHeading(TransformOf(creature).rotation), creature_locomotion::HeadingOf({1.0f, 0.0f}),
	            1e-3f);
}

// Stepping off, the creature is a turn behind its route follower: each turn it is put where the follower was at the end
// of the last turn, and only then does the follower move on
TEST_F(CreatureLocomotionSystemTest, SteppingOffItIsATurnBehindItsFollower)
{
	const auto creature = SteppingApe();
	ASSERT_EQ(Of(creature).motion, Motion::Stepping);
	int stepped = 0;
	for (int turn = 0; turn < 5; ++turn)
	{
		const auto& follower = *Of(creature).follower;
		const glm::vec2 was {follower.GetPosition().x, follower.GetPosition().z};
		_system.ProcessTurn();
		if (Of(creature).motion != Motion::Stepping)
		{
			break;
		}
		EXPECT_EQ(glm::xz(Of(creature).toPosition), was) << turn;
		const glm::vec2 now {Of(creature).follower->GetPosition().x, Of(creature).follower->GetPosition().z};
		EXPECT_GT(glm::distance(now, was), 0.0f) << turn;
		++stepped;
	}
	EXPECT_EQ(stepped, 5);
}

// A step off over, the creature walks on along the route in the same turn, at the step's speed, its walk taken up by
// how far through the step it was
TEST_F(CreatureLocomotionSystemTest, AStepOffOverWalksOnInTheSameTurn)
{
	const auto creature = SteppingApe();
	ASSERT_EQ(Of(creature).motion, Motion::Stepping);
	for (int turn = 0; turn < 30 && Of(creature).motion == Motion::Stepping; ++turn)
	{
		_system.ProcessTurn();
	}
	// the step of a second, 100 ms a turn, is over at 900 ms: the walk is taken up from there and walked on at once
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	EXPECT_FALSE(Of(creature).move.has_value());
	EXPECT_NE(Of(creature).walkTimeMs, 0.0f);
	// it stands where its follower got to this turn: it walked
	const auto& follower = *Of(creature).follower;
	EXPECT_EQ(glm::xz(Of(creature).toPosition), glm::vec2(follower.GetPosition().x, follower.GetPosition().z));
	EXPECT_GT(Of(creature).distance, 0.0f);
}

namespace
{
/// How far from the point a walk to anywhere from minDistance to maxDistance of it ends, on open ground
float WalkEnd(CreatureLocomotionSystem& system, entt::entity creature, float minDistance, float maxDistance)
{
	EXPECT_EQ(system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, minDistance, maxDistance), MoveResult::Started);
	auto& registry = test::creature_world::World::Registry();
	for (int turn = 0; turn < 3000 && registry.Get<CreatureLocomotion>(creature).motion != Motion::Standing; ++turn)
	{
		system.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<CreatureLocomotion>(creature).motion, Motion::Standing);
	return glm::distance(glm::xz(registry.Get<Transform>(creature).position), glm::vec2(300.0f, 100.0f));
}
} // namespace

TEST_F(CreatureLocomotionSystemTest, AWalkWithNoInsideGoesRightUpToThePointWhateverItsOutside)
{
	// the route is cut short only by the ring's inside: from 0 to 30 away, it walks onto the point
	EXPECT_LT(WalkEnd(_system, StartedApe(), 0.0f, 30.0f), 1.0f);
}

TEST_F(CreatureLocomotionSystemTest, AWalkStopsAtTheRingsInside)
{
	EXPECT_NEAR(WalkEnd(_system, StartedApe(), 5.0f, 30.0f), 5.0f, 1.0f);
}

namespace
{
/// Whether FillWall's wall stands; while it doesn't, the squares read are filled with nothing
bool g_WallUp = true;

/// A wall of touching circles 5 m round along z = 215, from x = 200 to x = 440, each put into a plan as the first
/// square holding a cell it covers is read
void FillWall(int32_t cellX, int32_t cellZ, route_planner::ObstacleGrid& holder)
{
	if (!g_WallUp)
	{
		return;
	}
	for (int32_t k = 0; k <= 30; ++k)
	{
		const route_planner::Point2D centre {.x = 200.0f + 8.0f * static_cast<float>(k), .z = 215.0f};
		if (CoversCell(centre, 5.0f, cellX, cellZ))
		{
			holder.AddObject(-1, centre, 5.0f, 0);
		}
	}
}
} // namespace

TEST_F(CreatureLocomotionSystemTest, AWayRoundManyTimesLongerThanTheWayThereIsStillAWayThere)
{
	// 30 m across a wall whose ends are 120 m off to either side: the way round is some 250 m, more than five times the
	// way there, and well within twice it and 5000
	g_WallUp = true;
	route_planner::ObstacleGrid::InstallCallbacks(&FillWall, nullptr);
	LoadWalkingRig();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(320.0f, 0.0f, 200.0f));
	_system.ProcessTurn();
	ASSERT_EQ(_system.MoveTo(creature, {320.0f, 230.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	float widest = 0.0f;
	for (int turn = 0; turn < 6000 && Of(creature).motion != Motion::Standing; ++turn)
	{
		_system.ProcessTurn();
		widest = std::max(widest, std::abs(TransformOf(creature).position.x - 320.0f));
	}
	EXPECT_FALSE(Of(creature).failed);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_GT(widest, 100.0f);
	EXPECT_LT(glm::distance(glm::xz(TransformOf(creature).position), glm::vec2(320.0f, 230.0f)), 6.0f);
	route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
}

namespace
{
/// A ring of overlapping circles 5 m round, 20 m about (320, 300), with nothing inside: no way in. Each comes into a
/// plan with the first square read that holds a cell it covers
void FillRing(int32_t cellX, int32_t cellZ, route_planner::ObstacleGrid& holder)
{
	constexpr int32_t k_Circles = 16;
	for (int32_t k = 0; k < k_Circles; ++k)
	{
		const double angle = 2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(k_Circles);
		const route_planner::Point2D centre {.x = 320.0f + static_cast<float>(20.0 * std::cos(angle)),
		                                     .z = 300.0f + static_cast<float>(20.0 * std::sin(angle))};
		if (CoversCell(centre, 5.0f, cellX, cellZ))
		{
			holder.AddObject(-1, centre, 5.0f, 0);
		}
	}
}

/// The creature sounds played at once
class PlayedSounds final: public ecs::systems::CreatureAudioSystemInterface
{
public:
	void Update(float /*turnFraction*/, std::chrono::duration<float, std::milli> /*gameTime*/) override {}
	void PlayQueued() override {}
	void QueueTurn(const ecs::systems::CreatureAnimationSystemInterface::TurnMoments& /*passed*/,
	               const ecs::systems::CreatureAnimationSystemInterface::TurnInputs& /*atEnd*/) override
	{
	}
	[[nodiscard]] bool IsMuted() const override { return false; }
	void SetMuted(bool /*muted*/) override {}
	[[nodiscard]] bool AreOtherVoicesEnabled() const override { return true; }
	void SetOtherVoicesEnabled(bool /*enabled*/) override {}
	void Play(entt::entity /*creature*/, const creature_audio::SoundEvent& /*event*/) override {}
	void PlayNow(entt::entity creature, audio::SoundAction action) override { played.emplace_back(creature, action); }

	std::vector<std::pair<entt::entity, audio::SoundAction>> played;
};
} // namespace

TEST_F(CreatureLocomotionSystemTest, WithNoWayThereItGivesUpWhatItWasDoingAndRefuses)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	const test::RestoreService<Locator::creatureAudioSystem> restoreAudio;
	auto& mind =
	    static_cast<test::creature_fakes::FakeMind&>(Locator::creatureMindSystem::emplace<test::creature_fakes::FakeMind>());
	auto& sounds = static_cast<PlayedSounds&>(Locator::creatureAudioSystem::emplace<PlayedSounds>());
	route_planner::ObstacleGrid::InstallCallbacks(&FillRing, nullptr);
	LoadWalkingRig();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(320.0f, 0.0f, 200.0f));
	_system.ProcessTurn();
	// the middle of the ring: the plan fails with nothing left to plan
	ASSERT_EQ(_system.MoveTo(creature, {320.0f, 300.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 600 && Of(creature).motion != Motion::Standing; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_TRUE(Of(creature).failed);
	// once: what it was doing given up as a failure, then the refusal
	ASSERT_EQ(mind.abandoned.size(), 1u);
	EXPECT_EQ(mind.abandoned.front(), creature);
	ASSERT_EQ(sounds.played.size(), 1u);
	EXPECT_EQ(sounds.played.front(), std::make_pair(creature, audio::SoundAction::Refusal));
	// it stood well short of the ring
	EXPECT_LT(TransformOf(creature).position.z, 280.0f);
	route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
}

TEST_F(CreatureLocomotionSystemTest, AWalkFromStandingKnowsNoObstacleFromAnEarlierWalk)
{
	g_WallUp = false;
	route_planner::ObstacleGrid::InstallCallbacks(&FillWall, nullptr);
	LoadWalkingRig();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(320.0f, 0.0f, 200.0f));
	_system.ProcessTurn();
	// a first walk reads the wall's middle square while the wall isn't there
	ASSERT_EQ(_system.MoveTo(creature, {320.0f, 180.0f}, Pace::Walk, 0.0f, 1.0f), MoveResult::Started);
	ASSERT_GT(RunUntilStanding(creature, 3000), 0);
	// the wall up, the next walk across it finds it and goes round
	g_WallUp = true;
	ASSERT_EQ(_system.MoveTo(creature, {320.0f, 230.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	float widest = 0.0f;
	for (int turn = 0; turn < 6000 && Of(creature).motion != Motion::Standing; ++turn)
	{
		_system.ProcessTurn();
		widest = std::max(widest, std::abs(TransformOf(creature).position.x - 320.0f));
	}
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_GT(widest, 100.0f);
	EXPECT_LT(glm::distance(glm::xz(TransformOf(creature).position), glm::vec2(320.0f, 230.0f)), 6.0f);
	route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
}

namespace
{
/// The map cells FillWall was asked for while the search's obstacle hook was installed, and while it wasn't
int g_FillsWithHook = 0;
int g_FillsWithoutHook = 0;
void FillWallCounting(int32_t cellX, int32_t cellZ, route_planner::ObstacleGrid& holder)
{
	++(route_planner::ObstacleGrid::HasObstacleHook() ? g_FillsWithHook : g_FillsWithoutHook);
	FillWall(cellX, cellZ, holder);
}
} // namespace

TEST_F(CreatureLocomotionSystemTest, AWalksFirstSearchRunsLongerNotingWhatItMeetsAndTheOthersRunTheFollowersOwn)
{
	EXPECT_EQ(creature_locomotion::k_SearchTurnsPerUpdate, 16);
	EXPECT_EQ(creature_locomotion::k_FirstSearchTurns, 256);
	g_WallUp = true;
	g_FillsWithHook = 0;
	g_FillsWithoutHook = 0;
	route_planner::ObstacleGrid::InstallCallbacks(&FillWallCounting, nullptr);
	LoadWalkingRig();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(320.0f, 0.0f, 200.0f));
	_system.ProcessTurn();
	// setting the plan up reads the square on the way there, before any search
	ASSERT_EQ(_system.MoveTo(creature, {320.0f, 230.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	EXPECT_EQ(g_FillsWithHook, 0);
	EXPECT_GT(g_FillsWithoutHook, 0);
	// the first search follows the wall into the squares beside it with the hook installed, and clears it after
	_system.ProcessTurn();
	EXPECT_GT(g_FillsWithHook, 0);
	EXPECT_FALSE(route_planner::ObstacleGrid::HasObstacleHook());
	const auto firstSearch = g_FillsWithHook;
	ASSERT_GT(RunUntilStanding(creature, 6000), 0);
	EXPECT_EQ(g_FillsWithHook, firstSearch);
	EXPECT_LT(glm::distance(glm::xz(TransformOf(creature).position), glm::vec2(320.0f, 230.0f)), 6.0f);
	route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
}

TEST_F(CreatureLocomotionSystemTest, TheCreaturesSearchingAsTheTurnStartsShareItsSearchTurnsAndEveryFollowerTakesTheShare)
{
	LoadWalkingRig();
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 300.0f));
	const auto idle = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f));
	_system.ProcessTurn();
	// before any share a creature has the 16 it was made with; with none searching, the share is all 32
	EXPECT_EQ(Of(first).searchTurns, creature_locomotion::k_SearchTurnsPerUpdate);
	_system.ShareSearchTurns();
	EXPECT_EQ(Of(idle).searchTurns, 32);
	ASSERT_EQ(_system.MoveTo(first, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	ASSERT_EQ(_system.MoveTo(second, {100.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	// a walk's fresh follower takes the creature's share
	ASSERT_NE(Of(first).follower, nullptr);
	EXPECT_EQ(Of(first).follower->GetTurnsPerUpdate(), 32);
	// both searching: 16 each, and the creature standing takes the same
	ASSERT_EQ(Of(first).follower->GetState(), route_planner::RouteFollower::State::Planning);
	ASSERT_EQ(Of(second).follower->GetState(), route_planner::RouteFollower::State::Planning);
	_system.ShareSearchTurns();
	EXPECT_EQ(Of(first).follower->GetTurnsPerUpdate(), 16);
	EXPECT_EQ(Of(second).follower->GetTurnsPerUpdate(), 16);
	EXPECT_EQ(Of(idle).searchTurns, 16);
}

TEST_F(CreatureLocomotionSystemTest, SentElsewhereWhileItsRouteIsPlannedTheSameFollowerTakesTheNewPoint)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	ASSERT_EQ(Of(creature).motion, Motion::Planning);
	const auto* follower = Of(creature).follower.get();
	ASSERT_NE(follower, nullptr);
	ASSERT_EQ(follower->GetState(), route_planner::RouteFollower::State::Planning);
	// further from the first point than half the way to it: the follower plans the new one in its stead
	ASSERT_EQ(_system.MoveTo(creature, {100.0f, 300.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	EXPECT_EQ(Of(creature).follower.get(), follower);
	EXPECT_EQ(Of(creature).motion, Motion::Planning);
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Standing; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_FALSE(Of(creature).failed);
	EXPECT_LT(glm::distance(glm::xz(TransformOf(creature).position), glm::vec2(100.0f, 300.0f)), 6.0f);
}

// Walking, it is bound to go no further along its route before turning off: the route re-planned on its way leaves
// from where it is, towards the new point
TEST_F(CreatureLocomotionSystemTest, SentElsewhereAsItWalksTheNewRouteTurnsOffWhereItIs)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 3000; ++turn)
	{
		_system.ProcessTurn();
		const auto* follower = Of(creature).follower.get();
		if (Of(creature).motion == Motion::Walking && follower != nullptr &&
		    follower->GetState() == route_planner::RouteFollower::State::Following && follower->GetAlongNode() > 10.0f)
		{
			break;
		}
	}
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 160.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	const auto& follower = *Of(creature).follower;
	ASSERT_EQ(follower.GetState(), route_planner::RouteFollower::State::Replanning);
	for (int turn = 0; turn < 64 && follower.GetState() == route_planner::RouteFollower::State::Replanning; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(follower.GetState(), route_planner::RouteFollower::State::Following);
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	const auto ahead = follower.GetRouteAhead();
	ASSERT_FALSE(ahead.empty());
	// nothing is kept straight on along +x first: the node it walks heads for the new point
	EXPECT_GT(ahead.front().to.z - ahead.front().from.z, 1.0f);
}

// Stepping off, it is bound to go the rest of its step's move along its route before a re-planned route turns off it
TEST_F(CreatureLocomotionSystemTest, SentElsewhereAsItStepsOffItKeepsToTheRestOfItsStepFirst)
{
	const auto creature = SteppingApe();
	ASSERT_EQ(Of(creature).motion, Motion::Stepping);
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 160.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	const auto& follower = *Of(creature).follower;
	ASSERT_EQ(follower.GetState(), route_planner::RouteFollower::State::Replanning);
	// the steps all go 5 straight on, at the scale it is drawn at
	float expected = 0.0f;
	for (int turn = 0; turn < 64 && follower.GetState() == route_planner::RouteFollower::State::Replanning; ++turn)
	{
		expected = creature_locomotion::SteppingStopDistance({0.0f, 0.0f, -5.0f}, Of(creature).scale, follower.GetAlongNode());
		_system.ProcessTurn();
	}
	ASSERT_EQ(follower.GetState(), route_planner::RouteFollower::State::Following);
	ASSERT_EQ(Of(creature).motion, Motion::Stepping);
	ASSERT_GT(expected, 0.01f);
	const auto ahead = follower.GetRouteAhead();
	ASSERT_GE(ahead.size(), 2u);
	// first a straight node that far along the old way, then the way to the new point
	EXPECT_NEAR(ahead.front().from.DistanceTo(ahead.front().to), expected, 1e-3f);
	EXPECT_NEAR(ahead.front().to.z, ahead.front().from.z, 1e-3f);
	EXPECT_GT(ahead.at(1).to.z - ahead.at(1).from.z, 1.0f);
}

TEST_F(CreatureLocomotionSystemTest, ItsPlansKeepThingsOffByItsNavRadius)
{
	// measures of its body that make its radius 5 at size 1
	const auto creature = StartedApe();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<CreatureBodyMetrics>(creature) = CreatureBodyMetrics {.restHeight = 48.0f, .reach = 16.0f};
	registry.Get<Creature>(creature).size = 1.0f;
	registry.Get<CreatureDrawPose>(creature).size.reset();
	ASSERT_EQ(ecs::object::GetNavRadius(creature), 5.0f);
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	ASSERT_NE(Of(creature).follower, nullptr);
	EXPECT_EQ(Of(creature).follower->GetMargin(), 5.0f);
}

// Stopped on its way as what it was doing ends, a walking creature brakes along its route: still moving, turning
// nowhere, until it stands where the route was cut short
TEST_F(CreatureLocomotionSystemTest, StoppedOnItsWayItBrakesAlongItsRouteToAStop)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 3000; ++turn)
	{
		_system.ProcessTurn();
		if (Of(creature).motion == Motion::Walking && Of(creature).speed > 1.0f)
		{
			break;
		}
	}
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	_system.BrakeToStop(creature);
	EXPECT_TRUE(Of(creature).braking);
	EXPECT_TRUE(_system.IsMoving(creature));
	ASSERT_NE(Of(creature).follower, nullptr);
	const auto end = Of(creature).follower->GetPlanStart();
	// short of the point it was going to
	EXPECT_LT(end.x, 295.0f);
	EXPECT_FALSE(_system.TurnToFace(creature, {100.0f, 300.0f}));
	EXPECT_FALSE(_system.TurnToHeading(creature, 1.0f));
	const auto turns = RunUntilStanding(creature, 300);
	ASSERT_GT(turns, 0);
	EXPECT_FALSE(Of(creature).braking);
	EXPECT_NEAR(TransformOf(creature).position.x, end.x, 1e-3f);
	// standing, it turns again
	EXPECT_TRUE(_system.TurnToFace(creature, {100.0f, 300.0f}));
}

// Stopped while its route is still planned, it stands at its next turn
TEST_F(CreatureLocomotionSystemTest, StoppedWhileItsRouteIsPlannedItStandsAtItsNextTurn)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	ASSERT_EQ(Of(creature).motion, Motion::Planning);
	_system.BrakeToStop(creature);
	EXPECT_TRUE(_system.IsMoving(creature));
	EXPECT_EQ(Of(creature).follower->GetState(), route_planner::RouteFollower::State::Idle);
	_system.ProcessTurn();
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_FALSE(_system.IsMoving(creature));
	EXPECT_FALSE(Of(creature).failed);
}

// Standing, or turning to face something, it stops as before
TEST_F(CreatureLocomotionSystemTest, NotOnAWalkTheStopOnItsWayIsTheStop)
{
	const auto creature = StartedApe();
	ASSERT_TRUE(_system.TurnToFace(creature, {100.0f, 300.0f}));
	_system.BrakeToStop(creature);
	EXPECT_FALSE(Of(creature).braking);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
}

// Every plan of its follower starts from its nav radius, setting off and sent elsewhere on its way alike
TEST_F(CreatureLocomotionSystemTest, ItsRoutePlansStartFromItsNavRadius)
{
	const auto creature = StartedApe();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<CreatureBodyMetrics>(creature) = CreatureBodyMetrics {.restHeight = 48.0f, .reach = 16.0f};
	registry.Get<Creature>(creature).size = 2.0f;
	registry.Get<CreatureDrawPose>(creature).size.reset();
	const auto navRadius = ecs::object::GetNavRadius(creature);
	ASSERT_NE(navRadius, Of(creature).radius);
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	ASSERT_NE(Of(creature).follower, nullptr);
	EXPECT_EQ(Of(creature).follower->GetDestRadius(), navRadius);
	ASSERT_EQ(_system.MoveTo(creature, {100.0f, 300.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	EXPECT_EQ(Of(creature).follower->GetDestRadius(), navRadius);
}

TEST_F(CreatureLocomotionSystemTest, InItsPenItsSpeedsAndStrideFollowThePenSize)
{
	const auto creature = StartedApe();
	auto& registry = test::creature_world::World::Registry();
	const auto ownSize = registry.Get<Creature>(creature).size;
	const auto ownScale = TransformOf(creature).scale;
	EXPECT_EQ(Of(creature).speeds.walk, creature_locomotion::SpeedsFor(ownSize).walk);
	EXPECT_EQ(Of(creature).scale, std::abs(ownScale.x));
	const auto ownRadius = Of(creature).radius;

	// drawn at 0.22 in its temple's pen, as the pen writes it at the start of the creature's turn
	auto& pose = registry.Get<CreatureDrawPose>(creature);
	pose.size = 0.22f;
	pose.given = 0.22f;
	pose.scale = ownScale * (0.22f / ownSize);
	_system.ProcessTurn();
	const auto pen = creature_locomotion::SpeedsFor(0.22f);
	EXPECT_EQ(Of(creature).speeds.walk, pen.walk);
	EXPECT_EQ(Of(creature).speeds.run, pen.run);
	EXPECT_EQ(Of(creature).scale, std::abs(pose.scale->x));
	// the radius it keeps clear by is its own size's, and its own size and Transform stay
	EXPECT_EQ(Of(creature).radius, ownRadius);
	EXPECT_EQ(registry.Get<Creature>(creature).size, ownSize);
	EXPECT_EQ(TransformOf(creature).scale, ownScale);

	// out of the pen, its body is given its own size again
	ecs::player_creature::GiveBodySize(registry, creature, ownSize, false);
	_system.ProcessTurn();
	EXPECT_EQ(Of(creature).speeds.run, creature_locomotion::SpeedsFor(ownSize).run);
	EXPECT_EQ(Of(creature).scale, std::abs(ownScale.x));
}

TEST_F(CreatureLocomotionSystemTest, AWalkBackGoesAtItsWalkAndTheHurry)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.WalkBack(creature, {300.0f, 100.0f}, 0.25f, 15.0f), MoveResult::Started);
	// the ape's walking fraction, 0.7, and the hurry
	EXPECT_FLOAT_EQ(Of(creature).fraction, 0.7f + 0.25f);
	EXPECT_EQ(Of(creature).destination, std::optional(glm::vec2(300.0f, 100.0f)));
	EXPECT_EQ(Of(creature).motion, Motion::Planning);
	EXPECT_EQ(_system.WalkBack(test::creature_world::World::Registry().Create(), {300.0f, 100.0f}, 0.25f, 15.0f),
	          MoveResult::Busy);
}

TEST_F(CreatureLocomotionSystemTest, StopStandsItWhereItIs)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Run, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 40; ++turn)
	{
		_system.ProcessTurn();
	}
	_system.Stop(creature);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_FALSE(Of(creature).destination.has_value());
	EXPECT_FALSE(_system.IsMoving(creature));
	const auto where = TransformOf(creature).position;
	_system.ProcessTurn();
	EXPECT_EQ(TransformOf(creature).position, where);
}

TEST_F(CreatureLocomotionSystemTest, PlaceGivesUpTheWalkAndPutsItThereAtOnceFacingAsItDid)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Run, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 40; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_TRUE(_system.IsMoving(creature));
	const auto heading = Of(creature).heading;
	// the given height counts for nothing: it stands on the ground there
	_system.Place(creature, {150.0f, 99.0f, 120.0f});
	EXPECT_FALSE(_system.IsMoving(creature));
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_FALSE(Of(creature).destination.has_value());
	EXPECT_FLOAT_EQ(Of(creature).heading, heading);
	const auto where = TransformOf(creature).position;
	EXPECT_FLOAT_EQ(where.x, 150.0f);
	EXPECT_FLOAT_EQ(where.z, 120.0f);
	EXPECT_NE(where.y, 99.0f);
	_system.ProcessTurn();
	EXPECT_EQ(TransformOf(creature).position, where);
}

TEST_F(CreatureLocomotionSystemTest, TurnToFaceTurnsOnTheSpot)
{
	const auto creature = StartedApe();
	const auto where = TransformOf(creature).position;
	// a quarter turn round: there are no spin animations, so the heading eases round over the stand's length
	ASSERT_TRUE(_system.TurnToFace(creature, {100.0f, 200.0f}));
	EXPECT_EQ(Of(creature).motion, Motion::Turning);
	EXPECT_GT(RunUntilStanding(creature, 100), 1);
	EXPECT_NEAR(Of(creature).heading, creature_locomotion::HeadingOf({0.0f, 1.0f}), 1e-4f);
	EXPECT_EQ(TransformOf(creature).position, where);
	// a point where it stands has no way to face
	EXPECT_FALSE(_system.TurnToFace(creature, glm::xz(where)));
}

TEST_F(CreatureLocomotionSystemTest, TurnToHeadingTurnsOnTheSpotHoweverLittleItIsOff)
{
	const auto creature = StartedApe();
	const auto where = TransformOf(creature).position;
	ASSERT_TRUE(_system.TurnToHeading(creature, 1.0f));
	EXPECT_EQ(Of(creature).motion, Motion::Turning);
	EXPECT_GT(RunUntilStanding(creature, 100), 1);
	EXPECT_NEAR(Of(creature).heading, 1.0f, 1e-4f);
	EXPECT_EQ(TransformOf(creature).position, where);
	// a little off is still a turn, not a snap
	ASSERT_TRUE(_system.TurnToHeading(creature, 1.05f));
	EXPECT_EQ(Of(creature).motion, Motion::Turning);
}

TEST_F(CreatureLocomotionSystemTest, TurnToHeadingEndsADanceFirstAndTurnsOnceItIsOver)
{
	const auto creature = StartedApe();
	auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
	const auto dance = creature_layers::PlayDance(animation.body, 500.0f);
	ASSERT_TRUE(dance.has_value());
	// still in its start, the finish is wanted and follows the start
	auto starting = *dance;
	starting = creature_layers::EndDance(starting);
	EXPECT_TRUE(starting.endWanted);
	EXPECT_EQ(starting.phase, creature_layers::BodyAction::Phase::Start);
	// in its moves, as here
	animation.body = *dance;
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	ASSERT_TRUE(_system.TurnToHeading(creature, 1.0f));
	// the dance's moves end and its finish plays; the turn waits for the body
	EXPECT_FALSE(creature_layers::IsDancing(animation.body));
	ASSERT_TRUE(Of(creature).turnAfterAction.has_value());
	EXPECT_FLOAT_EQ(*Of(creature).turnAfterAction, 1.0f);
	EXPECT_TRUE(_system.IsMoving(creature));
	EXPECT_NE(Of(creature).motion, Motion::Turning);
}

// The original's own headings for a creature taken hold of, as the yaw its body turns to (bits)
TEST(CreatureLocomotion, AHeldCreatureTurnsTowardsTheEyeAsTheOriginal)
{
	struct Case
	{
		glm::vec3 way;
		uint32_t yaw;
	};
	constexpr std::array<Case, 7> k_Cases {{
	    {{3.0f, 1.0f, 4.0f}, 0x401FE0BC},
	    {{-2.5f, 7.0f, 0.5f}, 0xBFE2541D},
	    {{0.03f, 0.0f, 0.0f}, 0x3FC90FDB},
	    {{0.0f, 0.0f, -0.05f}, 0x00000000},
	    {{-10.0f, 2.0f, -10.0f}, 0xBF490FDB},
	    {{5.0f, 0.0f, -0.01f}, 0x3FC8CE52},
	    // within 0.02 along both x and z: no turn, though the way has a direction
	    {{0.015f, 0.0f, 0.019f}, 0x00000000},
	}};
	const glm::vec3 body(100.0f, 5.0f, 200.0f);
	for (const auto& c : k_Cases)
	{
		const float heading = creature_locomotion::HeldTurnHeading({}, c.way);
		EXPECT_EQ(std::bit_cast<uint32_t>(-heading), c.yaw) << c.way.x << " " << c.way.z;
	}
	// the way is the eye less the body
	EXPECT_EQ(std::bit_cast<uint32_t>(-creature_locomotion::HeldTurnHeading(body, body + glm::vec3(3.0f, 1.0f, 4.0f))),
	          0x401FE0BCu);
}

TEST_F(CreatureLocomotionSystemTest, RunningAwayDrawsOnceFromTheSyncedStream)
{
	const auto creature = StartedApe();
	GameRandomSeeds before {};
	GameRandomSeeds after {};
	{
		const game_random::testing::ScopedState state;
		before = game_random::Current();
		ASSERT_EQ(_system.FleeFrom(creature, {90.0f, 100.0f}), MoveResult::Started);
		after = game_random::Current();
	}
	const game_random::testing::ScopedState reference;
	static_cast<void>(game_random::GameRand(40));
	EXPECT_EQ(game_random::Current().synced, after.synced);
	EXPECT_NE(after.synced, before.synced);
	EXPECT_EQ(after.local, before.local);
	// away from the threat, at least the species' distance
	ASSERT_TRUE(Of(creature).destination.has_value());
	EXPECT_GE(Of(creature).destination->x, 100.0f + 50.0f - 1e-3f);
}

TEST_F(CreatureLocomotionSystemTest, TheLegsAtAShareOfTheTurnAreWhatTheFrameDrawsThen)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Walking; ++turn)
	{
		_system.ProcessTurn();
	}
	_system.ProcessTurn();
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	const auto& self = Of(creature);
	ASSERT_FALSE(self.tracks.empty());
	const auto& animation = std::as_const(test::creature_world::World::Registry()).Get<const CreatureAnimation>(creature);
	for (const auto share : {0.5f, 1.0f})
	{
		_system.Update(share);
		// what the frame wrote, and the query and the pure part give the same, the body's own action left alone
		CreatureAnimationInputs inputs {.body = {.timeMs = 7.0f}, .slots = {}};
		_system.AnimationAt(creature, share, inputs);
		const auto pure = CreatureLocomotionSystem::SlotsAt(self.tracks, animation.breathPhase, share);
		ASSERT_EQ(animation.slots.size(), self.tracks.size());
		ASSERT_EQ(inputs.slots.size(), animation.slots.size());
		ASSERT_EQ(pure.size(), animation.slots.size());
		EXPECT_EQ(inputs.body.timeMs, 7.0f);
		for (size_t i = 0; i < animation.slots.size(); ++i)
		{
			const auto& track = self.tracks[i];
			EXPECT_EQ(animation.slots[i].animation, track.animation);
			EXPECT_EQ(inputs.slots[i].animation, track.animation);
			EXPECT_EQ(pure[i].timeMs, animation.slots[i].timeMs);
			EXPECT_EQ(inputs.slots[i].timeMs, animation.slots[i].timeMs);
			EXPECT_EQ(inputs.slots[i].weight, animation.slots[i].weight);
			if (!track.breathing && track.looping && track.durationMs > 0.0f)
			{
				// a looping track is that far on from where it was as the turn started, wrapped round
				EXPECT_EQ(animation.slots[i].timeMs, std::fmod(track.fromMs + (track.advanceMs * share), track.durationMs));
			}
		}
	}
	// past the turn's end it is drawn at the end, as the frame would draw it
	CreatureAnimationInputs past;
	_system.AnimationAt(creature, 1.5f, past);
	ASSERT_EQ(past.slots.size(), animation.slots.size());
	for (size_t i = 0; i < past.slots.size(); ++i)
	{
		EXPECT_EQ(past.slots[i].timeMs, animation.slots[i].timeMs);
	}
	// something that doesn't walk about gets nothing
	CreatureAnimationInputs none;
	_system.AnimationAt(test::creature_world::World::Registry().Create(), 1.0f, none);
	EXPECT_TRUE(none.slots.empty());
}

TEST_F(CreatureLocomotionSystemTest, TheTransformMovesOnlyOnceATurn)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Walking; ++turn)
	{
		_system.ProcessTurn();
	}
	_system.ProcessTurn();
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	const auto& self = Of(creature);
	ASSERT_NE(self.fromPosition, self.toPosition);
	const auto transform = TransformOf(creature);
	EXPECT_EQ(transform.position, self.toPosition);

	// a frame half way through the turn draws it half way, and leaves the Transform as it is
	_system.Update(0.5f);
	EXPECT_EQ(TransformOf(creature).position, transform.position);
	EXPECT_EQ(TransformOf(creature).rotation, transform.rotation);
	const auto& pose = test::creature_world::World::Registry().Get<CreatureDrawPose>(creature);
	EXPECT_EQ(pose.position, self.fromPosition + ((self.toPosition - self.fromPosition) * 0.5f));
	const auto heading = self.fromHeading + (creature_locomotion::WrapAngle(self.toHeading - self.fromHeading) * 0.5f);
	EXPECT_EQ(pose.rotation, affine::AngleY(-heading));
	// and the body is drawn there, while its Transform stays where the turn put it
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto between = ecs::creature_pose::BetweenTurns(registry, creature);
	ASSERT_TRUE(between.has_value());
	EXPECT_EQ(between->position, pose.position);
	EXPECT_EQ(between->rotation, pose.rotation);
	EXPECT_NE(between->position, transform.position);

	// put somewhere outside its turn, it is drawn there at once
	ecs::NotifyTeleported(creature);
	const auto snapped = ecs::creature_pose::BetweenTurns(registry, creature);
	ASSERT_TRUE(snapped.has_value());
	EXPECT_EQ(snapped->position, TransformOf(creature).position);
	EXPECT_EQ(snapped->rotation, TransformOf(creature).rotation);
	_system.Update(0.5f);
	EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(registry, creature).position, TransformOf(creature).position);

	// the next turn moves it on
	_system.ProcessTurn();
	EXPECT_NE(TransformOf(creature).position, transform.position);
	EXPECT_EQ(TransformOf(creature).position, Of(creature).toPosition);
}

TEST(CreaturePose, TheHeadingIsReadBackFromTheRotation)
{
	// the heading read back from the rotation a turn's end gives
	EXPECT_NEAR(ecs::creature_pose::ReadHeading(affine::AngleY(-1.25f)), 1.25f, 1e-6f);
}

TEST_F(CreatureLocomotionSystemTest, PutBackOnTheLandItCanStandOnIsAJump)
{
	const auto creature = StartedApe();
	_world.teleported.clear();
	// the land under it can no longer be stood on: setting off, it is put back where it can first (T4)
	Avoid(10, 10);
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), creature);
	const auto snapped = glm::xz(TransformOf(creature).position);
	EXPECT_TRUE(_system.IsValidPosition(snapped, land_avoid::k_CreatureTurningRadius));
	EXPECT_EQ(Of(creature).fromPosition, Of(creature).toPosition);

	// walking, it strays onto land it can't stand on: it is put back and sets off again (T5)
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Walking; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	_world.teleported.clear();
	const auto at = glm::xz(Of(creature).toPosition);
	const auto cellX = static_cast<int32_t>(at.x * 0.1f);
	const auto cellZ = static_cast<int32_t>(at.y * 0.1f);
	// the cells ahead of it
	for (int32_t x = cellX + 1; x <= cellX + 3; ++x)
	{
		Avoid(x, cellZ);
	}
	for (int turn = 0; turn < 200 && _world.teleported.empty(); ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_FALSE(_world.teleported.empty());
	EXPECT_EQ(_world.teleported.front(), creature);
	EXPECT_TRUE(Of(creature).destination.has_value());
}

namespace
{
/// A creature as its locomotion leaves it after a turn: its Transform where the turn ended, its drawn pose half way
struct PosedCreature
{
	ecs::Registry registry;
	entt::entity entity {registry.Create()};
	glm::vec3 at {100.0f, 2.0f, 50.0f};
	glm::vec3 drawnAt {95.0f, 2.0f, 50.0f};
	glm::mat3 drawnRotation {affine::AngleY(0.25f)};

	PosedCreature()
	{
		registry.Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(2.0f));
		auto& locomotion = registry.Assign<CreatureLocomotion>(entity);
		locomotion.started = true;
		locomotion.fromPosition = drawnAt;
		locomotion.toPosition = at;
		registry.Assign<CreatureDrawPose>(entity, CreatureDrawPose {.position = drawnAt, .rotation = drawnRotation});
	}
	[[nodiscard]] const ecs::Registry& Lookup() const { return registry; }
};

using StorageSizes = std::vector<std::pair<entt::id_type, size_t>>;

StorageSizes StorageSizesOf(const ecs::Registry& registry)
{
	StorageSizes sizes;
	registry.EachStorage(
	    [&sizes](entt::id_type id, const entt::sparse_set& storage) { sizes.emplace_back(id, storage.size()); });
	return sizes;
}
} // namespace

TEST(CreaturePose, BetweenTurnsIsTheDrawPoseWhileItsTurnHoldsTheTransform)
{
	const PosedCreature creature;
	const auto between = ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity);
	ASSERT_TRUE(between.has_value());
	EXPECT_EQ(between->position, creature.drawnAt);
	EXPECT_EQ(between->rotation, creature.drawnRotation);
	const auto drawn = ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity);
	EXPECT_EQ(drawn.position, creature.drawnAt);
	EXPECT_EQ(drawn.rotation, creature.drawnRotation);
}

TEST(CreaturePose, BetweenTurnsFallsBackWhenTheTransformWasMovedElsewhere)
{
	PosedCreature creature;
	const glm::vec3 elsewhere {300.0f, 0.0f, 300.0f};
	creature.registry.Get<Transform>(creature.entity).position = elsewhere;
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	const auto drawn = ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity);
	EXPECT_EQ(drawn.position, elsewhere);
	EXPECT_EQ(drawn.rotation, glm::mat3(1.0f));
}

TEST(CreaturePose, BetweenTurnsFallsBackBeforeTheFirstTurn)
{
	PosedCreature creature;
	creature.registry.Get<CreatureLocomotion>(creature.entity).started = false;
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity).position, creature.at);
}

TEST(CreaturePose, BetweenTurnsFallsBackInThePhysicsOrTheHand)
{
	{
		PosedCreature creature;
		creature.registry.Assign<PhysicsDrawPose>(creature.entity);
		EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	}
	{
		PosedCreature creature;
		const glm::vec3 inHand {10.0f, 40.0f, 12.0f};
		creature.registry.Assign<HandDrawPose>(creature.entity, HandDrawPose {.position = inHand});
		EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
		// drawn at the hand's pose, as ecs::DrawnModel draws it
		EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity).position, inHand);
	}
	// and anything that is no creature has no pose between turns
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(std::as_const(registry), thing).has_value());
}

TEST(CreaturePose, BetweenTurnsMakesNoStorage)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto before = StorageSizesOf(registry);
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(std::as_const(registry), thing).has_value());
	EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(std::as_const(registry), thing).position, glm::vec3(1.0f));
	EXPECT_EQ(StorageSizesOf(registry), before);

	// nor for a creature with no hand or physics pose
	const PosedCreature creature;
	const auto posed = StorageSizesOf(creature.registry);
	EXPECT_TRUE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	EXPECT_EQ(StorageSizesOf(creature.registry), posed);
}
