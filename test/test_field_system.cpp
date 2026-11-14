/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a field's crop looks (FieldSystemInterface::GetLook and its colour), over the field's own state, and how its draw
// sinks it with its food, in a registry of its own with no mesh boxes

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "3D/AxisAlignedBoundingBox.h"
#include "Common/Zoomer.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawList/GameBlocks.h"
#include "ECS/DrawList/ObjectDraw.h"
#include "ECS/DrawList/RegistryObjects.h"
#include "ECS/Fields.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/FieldSystem.h"
#include "ECS/Systems/Implementations/ObjectDrawListSystem.h"

using namespace openblack;
using ecs::components::Field;

namespace
{
Field FieldAt(float growth, float food)
{
	Field field {.town = -1};
	field.growth = growth;
	field.food = food;
	return field;
}

/// A camera that sees the test block whole: X = x, Y = y and the depth z + 1000
ecs::systems::DrawListFrame Frame()
{
	affine::AffineMatrix m;
	m.m[11] = 1000.0f;
	return {.camera = {.eye = {0.0f, 0.0f, 0.0f},
	                   .focus = {0.0f, 0.0f, 1.0f},
	                   .worldToClipping = m,
	                   .nearW = 0.3f,
	                   .half = {320.0f, 240.0f},
	                   .maxScreen = std::nullopt,
	                   .landReflection = false}};
}
} // namespace

TEST(FieldSystem, HiddenWhileTooYoungOrTooEmpty)
{
	const ecs::systems::FieldSystem fields;
	// A quarter of the growing age (80) and 25 food are the least that shows
	EXPECT_FALSE(fields.GetLook(FieldAt(19.9f, 100.0f)).has_value());
	EXPECT_FALSE(fields.GetLook(FieldAt(40.0f, 24.0f)).has_value());
	EXPECT_TRUE(fields.GetLook(FieldAt(20.0f, 25.0f)).has_value());
}

TEST(FieldSystem, GreenWhileGrowingThenWhite)
{
	const ecs::systems::FieldSystem fields;
	// Full while growing: olive
	const auto full = fields.GetLook(FieldAt(40.0f, 350.0f));
	ASSERT_TRUE(full.has_value());
	EXPECT_EQ(full->tint, 0x799119u);
	EXPECT_FLOAT_EQ(full->height, 0.0f);
	EXPECT_FALSE(full->sways);
	// Half full: half sunk
	const auto half = fields.GetLook(FieldAt(40.0f, 175.0f));
	ASSERT_TRUE(half.has_value());
	EXPECT_FLOAT_EQ(half->height, -0.5f);
	// Ripe: white, and it sways
	const auto ripe = fields.GetLook(FieldAt(1200.0f, 350.0f));
	ASSERT_TRUE(ripe.has_value());
	EXPECT_EQ(ripe->tint, 0xFFFFFFu);
	EXPECT_TRUE(ripe->sways);
}

TEST(FieldSystem, LightGreenWhenEmptyAndHalfWayToWhiteHalfWayToRipe)
{
	const ecs::systems::FieldSystem fields;
	// Growing and empty: light green (too empty to be drawn, but that is its colour)
	EXPECT_EQ(ecs::FieldDrawColour(FieldAt(50.0f, 0.0f)), glm::u8vec3(0xAA, 0xD4, 0x43));
	// Halfway from the growing age to ripe, three quarters full: halfway from olive to white, in whole steps, and a
	// quarter sunk
	const auto blend = [](uint32_t from, uint32_t to, uint32_t step) {
		uint32_t colour = 0;
		for (const uint32_t shift : {16u, 8u, 0u})
		{
			const uint32_t a = (from >> shift) & 0xFFu;
			const uint32_t b = (to >> shift) & 0xFFu;
			colour |= ((a * (255u - step) + b * step) / 255u) << shift;
		}
		return colour;
	};
	const auto ripening = fields.GetLook(FieldAt(640.0f, 262.5f));
	ASSERT_TRUE(ripening.has_value());
	EXPECT_EQ(ripening->tint, blend(0x799119u, 0xFFFFFFu, 127u));
	EXPECT_FLOAT_EQ(ripening->height, -0.25f);
	EXPECT_FALSE(ripening->sways);
}

