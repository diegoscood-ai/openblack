/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_near::NearestToLandBehindHand, the object the hand's press takes when nothing is under the cursor (issue #130):
// characterisation tests over a fake patch of forest like Land 1's, pinning what openblack does today. The original's
// rule (docs/bw1-notes/hand-and-interface.md, "A tree under the cursor") is the nearest object but a fragment under
// 5 m of the land behind the hand, by 2D distance, only when that distance is not more than the action point's own,
// the first of a tie in the cell walk (x outer, z inner, the fixed list then the mobile one), with no random draw.
// Every case below agrees with that rule; the one thing ours adds is the skip of an object that is not available or
// has no position (SkippedObjectIsNeverTaken), which the rule does not mention.
//
// The distance goes through the game's table inverse root, so it is only good to about 0.1%: every case keeps a
// margin of at least 2%, except the ties, which are built from mirrored offsets so that both distances are the same
// bits.

#include <cstddef>

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/HandNearObject.h"

using openblack::ecs::hand_near::Candidate;
using openblack::ecs::hand_near::NearestToLandBehindHand;

namespace
{
/// Map cells holding objects: each object goes into the cell of its position, in its fixed or its mobile list, in
/// the order it was added. The walk records the cells it was asked for
class FakeLand
{
public:
	entt::entity AddFixed(glm::vec3 position, bool fragment = false) { return Add(position, fragment, false); }
	entt::entity AddMobile(glm::vec3 position) { return Add(position, false, true); }

	/// The lookup gives nothing for it, as the game's does for an object that is not available or has no position
	void Skip(entt::entity object) { _objects[Index(object)].skipped = true; }

	[[nodiscard]] std::optional<entt::entity> Press(glm::vec3 behind, glm::vec3 at)
	{
		_walked.clear();
		return NearestToLandBehindHand(
		    behind, at,
		    [this](glm::ivec2 cell, const auto& visit) {
			    _walked.push_back(cell);
			    const auto found = _cells.find({cell.x, cell.y});
			    if (found == _cells.end())
			    {
				    return;
			    }
			    for (const auto* list : {&found->second.fixed, &found->second.mobile})
			    {
				    for (const entt::entity object : *list)
				    {
					    if (!visit(object))
					    {
						    return;
					    }
				    }
			    }
		    },
		    [this](entt::entity object) -> std::optional<Candidate> {
			    const Entry& entry = _objects[Index(object)];
			    if (entry.skipped)
			    {
				    return std::nullopt;
			    }
			    return entry.candidate;
		    });
	}

	[[nodiscard]] const std::vector<glm::ivec2>& Walked() const { return _walked; }

private:
	struct Entry
	{
		Candidate candidate;
		bool skipped {false};
	};
	struct Lists
	{
		std::vector<entt::entity> fixed;
		std::vector<entt::entity> mobile;
	};

	static std::size_t Index(entt::entity object) { return static_cast<std::size_t>(object); }

	entt::entity Add(glm::vec3 position, bool fragment, bool mobile)
	{
		const auto object = static_cast<entt::entity>(_objects.size());
		_objects.push_back({.candidate = {.position = position, .fragment = fragment}});
		const glm::ivec2 cell = openblack::map_coords::CellOf(position);
		auto& lists = _cells[{cell.x, cell.y}];
		(mobile ? lists.mobile : lists.fixed).push_back(object);
		return object;
	}

