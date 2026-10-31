/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ScriptHeaders/ScriptCreatureRules.h"

using namespace openblack;
using namespace openblack::script::creature_rules;

TEST(ScriptCreatureNatives, ConfinementIsOnTheGroundAsPreciseAsAMapPosition)
{
	const auto area = ConfinementFor({100.0f, 37.5f, 200.0f}, 25.0f);
	EXPECT_FLOAT_EQ(area.centre.y, 0.0f);
	EXPECT_FLOAT_EQ(area.radius, 25.0f);
	EXPECT_FLOAT_EQ(area.centre.x, map_coords::Quantise(100.0f));
	EXPECT_FLOAT_EQ(area.centre.z, map_coords::Quantise(200.0f));
	// A hundred-thousandth of a metre is less than a map unit: the centre keeps the whole units only
	const auto fine = ConfinementFor({10.00001f, 0.0f, 0.0f}, 1.0f);
	EXPECT_NEAR(fine.centre.x, 10.0f, 2e-4f);
	EXPECT_LE(fine.centre.x, 10.00001f);
}

TEST(ScriptCreatureNatives, ANegativeRadiusIsKeptAsGiven)
{
	// The game stores the radius as the script gives it; a radius of 0 or less keeps the creature nowhere
	EXPECT_FLOAT_EQ(ConfinementFor({0.0f, 0.0f, 0.0f}, -3.0f).radius, -3.0f);
}
