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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/StateHash.h"
#include "ECS/Components/Creature.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"

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
	// owner, species, leashable, inDevScript, alignment, fatness, strength, size, phase, has a home, home x y z
	ASSERT_EQ(trace[0].values.size(), 13u);
	EXPECT_EQ(trace[0].values[4], "f3e800000/0.25");
	EXPECT_EQ(trace[0].values[8], "u4294967295");
}

TEST(StateHashTrace, AHashFilesEntityTraceGoesNextToIt)
{
	EXPECT_EQ(state_hash::EntitiesFileOf("C:/runs/x_hash.txt"), "C:/runs/x_hash_entities.txt");
	EXPECT_EQ(state_hash::EntitiesFileOf("x_hash"), "x_hash_entities.txt");
}
