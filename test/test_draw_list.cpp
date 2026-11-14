/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object draw list's rebuild rule, its walk over the blocks' object arrays and its two passes, against the
// original's rules: fake objects, fake blocks and a fake Draw, no game data.

#include <cstdint>

#include <limits>
#include <optional>
#include <set>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/DrawList/BlockCull.h"
#include "ECS/DrawList/ObjectList.h"

using namespace openblack::ecs::draw_list;

namespace
{
constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();

[[nodiscard]] entt::entity Id(uint32_t n)
{
	return static_cast<entt::entity>(n);
}

/// Objects as plain sets: everything is available unless removed
class FakeProbe final: public EntityProbe
{
public:
	[[nodiscard]] bool Available(entt::entity entity) const override
	{
		return entity != entt::null && !unavailable.contains(entity);
	}
	[[nodiscard]] bool DontDraw(entt::entity entity) const override { return dontDraw.contains(entity); }
	[[nodiscard]] bool IsHuman(entt::entity entity) const override { return humans.contains(entity); }
	[[nodiscard]] bool IsComplex(entt::entity entity) const override { return complex.contains(entity); }
	[[nodiscard]] bool Listed(entt::entity entity) const override { return listed.contains(entity); }
	void SetListed(entt::entity entity, bool on) override
	{
		if (on)
		{
			listed.insert(entity);
		}
		else
		{
			listed.erase(entity);
		}
	}

	std::set<entt::entity> unavailable;
	std::set<entt::entity> dontDraw;
	std::set<entt::entity> humans;
	std::set<entt::entity> complex;
	std::set<entt::entity> listed;
};

/// Land blocks with their distances, each in its own table slot, and the objects of each slot
struct FakeLand
{
	explicit FakeLand(std::vector<float> distances)
	{
		for (size_t i = 0; i < distances.size(); ++i)
		{
			states.push_back(BlockState {.distance = distances[i]});
			slotOfBlock.push_back(static_cast<uint16_t>(i));
		}
		arrays.resize(distances.size());
	}

	[[nodiscard]] BlockArraysView View()
	{
		spans.assign(arrays.begin(), arrays.end());
		return {.slotOfBlock = slotOfBlock, .slots = spans};
	}

	std::vector<BlockState> states;
	std::vector<uint16_t> slotOfBlock;
	std::vector<std::vector<entt::entity>> arrays;
	std::vector<std::span<const entt::entity>> spans;
};

/// A Draw that records its calls and answers from a table (nullopt for an object with no answer)
struct FakeDraw
{
	[[nodiscard]] Consumer AsConsumer()
	{
		return [this](entt::entity entity) -> std::optional<bool> {
			calls.push_back(entity);
			if (onScreen.contains(entity))
			{
				return true;
			}
			if (offScreen.contains(entity))
			{
				return false;
			}
			return std::nullopt;
		};
	}

	std::set<entt::entity> onScreen;
	std::set<entt::entity> offScreen;
	std::vector<entt::entity> calls;
};

[[nodiscard]] RebuildState Settled(uint32_t lastTurn)
{
	return {.count = 0, .lastTurn = lastTurn, .rebuiltThisFrame = false};
}
} // namespace

// The rebuild rule

TEST(DrawListRebuild, TheFirstFrameRebuildsAndRunsTheFullPass)
{
	RebuildState state;
	EXPECT_TRUE(RebuildDue(state, 0, false));
	StartRebuild(state, 0);
	EXPECT_EQ(state.count, 0);
	EXPECT_TRUE(TakeFullPass(state, {}, {}));
	EXPECT_FALSE(state.rebuiltThisFrame);
}

