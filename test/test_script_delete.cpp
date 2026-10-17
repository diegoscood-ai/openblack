/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What OBJECT_DELETE does by its mode (ECS/ScriptTypes.h: ScriptDeleteOf), and which things a script holds are objects
// (script_type::IsObject): the modes past 0 act on objects only. Then the kinds of things the scripts' finders see
// (script_type::TypeOf, SubtypeOf, Matches), with raffclar's cases (0222f9f5, test_script_find.cpp).

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Weather/WeatherThing.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace script_type = openblack::ecs::script_type;
using script_type::ScriptDelete;
using script_type::ScriptDeleteOf;

TEST(ScriptDelete, ModeZeroDeletesAnyThing)
{
	EXPECT_EQ(ScriptDeleteOf(0, true, false, false), ScriptDelete::Delete);
	EXPECT_EQ(ScriptDeleteOf(0, false, false, false), ScriptDelete::Delete);
	EXPECT_EQ(ScriptDeleteOf(0, true, true, false), ScriptDelete::Delete);
}

TEST(ScriptDelete, ModeOneFizzlesACreatureAndGhostsAnyOtherObject)
{
	EXPECT_EQ(ScriptDeleteOf(1, true, true, false), ScriptDelete::Fizzle);
	EXPECT_EQ(ScriptDeleteOf(1, true, false, false), ScriptDelete::GhostAndDelete);
	EXPECT_EQ(ScriptDeleteOf(1, false, false, false), ScriptDelete::Nothing);
}

TEST(ScriptDelete, ModeTwoBreaksUpObjectsOnly)
{
	EXPECT_EQ(ScriptDeleteOf(2, true, false, false), ScriptDelete::BreakUpAndDelete);
	EXPECT_EQ(ScriptDeleteOf(2, true, true, false), ScriptDelete::BreakUpAndDelete);
	EXPECT_EQ(ScriptDeleteOf(2, false, false, false), ScriptDelete::Nothing);
}

TEST(ScriptDelete, ModeThreeIsForTheCitadelHeartAlone)
{
	EXPECT_EQ(ScriptDeleteOf(3, true, false, true), ScriptDelete::CitadelHeart);
	EXPECT_EQ(ScriptDeleteOf(3, true, false, false), ScriptDelete::Nothing);
	EXPECT_EQ(ScriptDeleteOf(3, false, false, false), ScriptDelete::Nothing);
}

TEST(ScriptDelete, ModesPastThreeDoNothing)
{
	EXPECT_EQ(ScriptDeleteOf(4, true, false, false), ScriptDelete::Nothing);
	EXPECT_EQ(ScriptDeleteOf(-1, true, false, false), ScriptDelete::Nothing);
}