	std::vector<Entry> _objects;
	std::map<std::pair<int, int>, Lists> _cells;
	std::vector<glm::ivec2> _walked;
};

/// The land behind the hand, at (x, 0, z) as the landscape draw leaves it
glm::vec3 Behind(float x, float z)
{
	return {x, 0.0f, z};
}

/// An action point 20 m from the land behind the hand: the gate never holds the object back
glm::vec3 FarFrom(glm::vec3 behind)
{
	return {behind.x, 0.0f, behind.z + 20.0f};
}

/// A patch of forest like Land 1's: ten trees in about 15 x 13 m around (1001..1016, 1002..1015), in rows 4 to 5 m
/// apart, on a slope (their heights differ: the distance is measured on the map only), a broken piece of a tree, and a
/// villager walking through. The patch spans the map cells 100 and 101 on both axes (10 m cells)
class HandNearObjectForest: public ::testing::Test
{
protected:
	FakeLand land;
	// added before the trees: the walk still takes the fixed list of a cell before its mobile one
	const entt::entity villager = land.AddMobile({1010.5f, 22.0f, 1013.0f});
	// row 1, z = 1002
	const entt::entity tree0 = land.AddFixed({1002.0f, 20.0f, 1002.0f});
	const entt::entity tree1 = land.AddFixed({1007.0f, 20.5f, 1002.0f});
	const entt::entity tree2 = land.AddFixed({1013.0f, 21.0f, 1002.0f});
	// row 2, z = 1006
	const entt::entity tree3 = land.AddFixed({1004.5f, 20.5f, 1006.0f});
	const entt::entity tree4 = land.AddFixed({1010.5f, 21.0f, 1006.0f});
	const entt::entity tree5 = land.AddFixed({1016.0f, 21.5f, 1006.0f});
	// row 3, z = 1010.5
	const entt::entity tree6 = land.AddFixed({1001.0f, 21.0f, 1010.5f});
	const entt::entity tree7 = land.AddFixed({1006.0f, 21.5f, 1010.5f});
	const entt::entity tree8 = land.AddFixed({1013.0f, 22.0f, 1010.5f});
	// row 4, z = 1015
	const entt::entity tree9 = land.AddFixed({1009.0f, 22.5f, 1015.0f});
	// a fragment, between tree 4 and tree 8
	const entt::entity fragment = land.AddFixed({1011.0f, 21.0f, 1007.5f}, true);
};
} // namespace

// Between tree 7 and tree 9 the press takes the nearer one: 1.80 m against 3.61 m (the villager at 3.64), then,
// nearer to tree 9, 1.61 m against 3.80 m (the villager at 2.38)
TEST_F(HandNearObjectForest, PressBetweenTwoTreesTakesTheNearest)
{
	const glm::vec3 nearSeven = Behind(1007.0f, 1012.0f);
	EXPECT_EQ(land.Press(nearSeven, FarFrom(nearSeven)), tree7);
	const glm::vec3 nearNine = Behind(1008.2f, 1013.6f);
	EXPECT_EQ(land.Press(nearNine, FarFrom(nearNine)), tree9);
}

// West of the patch, tree 0 is the nearest: at 4.9 m it is taken, at 5.1 m nothing is (the next is tree 3 at 8.4 m).
// Its cell (100, 100) is walked both times, so it is the 5 m test that leaves it out, not the walk
TEST_F(HandNearObjectForest, PressJustInsideAndJustOutsideFiveMetres)
{
	const glm::vec3 inside = Behind(997.1f, 1002.0f);
	EXPECT_EQ(land.Press(inside, FarFrom(inside)), tree0);
	const glm::vec3 outside = Behind(996.9f, 1002.0f);
	EXPECT_EQ(land.Press(outside, FarFrom(outside)), std::nullopt);
	EXPECT_EQ(land.Walked().back(), glm::ivec2(100, 100));
}

// Tree 3 is 1.5 m north of the land behind the hand (the next are trees 0 and 1 at 3.54 m). An action point 1.5 m east
// is at the same distance, to the bit (mirrored offsets around a point with x = z), and the tree is taken (not
// farther: <=); 1.4 m east it is not; 1.6 m east it is
TEST_F(HandNearObjectForest, ActionPointGateOnBothSidesOfEquality)
{
	const glm::vec3 behind = Behind(1004.5f, 1004.5f);
	EXPECT_EQ(land.Press(behind, {1006.0f, 0.0f, 1004.5f}), tree3);
	EXPECT_EQ(land.Press(behind, {1005.9f, 0.0f, 1004.5f}), std::nullopt);
	EXPECT_EQ(land.Press(behind, {1006.1f, 0.0f, 1004.5f}), tree3);
}