// A drawn crop starts its one-second ease to its food's height again from where it is at every draw, then steps it by
// the frame's whole game milliseconds times 0.001 in floats, the speed's terms summed in the game's order for this step
// (Zoomer::UpdateInlineSpeed): a paused frame (0 ms) does not move it, and two 7 ms draws are not one 14 ms ease
TEST(FieldSystem, ADrawnCropStartsItsEaseAgainAndStepsByTheFramesWholeGameMilliseconds)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::meshBoxProvider> restoreBoxes;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& boxes = static_cast<test::FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>());
	boxes.meshBox = [](entt::id_type) { return std::optional<AxisAlignedBoundingBox> {}; };
	auto& registry = Locator::entitiesRegistry::value();
	const auto field = registry.Create();
	registry.Assign<Field>(field, FieldAt(40.0f, 175.0f));
	registry.Assign<ecs::components::Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Mesh>(field, entt::id_type {1}, int8_t {0}, int8_t {0});
	ecs::systems::FieldSystem fields;

	// half full: on its way to -0.5; two paused draws leave it where it starts
	EXPECT_TRUE(fields.Draw(field, 0));
	EXPECT_TRUE(fields.Draw(field, 0));
	EXPECT_EQ(registry.Get<Field>(field).sink.value, 0.0f);
	// two 7 ms draws, each a new ease from the last one's value and speed
	Zoomer expected;
	for (int i = 0; i < 2; ++i)
	{
		EXPECT_TRUE(fields.Draw(field, 7));
		expected.SetDestinationWithSpeedAndTime(-0.5f, 0.0f, 1.0f);
		expected.UpdateInlineSpeed(7.0f * 0.001f);
		EXPECT_EQ(registry.Get<Field>(field).sink.value, expected.value);
		EXPECT_EQ(registry.Get<Field>(field).sink.speed, expected.speed);
	}
	Zoomer once;
	once.SetDestinationWithSpeedAndTime(-0.5f, 0.0f, 1.0f);
	once.UpdateInlineSpeed(14.0f * 0.001f);
	EXPECT_NE(registry.Get<Field>(field).sink.value, once.value);
	// a whole second's draw: there
	EXPECT_TRUE(fields.Draw(field, 1000));
	EXPECT_EQ(registry.Get<Field>(field).sink.value, -0.5f);
}

// A crop too empty to show is hidden and its draw returns before the ease: it waits where it was
TEST(FieldSystem, AHiddenCropIsNotSteppedAndWaitsWhereItWas)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::meshBoxProvider> restoreBoxes;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& boxes = static_cast<test::FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>());
	boxes.meshBox = [](entt::id_type) { return std::optional<AxisAlignedBoundingBox> {}; };
	auto& registry = Locator::entitiesRegistry::value();
	const auto field = registry.Create();
	registry.Assign<Field>(field, FieldAt(40.0f, 175.0f));
	registry.Assign<ecs::components::Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Mesh>(field, entt::id_type {1}, int8_t {0}, int8_t {0});
	ecs::systems::FieldSystem fields;
	EXPECT_TRUE(fields.Draw(field, 100));
	const auto before = registry.Get<Field>(field).sink;
	// 24 food: below the least that shows
	registry.Get<Field>(field).food = 24.0f;
	EXPECT_FALSE(fields.Draw(field, 100));
	EXPECT_EQ(registry.Get<Field>(field).sink.value, before.value);
	EXPECT_EQ(registry.Get<Field>(field).sink.time, before.time);
	ASSERT_TRUE(registry.AllOf<ecs::components::Alpha>(field));
	EXPECT_EQ(registry.Get<ecs::components::Alpha>(field).value, 0.0f);
	// shown again: drawn opaque, the ease going on from where it waited
	registry.Get<Field>(field).food = 175.0f;
	EXPECT_TRUE(fields.Draw(field, 0));
	EXPECT_FALSE(registry.AllOf<ecs::components::Alpha>(field));
	EXPECT_EQ(registry.Get<Field>(field).sink.value, before.value);
}

