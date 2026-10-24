/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the scripts' SET_POSITION does to a thing (ECS/ScriptSetPosition.h): the refusals, the creature, the things that
// are not objects and the other objects, and an object's turn once it is moved; and SET_FOCUS's turn of an object that
// is not living. And the YPOS property: a thing's height above the land, read and set.

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/MapCells.h"
#include "ECS/ScriptSetPosition.h"
#include "support/LandFakes.h"

using namespace openblack;
namespace set_position = openblack::ecs::script_set_position;
using set_position::Facts;
using set_position::Move;
using set_position::PlanFor;

namespace
{
/// A villager standing in the map
constexpr Facts k_Villager {.valid = true, .object = true, .inMap = true, .living = true};
} // namespace

TEST(ScriptSetPosition, NothingMovesForAThingGoneHeldOrFlying)
{
	EXPECT_EQ(PlanFor({}).move, Move::Refused);
	auto held = k_Villager;
	held.inHand = true;
	EXPECT_EQ(PlanFor(held).move, Move::Refused);
	auto flying = k_Villager;
	flying.flying = true;
	EXPECT_EQ(PlanFor(flying).move, Move::Refused);
	// a marker or a creature is refused the same, before anything else is asked
	EXPECT_EQ(PlanFor({.valid = true, .inHand = true, .scriptMarker = true}).move, Move::Refused);
	EXPECT_EQ(PlanFor({.valid = true, .flying = true, .object = true, .inMap = true, .creature = true}).move, Move::Refused);
}

TEST(ScriptSetPosition, AnObjectOutOfTheMapIsRefusedButAThingThatIsNotAnObjectIsNot)
{
	auto out = k_Villager;
	out.inMap = false;
	EXPECT_EQ(PlanFor(out).move, Move::Refused);
	EXPECT_EQ(PlanFor({.valid = true, .object = true, .creature = true}).move, Move::Refused);
	// a flock or a marker has no place in the map to be out of
	EXPECT_EQ(PlanFor({.valid = true}).move, Move::Copy);
	EXPECT_EQ(PlanFor({.valid = true, .scriptMarker = true}).move, Move::Copy);
}

TEST(ScriptSetPosition, OnlyAScriptMarkerKeepsItsHeight)
{
	const auto marker = PlanFor({.valid = true, .scriptMarker = true});
	EXPECT_TRUE(marker.keepsHeight);
	EXPECT_FALSE(marker.intoScriptState);
	EXPECT_FALSE(PlanFor({.valid = true}).keepsHeight);
	EXPECT_FALSE(PlanFor(k_Villager).keepsHeight);
	EXPECT_FALSE(PlanFor({.valid = true, .object = true, .inMap = true, .creature = true}).keepsHeight);
}

TEST(ScriptSetPosition, ACreatureIsPlacedAndNeverPutIntoAScriptState)
{
	const auto plan =
	    PlanFor({.valid = true, .object = true, .inMap = true, .creature = true, .living = true, .controlledByScript = true});
	EXPECT_EQ(plan.move, Move::Creature);
	EXPECT_FALSE(plan.intoScriptState);
}

TEST(ScriptSetPosition, ALivingThingAScriptControlsGoesIntoScriptStateFourAfterTheMove)
{
	auto controlled = k_Villager;
	controlled.controlledByScript = true;
	const auto plan = PlanFor(controlled);
	EXPECT_EQ(plan.move, Move::Object);
	EXPECT_TRUE(plan.intoScriptState);
	// not controlled, or controlled but not living: moved only
	EXPECT_FALSE(PlanFor(k_Villager).intoScriptState);
	EXPECT_EQ(PlanFor(k_Villager).move, Move::Object);
	EXPECT_FALSE(PlanFor({.valid = true, .object = true, .inMap = true, .controlledByScript = true}).intoScriptState);
	// a thing that is not an object never goes into a script state
	EXPECT_FALSE(PlanFor({.valid = true, .living = true, .controlledByScript = true}).intoScriptState);
}

TEST(ScriptSetPosition, ATurnAboutYAloneIsKeptAsItIs)
{
	for (const float angle : {0.0f, 1.0f, -2.5f, 3.14159f})
	{
		const auto rotation = affine::AngleY(angle);
		const auto upright = set_position::UprightRotation(rotation);
		for (int c = 0; c < 3; ++c)
		{
			for (int r = 0; r < 3; ++r)
			{
				EXPECT_EQ(upright[c][r], rotation[c][r]) << angle << " " << c << " " << r;
			}
		}
	}
}

TEST(ScriptSetPosition, ATiltIsLostAndTheYAngleKept)
{
	const float y = 0.75f;
	const auto tilted = affine::RotationYXZ(y, 0.3f, -0.2f);
	const auto upright = set_position::UprightRotation(tilted);
	const auto expected = affine::AngleY(ecs::map_cells::detail::YAngleOf(tilted));
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_EQ(upright[c][r], expected[c][r]) << c << " " << r;
		}
	}
	EXPECT_EQ(upright[1], glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_NEAR(ecs::map_cells::detail::YAngleOf(upright), y, 1e-5f);
}

TEST(ScriptSetPosition, FocusFacesThePointFromTheWholeMapUnitsAcross)
{
	// the place in whole map units across the land; the height does not count
	const glm::vec3 at(100.3f, 7.0f, 200.7f);
	const glm::vec3 target(110.0f, 0.0f, 195.0f);
	const glm::vec3 from(map_coords::Quantise(at.x), 0.0f, map_coords::Quantise(at.z));
	EXPECT_EQ(set_position::FocusYAngle(at, target), static_cast<float>(affine::GetYAngleBetween(from, target)));
	EXPECT_EQ(set_position::FocusYAngle(glm::vec3(at.x, -30.0f, at.z), target), set_position::FocusYAngle(at, target));
}

TEST(ScriptSetPosition, AThingsYPosIsItsHeightAboveTheLandAtItsPointInWholeMapUnits)
{
	const test::HeightFieldIsland island([](glm::vec2 at) { return 2.0f * at.x + at.y; });
	const glm::vec3 at(10.03f, 50.0f, 20.07f);
	const glm::vec2 point(map_coords::Quantise(at.x), map_coords::Quantise(at.z));
	EXPECT_EQ(set_position::HeightAboveLand(&island, at), at.y - (2.0f * point.x + point.y));
	ASSERT_FALSE(island.asked.empty());
	EXPECT_EQ(island.asked.back(), point);
	// without a land the height is the thing's own
	EXPECT_EQ(set_position::HeightAboveLand(nullptr, at), at.y);
}

TEST(ScriptSetPosition, SettingAThingsYPosPutsItThatHighAboveTheLandWhereItStands)
{
	const test::HeightFieldIsland island([](glm::vec2 at) { return 2.0f * at.x + at.y; });
	const glm::vec3 at(10.03f, 50.0f, 20.07f);
	const glm::vec2 point(map_coords::Quantise(at.x), map_coords::Quantise(at.z));
	// the point across is kept as it is; only the height moves
	EXPECT_EQ(set_position::AtHeightAboveLand(&island, at, 3.0f), glm::vec3(at.x, (2.0f * point.x + point.y) + 3.0f, at.z));
	// and it reads back as it was set, below the land too
	EXPECT_FLOAT_EQ(set_position::HeightAboveLand(&island, set_position::AtHeightAboveLand(&island, at, -4.5f)), -4.5f);
	EXPECT_EQ(set_position::AtHeightAboveLand(nullptr, at, 7.0f), glm::vec3(at.x, 7.0f, at.z));
}
