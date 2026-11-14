/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// (openblack) The state hash's entities trace (Debug/StateHash.h): what a traced hasher writes down is what it folds,
// and the hash is the same with or without the trace.

#include <cstdint>

#include <map>
#include <string>
#include <vector>

#include <LHVMTypes.h>
#include <gtest/gtest.h>

#include "Debug/StateHash.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Town.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownBelief.h"

using namespace openblack;

namespace
{
/// The same values, folded by `fold` into a plain hasher and a traced one
template <typename Fold>
std::pair<uint64_t, uint64_t> BothWays(Fold fold, state_hash::Trace& trace)
{
	state_hash::Hasher plain;
	fold(plain);
	state_hash::Hasher traced(&trace);
	fold(traced);
	return {plain.Value(), traced.Value()};
}
} // namespace

TEST(StateHashTrace, TheTraceDoesNotChangeTheHash)
{
	state_hash::Trace trace;
	const auto [plain, traced] = BothWays(
	    [](state_hash::Hasher& h) {
		    h.U32(7);
		    h.Entity(42);
		    h.Float(1.5f);
		    h.U64(0x1234u);
		    h.Vec3(glm::vec3(1.0f, -2.0f, 0.1f));
		    h.Mat3(glm::mat3(1.0f));
	    },
	    trace);
	EXPECT_EQ(plain, traced);
}

TEST(StateHashTrace, AnEntityFoldsAsItsId)
{
	state_hash::Hasher entity;
	entity.Entity(42);
	state_hash::Hasher u32;
	u32.U32(42);
	EXPECT_EQ(entity.Value(), u32.Value());
}

TEST(StateHashTrace, TheValuesGoToTheEntityBeforeThem)
{
	state_hash::Trace trace;
	state_hash::Hasher h(&trace);
	h.U32(7);
	h.Entity(42);
	h.Float(1.5f);
	h.U64(0xABu);
	h.Entity(43);
	h.U32(1);
	ASSERT_EQ(trace.size(), 3u);
	EXPECT_EQ(trace[0].key, "-");
	EXPECT_EQ(trace[0].values, std::vector<std::string> {"u7"});
	EXPECT_EQ(trace[1].key, "e42");
	EXPECT_EQ(trace[1].values, (std::vector<std::string> {"f3fc00000/1.5", "xab"}));
	EXPECT_EQ(trace[2].key, "e43");
	EXPECT_EQ(trace[2].values, std::vector<std::string> {"u1"});
	const auto lines = state_hash::TraceLines(6, "transform", trace);
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[1], "6 transform e42 f3fc00000/1.5 xab");
}

TEST(StateHashTrace, ATownsBeliefIsFoldedPendingRecentAndBoredomToo)
{
	// a town's record: its entity, then its belief, pending and recent belief per player and its boredom per reaction;
	// a pending belief that differs is a different hash
	ecs::components::TownBelief belief;
	belief.belief.at(1) = 0.25f;
	state_hash::Trace trace;
	state_hash::Hasher h(&trace);
	ecs::town_belief::HashTown(h, 12, belief);
	ASSERT_EQ(trace.size(), 1u);
	EXPECT_EQ(trace[0].key, "e12");
	EXPECT_EQ(trace[0].values.size(),
	          belief.belief.size() + belief.pending.size() + belief.recent.size() + belief.boredom.size());
	EXPECT_EQ(trace[0].values.at(1), "f3e800000/0.25");
	state_hash::Hasher same;
	ecs::town_belief::HashTown(same, 12, belief);
	belief.pending.at(1) = 0.01f;
	state_hash::Hasher moved;
	ecs::town_belief::HashTown(moved, 12, belief);
	EXPECT_EQ(same.Value(), h.Value());
	EXPECT_NE(moved.Value(), h.Value());
}

TEST(StateHashTrace, ACreaturesTraceHoldsWhatItsPartFolds)
{
	ecs::Registry registry;
	const auto entity = registry.Create();
	registry.AssignState<ecs::components::Creature>(entity, ecs::components::Creature {
	                                                            .owner = PlayerNames::PLAYER_ONE,
	                                                            .species = CreatureType::Mandrill,
	                                                            .mind = 0,
	                                                            .alignment = 0.25f,
	                                                        });
	state_hash::Trace trace;
	const ecs::Registry& view = registry;
	const auto [plain, traced] =
	    BothWays([&view](state_hash::Hasher& h) { ecs::player_creature::HashCreatures(h, view); }, trace);
	EXPECT_EQ(plain, traced);
	ASSERT_EQ(trace.size(), 1u);
	EXPECT_EQ(trace[0].key, "e" + std::to_string(static_cast<uint32_t>(entity)));
	// owner, species, leashable, inDevScript, alignment, fatness, strength, size, phase, has a home, home x y z, the
	// leashes known
	ASSERT_EQ(trace[0].values.size(), 14u);
	EXPECT_EQ(trace[0].values[4], "f3e800000/0.25");
	EXPECT_EQ(trace[0].values[8], "u4294967295");
}

TEST(StateHashTrace, AHashFilesEntityTraceGoesNextToIt)
{
	EXPECT_EQ(state_hash::EntitiesFileOf("C:/runs/x_hash.txt"), "C:/runs/x_hash_entities.txt");
	EXPECT_EQ(state_hash::EntitiesFileOf("x_hash"), "x_hash_entities.txt");
}

TEST(StateHashTrace, TheScriptsPartFoldsEachGlobalThenEachTasksScriptAndPlace)
{
	const std::vector<lhvm::VMVar> globals {
	    {lhvm::DataType::Float, lhvm::VMValue(0.0f), "Null variable"},
	    {lhvm::DataType::Float, lhvm::VMValue(1.0f), "GateKey2Present"},
	    {lhvm::DataType::Int, lhvm::VMValue(int32_t {82}), "LostBrotherFinished"},
	};
	std::map<uint32_t, lhvm::VMTask> tasks;
	lhvm::VMTask task;
	task.scriptId = 77;
	task.instructionAddress = 1234;
	tasks.emplace(5, task);
	state_hash::Trace trace;
	const auto [plain, traced] = BothWays([&](state_hash::Hasher& h) { state_hash::HashScripts(h, globals, tasks); }, trace);
	EXPECT_EQ(plain, traced);
	// the null variable is not folded: two globals, then the task
	ASSERT_EQ(trace.size(), 3u);
	EXPECT_EQ(trace[0].key, "e1");
	EXPECT_EQ(trace[0].values, (std::vector<std::string> {"u2", "f3f800000/1"}));
	EXPECT_EQ(trace[1].key, "e2");
	EXPECT_EQ(trace[1].values, (std::vector<std::string> {"u1", "u82"}));
	EXPECT_EQ(trace[2].key, "e" + std::to_string(state_hash::k_TaskKeyBase + 5));
	EXPECT_EQ(trace[2].values, (std::vector<std::string> {"u77", "u1234"}));
	// a task further on, or a global changed, is another hash
	auto later = tasks;
	later.at(5).instructionAddress = 1235;
	state_hash::Hasher moved;
	state_hash::HashScripts(moved, globals, later);
	EXPECT_NE(moved.Value(), plain);
	auto changed = globals;
	changed[1].value = lhvm::VMValue(2.0f);
	state_hash::Hasher set;
	state_hash::HashScripts(set, changed, tasks);
	EXPECT_NE(set.Value(), plain);
}