TEST(DrawListRebuild, ACountOfTwoGivesTwoRebuildsInARow)
{
	RebuildState state = Settled(5);
	state.count = 2;
	EXPECT_TRUE(RebuildDue(state, 5, false));
	StartRebuild(state, 5);
	EXPECT_EQ(state.count, 1);
	EXPECT_TRUE(RebuildDue(state, 5, false));
	StartRebuild(state, 5);
	EXPECT_EQ(state.count, 0);
	EXPECT_FALSE(RebuildDue(state, 5, false));
}

TEST(DrawListRebuild, TheTurnRuleRebuildsOnTheEleventhTurn)
{
	const RebuildState state = Settled(20);
	EXPECT_FALSE(RebuildDue(state, 30, false));
	EXPECT_TRUE(RebuildDue(state, 31, false));
}

TEST(DrawListRebuild, StartRebuildStoresTheTurnAndAZeroCountStaysZero)
{
	RebuildState state = Settled(0);
	StartRebuild(state, 31);
	EXPECT_EQ(state.count, 0);
	EXPECT_EQ(state.lastTurn, 31u);
	EXPECT_TRUE(state.rebuiltThisFrame);
	EXPECT_FALSE(RebuildDue(state, 41, false));
	EXPECT_TRUE(RebuildDue(state, 42, false));
}

TEST(DrawListRebuild, APausedTurnDoesNotTrigger)
{
	RebuildState state = Settled(0);
	StartRebuild(state, 11);
	for (int frame = 0; frame < 100; ++frame)
	{
		EXPECT_FALSE(RebuildDue(state, 11, false));
	}
}

TEST(DrawListRebuild, TheLandFlagTriggers)
{
	const RebuildState state = Settled(40);
	EXPECT_FALSE(RebuildDue(state, 40, false));
	EXPECT_TRUE(RebuildDue(state, 40, true));
}

TEST(DrawListRebuild, ATurnThatWentBackWaitsUntilItPassesTheLastRebuild)
{
	// The turn rule alone, with the turn set back below the last rebuild's (which is never reset). At a real land load
	// the cleared map's rebuild count rebuilds anyway
	const RebuildState state = Settled(500);
	EXPECT_FALSE(RebuildDue(state, 0, false));
	EXPECT_FALSE(RebuildDue(state, 11, false));
	EXPECT_FALSE(RebuildDue(state, 510, false));
	EXPECT_TRUE(RebuildDue(state, 511, false));
}

TEST(DrawListRebuild, TheTurnCompareIsUnsigned)
{
	// The last turn plus 10 wraps to 2
	const RebuildState state = Settled(0xFFFFFFF8u);
	EXPECT_FALSE(RebuildDue(state, 2, false));
	EXPECT_TRUE(RebuildDue(state, 3, false));
}

// Which pass runs

TEST(DrawListPass, AStillCameraRunsTheStillPass)
{
	RebuildState state = Settled(0);
	EXPECT_FALSE(TakeFullPass(state, {}, {}));
	// Exactly 1 away (squared) is still
	EXPECT_FALSE(TakeFullPass(state, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}));
}

TEST(DrawListPass, AMovedEyeOrFocusRunsTheFullPassAndStoresBoth)
{
	RebuildState state = Settled(0);
	EXPECT_TRUE(TakeFullPass(state, {1.5f, 0.0f, 0.0f}, {0.0f, 0.5f, 0.0f}));
	EXPECT_EQ(state.p, glm::vec3(1.5f, 0.0f, 0.0f));
	EXPECT_EQ(state.f, glm::vec3(0.0f, 0.5f, 0.0f));

	EXPECT_TRUE(TakeFullPass(state, {1.5f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}));
	EXPECT_EQ(state.f, glm::vec3(0.0f, 2.0f, 0.0f));
}

TEST(DrawListPass, CameraMovedDoesNotStore)
{
	const RebuildState state = Settled(0);
	EXPECT_TRUE(CameraMoved(state, {0.0f, 0.0f, 2.0f}, {}));
	EXPECT_EQ(state.p, glm::vec3(0.0f));
}

