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
// Last, the native over the world's lists: mode 1 leaves a ghost of the object's model as it deletes it.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string_view>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ParticleContainer.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Weather/WeatherThing.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

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

TEST(ScriptDelete, APuzzleGameIsDeletedWhateverTheMode)
{
	using script_type::ScriptDeleteOfAnyThing;
	for (const int32_t mode : {-1, 0, 1, 2, 3, 4})
	{
		EXPECT_EQ(ScriptDeleteOfAnyThing(mode, true, false, false, false), ScriptDelete::PuzzleGame) << mode;
		// any other thing by its mode
		EXPECT_EQ(ScriptDeleteOfAnyThing(mode, false, true, false, false), ScriptDeleteOf(mode, true, false, false)) << mode;
		EXPECT_EQ(ScriptDeleteOfAnyThing(mode, false, false, false, false), ScriptDeleteOf(mode, false, false, false)) << mode;
	}
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
	// a dance, a script's marker and a special effect's particle container: places, not objects
	EXPECT_FALSE(script_type::IsObject(With<Dance>()));
	EXPECT_FALSE(script_type::IsObject(With<ScriptMarker>()));
	EXPECT_FALSE(script_type::IsObject(With<ParticleContainer>()));
	EXPECT_FALSE(script_type::IsObject(ecs::archetypes::ScriptMarkerArchetype::Create(glm::vec3(100.0f, 0.0f, 200.0f))));
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

TEST_F(ScriptIsObject, ARewardATimerAndAScaffoldHaveTheirOwnTypes)
{
	// a reward chest is 7 with its kind as its sub-type, a script timer 17, a scaffold 39 (not its mobile object's 20)
	const auto chest = With<Reward>();
	_registry->Get<Reward>(chest).type = RewardObjectInfo::SmallFood;
	EXPECT_EQ(script_type::TypeOf(chest), script::ObjectType::Reward);
	EXPECT_EQ(static_cast<int>(script::ObjectType::Reward), 7);
	EXPECT_EQ(script_type::SubtypeOf(chest), static_cast<uint32_t>(RewardObjectInfo::SmallFood));
	EXPECT_TRUE(script_type::Matches(chest, script::ObjectType::Reward, static_cast<uint32_t>(RewardObjectInfo::SmallFood)));
	const auto timer = With<ScriptTimer>();
	EXPECT_EQ(script_type::TypeOf(timer), script::ObjectType::Timer);
	EXPECT_EQ(static_cast<int>(script::ObjectType::Timer), 17);
	const auto scaffold = With<Scaffold>();
	_registry->Assign<MobileObject>(scaffold);
	EXPECT_EQ(script_type::TypeOf(scaffold), script::ObjectType::Scaffold);
	EXPECT_EQ(static_cast<int>(script::ObjectType::Scaffold), 39);
	EXPECT_TRUE(script_type::Matches(scaffold, script::ObjectType::Scaffold, script_type::k_AnySubtype));
	EXPECT_FALSE(script_type::Matches(scaffold, script::ObjectType::MobileObject, script_type::k_AnySubtype));
}

TEST_F(ScriptIsObject, NothingIsNoObject)
{
	EXPECT_FALSE(script_type::IsObject(entt::null));
	const auto gone = _registry->Create();
	_registry->Destroy(gone);
	EXPECT_FALSE(script_type::IsObject(gone));
}

TEST_F(ScriptIsObject, CreaturesBuildingsFeaturesAndTreesNeverLean)
{
	// their angles about x and z are always 0, whatever their turn
	EXPECT_TRUE(script_type::NeverLeans(With<Creature>()));
	EXPECT_TRUE(script_type::NeverLeans(With<Animal>()));
	EXPECT_TRUE(script_type::NeverLeans(With<Feature>()));
	EXPECT_TRUE(script_type::NeverLeans(With<Tree>()));
	EXPECT_TRUE(script_type::NeverLeans(With<Flowers>()));
	// rocks, pots and vortices may lean; a town and no thing at all are no objects
	EXPECT_FALSE(script_type::NeverLeans(With<MobileStatic>()));
	EXPECT_FALSE(script_type::NeverLeans(With<Pot>()));
	EXPECT_FALSE(script_type::NeverLeans(With<LandscapeVortex>()));
	EXPECT_FALSE(script_type::NeverLeans(With<Town>()));
	EXPECT_FALSE(script_type::NeverLeans(entt::null));
	// a tree that came down may lean
	const auto fallen = With<Tree>();
	_registry->Assign<DeadTree>(fallen);
	EXPECT_FALSE(script_type::NeverLeans(fallen));
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

TEST_F(ScriptIsObject, AVortexIsTheVortexTypeItsKindItsSubtype)
{
	// SCRIPT_OBJECT_TYPE 23 (VORTEX) for every kind, and its kind as its sub-type, as GAME_SUB_TYPE and the finders read it
	for (const auto kind : {VortexType::In, VortexType::Out, VortexType::Volcano})
	{
		const auto vortex = With<LandscapeVortex>();
		_registry->Get<LandscapeVortex>(vortex).type = kind;
		EXPECT_EQ(script_type::TypeOf(vortex), script::ObjectType::Vortex);
		EXPECT_EQ(script_type::SubtypeOf(vortex), static_cast<uint32_t>(kind));
		EXPECT_TRUE(script_type::Matches(vortex, script::ObjectType::Vortex, static_cast<uint32_t>(kind)));
	}
}

TEST_F(ScriptIsObject, ACreaturesSubtypeIsItsSpeciesAsTheScriptsNumberThem)
{
	// the creature table's own number: the giant ape first (0), then the cow to the gorilla
	const auto ape = With<Creature>();
	_registry->Get<Creature>(ape).species = CreatureType::GiantApe;
	EXPECT_EQ(script_type::SubtypeOf(ape), 0u);
	EXPECT_TRUE(script_type::Matches(ape, script::ObjectType::Creature, 0));
	const auto ogre = With<Creature>();
	_registry->Get<Creature>(ogre).species = CreatureType::Ogre;
	EXPECT_EQ(script_type::SubtypeOf(ogre), static_cast<uint32_t>(CreatureType::Ogre));
}

TEST_F(ScriptIsObject, ASeedAOneShotSpellMadeIsTheOneShotTypeOfItsOwnSeed)
{
	// A seed that tapping a one-shot globe put in the hand is script type 30, and its sub-type is its own seed's row, as a
	// seed from a worship site's icon (type 24) has: a script waits for a held "type 30 5", a physical shield
	EXPECT_EQ(static_cast<int>(script::ObjectType::OneShotSpell), 30);
	const auto fromGlobe = With<SpellSeed>();
	auto& seed = _registry->Get<SpellSeed>(fromGlobe);
	seed.fromOneShot = true;
	seed.seedType = SpellSeedType::PhysicalShield;
	EXPECT_EQ(script_type::TypeOf(fromGlobe), script::ObjectType::OneShotSpell);
	EXPECT_EQ(script_type::SubtypeOf(fromGlobe), 5u);
	EXPECT_TRUE(script_type::Matches(fromGlobe, script::ObjectType::OneShotSpell, 5));
	EXPECT_TRUE(script_type::Matches(fromGlobe, script::ObjectType::OneShotSpell, script_type::k_AnySubtype));
	EXPECT_FALSE(script_type::Matches(fromGlobe, script::ObjectType::OneShotSpell, 4));
	EXPECT_FALSE(script_type::Matches(fromGlobe, script::ObjectType::SpellSeed, 5));

	const auto fromIcon = With<SpellSeed>();
	_registry->Get<SpellSeed>(fromIcon).seedType = SpellSeedType::PhysicalShield;
	EXPECT_EQ(script_type::TypeOf(fromIcon), script::ObjectType::SpellSeed);
	EXPECT_EQ(script_type::SubtypeOf(fromIcon), 5u);
	EXPECT_FALSE(script_type::Matches(fromIcon, script::ObjectType::OneShotSpell, 5));
}

namespace
{
/// OBJECT_DELETE as a script calls it, over the world's lists
class ScriptDeleteNative: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// entity 0 is the scripts' "no thing"
		Locator::entitiesRegistry::value().Create();
		Locator::vm::emplace<lhvm::LHVM>();
		ecs::object_ghosts::Clear();
	}

	void TearDown() override
	{
		ecs::object_ghosts::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	/// OBJECT_DELETE of the thing in the mode: the script pushes the object, then the mode
	void Delete(entt::entity thing, int32_t mode)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "OBJECT_DELETE"; });
		ASSERT_NE(native, table.end());
		auto& vm = Locator::vm::value();
		vm.Pusho(static_cast<uint32_t>(entt::to_integral(thing)));
		vm.Pushi(mode);
		native->impl();
	}

	/// An object with a model, standing at `at`
	static entt::entity ObjectWithAModel(glm::vec3 at)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto thing = registry.Create();
		registry.Assign<Transform>(thing, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(thing, entt::id_type {91}, static_cast<int8_t>(0), static_cast<int8_t>(-1));
		return thing;
	}

	test::RestoreService<Locator::vm> _restoreVm;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(ScriptDeleteNative, ModeOneLeavesAGhostOfTheObjectsModelAndDeletesIt)
{
	const glm::vec3 at(20.0f, 3.0f, 40.0f);
	const auto thing = ObjectWithAModel(at);
	Delete(thing, 1);
	// the ghost is taken before it goes: its model, where it stood
	EXPECT_FALSE(ecs::IsAvailable(thing));
	ASSERT_EQ(ecs::object_ghosts::Count(), 1u);
	const auto draws = ecs::object_ghosts::Instances();
	ASSERT_FALSE(draws.empty());
	EXPECT_EQ(draws[0].meshId, 91u);
	EXPECT_EQ(glm::vec3(draws[0].model[3]), at);
}

TEST_F(ScriptDeleteNative, TheOtherModesLeaveNoGhost)
{
	for (const int32_t mode : {0, 2, 3, 4})
	{
		const auto thing = ObjectWithAModel({20.0f, 3.0f, 40.0f});
		Delete(thing, mode);
		EXPECT_EQ(ecs::object_ghosts::Count(), 0u) << mode;
	}
}
