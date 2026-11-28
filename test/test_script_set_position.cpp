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
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Spell.h"
#include "ECS/CreaturePose.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
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

TEST(ScriptSetPosition, OnlyAThingInThePhysicsIsFlying)
{
	ecs::Registry registry;
	// thrown and still moving, carried by a tornado or left hanging: the physics marks it
	const auto thrown = registry.Create();
	registry.Assign<ecs::components::InPhysics>(thrown);
	EXPECT_TRUE(set_position::IsFlying(registry, thrown));
	// a thing come to rest has lost the mark, though the physics may keep its body to be knocked: it is moved
	const auto landed = registry.Create();
	EXPECT_FALSE(set_position::IsFlying(registry, landed));
	// nor is an id that names nothing
	EXPECT_FALSE(set_position::IsFlying(registry, entt::null));
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

TEST(ScriptSetPosition, ACreaturePutSomewhereStandsAtOnceLookingAhead)
{
	ecs::components::CreatureAnimation animation;
	animation.body.kind = creature_layers::BodyAction::Kind::Dance;
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	animation.body.timeMs = 400.0f;
	animation.slots.push_back({.animation = 3, .timeMs = 120.0f, .weight = 1.0f, .mirrored = false});
	animation.face = creature_layers::PullFace(animation.face, 7, 2000.0f);
	animation.face.current = 7;
	animation.face.timeMs = 80.0f;
	animation.lookAt = glm::vec3(10.0f, 2.0f, 30.0f);
	animation.yaw = {.angle = 0.5f, .velocity = 0.2f};
	animation.pitch = {.angle = -0.25f, .velocity = 0.1f};
	animation.breathPhase = 0.3f;
	animation.gesture = {.animation = 9, .timeMs = 50.0f};

	set_position::StandStill(animation);

	// the action and the walk's blend stop: it stands, from the start
	EXPECT_EQ(animation.body.kind, creature_layers::BodyAction::Kind::Stand);
	EXPECT_EQ(animation.body.timeMs, 0.0f);
	EXPECT_TRUE(animation.slots.empty());
	// no face, none wanted
	EXPECT_FALSE(animation.face.current.has_value());
	EXPECT_FALSE(animation.face.wanted.has_value());
	EXPECT_EQ(animation.face.timeMs, 0.0f);
	// the head straight ahead at once, looking at nothing
	EXPECT_FALSE(animation.lookAt.has_value());
	EXPECT_EQ(animation.yaw.angle, 0.0f);
	EXPECT_EQ(animation.yaw.velocity, 0.0f);
	EXPECT_EQ(animation.pitch.angle, 0.0f);
	EXPECT_EQ(animation.pitch.velocity, 0.0f);
	// the breath and a gesture play on
	EXPECT_EQ(animation.breathPhase, 0.3f);
	ASSERT_TRUE(animation.gesture.animation.has_value());
	EXPECT_EQ(*animation.gesture.animation, 9u);
}

// A body the hand took off the game starts its look afresh as it comes back: it stands at once playing nothing, its face
// none, its head straight ahead at nothing, and the sounds its last turn queued are dropped, as the original's look reset
// on reconnecting the body (docs/wiki/project/progress/hand/creature_contact.md)
TEST(ScriptSetPosition, ABodyBackFromTheHandStartsItsLookAfresh)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	auto& animation = registry.Assign<ecs::components::CreatureAnimation>(creature);
	animation.body.kind = creature_layers::BodyAction::Kind::Dance;
	animation.body.timeMs = 400.0f;
	animation.slots.push_back({.animation = 3, .timeMs = 120.0f, .weight = 1.0f, .mirrored = false});
	animation.face.current = 7;
	animation.lookAt = glm::vec3(10.0f, 2.0f, 30.0f);
	animation.yaw = {.angle = 0.5f, .velocity = 0.2f};
	auto& audio = registry.Assign<ecs::components::CreatureAudio>(creature);
	audio.queued.Add({.fraction = 0.5f, .action = {}, .mode = 0, .generic = true});
	ASSERT_EQ(audio.queued.Pending().size(), 1u);

	ecs::creature_pose::ResetLook(registry, creature);

	EXPECT_EQ(animation.body.kind, creature_layers::BodyAction::Kind::Stand);
	EXPECT_EQ(animation.body.timeMs, 0.0f);
	EXPECT_TRUE(animation.slots.empty());
	EXPECT_FALSE(animation.face.current.has_value());
	EXPECT_FALSE(animation.lookAt.has_value());
	EXPECT_EQ(animation.yaw.angle, 0.0f);
	EXPECT_TRUE(audio.queued.Pending().empty());
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

TEST(ScriptSetPosition, ATimersYPosIsZeroAndACastMiraclesIsTheHeightItsPlaceKeeps)
{
	ecs::Registry registry;
	// a timer stands at the zero place it is made with
	const auto timer = registry.Create();
	registry.Assign<ecs::components::ScriptTimer>(timer);
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, nullptr, timer), 0.0f);
	// a miracle keeps its place as the map has it: its height above the land is read as it is, below the land too
	const auto spell = registry.Create();
	registry.Assign<ecs::components::Spell>(spell).position = glm::vec3(120.5f, 3.25f, 80.0f);
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, nullptr, spell), 3.25f);
	registry.Get<ecs::components::Spell>(spell).position.y = -1.5f;
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, nullptr, spell), -1.5f);
	// a thing with no place of its own kept, or none at all, has no answer here
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, nullptr, registry.Create()), std::nullopt);
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, nullptr, entt::null), std::nullopt);
}

