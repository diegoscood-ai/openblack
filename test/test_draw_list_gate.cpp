/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/ECS/DrawList/DrawGate.h: which objects the object draw list leaves out of a frame's draw (the list ran, in the
// map, not DontDraw, not drawn by the pass; a Norse Gate's lamp, its gate not found on the screen by its Draw; a wood
// or food pile sunk wholly into the land) and the frame's part for the renderer, sorted, with why. A registry of its
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
#include "ECS/Components/Pot.h"
#include "ECS/Components/StreetLantern.h"
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
	draw_gate::FillFrame(true, drawnThisFrame, drawnThisFrame, registry, inMap, frame);
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
	draw_gate::FillFrame(false, drawnThisFrame, drawnThisFrame, registry, inMap, frame);
	EXPECT_FALSE(frame.ran);
	EXPECT_TRUE(frame.hidden.empty());
	EXPECT_TRUE(frame.reasons.empty());
	EXPECT_TRUE(asked.empty());

	// the drawn objects may come in any order
	const V both {listedNotDrawn, drawn};
	draw_gate::FillFrame(true, both, both, registry, inMap, frame);
	EXPECT_EQ(frame.hidden, (V {notListed}));
	EXPECT_EQ(frame.reasons, (std::vector<ObjectListHidden> {ObjectListHidden::NotListed}));
}

TEST(DrawGate, TheDrawLeavesOutOnlyTheHiddenObjectsOfARunListWhenApplied)
{
	Registry registry;
	const auto hidden = MakeObject(registry);
	const auto drawn = MakeObject(registry);
	graphics::ObjectListFrame frame {.ran = true, .hidden = {hidden}, .reasons = {ObjectListHidden::NotListed}};
	// the switch off: the hidden objects are only read out
	EXPECT_FALSE(frame.LeftOut(hidden));
	frame.applied = true;
	EXPECT_TRUE(frame.LeftOut(hidden));
	EXPECT_FALSE(frame.LeftOut(drawn));
	// a frame that does not run the list leaves nothing out
	frame.ran = false;
	EXPECT_FALSE(frame.LeftOut(hidden));
}

TEST(DrawGate, AGatesLampIsHiddenUnlessItsGatesDrawFoundTheGateOnTheScreen)
{
	EXPECT_FALSE(draw_gate::GateLampHidden(true, true));
	EXPECT_TRUE(draw_gate::GateLampHidden(true, false));
	// the list did not run this frame: nothing is hidden
	EXPECT_FALSE(draw_gate::GateLampHidden(false, false));

	// the gate's two lamps are not map objects: only the gate's Draw draws them, right after the gate's own on-screen
	// test, whether the map files them or not
	Registry registry;
	const auto gate = MakeObject(registry);
	const auto offScreenGate = MakeObject(registry);
	const auto lamp = MakeObject(registry);
	const auto offScreenLamp = MakeObject(registry);
	const auto orphanLamp = MakeObject(registry);
	registry.Assign<components::GateLamp>(lamp, gate);
	registry.Assign<components::GateLamp>(offScreenLamp, offScreenGate);
	registry.Assign<components::GateLamp>(orphanLamp, entt::entity {entt::null});
	registry.AssignState<components::DrawListed>(gate);
	registry.AssignState<components::DrawListed>(offScreenGate);
	const std::set<entt::entity> outOfMap {lamp, offScreenLamp, orphanLamp};
	const auto inMap = [&outOfMap](entt::entity entity) { return !outOfMap.contains(entity); };

	graphics::ObjectListFrame frame;
	const V drawn {gate, offScreenGate};
	const V onScreen {gate};
	draw_gate::FillFrame(true, drawn, onScreen, registry, inMap, frame);
	V expected {offScreenLamp, orphanLamp};
	std::ranges::sort(expected);
	ASSERT_EQ(frame.hidden, expected);
	EXPECT_EQ(frame.reasons,
	          (std::vector<ObjectListHidden> {ObjectListHidden::GateNotOnScreen, ObjectListHidden::GateNotOnScreen}));

	// a gate the pass did not draw at all leaves its lamps out with it
	draw_gate::FillFrame(true, V {}, V {}, registry, inMap, frame);
	V all {gate, offScreenGate, lamp, offScreenLamp, orphanLamp};
	std::ranges::sort(all);
	EXPECT_EQ(frame.hidden, all);

	// a frame that does not run the list draws the lamps
	draw_gate::FillFrame(false, V {}, V {}, registry, inMap, frame);
	EXPECT_TRUE(frame.hidden.empty());
}