// The object draw list draws the fields it meets through the game's field step (draw_list::DrawField, the first step of
// the list's consumer): a crop that shows sinks on the full pass and again on the still pass that follows, each draw
// starting its ease again, in the matrix it is drawn with only: its Transform never moves; a field just made by a land
// script (no growth, no food, as every field of Land 1 at its start) is hidden, so its draw returns before the ease,
// with the list's flag kept
TEST(FieldSystem, TheObjectDrawListSinksAShownCropAtEachDrawAndLeavesAHiddenOneWhereItIs)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	test::RestoreService<Locator::meshBoxProvider> restoreBoxes;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& boxes = static_cast<test::FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>());
	// a crop mesh 2 m high: half height 1
	boxes.meshBox = [](entt::id_type) {
		return std::optional(AxisAlignedBoundingBox {.minima = {-1.0f, 0.0f, -1.0f}, .maxima = {1.0f, 2.0f, 1.0f}});
	};
	auto& registry = Locator::entitiesRegistry::value();
	const auto makeField = [&registry](float growth, float food) {
		const auto field = registry.Create();
		registry.Assign<Field>(field, FieldAt(growth, food));
		registry.Assign<ecs::components::Transform>(field, glm::vec3(0.0f, 28.81f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ecs::components::Mesh>(field, entt::id_type {1}, int8_t {0}, int8_t {0});
		return field;
	};
	const auto shown = makeField(40.0f, 175.0f);
	const auto hidden = makeField(0.0f, 0.0f);
	ecs::draw_list::GameBlocks arrays;
	arrays.blocks[0].Insert(shown);
	arrays.blocks[0].Insert(hidden);
	const std::vector<ecs::systems::DrawListBlock> blocks {
	    {.mapPos = {0.0f, 0.0f}, .highestAltitude = 0, .slot = 0, .initial = {}},
	};
	ecs::draw_list::RegistryObjects objects(registry);
	ecs::systems::FieldSystem fields;
	ecs::systems::ObjectDrawListSystem drawList;
	std::vector<entt::entity> drawn;
	// the game's consumer: the field's step first (no mesh cache here, so the screen test keeps the list's flag)
	const ecs::draw_list::OnScreenView view {};
	const ecs::draw_list::Consumer consumer = [&](entt::entity object) -> std::optional<bool> {
		drawn.push_back(object);
		if (const auto result = ecs::draw_list::DrawField(registry, fields, object, 7, view); result.has_value())
		{
			return *result;
		}
		return true;
	};

	Zoomer expected;
	for (const uint32_t turn : {0u, 5u})
	{
		drawn.clear();
		drawList.Update(Frame(), {.blocks = blocks, .blockArrays = &arrays, .turn = turn, .objects = &objects}, consumer);
		// the full pass first, then the still pass: both draw both fields
		EXPECT_EQ(drawList.LastPassFull(), turn == 0u);
		EXPECT_EQ(drawn, (std::vector<entt::entity> {shown, hidden}));
		expected.SetDestinationWithSpeedAndTime(-0.5f, 0.0f, 1.0f);
		expected.UpdateInlineSpeed(7.0f * 0.001f);
		EXPECT_EQ(registry.Get<Field>(shown).sink.value, expected.value);
		// the drawn matrix rises by twice the sink x scale x half height; the field itself stays where it is
		const float sunk = 1.0f * 1.0f * expected.value;
		EXPECT_EQ(registry.Get<Field>(shown).drawnRise, sunk + sunk);
		EXPECT_EQ(registry.Get<ecs::components::Transform>(shown).position.y, 28.81f);
		EXPECT_EQ(registry.Get<Field>(hidden).sink.value, 0.0f);
		EXPECT_EQ(registry.Get<Field>(hidden).drawnRise, 0.0f);
		EXPECT_EQ(registry.Get<ecs::components::Transform>(hidden).position.y, 28.81f);
		EXPECT_EQ(drawList.ActiveOf(hidden), std::optional(ecs::draw_list::Active::Yes));
	}
	EXPECT_NE(registry.Get<Field>(shown).sink.value, 0.0f);
	EXPECT_LT(registry.Get<Field>(shown).drawnRise, 0.0f);
}
