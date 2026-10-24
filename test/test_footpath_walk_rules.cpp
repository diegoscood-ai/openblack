/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The walk along a footpath, on fake footpaths: the choice of a walk to an object, and what a Living on a footpath does
// each time it reaches its goal. The expected goals, nodes and directions are the original game's, run on the same
// footpaths; the draws are the first two of the synced stream from the campaign seed (and from one other seed).

#include <cmath>
#include <cstdint>

#include <limits>
#include <map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/FootpathWalkRules.h"
#include "ECS/Footpaths.h"
#include "ECS/LivingFootpath.h"

using namespace openblack;
using namespace openblack::ecs;
using footpath_walk::NodeReached;
using map_coords::MapCoords;

namespace
{
/// Ten metres in map units
constexpr int32_t k_TenMetres = 0x10000;

/// The first two GameRand(0x2000) draws from the campaign seed, and from 0x12345678
constexpr uint32_t k_CampaignFirst = 2262;
constexpr uint32_t k_CampaignSecond = 388;
constexpr uint32_t k_OtherFirst = 6299;
constexpr uint32_t k_OtherSecond = 5630;

struct FakeNode
{
	MapCoords coords;
	uint8_t flags {0};
};

/// Footpaths as node lists from the head, a node's id its place in the list. NextNodeNear gives a canned answer and
/// records how it was asked
class FakeFootpaths final: public footpath_walk::FootpathQueries
{
public:
	std::map<entt::entity, std::vector<FakeNode>> footpaths;
	int32_t nearAnswer {0};
	footpaths::NodeId nearNode {footpaths::k_NoNode};
	mutable int32_t nearCalls {0};
	mutable int32_t nearDirection {-1};
	mutable float nearMaxDistance {0.0f};

	int32_t NextNodeNear(entt::entity footpath, [[maybe_unused]] const MapCoords& pos, footpaths::NodeId& node, MapCoords& out,
	                     int32_t direction, float maxDistance) const override
	{
		++nearCalls;
		nearDirection = direction;
		nearMaxDistance = maxDistance;
		if (nearAnswer == 1)
		{
			node = nearNode;
			out = footpaths.at(footpath).at(static_cast<size_t>(nearNode)).coords;
		}
		return nearAnswer;
	}

	int32_t StepNextNode(entt::entity footpath, footpaths::NodeId& node, MapCoords& out, int32_t direction) const override
	{
		const auto& nodes = footpaths.at(footpath);
		auto at = node;
		for (;;)
		{
			at = direction != 0 ? at - 1 : at + 1;
			if (at < 0 || static_cast<size_t>(at) >= nodes.size())
			{
				return footpaths::k_EndOfFootpath;
			}
			if ((nodes.at(static_cast<size_t>(at)).flags & footpaths::k_HiddenMask) == 0)
			{
				node = at;
				out = nodes.at(static_cast<size_t>(at)).coords;
				return 1;
			}
		}
	}

	[[nodiscard]] footpaths::NodeId HeadNode(entt::entity footpath) const override
	{
		return footpaths.at(footpath).empty() ? footpaths::k_NoNode : 0;
	}

	[[nodiscard]] footpaths::NodeId TailNode(entt::entity footpath) const override
	{
		return static_cast<footpaths::NodeId>(footpaths.at(footpath).size()) - 1;
	}

	[[nodiscard]] MapCoords NodeCoords(entt::entity footpath, footpaths::NodeId node) const override
	{
		return footpaths.at(footpath).at(static_cast<size_t>(node)).coords;
	}
};

const auto k_A = static_cast<entt::entity>(1);
const auto k_B = static_cast<entt::entity>(2);

MapCoords At(int32_t x, int32_t z = 0)
{
	return MapCoords {x, z, 0.0f};
}

/// Four nodes ten metres apart along x
std::vector<FakeNode> Line()
{
	return {{At(0)}, {At(k_TenMetres)}, {At(2 * k_TenMetres)}, {At(3 * k_TenMetres)}};
}

/// Draws from a fixed list, counting them
struct Draws
{
	std::vector<uint32_t> values;
	std::vector<uint32_t> asked;

	footpath_walk::GameRandDraw Fn()
	{
		return [this](uint32_t n) {
			asked.push_back(n);
			return values.at(asked.size() - 1);
		};
	}
};

components::LivingFootpath OnNode(footpaths::NodeId node, bool reverse, entt::entity next = entt::null)
{
	return components::LivingFootpath {.next = next, .footpath = k_A, .node = node, .reverse = reverse};
}
} // namespace