TEST(DrawGate, AGatesLampIsLitAtItsGatesPlace)
{
	// the gate's draw lights and hazes the gate at its own place and hands that colour and specular to both its lamps:
	// a lamp 15 units off its gate is lit where the gate stands
	Registry registry;
	const auto gate = MakeObject(registry);
	registry.Get<components::Transform>(gate).position = glm::vec3(1200.0f, 30.0f, 900.0f);
	const auto lamp = MakeObject(registry);
	registry.Get<components::Transform>(lamp).position = glm::vec3(1185.0f, 40.0f, 900.0f);
	registry.Assign<components::GateLamp>(lamp, gate);
	EXPECT_EQ(draw_gate::LampLightOrigin(registry, lamp), glm::vec3(1200.0f, 30.0f, 900.0f));
	// the gate itself, and anything that is not a lamp, take their own light
	EXPECT_FALSE(draw_gate::LampLightOrigin(registry, gate).has_value());
	// a lamp whose gate is gone has no gate's light to take
	const auto orphan = MakeObject(registry);
	registry.Assign<components::GateLamp>(orphan, entt::entity {entt::null});
	EXPECT_FALSE(draw_gate::LampLightOrigin(registry, orphan).has_value());
	registry.Destroy(gate);
	EXPECT_FALSE(draw_gate::LampLightOrigin(registry, lamp).has_value());
}

TEST(DrawGate, AWoodOrFoodPileIsDrawnOnlyWhileSomeOfItIsAboveTheLand)
{
	// the original's pile draws: drawn while the sink offset is above minus the pile's height, else nothing
	EXPECT_FALSE(draw_gate::PileBuried(0.0f, 2.0f));
	EXPECT_FALSE(draw_gate::PileBuried(-1.0f, 2.0f));
	EXPECT_FALSE(draw_gate::PileBuried(-1.999f, 2.0f));
	EXPECT_TRUE(draw_gate::PileBuried(-2.0f, 2.0f));
	EXPECT_TRUE(draw_gate::PileBuried(-2.5f, 2.0f));
	// an empty pile's target, (0 - 1) x its height, is exactly minus its height: buried
	const float height = 3.7f;
	EXPECT_TRUE(draw_gate::PileBuried((0.0f - 1.0f) * height, height));
	// a pile with no mesh has no height: buried at 0, drawn once above it
	EXPECT_TRUE(draw_gate::PileBuried(0.0f, 0.0f));
	EXPECT_FALSE(draw_gate::PileBuried(0.001f, 0.0f));

	Registry registry;
	const auto makePile = [&registry](float offset) {
		const auto pile = MakeObject(registry);
		registry.Assign<components::Pot>(pile, uint16_t {0}, uint16_t {100}, PotInfo::WoodPile_1);
		auto& sink = registry.Assign<components::PileSink>(pile, 0.0f, 2.0f);
		sink.offset.SetPosition(offset);
		registry.AssignState<components::DrawListed>(pile);
		return pile;
	};
	const auto buried = makePile(-2.0f);
	const auto rising = makePile(-1.5f);
	const auto notDrawnBuried = makePile(-2.0f);
	// a field sinks too, but has its own draw
	const auto field = MakeObject(registry);
	registry.Assign<components::PileSink>(field, 0.0f, 2.0f).offset.SetPosition(-2.0f);
	registry.AssignState<components::DrawListed>(field);
	const auto inMap = [](entt::entity /*entity*/) { return true; };

	graphics::ObjectListFrame frame;
	const V drawn {buried, rising, field};
	draw_gate::FillFrame(true, drawn, drawn, registry, inMap, frame);
	V expected {buried, notDrawnBuried};
	std::ranges::sort(expected);
	ASSERT_EQ(frame.hidden, expected);
	ASSERT_EQ(frame.reasons.size(), 2u);
	for (size_t i = 0; i < frame.hidden.size(); ++i)
	{
		// one the pass did not draw is left out for that first
		EXPECT_EQ(frame.reasons[i], frame.hidden[i] == buried ? ObjectListHidden::Buried : ObjectListHidden::NotDrawn);
	}

	// a frame that does not run the list leaves nothing out
	draw_gate::FillFrame(false, drawn, drawn, registry, inMap, frame);
	EXPECT_TRUE(frame.hidden.empty());
}