TEST(DrawListPass, ANaNDistanceCountsAsStill)
{
	RebuildState state = Settled(0);
	EXPECT_FALSE(CameraMoved(state, {k_NaN, 0.0f, 0.0f}, {}));
	EXPECT_FALSE(CameraMoved(state, {}, {0.0f, k_NaN, 0.0f}));
	EXPECT_FALSE(TakeFullPass(state, {k_NaN, 0.0f, 0.0f}, {}));
}

TEST(DrawListPass, ARebuildFrameDoesNotStoreTheCameraSoDriftAddsUp)
{
	RebuildState state = Settled(0);
	// A rebuild with the camera moved: the pass is full, but the pair is not stored
	StartRebuild(state, 11);
	EXPECT_TRUE(TakeFullPass(state, {0.8f, 0.0f, 0.0f}, {}));
	EXPECT_EQ(state.p, glm::vec3(0.0f));
	// Each later step is small, but it is measured from the stored pair, not from the frame before
	EXPECT_FALSE(TakeFullPass(state, {0.9f, 0.0f, 0.0f}, {}));
	EXPECT_TRUE(TakeFullPass(state, {1.2f, 0.0f, 0.0f}, {}));
	EXPECT_EQ(state.p, glm::vec3(1.2f, 0.0f, 0.0f));
}

// Joining the list

TEST(DrawListAdd, TheTests)
{
	EXPECT_TRUE(TryAdd(0, false, true, false));
	EXPECT_TRUE(TryAdd(k_MaxEntries - 1, false, true, false));
	EXPECT_FALSE(TryAdd(k_MaxEntries, false, true, false));
	EXPECT_FALSE(TryAdd(0, true, true, false));
	EXPECT_FALSE(TryAdd(0, false, false, false));
	EXPECT_FALSE(TryAdd(0, false, true, true));
}

// The walk

TEST(DrawListCollect, BlocksNearestFirstEachArrayInOrderAndTheGlobalListAfterTheFirstBlock)
{
	FakeLand land({30.0f, 10.0f, 20.0f});
	land.arrays[0] = {Id(1), Id(2)};
	land.arrays[1] = {Id(3)};
	land.arrays[2] = {Id(4), Id(5)};
	const std::vector<entt::entity> global {Id(8), Id(9)};
	const std::vector<uint16_t> visible {1, 2, 0};
	FakeProbe probe;
	List list;

	Collect(visible, land.states, land.View(), global, probe, k_VanishObjectDistance, list);

	const std::vector<entt::entity> expected {Id(3), Id(8), Id(9), Id(4), Id(5), Id(1), Id(2)};
	EXPECT_EQ(list.entries, expected);
	EXPECT_EQ(list.active.size(), expected.size());
	for (const auto entity : expected)
	{
		EXPECT_TRUE(probe.Listed(entity));
	}
}

TEST(DrawListCollect, NoBlockMeansNoGlobalList)
{
	FakeLand land({10.0f});
	land.arrays[0] = {Id(1)};
	const std::vector<entt::entity> global {Id(8)};
	FakeProbe probe;
	List list;

	Collect({}, land.states, land.View(), global, probe, k_VanishObjectDistance, list);
	EXPECT_TRUE(list.entries.empty());
	EXPECT_FALSE(probe.Listed(Id(8)));

	// The only block is at the vanish distance, so the walk ends before it
	const std::vector<uint16_t> visible {0};
	Collect(visible, land.states, land.View(), global, probe, 10.0f, list);
	EXPECT_TRUE(list.entries.empty());
}