TEST(FootpathWalkRules, SameSpotIsTheGamesEquality)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(footpath_walk::SameSpot({1, 2, 1.0f}, {1, 2, 1.0f}));
	// NaN altitudes are not ordered apart, so they count as equal, against NaN or a number
	EXPECT_TRUE(footpath_walk::SameSpot({1, 2, nan}, {1, 2, nan}));
	EXPECT_TRUE(footpath_walk::SameSpot({1, 2, nan}, {1, 2, 0.0f}));
	EXPECT_TRUE(footpath_walk::SameSpot({1, 2, 0.0f}, {1, 2, -0.0f}));
	EXPECT_FALSE(footpath_walk::SameSpot({1, 2, 0.0f}, {1, 2, 1.0f}));
	EXPECT_FALSE(footpath_walk::SameSpot({1, 2, 0.0f}, {1, 3, 0.0f}));
}

TEST(FootpathWalkRules, OnlyALivingAtTheArrivePointSentElsewhereWalksDirectly)
{
	using footpath_walk::WalkToObject;
	const auto door = At(5, 7);
	EXPECT_EQ(footpath_walk::ChooseWalkToObject(door, door, At(9, 9)), WalkToObject::Direct);
	// sent to where it already stands, or standing anywhere else: through the object
	EXPECT_EQ(footpath_walk::ChooseWalkToObject(door, door, door), WalkToObject::ThroughObject);
	EXPECT_EQ(footpath_walk::ChooseWalkToObject(At(5, 8), door, At(9, 9)), WalkToObject::ThroughObject);
}

TEST(FootpathWalkRules, ANodeWithMoreBeyondGetsAGoalOffTheNode)
{
	FakeFootpaths fake;
	fake.footpaths[k_A] = Line();
	Draws draws {.values = {k_CampaignFirst, k_CampaignSecond}};
	auto walk = OnNode(1, false);
	const auto r = footpath_walk::ReachNode(fake, walk, At(k_TenMetres), draws.Fn());
	EXPECT_EQ(r.outcome, NodeReached::Outcome::NextNode);
	EXPECT_EQ(r.leftFootpath, k_A);
	EXPECT_EQ(r.leftNode, 1);
	EXPECT_TRUE(r.joinsNode);
	EXPECT_EQ(walk.node, 2);
	EXPECT_EQ(r.goal.x, 129238);
	EXPECT_EQ(r.goal.z, -3708);
	EXPECT_EQ(draws.asked, (std::vector<uint32_t> {0x2000, 0x2000}));
}

TEST(FootpathWalkRules, TheLastNodeIsReachedExactly)
{
	FakeFootpaths fake;
	fake.footpaths[k_A] = Line();
	Draws draws;
	auto walk = OnNode(2, false);
	const auto r = footpath_walk::ReachNode(fake, walk, At(2 * k_TenMetres), draws.Fn());
	EXPECT_EQ(r.outcome, NodeReached::Outcome::NextNode);
	EXPECT_EQ(walk.node, 3);
	EXPECT_EQ(r.goal, At(3 * k_TenMetres));
	EXPECT_TRUE(draws.asked.empty());
}

TEST(FootpathWalkRules, ABackwardWalkGoesTowardTheHead)
{
	FakeFootpaths fake;
	fake.footpaths[k_A] = Line();
	Draws draws {.values = {k_CampaignFirst, k_CampaignSecond}};
	auto walk = OnNode(2, true);
	const auto r = footpath_walk::ReachNode(fake, walk, At(2 * k_TenMetres), draws.Fn());
	EXPECT_EQ(walk.node, 1);
	EXPECT_EQ(r.goal.x, 63702);
	EXPECT_EQ(r.goal.z, -3708);
}

TEST(FootpathWalkRules, AHiddenNodeIsSteppedOver)
{
	FakeFootpaths fake;
	fake.footpaths[k_A] = {{At(0)}, {At(k_TenMetres)}, {At(2 * k_TenMetres), 4}, {At(3 * k_TenMetres)}, {At(4 * k_TenMetres)}};
	Draws draws {.values = {k_CampaignFirst, k_CampaignSecond}};
	auto walk = OnNode(1, false);
	const auto r = footpath_walk::ReachNode(fake, walk, At(k_TenMetres), draws.Fn());
	EXPECT_EQ(walk.node, 3);
	EXPECT_EQ(r.goal.x, 194774);
	EXPECT_EQ(r.goal.z, -3708);
}

