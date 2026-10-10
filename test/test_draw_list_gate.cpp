/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/ECS/DrawList/DrawGate.h: which objects the object draw list leaves out of a frame's draw (the list ran, in the
// map, not DontDraw, not drawn by the pass) and the frame's part for the renderer, sorted, with why. A registry of its
// own and a fake map: nothing here reads the locator.

#include <algorithm>
#include <set>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/DontDraw.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawList/DrawGate.h"
#include "ECS/Registry.h"
#include "Graphics/ObjectListFrame.h"

using namespace openblack;
using namespace openblack::ecs;
using graphics::ObjectListHidden;
using V = std::vector<entt::entity>;

namespace
{
/// An object the renderer would give a row: a Mesh and a Transform
entt::entity MakeObject(Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<components::Mesh>(entity, entt::id_type {1}, int8_t {0}, int8_t {0});
	registry.Assign<components::Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}
} // namespace

TEST(DrawGate, HiddenOnlyWhenTheListRanAndAMapObjectWasNotDrawn)
{
	EXPECT_TRUE(draw_gate::Hidden(true, true, false, false));
	// the list did not run this frame (the temple, a film): nothing is hidden
	EXPECT_FALSE(draw_gate::Hidden(false, true, false, false));
	// not in the map: held, flying, the hand
	EXPECT_FALSE(draw_gate::Hidden(true, false, false, false));
	// DontDraw: something else draws its look
	EXPECT_FALSE(draw_gate::Hidden(true, true, true, false));
	// drawn by the pass
	EXPECT_FALSE(draw_gate::Hidden(true, true, false, true));
}

TEST(DrawGate, TheFrameListsTheHiddenObjectsSortedWithWhy)
{
	Registry registry;
	const auto drawn = MakeObject(registry);
	const auto notListed = MakeObject(registry);
	const auto listedNotDrawn = MakeObject(registry);
	const auto held = MakeObject(registry);
	const auto dontDraw = MakeObject(registry);
	const auto noMesh = registry.Create();
	registry.Assign<components::Transform>(noMesh, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.AssignState<components::DrawListed>(drawn);
	registry.AssignState<components::DrawListed>(listedNotDrawn);
	registry.AssignState<components::DontDraw>(dontDraw);
	const std::set<entt::entity> outOfMap {held};
	std::vector<entt::entity> asked;
	const auto inMap = [&outOfMap, &asked](entt::entity entity) {
		asked.push_back(entity);
		return !outOfMap.contains(entity);
	};

	graphics::ObjectListFrame frame;
	const V drawnThisFrame {drawn};
	draw_gate::FillFrame(true, drawnThisFrame, registry, inMap, frame);
	EXPECT_TRUE(frame.ran);
	V expected {notListed, listedNotDrawn};
	std::ranges::sort(expected);
	ASSERT_EQ(frame.hidden, expected);
	ASSERT_EQ(frame.reasons.size(), 2u);
	for (size_t i = 0; i < frame.hidden.size(); ++i)
	{
		EXPECT_EQ(frame.reasons[i], frame.hidden[i] == notListed ? ObjectListHidden::NotListed : ObjectListHidden::NotDrawn);
	}

	// a frame that does not run the list hides nothing and asks the map nothing; the last frame's part is cleared
	asked.clear();
	draw_gate::FillFrame(false, drawnThisFrame, registry, inMap, frame);
	EXPECT_FALSE(frame.ran);
	EXPECT_TRUE(frame.hidden.empty());
	EXPECT_TRUE(frame.reasons.empty());
	EXPECT_TRUE(asked.empty());

	// the drawn objects may come in any order
	const V both {listedNotDrawn, drawn};
	draw_gate::FillFrame(true, both, registry, inMap, frame);
	EXPECT_EQ(frame.hidden, (V {notListed}));
	EXPECT_EQ(frame.reasons, (std::vector<ObjectListHidden> {ObjectListHidden::NotListed}));
}