TEST(DrawListCollect, TheVanishDistanceEndsTheWalkButANaNDoesNot)
{
	FakeLand land({5.0f, k_NaN, 50.0f, 6.0f});
	land.arrays[0] = {Id(1)};
	land.arrays[1] = {Id(2)};
	land.arrays[2] = {Id(3)};
	land.arrays[3] = {Id(4)};
	// A break, not a skip: the block at 6 after the one at 50 is not reached
	const std::vector<uint16_t> visible {1, 0, 2, 3};
	FakeProbe probe;
	List list;

	Collect(visible, land.states, land.View(), {}, probe, 40.0f, list);

	const std::vector<entt::entity> expected {Id(2), Id(1)};
	EXPECT_EQ(list.entries, expected);
}

TEST(DrawListCollect, AnObjectInTwoBlocksIsListedOnce)
{
	FakeLand land({1.0f, 2.0f});
	land.arrays[0] = {Id(1), Id(2)};
	land.arrays[1] = {Id(2), Id(3)};
	const std::vector<uint16_t> visible {0, 1};
	FakeProbe probe;
	List list;

	Collect(visible, land.states, land.View(), {}, probe, k_VanishObjectDistance, list);

	const std::vector<entt::entity> expected {Id(1), Id(2), Id(3)};
	EXPECT_EQ(list.entries, expected);
}

TEST(DrawListCollect, UnavailableAndDontDrawObjectsAreSkipped)
{
	FakeLand land({1.0f});
	land.arrays[0] = {Id(1), Id(2), Id(3), entt::null};
	const std::vector<uint16_t> visible {0};
	FakeProbe probe;
	probe.unavailable.insert(Id(1));
	probe.dontDraw.insert(Id(2));
	List list;

	Collect(visible, land.states, land.View(), {}, probe, k_VanishObjectDistance, list);

	const std::vector<entt::entity> expected {Id(3)};
	EXPECT_EQ(list.entries, expected);
	EXPECT_FALSE(probe.Listed(Id(1)));
	EXPECT_FALSE(probe.Listed(Id(2)));
}

TEST(DrawListCollect, AFullListSkipsLaterObjects)
{
	FakeLand land({1.0f, 2.0f});
	for (uint32_t n = 1; n <= k_MaxEntries + 1; ++n)
	{
		land.arrays[0].push_back(Id(n));
	}
	land.arrays[1] = {Id(10000)};
	const std::vector<entt::entity> global {Id(20000)};
	const std::vector<uint16_t> visible {0, 1};
	FakeProbe probe;
	List list;

	Collect(visible, land.states, land.View(), global, probe, k_VanishObjectDistance, list);

	ASSERT_EQ(list.entries.size(), k_MaxEntries);
	EXPECT_EQ(list.entries.back(), Id(static_cast<uint32_t>(k_MaxEntries)));
	EXPECT_FALSE(probe.Listed(Id(static_cast<uint32_t>(k_MaxEntries + 1))));
	EXPECT_FALSE(probe.Listed(Id(10000)));
	EXPECT_FALSE(probe.Listed(Id(20000)));
}

TEST(DrawListCollect, ARebuildClearsTheOldEntriesMarks)
{
	FakeLand land({1.0f});
	land.arrays[0] = {Id(1), Id(2)};
	const std::vector<uint16_t> visible {0};
	FakeProbe probe;
	List list;
	Collect(visible, land.states, land.View(), {}, probe, k_VanishObjectDistance, list);

	// The object moved out: the next rebuild unmarks it and lists the other again
	land.arrays[0] = {Id(2)};
	Collect(visible, land.states, land.View(), {}, probe, k_VanishObjectDistance, list);

	const std::vector<entt::entity> expected {Id(2)};
	EXPECT_EQ(list.entries, expected);
	EXPECT_FALSE(probe.Listed(Id(1)));
	EXPECT_TRUE(probe.Listed(Id(2)));
}