TEST(FootpathWalkRules, TheDrawsMoveXThenZ)
{
	FakeFootpaths fake;
	fake.footpaths[k_A] = Line();
	Draws draws {.values = {k_OtherFirst, k_OtherSecond}};
	auto walk = OnNode(1, false);
	const auto r = footpath_walk::ReachNode(fake, walk, At(k_TenMetres), draws.Fn());
	EXPECT_EQ(r.goal.x, 133275);
	EXPECT_EQ(r.goal.z, 1534);
}

TEST(FootpathWalkRules, TheEndWithNoOtherFootpathFinishes)
{
	FakeFootpaths fake;
	fake.footpaths[k_A] = Line();
	Draws draws;
	auto walk = OnNode(3, false);
	const auto r = footpath_walk::ReachNode(fake, walk, At(3 * k_TenMetres), draws.Fn());
	EXPECT_EQ(r.outcome, NodeReached::Outcome::Finished);
	EXPECT_EQ(r.leftNode, 3);
	EXPECT_FALSE(r.joinsNode);
	EXPECT_EQ(walk.node, footpaths::k_NoNode);
	EXPECT_EQ(walk.footpath, k_A);
	EXPECT_TRUE(draws.asked.empty());
}

TEST(FootpathWalkRules, TheNextFootpathStartsFromItsNearerEnd)
{
	struct Case
	{
		std::vector<FakeNode> next;
		bool reverse;
		footpaths::NodeId node;
		MapCoords goal;
	};
	const std::vector<Case> cases = {
	    // the tail nearer: toward the head, one node in from the tail
	    {{{At(9 * k_TenMetres)}, {At(5 * k_TenMetres)}, {At(4 * k_TenMetres)}}, true, 1, At(5 * k_TenMetres)},
	    // the head nearer: away from the head, one node in from the head
	    {{{At(4 * k_TenMetres)}, {At(5 * k_TenMetres)}, {At(9 * k_TenMetres)}}, false, 1, At(5 * k_TenMetres)},
	    // both ends as far: away from the head
	    {{{At(3 * k_TenMetres, k_TenMetres)}, {At(9 * k_TenMetres)}, {At(3 * k_TenMetres, -k_TenMetres)}},
	     false,
	     1,
	     At(9 * k_TenMetres)},
	    // a single node: that node
	    {{{At(5 * k_TenMetres)}}, false, 0, At(5 * k_TenMetres)},
	};
	for (const auto& c : cases)
	{
		FakeFootpaths fake;
		fake.footpaths[k_A] = Line();
		fake.footpaths[k_B] = c.next;
		Draws draws;
		auto walk = OnNode(3, false, k_B);
		const auto r = footpath_walk::ReachNode(fake, walk, At(3 * k_TenMetres), draws.Fn());
		EXPECT_EQ(r.outcome, NodeReached::Outcome::NextFootpath);
		EXPECT_EQ(r.leftFootpath, k_A);
		EXPECT_EQ(r.leftNode, 3);
		EXPECT_TRUE(r.joinsNode);
		EXPECT_EQ(walk.footpath, k_B);
		EXPECT_TRUE(walk.next == entt::null);
		EXPECT_EQ(walk.reverse, c.reverse);
		EXPECT_EQ(walk.node, c.node);
		EXPECT_EQ(r.goal, c.goal);
		EXPECT_TRUE(draws.asked.empty());
	}
}

TEST(FootpathWalkRules, WithNoNodeOnlyANodeCloseByIsPickedUp)
{
	FakeFootpaths fake;
	fake.footpaths[k_A] = Line();
	Draws draws;
	auto walk = OnNode(footpaths::k_NoNode, true);
	// none close enough: the walk ends, with nothing to leave
	const auto r = footpath_walk::ReachNode(fake, walk, At(k_TenMetres + 100), draws.Fn());
	EXPECT_EQ(fake.nearCalls, 1);
	EXPECT_EQ(fake.nearDirection, 1);
	EXPECT_FLOAT_EQ(fake.nearMaxDistance, 0.03f);
	EXPECT_EQ(r.outcome, NodeReached::Outcome::Finished);
	EXPECT_EQ(r.leftNode, footpaths::k_NoNode);
	EXPECT_FALSE(r.joinsNode);
	// one found: it is joined and walked to like any other
	fake.nearAnswer = 1;
	fake.nearNode = 2;
	Draws more {.values = {k_CampaignFirst, k_CampaignSecond}};
	walk = OnNode(footpaths::k_NoNode, true);
	const auto found = footpath_walk::ReachNode(fake, walk, At(k_TenMetres), more.Fn());
	EXPECT_EQ(found.outcome, NodeReached::Outcome::NextNode);
	EXPECT_TRUE(found.joinsNode);
	EXPECT_EQ(walk.node, 2);
	EXPECT_EQ(found.goal.x, 129238);
}
