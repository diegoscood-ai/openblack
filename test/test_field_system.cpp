/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a field's crop looks (FieldSystemInterface::GetLook), over the field's own state: no services

#include <gtest/gtest.h>

#include "ECS/Components/Field.h"

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