// A flock's height is that of its domain's centre above the land there, where the script made it and wherever its
// centre moves, and a dance's the height its place keeps, as the original reads each one's own place
TEST(ScriptSetPosition, AFlocksYPosIsItsCentresHeightAboveTheLandAndADancesItsPlaces)
{
	const test::HeightFieldIsland island([](glm::vec2 at) { return 2.0f * at.x + at.y; });
	ecs::Registry registry;
	const glm::vec3 at(10.03f, 50.0f, 20.07f);
	const auto flock = registry.Create();
	registry.Assign<ecs::components::Flock>(flock).domainCentre = at;
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, &island, flock), set_position::HeightAboveLand(&island, at));
	const glm::vec3 moved(40.5f, 12.0f, 7.25f);
	registry.Get<ecs::components::Flock>(flock).domainCentre = moved;
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, &island, flock), set_position::HeightAboveLand(&island, moved));
	const auto dance = registry.Create();
	registry.Assign<ecs::components::Dance>(dance).place = map_coords::MapCoords {.x = 100, .z = 200, .altitude = -3.5f};
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, &island, dance), -3.5f);
}

TEST(ScriptSetPosition, AnInfluenceRingsYPosIsTheHeightAboveTheLandOfItsPoint)
{
	const test::HeightFieldIsland island([](glm::vec2 at) { return 2.0f * at.x + at.y; });
	ecs::Registry registry;
	// a ring made at a point or following a thing keeps its point in the world: the land under it is taken away, at
	// its point in whole map units, as for a thing with a place
	const glm::vec3 at(10.03f, 50.0f, 20.07f);
	const auto ring = registry.Create();
	registry.Assign<ecs::components::InfluenceRing>(ring, at, PlayerNames::PLAYER_ONE, 100.0f, 0);
	const glm::vec2 point(map_coords::Quantise(at.x), map_coords::Quantise(at.z));
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, &island, ring), at.y - (2.0f * point.x + point.y));
	// it follows its thing's point as that moves
	const glm::vec3 moved(40.5f, 12.0f, 7.25f);
	registry.Get<ecs::components::InfluenceRing>(ring).position = moved;
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, &island, ring), set_position::HeightAboveLand(&island, moved));
	EXPECT_EQ(set_position::PlacelessHeightAboveLand(registry, nullptr, ring), moved.y);
}