class ScriptIsObject: public ::testing::Test
{
protected:
	void SetUp() override { _registry = &Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	template <typename Component>
	entt::entity With()
	{
		const auto thing = _registry->Create();
		_registry->Assign<Transform>(thing);
		_registry->Assign<Component>(thing);
		return thing;
	}

	ecs::Registry* _registry {nullptr};

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};

TEST_F(ScriptIsObject, TheThingsThatAreNotObjects)
{
	EXPECT_FALSE(script_type::IsObject(With<Town>()));
	EXPECT_FALSE(script_type::IsObject(With<Flock>()));
	EXPECT_FALSE(script_type::IsObject(With<ScriptTimer>()));
	EXPECT_FALSE(script_type::IsObject(With<Mist>()));
	EXPECT_FALSE(script_type::IsObject(With<InfluenceRing>()));
	EXPECT_FALSE(script_type::IsObject(With<WeatherThing>()));
	EXPECT_FALSE(script_type::IsObject(With<PuzzleGame>()));
	EXPECT_FALSE(script_type::IsObject(With<Spell>()));
}

TEST_F(ScriptIsObject, ObjectsAndTheCitadelHeart)
{
	EXPECT_TRUE(script_type::IsObject(With<Tree>()));
	EXPECT_TRUE(script_type::IsObject(With<Creature>()));
	// the citadel heart is an object: mode 3 acts on it
	EXPECT_TRUE(script_type::IsObject(With<CitadelHeart>()));
}

TEST_F(ScriptIsObject, AMarkerIsTheMarkerType)
{
	// a script's marker is SCRIPT_OBJECT_TYPE 1 (MARKER) by its tag, so CALL finds it by that type with any sub-type;
	// a thing with only a place is no type at all
	const auto marker = ecs::archetypes::ScriptMarkerArchetype::Create(glm::vec3(100.0f, 0.0f, 200.0f));
	EXPECT_TRUE(_registry->AllOf<ScriptMarker>(marker));
	EXPECT_EQ(script_type::TypeOf(marker), script::ObjectType::Marker);
	EXPECT_TRUE(script_type::Matches(marker, script::ObjectType::Marker, script_type::k_AnySubtype));
	EXPECT_FALSE(script_type::Matches(marker, script::ObjectType::None, script_type::k_AnySubtype));
	const auto place = _registry->Create();
	_registry->Assign<Transform>(place);
	EXPECT_EQ(script_type::TypeOf(place), script::ObjectType::None);
}

TEST_F(ScriptIsObject, FlowersAreAFeatureWithNoSubtype)
{
	// a patch of flowers is SCRIPT_OBJECT_TYPE 3 (FEATURE), as a feature; it has no feature info row, so no sub-type
	const auto flowers = With<Flowers>();
	EXPECT_TRUE(script_type::IsObject(flowers));
	EXPECT_EQ(script_type::TypeOf(flowers), script::ObjectType::Feature);
	EXPECT_EQ(script_type::SubtypeOf(flowers), script_type::k_NoSubtype);
	EXPECT_TRUE(script_type::Matches(flowers, script::ObjectType::Feature, script_type::k_AnySubtype));
}

TEST_F(ScriptIsObject, AFootballIsTheBallType)
{
	// a football is SCRIPT_OBJECT_TYPE 28 (BALL), not its mobile object class's 20, and an object; CALL finds it by that
	// type with any sub-type (BALL has none)
	const auto ball = With<Ball>();
	EXPECT_TRUE(script_type::IsObject(ball));
	EXPECT_EQ(script_type::TypeOf(ball), script::ObjectType::Ball);
	EXPECT_EQ(static_cast<int>(script::ObjectType::Ball), 28);
	EXPECT_TRUE(script_type::Matches(ball, script::ObjectType::Ball, script_type::k_AnySubtype));
	EXPECT_FALSE(script_type::Matches(ball, script::ObjectType::MobileObject, script_type::k_AnySubtype));
}

TEST_F(ScriptIsObject, NothingIsNoObject)
{
	EXPECT_FALSE(script_type::IsObject(entt::null));
	const auto gone = _registry->Create();
	_registry->Destroy(gone);
	EXPECT_FALSE(script_type::IsObject(gone));
}

TEST_F(ScriptIsObject, AFinderMatchesTheTypeAndTheSubtype)
{
	const auto feature = _registry->Create();
	_registry->Assign<Transform>(feature);
	_registry->Assign<Feature>(feature, FeatureInfo {7});
	EXPECT_TRUE(script_type::Matches(feature, script::ObjectType::Feature, 7));
	EXPECT_TRUE(script_type::Matches(feature, script::ObjectType::Feature, script_type::k_AnySubtype));
	EXPECT_FALSE(script_type::Matches(feature, script::ObjectType::Feature, 8));
	EXPECT_FALSE(script_type::Matches(feature, script::ObjectType::Abode, script_type::k_AnySubtype));
}

TEST_F(ScriptIsObject, AThingWithNoSubtypeMatchesOnlyAnySubtype)
{
	EXPECT_FALSE(script_type::Matches(With<Flowers>(), script::ObjectType::Feature, 0));
}

TEST_F(ScriptIsObject, KindsOfThings)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	// the info row of a rock's mobile static says it is a rock
	info->mobileStatic.at(25).mobileType = static_cast<MobileStaticInfo>(2);
	Locator::infoConstants::reset(info.release());

	const auto make = [this]() {
		const auto thing = _registry->Create();
		_registry->Assign<Transform>(thing);
		return thing;
	};
	const auto feature = make();
	_registry->Assign<Feature>(feature, FeatureInfo {3});
	const auto still = make();
	_registry->Assign<MobileStatic>(still, MobileStaticInfo {5});
	const auto rock = make();
	_registry->Assign<MobileStatic>(rock, MobileStaticInfo {25});
	const auto sheep = make();
	_registry->Assign<Animal>(sheep).type = AnimalInfo::Sheep;
	const auto crow = make();
	_registry->Assign<Animal>(crow).type = AnimalInfo::Crow;
	const auto pot = make();
	_registry->Assign<Pot>(pot).type = PotInfo {10};
	const auto sign = make();
	_registry->Assign<ScriptHighlight>(sign).infoIndex = 1; // the did-you-know sign's row

	const auto expect = [](entt::entity thing, script::ObjectType type, uint32_t subtype) {
		EXPECT_EQ(script_type::TypeOf(thing), type);
		EXPECT_EQ(script_type::SubtypeOf(thing), subtype);
	};
	expect(feature, script::ObjectType::Feature, 3u);
	expect(still, script::ObjectType::MobileStatic, 5u);
	expect(rock, script::ObjectType::Rock, 25u);
	expect(sheep, script::ObjectType::Animal, static_cast<uint32_t>(AnimalInfo::Sheep));
	expect(crow, script::ObjectType::Bird, static_cast<uint32_t>(AnimalInfo::Crow));
	expect(pot, script::ObjectType::Store, 10u);
	// a script's scroll or sign (script type 37), by its row of the highlights' table
	EXPECT_EQ(static_cast<int>(script::ObjectType::Highlight), 37);
	expect(sign, script::ObjectType::Highlight, 1u);
}