// Tree 4 (1010.5, 1006) and tree 7 (1006, 1010.5) mirror each other around (1008.25, 1008.25), 3.18 m away to the bit
// (the fragment at 2.85 m is nearer and left out). Tree 4 was added first, but tree 7's cell (100, 101) is walked before
// tree 4's (101, 100), x outer: the first found keeps the tie (strict <)
TEST_F(HandNearObjectForest, TieGoesToTheFirstCellInTheWalk)
{
	const glm::vec3 behind = Behind(1008.25f, 1008.25f);
	EXPECT_EQ(land.Press(behind, FarFrom(behind)), tree7);
}

// Tree 8 (1013, 1010.5) and the villager (1010.5, 1013) mirror each other around (1011.5, 1011.5), 1.80 m away to the
// bit, in the same cell (101, 101). The villager was added first, but the fixed list is walked before the mobile one
TEST_F(HandNearObjectForest, TieInOneCellGoesToTheFixedList)
{
	const glm::vec3 behind = Behind(1011.5f, 1011.5f);
	EXPECT_EQ(land.Press(behind, FarFrom(behind)), tree8);
}

// Next to the fragment (0.36 m) the press takes tree 4 (1.24 m). A fragment alone, right under the press, is never
// taken
TEST_F(HandNearObjectForest, FragmentIsNeverTaken)
{
	const glm::vec3 behind = Behind(1010.8f, 1007.2f);
	EXPECT_EQ(land.Press(behind, FarFrom(behind)), tree4);

	FakeLand bare;
	const glm::vec3 onFragment = Behind(1011.0f, 1007.5f);
	bare.AddFixed({1011.0f, 21.0f, 1007.5f}, true);
	EXPECT_EQ(bare.Press(onFragment, FarFrom(onFragment)), std::nullopt);
}

// Ours only: an object the lookup skips (in the game, one that is not available or has no position) is never taken.
// With tree 4 skipped, the press next to the fragment takes tree 8 (3.97 m). The original's rule says nothing of this
TEST_F(HandNearObjectForest, SkippedObjectIsNeverTaken)
{
	land.Skip(tree4);
	const glm::vec3 behind = Behind(1010.8f, 1007.2f);
	EXPECT_EQ(land.Press(behind, FarFrom(behind)), tree8);
}

// North-east of the patch the nearest is tree 8 at 15.3 m: nothing
TEST_F(HandNearObjectForest, NothingWithinFiveMetres)
{
	const glm::vec3 behind = Behind(1025.0f, 1020.0f);
	EXPECT_EQ(land.Press(behind, FarFrom(behind)), std::nullopt);
}

// Around (1003, 1004) the square +-5 m covers the cells 99..100 on both axes, walked x outer, z inner; tree 0 (2.24 m)
// is taken over tree 3 (2.5 m)
TEST_F(HandNearObjectForest, WalksTheSquareXOuterZInner)
{
	const glm::vec3 behind = Behind(1003.0f, 1004.0f);
	EXPECT_EQ(land.Press(behind, FarFrom(behind)), tree0);
	const std::vector<glm::ivec2> expected {{99, 99}, {99, 100}, {100, 99}, {100, 100}};
	EXPECT_EQ(land.Walked(), expected);
}

// At the map's corner the square reaches the cells -1, which are off the map and not walked
TEST(HandNearObject, CellsOffTheMapAreNotWalked)
{
	FakeLand land;
	const entt::entity tree = land.AddFixed({1.0f, 5.0f, 1.0f});
	const glm::vec3 behind = Behind(2.0f, 3.0f);
	EXPECT_EQ(land.Press(behind, FarFrom(behind)), tree);
	const std::vector<glm::ivec2> expected {{0, 0}};
	EXPECT_EQ(land.Walked(), expected);
}