TEST(DrawListCollect, ANulledEntryKeepsItsMarkAcrossARebuild)
{
	FakeLand land({1.0f});
	land.arrays[0] = {Id(1), Id(2)};
	const std::vector<uint16_t> visible {0};
	FakeProbe probe;
	List list;
	Collect(visible, land.states, land.View(), {}, probe, k_VanishObjectDistance, list);

	// A pass nulls the first entry while the object is unavailable; it comes back, still marked
	probe.unavailable.insert(Id(1));
	FakeDraw draw;
	FullPass(list, probe, draw.AsConsumer());
	ASSERT_EQ(list.entries[0], entt::entity {entt::null});
	probe.unavailable.erase(Id(1));

	Collect(visible, land.states, land.View(), {}, probe, k_VanishObjectDistance, list);

	const std::vector<entt::entity> expected {Id(2)};
	EXPECT_EQ(list.entries, expected);
	EXPECT_TRUE(probe.Listed(Id(1)));
}

// The passes

TEST(DrawListFullPass, ActiveFromTheFlagOrAHumanOrComplexObject)
{
	List list {.entries = {Id(1), Id(2), Id(3), Id(4), Id(5), entt::null, Id(7)}};
	FakeProbe probe;
	probe.humans.insert(Id(3));
	probe.complex.insert(Id(4));
	probe.unavailable.insert(Id(7));
	FakeDraw draw;
	draw.onScreen = {Id(1)};
	draw.offScreen = {Id(2), Id(3), Id(4)};

	FullPass(list, probe, draw.AsConsumer());

	const std::vector<Active> expected {Active::Yes,       Active::No, Active::Yes, Active::Yes,
	                                    Active::NotPorted, Active::No, Active::No};
	EXPECT_EQ(list.active, expected);
	// The null and the unavailable entries are not drawn, and the unavailable one is nulled
	const std::vector<entt::entity> calls {Id(1), Id(2), Id(3), Id(4), Id(5)};
	EXPECT_EQ(draw.calls, calls);
	EXPECT_EQ(list.entries[6], entt::entity {entt::null});
}

TEST(DrawListStillPass, OnlyActiveEntriesAreDrawnAndActiveCarriesOver)
{
	List list {.entries = {Id(1), Id(2), Id(3)}};
	FakeProbe probe;
	FakeDraw draw;
	draw.onScreen = {Id(1), Id(3)};
	draw.offScreen = {Id(2)};
	FullPass(list, probe, draw.AsConsumer());

	// Off screen now, but the still pass neither asks nor recomputes Active
	draw.onScreen = {};
	draw.offScreen = {Id(1), Id(2), Id(3)};
	for (int frame = 0; frame < 3; ++frame)
	{
		draw.calls.clear();
		StillPass(list, probe, draw.AsConsumer());
		const std::vector<entt::entity> calls {Id(1), Id(3)};
		EXPECT_EQ(draw.calls, calls);
	}
	const std::vector<Active> expected {Active::Yes, Active::No, Active::Yes};
	EXPECT_EQ(list.active, expected);
}

TEST(DrawListStillPass, AnUnavailableActiveEntryIsNulled)
{
	List list {.entries = {Id(1), Id(2)}};
	FakeProbe probe;
	FakeDraw draw;
	draw.onScreen = {Id(1), Id(2)};
	FullPass(list, probe, draw.AsConsumer());

	probe.unavailable.insert(Id(1));
	draw.calls.clear();
	StillPass(list, probe, draw.AsConsumer());

	const std::vector<entt::entity> calls {Id(2)};
	EXPECT_EQ(draw.calls, calls);
	EXPECT_EQ(list.entries[0], entt::entity {entt::null});
	EXPECT_EQ(list.active[0], Active::No);
}

TEST(DrawListStillPass, AnObjectWithNoPortedDrawIsNotDrawn)
{
	List list {.entries = {Id(1)}};
	FakeProbe probe;
	FakeDraw draw;
	FullPass(list, probe, draw.AsConsumer());
	ASSERT_EQ(list.active[0], Active::NotPorted);

	draw.calls.clear();
	StillPass(list, probe, draw.AsConsumer());
	EXPECT_TRUE(draw.calls.empty());
}
