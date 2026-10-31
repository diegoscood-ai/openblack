/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a field's crop looks (FieldSystemInterface::GetLook and its colour), over the field's own state: no services

#include <cstdint>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Field.h"
#include "ECS/Fields.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/FieldSystem.h"

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
